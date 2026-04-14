#include "HVLineLaser3DCamera.h"

#include "HVLineLaser3DCameraConversion.h"
#include "HVUtils.h"
#include "json.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <sstream>

namespace {

std::mutex g_active_instance_mutex;
HVLineLaser3DCamera* g_active_instance = nullptr;

std::string SdkErrorMessage(const std::string& operation, int code)
{
    std::ostringstream oss;
    oss << operation << " failed, SDK error code: " << code;
    return oss.str();
}

bool ReadIpv4Part(const std::string& token, unsigned char& out_value)
{
    if (token.empty()) {
        return false;
    }

    int value = -1;
    std::istringstream stream(token);
    stream >> value;
    if (!stream || !stream.eof() || value < 0 || value > 255) {
        return false;
    }

    out_value = static_cast<unsigned char>(value);
    return true;
}

}  // namespace

HVLineLaser3DCamera::HVLineLaser3DCamera()
    : contract_(line_laser_3d_camera::BuildContract())
    , ip_("192.168.0.10")
{
    ClearOutputs();
}

HVLineLaser3DCamera::~HVLineLaser3DCamera()
{
    CloseCamera();
}

int HVLineLaser3DCamera::init()
{
    if (initialized_ && connected_) {
        return SUCCESS;
    }

    SR7IF_ETHERNET_CONFIG config = {};
    const int parse_result = ParseIpConfig(config);
    if (parse_result != SUCCESS) {
        return parse_result;
    }

    int sdk_result = SR7IF_EthernetOpenExt(
        static_cast<unsigned int>(device_id_),
        &config,
        connect_timeout_ms_,
        nullptr);
    if (sdk_result != SR7IF_OK) {
        SetFailure(ALGORITHM_RUN_ERROR, SdkErrorMessage("SR7IF_EthernetOpenExt", sdk_result));
        return ALGORITHM_RUN_ERROR;
    }

    connected_ = true;

    int target[4] = { 0, 0, 0, 0 };
    int batch_points = batch_points_;
    sdk_result = SR7IF_SetSetting(
        static_cast<unsigned int>(device_id_),
        0x01,
        -1,
        0x00,
        0x0a,
        target,
        &batch_points,
        2);
    if (sdk_result != SR7IF_OK) {
        CloseCamera();
        SetFailure(ALGORITHM_RUN_ERROR, SdkErrorMessage("SR7IF_SetSetting(batch_points)", sdk_result));
        return ALGORITHM_RUN_ERROR;
    }

    {
        std::lock_guard<std::mutex> guard(g_active_instance_mutex);
        g_active_instance = this;
    }

    sdk_result = SR7IF_SetBatchOneTimeDataHandler(
        static_cast<unsigned int>(device_id_),
        BatchOneTimeCallback);
    if (sdk_result != SR7IF_OK) {
        CloseCamera();
        SetFailure(ALGORITHM_RUN_ERROR, SdkErrorMessage("SR7IF_SetBatchOneTimeDataHandler", sdk_result));
        return ALGORITHM_RUN_ERROR;
    }

    sdk_result = SR7IF_StartMeasureWithCallback(device_id_, 1);
    if (sdk_result != SR7IF_OK) {
        CloseCamera();
        SetFailure(ALGORITHM_RUN_ERROR, SdkErrorMessage("SR7IF_StartMeasureWithCallback", sdk_result));
        return ALGORITHM_RUN_ERROR;
    }

    initialized_ = true;
    execute_status_ = NODE_STATUS_NOT_RUN;
    error_message_.clear();
    return SUCCESS;
}

