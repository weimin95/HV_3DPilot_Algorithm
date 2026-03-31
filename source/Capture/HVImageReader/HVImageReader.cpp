#include "HVImageReader.h"
#include "HVUtils.h"
#include "json.hpp"
#include "HVI18n.h"

#include <chrono>
#include <fstream>

namespace {

const hvi18n::Dictionary kTexts = {
    { "algorithm.display", { "图像读取", "Image read" } },
    { "input.image_path.name", { "图像路径", "image path" } },
    { "input.image_path.desc", { "图像文件路径", "Image file path" } },
    { "output.image.name", { "输出图像", "output image" } },
    { "msg.read_failed", { "读取图像失败", "Read image failed" } },
    { "msg.read_success", { "读取图像成功", "Read image success" } }
};

std::string Tr(int language, const std::string& key) {
    return hvi18n::Translate(kTexts, key, language);
}

}  // namespace

HVImageReader::HVImageReader()
{
	
}

HVImageReader::~HVImageReader()
{

}

int HVImageReader::init()
{
	execute_status = NODE_STATUS_NOT_RUN;
	error_msg.clear();
	return SUCCESS;
}

int HVImageReader::run()
{
	auto start = std::chrono::high_resolution_clock::now();

    execute_status = NODE_STATUS_RUNNING;
    error_msg.clear();
	cv::Mat img = cv::imread(image_path, cv::IMREAD_UNCHANGED);
	if (img.empty()) 
	{
		error_msg = "msg.read_failed";
		execute_status = ALGORITHM_RUN_ERROR;
		return ALGORITHM_RUN_ERROR;
	}
    execute_status = SUCCESS;
	resultImg = std::make_shared<ImageDataInfo2D>(ImageConverter::FromMat(img));

	auto end = std::chrono::high_resolution_clock::now();
	run_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	error_msg = "msg.read_success";

	return SUCCESS;
}

int HVImageReader::set_algorithm_params(const std::vector<void*>& params, const std::vector<int>& paramID)
{
	if (params.size() <1 || params[0] == nullptr)
	{
		return INVALID_PARAMS_NUM;
	}

	image_path = cast_param<std::string>(params, 0);

	return SUCCESS;
}

std::vector<void*> HVImageReader::get_algorithm_result()
{
	if (execute_status == SUCCESS)
	    return { &resultImg, &execute_status };
	return { nullptr, &execute_status };
}

std::vector<int> HVImageReader::get_algorithm_input_params_type()
{
	return { HV_STRING };
}

std::vector<int> HVImageReader::get_algorithm_output_params_type()
{
	return { HV_IMAGEDATAINFO2D, HV_INT };
}

std::vector<std::string> HVImageReader::get_algorithm_input_params_name()
{
	return { Tr(language_, "input.image_path.name") };
}

std::vector<std::string> HVImageReader::get_algorithm_output_params_name()
{
	return {
        Tr(language_, "output.image.name"),
        language_ == static_cast<int>(UIPilotLanguage::EN_US) ? "Execute status" : "运行状态"
    };
}

std::vector<bool> HVImageReader::get_algorithm_input_params_bindable()
{
    // Used as the default UI input mode hint, not as a hard binding restriction.
    return { false };
}

std::vector<ParamMetadata> HVImageReader::get_algorithm_input_params_metadata()
{
    std::vector<ParamMetadata> metadata_list;

    // 参数0: 图像文件路径 (文件路径约束)
    ParamMetadata meta0;
    meta0.param_name = Tr(language_, "input.image_path.name");
    meta0.param_description = Tr(language_, "input.image_path.desc");
    meta0.param_type = HV_STRING;
    meta0.constraint_type = CONSTRAINT_FILE_PATH;
    meta0.file_filter = "*.jpg;*.jpeg;*.png;*.bmp;*.tiff;*.tif";
    metadata_list.push_back(meta0);

    return metadata_list;
}

int HVImageReader::get_algorithm_execute_status()
{
	return execute_status;
}

std::string HVImageReader::get_algorithm_error_message()
{
    if (error_msg.empty()) {
        return "";
    }
	return Tr(language_, error_msg);
}

long HVImageReader::get_algorithm_use_time()
{
	return run_time;
}

bool HVImageReader::algorithm_params_setting_status()
{
	return true;
}

bool HVImageReader::algorithm_init_status()
{
	return true;
}

bool HVImageReader::save_params_to_json(const std::string& filePath)
{
    try {
        nlohmann::json params_json = nlohmann::json::array();

        add_param(params_json, "image_path", HV_STRING, this->image_path);

        // 写入文件
        std::ofstream file(filePath);
        if (!file.is_open()) {
            return false;
        }
        file << params_json.dump(4);
        file.close();

        return true;
    }
    catch (...) {
        return false;
    }
}

bool HVImageReader::load_params_from_json(const std::string& filePath)
{
    try {
        // 读取文件
        std::ifstream file(filePath);
        if (!file.is_open()) {
            return false;
        }

        nlohmann::json params_json;
        file >> params_json;
        file.close();

        // 检查JSON是否为数组
        if (!params_json.is_array()) {
            return false;
        }

        // 遍历参数数组
        for (const auto& param_json : params_json) {
            // 检查必要字段是否存在
            if (!param_json.contains("name") || !param_json.contains("type")) {
                continue;
            }

            std::string param_name = param_json["name"];
            int param_type = param_json["type"];

            // 根据参数名称进行处理
            if (param_name == "image_path") {
                // 设置到类成员变量
                this->image_path = param_json["value"];
            }
        }

        return true;
    }
    catch (...) {
        return false;
    }
}

std::vector<void*> HVImageReader::get_current_params()
{
	return { &image_path };
}

AlgorithmType HVImageReader::get_algorithm_type()
{
    return AlgorithmType::Capture;
}

void HVImageReader::set_language(int language)
{
    if (hvi18n::IsSupportedLanguage(language)) {
        language_ = language;
    }
}

int HVImageReader::get_language() const
{
    return language_;
}

std::string HVImageReader::get_algorithm_display_name()
{
    return Tr(language_, "algorithm.display");
}

NodeEngine* CreateInstance() {
	// 每一个 DLL 内部返回自己具体的实现类
	return new HVImageReader();
}

std::string GetInstanceName() {
	return "Image read"; // 告知主程序此 DLL 代表的类型
}

extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion() {
    return NODE_ENGINE_ABI_VERSION;
}