int HVLineLaser3DCamera::run()
{
    const auto start_time = std::chrono::steady_clock::now();
    ClearOutputs();
    ResetCallbackState();

    const int init_result = EnsureInitialized();
    if (init_result != SUCCESS) {
        return init_result;
    }

    {
        std::lock_guard<std::mutex> guard(g_active_instance_mutex);
        g_active_instance = this;
    }

    execute_status_ = NODE_STATUS_RUNNING;
    const int sdk_result = SR7IF_TriggerOneBatch(device_id_);
    if (sdk_result != SR7IF_OK) {
        SetFailure(ALGORITHM_RUN_ERROR, SdkErrorMessage("SR7IF_TriggerOneBatch", sdk_result));
        return ALGORITHM_RUN_ERROR;
    }

    std::unique_lock<std::mutex> lock(mutex_);
    const bool received = callback_cv_.wait_for(
        lock,
        std::chrono::milliseconds(wait_timeout_ms_),
        [this]() { return callback_received_; });
    if (!received) {
        lock.unlock();
        SetFailure(ALGORITHM_RUN_ERROR, "Timed out waiting for line laser camera batch callback");
        return ALGORITHM_RUN_ERROR;
    }

    if (callback_status_ != SR7IF_OK) {
        const int callback_status = callback_status_;
        lock.unlock();
        SetFailure(ALGORITHM_RUN_ERROR, SdkErrorMessage("batch callback", callback_status));
        return ALGORITHM_RUN_ERROR;
    }
    lock.unlock();

    const double x_pitch_mm = SR7IF_ProfileData_XPitch(static_cast<unsigned int>(device_id_), nullptr);
    const int output_result = BuildOutputsFromLastFrame(x_pitch_mm);
    if (output_result != SUCCESS) {
        return output_result;
    }

    const auto end_time = std::chrono::steady_clock::now();
    use_time_ms_ = static_cast<long>(
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count());
    execute_status_ = SUCCESS;
    error_message_.clear();
    return SUCCESS;
}

int HVLineLaser3DCamera::set_algorithm_params(const std::vector<void*>& params, const std::vector<int>& paramID)
{
    if (params.empty()) {
        return SUCCESS;
    }

    if (paramID.empty()) {
        if (params.size() != contract_.input_types.size()) {
            return INVALID_PARAMS_NUM;
        }
        for (size_t i = 0; i < params.size(); ++i) {
            const int result = ApplyParam(static_cast<int>(i), params[i]);
            if (result != SUCCESS) {
                return result;
            }
        }
    }
    else {
        if (params.size() != paramID.size()) {
            return INVALID_PARAMS_NUM;
        }
        for (size_t i = 0; i < params.size(); ++i) {
            const int result = ApplyParam(paramID[i], params[i]);
            if (result != SUCCESS) {
                return result;
            }
        }
    }

    params_set_ = true;
    CloseCamera();
    return SUCCESS;
}

std::vector<void*> HVLineLaser3DCamera::get_current_params()
{
    return {
        &ip_,
        &device_id_,
        &batch_points_,
        &head_index_,
        &connect_timeout_ms_,
        &wait_timeout_ms_,
        &y_pitch_mm_,
    };
}

std::vector<void*> HVLineLaser3DCamera::get_algorithm_result()
{
    return {
        &point_cloud_,
        &depth_image_,
        &gray_image_,
        &execute_status_,
    };
}

std::vector<int> HVLineLaser3DCamera::get_algorithm_input_params_type()
{
    return contract_.input_types;
}

std::vector<int> HVLineLaser3DCamera::get_algorithm_output_params_type()
{
    return contract_.output_types;
}

std::vector<std::string> HVLineLaser3DCamera::get_algorithm_input_params_name()
{
    return contract_.input_names;
}

std::vector<std::string> HVLineLaser3DCamera::get_algorithm_output_params_name()
{
    return contract_.output_names;
}

std::vector<bool> HVLineLaser3DCamera::get_algorithm_input_params_bindable()
{
    return contract_.input_bindable_hints;
}

std::vector<ParamMetadata> HVLineLaser3DCamera::get_algorithm_input_params_metadata()
{
    return contract_.input_metadata;
}

int HVLineLaser3DCamera::get_algorithm_execute_status()
{
    return execute_status_;
}

std::string HVLineLaser3DCamera::get_algorithm_error_message()
{
    return error_message_;
}

long HVLineLaser3DCamera::get_algorithm_use_time()
{
    return use_time_ms_;
}

bool HVLineLaser3DCamera::algorithm_params_setting_status()
{
    return params_set_;
}

bool HVLineLaser3DCamera::algorithm_init_status()
{
    return initialized_;
}

bool HVLineLaser3DCamera::save_params_to_json(const std::string& filePath)
{
    try {
        nlohmann::json params_json = nlohmann::json::array();
        add_param(params_json, "ip", HV_STRING, ip_);
        add_param(params_json, "device_id", HV_INT, device_id_);
        add_param(params_json, "batch_points", HV_INT, batch_points_);
        add_param(params_json, "head_index", HV_INT, head_index_);
        add_param(params_json, "connect_timeout_ms", HV_INT, connect_timeout_ms_);
        add_param(params_json, "wait_timeout_ms", HV_INT, wait_timeout_ms_);
        add_param(params_json, "y_pitch_mm", HV_DOUBLE, y_pitch_mm_);

        std::ofstream file(filePath);
        if (!file.is_open()) {
            return false;
        }
        file << params_json.dump(4);
        return true;
    }
    catch (...) {
        return false;
    }
}

bool HVLineLaser3DCamera::load_params_from_json(const std::string& filePath)
{
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            return false;
        }

        nlohmann::json params_json;
        file >> params_json;
        if (!params_json.is_array()) {
            return false;
        }

        for (const auto& param_json : params_json) {
            if (!param_json.contains("name") || !param_json.contains("value")) {
                continue;
            }
            const std::string name = param_json["name"];
            if (name == "ip") {
                ip_ = param_json["value"].get<std::string>();
            }
            else if (name == "device_id") {
                device_id_ = param_json["value"].get<int>();
            }
            else if (name == "batch_points") {
                batch_points_ = param_json["value"].get<int>();
            }
            else if (name == "head_index") {
                head_index_ = param_json["value"].get<int>();
            }
            else if (name == "connect_timeout_ms") {
                connect_timeout_ms_ = param_json["value"].get<int>();
            }
            else if (name == "wait_timeout_ms") {
                wait_timeout_ms_ = param_json["value"].get<int>();
            }
            else if (name == "y_pitch_mm") {
                y_pitch_mm_ = param_json["value"].get<double>();
            }
        }

        CloseCamera();
        return true;
    }
    catch (...) {
        return false;
    }
}

AlgorithmType HVLineLaser3DCamera::get_algorithm_type()
{
    return contract_.algorithm_type;
}

void HVLineLaser3DCamera::set_language(int language)
{
    if (language == static_cast<int>(UIPilotLanguage::ZH_CN) ||
        language == static_cast<int>(UIPilotLanguage::EN_US)) {
        language_ = language;
    }
}

int HVLineLaser3DCamera::get_language() const
{
    return language_;
}

std::string HVLineLaser3DCamera::get_algorithm_display_name()
{
    return language_ == static_cast<int>(UIPilotLanguage::EN_US)
        ? contract_.en_display_name
        : contract_.zh_display_name;
}

void HVLineLaser3DCamera::BatchOneTimeCallback(const void* info, const SR7IF_Data* data)
{
    HVLineLaser3DCamera* instance = nullptr;
    {
        std::lock_guard<std::mutex> guard(g_active_instance_mutex);
        instance = g_active_instance;
    }

    if (instance != nullptr) {
        instance->OnBatchOneTimeData(info, data);
    }
}

int HVLineLaser3DCamera::ApplyParam(int param_id, void* param)
{
    if (param == nullptr || param_id < 0 || param_id >= static_cast<int>(contract_.input_types.size())) {
        return INVALID_PARAMS_NUM;
    }

    switch (param_id) {
    case 0:
        ip_ = *static_cast<std::string*>(param);
        return SUCCESS;
    case 1: {
        const int value = *static_cast<int*>(param);
        if (value < 0 || value > 63) {
            return INVALID_PARAMS_NUM;
        }
        device_id_ = value;
        return SUCCESS;
    }
    case 2: {
        const int value = *static_cast<int*>(param);
        if (value <= 0 || value > 65535) {
            return INVALID_PARAMS_NUM;
        }
        batch_points_ = value;
        return SUCCESS;
    }
    case 3: {
        const int value = *static_cast<int*>(param);
        if (value < 0 || value > 1) {
            return INVALID_PARAMS_NUM;
        }
        head_index_ = value;
        return SUCCESS;
    }
    case 4: {
        const int value = *static_cast<int*>(param);
        if (value < 100) {
            return INVALID_PARAMS_NUM;
        }
        connect_timeout_ms_ = value;
        return SUCCESS;
    }
    case 5: {
        const int value = *static_cast<int*>(param);
        if (value <= 0) {
            return INVALID_PARAMS_NUM;
        }
        wait_timeout_ms_ = value;
        return SUCCESS;
    }
    case 6: {
        const double value = *static_cast<double*>(param);
        if (value < 0.0) {
            return INVALID_PARAMS_NUM;
        }
        y_pitch_mm_ = value;
        return SUCCESS;
    }
    default:
        return INVALID_PARAMS_NUM;
    }
}

int HVLineLaser3DCamera::EnsureInitialized()
{
    if (initialized_ && connected_) {
        return SUCCESS;
    }
    return init();
}

int HVLineLaser3DCamera::ParseIpConfig(SR7IF_ETHERNET_CONFIG& config)
{
    std::istringstream ip_stream(ip_);
    std::string token;
    for (int i = 0; i < 4; ++i) {
        if (!std::getline(ip_stream, token, '.') ||
            !ReadIpv4Part(token, config.abyIpAddress[i])) {
            SetFailure(INVALID_PARAMS_NUM, "Invalid line laser camera IP address");
            return INVALID_PARAMS_NUM;
        }
    }

    if (std::getline(ip_stream, token, '.')) {
        SetFailure(INVALID_PARAMS_NUM, "Invalid line laser camera IP address");
        return INVALID_PARAMS_NUM;
    }
    return SUCCESS;
}

int HVLineLaser3DCamera::BuildOutputsFromLastFrame(double x_pitch_mm)
{
    std::vector<int> height_data;
    std::vector<unsigned char> gray_data;
    int frame_width = 0;
    int frame_height = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        height_data = height_buffer_;
        gray_data = gray_buffer_;
        frame_width = frame_width_;
        frame_height = frame_height_;
    }

    const size_t expected_count = static_cast<size_t>(std::max(frame_width, 0)) *
        static_cast<size_t>(std::max(frame_height, 0));
    if (frame_width <= 0 || frame_height <= 0 || height_data.size() < expected_count) {
        SetFailure(ALGORITHM_RUN_ERROR, "Line laser camera returned an empty frame");
        return ALGORITHM_RUN_ERROR;
    }

    point_cloud_ = std::make_shared<HVPointCloud>(
        line_laser_3d_camera::BuildPointCloud(
            height_data,
            static_cast<size_t>(frame_width),
            static_cast<size_t>(frame_height),
            x_pitch_mm,
            y_pitch_mm_));
    depth_image_ = std::make_shared<ImageDataInfoDepth>(
        line_laser_3d_camera::BuildDepthImage(
            height_data,
            static_cast<size_t>(frame_width),
            static_cast<size_t>(frame_height)));
    gray_image_ = std::make_shared<ImageDataInfo2D>(
        line_laser_3d_camera::BuildGrayImage(
            gray_data,
            static_cast<size_t>(frame_width),
            static_cast<size_t>(frame_height)));
    return SUCCESS;
}

void HVLineLaser3DCamera::OnBatchOneTimeData(const void* info, const SR7IF_Data* data)
{
    std::lock_guard<std::mutex> lock(mutex_);
    callback_received_ = true;

    const auto* callback_info = static_cast<const SR7IF_STR_CALLBACK_INFO*>(info);
    if (callback_info == nullptr) {
        callback_status_ = ALGORITHM_RUN_ERROR;
        callback_cv_.notify_all();
        return;
    }

    callback_status_ = callback_info->returnStatus;
    if (callback_info->returnStatus != SR7IF_OK) {
        callback_cv_.notify_all();
        return;
    }

    if (head_index_ < 0 || head_index_ >= callback_info->HeadNumber) {
        callback_status_ = SR7IF_ERROR_CAMERA_NOT_ONLINE;
        callback_cv_.notify_all();
        return;
    }

    frame_width_ = callback_info->xPoints;
    frame_height_ = callback_info->BatchPoints;
    const size_t point_count = static_cast<size_t>(std::max(frame_width_, 0)) *
        static_cast<size_t>(std::max(frame_height_, 0));
    if (point_count == 0U) {
        callback_status_ = ALGORITHM_RUN_ERROR;
        callback_cv_.notify_all();
        return;
    }

    const int* height_data = SR7IF_GetBatchProfilePoint(data, head_index_);
    if (height_data == nullptr) {
        callback_status_ = ALGORITHM_RUN_ERROR;
        callback_cv_.notify_all();
        return;
    }
    height_buffer_.assign(height_data, height_data + point_count);

    gray_buffer_.assign(point_count, 0);
    const unsigned char* gray_data = SR7IF_GetBatchIntensityPoint(data, head_index_);
    if (gray_data != nullptr) {
        std::memcpy(gray_buffer_.data(), gray_data, point_count);
    }

    encoder_buffer_.assign(static_cast<size_t>(std::max(frame_height_, 0)), 0U);
    const unsigned int* encoder_data = SR7IF_GetBatchEncoderPoint(data, head_index_);
    if (encoder_data != nullptr && frame_height_ > 0) {
        std::memcpy(
            encoder_buffer_.data(),
            encoder_data,
            static_cast<size_t>(frame_height_) * sizeof(unsigned int));
    }

    callback_cv_.notify_all();
}

void HVLineLaser3DCamera::CloseCamera()
{
    {
        std::lock_guard<std::mutex> guard(g_active_instance_mutex);
        if (g_active_instance == this) {
            g_active_instance = nullptr;
        }
    }

    if (connected_) {
        SR7IF_StopMeasure(static_cast<unsigned int>(device_id_));
        SR7IF_CommClose(static_cast<unsigned int>(device_id_));
    }
    connected_ = false;
    initialized_ = false;
}

void HVLineLaser3DCamera::ClearOutputs()
{
    point_cloud_ = std::make_shared<HVPointCloud>();
    depth_image_ = std::make_shared<ImageDataInfoDepth>();
    gray_image_ = std::make_shared<ImageDataInfo2D>();
}

void HVLineLaser3DCamera::ResetCallbackState()
{
    std::lock_guard<std::mutex> lock(mutex_);
    callback_received_ = false;
    callback_status_ = NODE_STATUS_NOT_RUN;
    frame_width_ = 0;
    frame_height_ = 0;
    height_buffer_.clear();
    gray_buffer_.clear();
    encoder_buffer_.clear();
}

void HVLineLaser3DCamera::SetFailure(int status, const std::string& message)
{
    execute_status_ = status;
    error_message_ = message;
}

extern "C" __declspec(dllexport) NodeEngine* CreateInstance()
{
    return new HVLineLaser3DCamera();
}

extern "C" __declspec(dllexport) std::string GetInstanceName()
{
    return line_laser_3d_camera::BuildContract().algorithm_name;
}

extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion()
{
    return NODE_ENGINE_ABI_VERSION;
}
