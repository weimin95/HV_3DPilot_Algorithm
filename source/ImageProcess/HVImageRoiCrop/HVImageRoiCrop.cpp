#include "HVImageRoiCrop.h"

#include "HVI18n.h"
#include "HVUtils.h"
#include "3d_pliot_error.h"

#include <chrono>
#include <cstring>
#include <fstream>

namespace {

constexpr int kInputTypeImage2D = 0;
constexpr int kInputTypeDepth = 1;
constexpr int kInputTypePointCloud = 2;

const hvi18n::Dictionary kTexts = {
    { "algorithm.display", { "ROI裁剪", "Image ROI crop" } },
    { "input.type.name", { "输入类型", "input type" } },
    { "input.image.name", { "输入2D图像", "input 2D image" } },
    { "input.depth.name", { "输入深度图", "input depth image" } },
    { "input.cloud.name", { "输入点云", "input point cloud" } },
    { "input.roi.name", { "ROI", "ROI" } },
    { "output.image.name", { "裁剪2D图像", "cropped 2D image" } },
    { "output.depth.name", { "裁剪深度图", "cropped depth image" } },
    { "output.cloud.name", { "裁剪点云", "cropped point cloud" } },
    { "input.type.desc", { "选择本次裁剪使用的输入数据类型", "Select the input data type to crop" } },
    { "input.image.desc", { "待裁剪的2D图像，仅输入类型为2D图像时使用", "2D image used when input type is 2D image" } },
    { "input.depth.desc", { "待裁剪的深度图，仅输入类型为深度图时使用", "Depth image used when input type is depth image" } },
    { "input.cloud.desc", { "待裁剪的点云，仅输入类型为点云时使用", "Point cloud used when input type is point cloud" } },
    { "input.roi.desc", { "2D图像/深度图使用旋转矩形，点云使用长方体或旋转长方体", "Use rotated rectangle for images/depth images, box or rotated box for point clouds" } },
    { "option.image", { "2D图像", "2D image" } },
    { "option.depth", { "深度图", "Depth image" } },
    { "option.cloud", { "点云", "Point cloud" } },
    { "msg.image_input_null", { "输入2D图像为空", "Input 2D image is null" } },
    { "msg.depth_input_null", { "输入深度图为空", "Input depth image is null" } },
    { "msg.cloud_input_null", { "输入点云为空", "Input point cloud is null" } },
    { "msg.invalid_image_roi", { "2D图像和深度图裁剪仅支持矩形或旋转矩形ROI", "2D image and depth image crop only support rectangle or rotated rectangle ROI" } },
    { "msg.invalid_cloud_roi", { "点云裁剪仅支持长方体或旋转长方体ROI", "Point cloud crop only supports box or rotated box ROI" } },
    { "msg.unsupported_input_type", { "不支持的输入类型", "Unsupported input type" } },
    { "msg.image_success", { "2D图像ROI裁剪成功", "2D image ROI crop success" } },
    { "msg.depth_success", { "深度图ROI裁剪成功", "Depth image ROI crop success" } },
    { "msg.cloud_success", { "点云ROI裁剪成功", "Point cloud ROI crop success" } }
};

std::string Tr(int language, const std::string& key) {
    return hvi18n::Translate(kTexts, key, language);
}

bool IsImageCropGeometry(const HVGeometryInfo& roi) {
    return roi.shape_type_ == HVGeometryShapeType::Rectangle ||
        roi.shape_type_ == HVGeometryShapeType::RotatedRectangle;
}

bool IsPointCloudCropGeometry(const HVGeometryInfo& roi) {
    return roi.shape_type_ == HVGeometryShapeType::Box ||
        roi.shape_type_ == HVGeometryShapeType::RotatedBox;
}

HVGeometryInfo NormalizeImageCropGeometry(const HVGeometryInfo& roi) {
    if (roi.shape_type_ != HVGeometryShapeType::Rectangle) {
        return roi;
    }

    HVGeometryInfo normalized = roi;
    const HVRect& rect = roi.AsRect();
    normalized.SetRotatedRect(HVRotatedRect(rect.Center(), rect.width_, rect.height_, 0.0));
    return normalized;
}

bool BuildMaskedDepthFromRoi(
    const HVGeometryInfo& roi,
    const ImageDataInfoDepth& src_depth,
    ImageDataInfoDepth& out_depth)
{
    if (src_depth.empty()) {
        out_depth = ImageDataInfoDepth();
        return false;
    }

    cv::Mat mask;
    if (!BuildRoiMask(roi, static_cast<int>(src_depth.width), static_cast<int>(src_depth.height), mask)) {
        out_depth = ImageDataInfoDepth();
        return false;
    }

    out_depth = ImageDataInfoDepth(
        src_depth.width,
        src_depth.height);

    const size_t pixel_size = src_depth.getPixelSize();
    const unsigned char* src_data = reinterpret_cast<const unsigned char*>(src_depth.image_data);
    unsigned char* dst_data = reinterpret_cast<unsigned char*>(out_depth.image_data);
    for (size_t row = 0; row < src_depth.height; ++row) {
        for (size_t col = 0; col < src_depth.width; ++col) {
            if (mask.at<unsigned char>(static_cast<int>(row), static_cast<int>(col)) == 0) {
                continue;
            }
            const size_t offset = (row * src_depth.width + col) * pixel_size;
            std::memcpy(dst_data + offset, src_data + offset, pixel_size);
        }
    }
    return true;
}

}  // namespace

HVImageRoiCrop::HVImageRoiCrop() = default;

int HVImageRoiCrop::init()
{
    execute_status_ = NODE_STATUS_NOT_RUN;
    run_time_ms_ = 0;
    error_message_key_.clear();
    ClearOutputs();
    return SUCCESS;
}

int HVImageRoiCrop::run()
{
    const auto start = std::chrono::steady_clock::now();
    execute_status_ = NODE_STATUS_RUNNING;
    error_message_key_.clear();
    ClearOutputs();

    int result = SUCCESS;
    switch (input_type_) {
    case kInputTypeImage2D:
        result = RunImageCrop();
        break;
    case kInputTypeDepth:
        result = RunDepthCrop();
        break;
    case kInputTypePointCloud:
        result = RunPointCloudCrop();
        break;
    default:
        SetFailure(ALGORITHM_RUN_ERROR, "msg.unsupported_input_type");
        result = ALGORITHM_RUN_ERROR;
        break;
    }

    const auto end = std::chrono::steady_clock::now();
    run_time_ms_ = static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
    return result;
}

int HVImageRoiCrop::set_algorithm_params(const std::vector<void*>& params, const std::vector<int>& paramID)
{
    if (paramID.empty()) {
        for (int param_id = 0; param_id < static_cast<int>(params.size()); ++param_id) {
            const int result = ApplyParam(param_id, params[param_id]);
            if (result != SUCCESS) {
                return result;
            }
        }
        return SUCCESS;
    }

    if (params.size() != paramID.size()) {
        return INVALID_PARAMS_NUM;
    }

    for (size_t i = 0; i < paramID.size(); ++i) {
        const int result = ApplyParam(paramID[i], params[i]);
        if (result != SUCCESS) {
            return result;
        }
    }
    return SUCCESS;
}

std::vector<void*> HVImageRoiCrop::get_current_params()
{
    return { &input_type_, &input_2d_image_, &input_depth_image_, &input_point_cloud_, &roi_ };
}

std::vector<void*> HVImageRoiCrop::get_algorithm_result()
{
    if (execute_status_ == SUCCESS) {
        return {
            cropped_2d_image_ ? static_cast<void*>(&cropped_2d_image_) : nullptr,
            cropped_depth_image_ ? static_cast<void*>(&cropped_depth_image_) : nullptr,
            cropped_point_cloud_ ? static_cast<void*>(&cropped_point_cloud_) : nullptr,
            &execute_status_
        };
    }
    return { nullptr, nullptr, nullptr, &execute_status_ };
}

std::vector<int> HVImageRoiCrop::get_algorithm_input_params_type()
{
    return { HV_INT, HV_IMAGEDATAINFO2D, HV_IMAGEDATAINFODEPTH, HV_POINTCLOUD, HV_IMAGEROI };
}

std::vector<int> HVImageRoiCrop::get_algorithm_output_params_type()
{
    return { HV_IMAGEDATAINFO2D, HV_IMAGEDATAINFODEPTH, HV_POINTCLOUD, HV_INT };
}

std::vector<std::string> HVImageRoiCrop::get_algorithm_input_params_name()
{
    return {
        Tr(language_, "input.type.name"),
        Tr(language_, "input.image.name"),
        Tr(language_, "input.depth.name"),
        Tr(language_, "input.cloud.name"),
        Tr(language_, "input.roi.name")
    };
}

std::vector<std::string> HVImageRoiCrop::get_algorithm_output_params_name()
{
    return {
        Tr(language_, "output.image.name"),
        Tr(language_, "output.depth.name"),
        Tr(language_, "output.cloud.name"),
        language_ == static_cast<int>(UIPilotLanguage::EN_US) ? "Execute status" : "运行状态"
    };
}

std::vector<bool> HVImageRoiCrop::get_algorithm_input_params_bindable()
{
    // 这里只作为默认输入模式提示。三个数据输入口仍可绑定，但默认不强制绑定未选择的输入类型。
    return { false, false, false, false, false };
}

std::vector<ParamMetadata> HVImageRoiCrop::get_algorithm_input_params_metadata()
{
    std::vector<ParamMetadata> metadata_list;

    ParamMetadata input_type_meta;
    input_type_meta.param_name = Tr(language_, "input.type.name");
    input_type_meta.param_description = Tr(language_, "input.type.desc");
    input_type_meta.param_type = HV_INT;
    input_type_meta.constraint_type = CONSTRAINT_OPTIONS;
    input_type_meta.options_constraint.AddOption(kInputTypeImage2D, Tr(language_, "option.image"));
    input_type_meta.options_constraint.AddOption(kInputTypeDepth, Tr(language_, "option.depth"));
    input_type_meta.options_constraint.AddOption(kInputTypePointCloud, Tr(language_, "option.cloud"));
    input_type_meta.options_constraint.default_index = 0;
    input_type_meta.param_group = PARAM_GROUP_BASIC;
    metadata_list.push_back(input_type_meta);

    ParamMetadata image_meta;
    image_meta.param_name = Tr(language_, "input.image.name");
    image_meta.param_description = Tr(language_, "input.image.desc");
    image_meta.param_type = HV_IMAGEDATAINFO2D;
    image_meta.constraint_type = CONSTRAINT_NONE;
    image_meta.param_group = PARAM_GROUP_BASIC;
    image_meta.dependencies.push_back(ParamDependency(0, DEPENDS_ON_EQUALS, { "0" }));
    metadata_list.push_back(image_meta);

    ParamMetadata depth_meta;
    depth_meta.param_name = Tr(language_, "input.depth.name");
    depth_meta.param_description = Tr(language_, "input.depth.desc");
    depth_meta.param_type = HV_IMAGEDATAINFODEPTH;
    depth_meta.constraint_type = CONSTRAINT_NONE;
    depth_meta.param_group = PARAM_GROUP_BASIC;
    depth_meta.dependencies.push_back(ParamDependency(0, DEPENDS_ON_EQUALS, { "1" }));
    metadata_list.push_back(depth_meta);

    ParamMetadata cloud_meta;
    cloud_meta.param_name = Tr(language_, "input.cloud.name");
    cloud_meta.param_description = Tr(language_, "input.cloud.desc");
    cloud_meta.param_type = HV_POINTCLOUD;
    cloud_meta.constraint_type = CONSTRAINT_NONE;
    cloud_meta.param_group = PARAM_GROUP_BASIC;
    cloud_meta.dependencies.push_back(ParamDependency(0, DEPENDS_ON_EQUALS, { "2" }));
    metadata_list.push_back(cloud_meta);

    ParamMetadata roi_meta;
    roi_meta.param_name = Tr(language_, "input.roi.name");
    roi_meta.param_description = Tr(language_, "input.roi.desc");
    roi_meta.param_type = HV_IMAGEROI;
    roi_meta.constraint_type = CONSTRAINT_NONE;
    roi_meta.param_group = PARAM_GROUP_BASIC;
    metadata_list.push_back(roi_meta);

    return metadata_list;
}

int HVImageRoiCrop::get_algorithm_execute_status()
{
    return execute_status_;
}

std::string HVImageRoiCrop::get_algorithm_error_message()
{
    if (error_message_key_.empty()) {
        return "";
    }
    return Tr(language_, error_message_key_);
}

long HVImageRoiCrop::get_algorithm_use_time()
{
    return run_time_ms_;
}

bool HVImageRoiCrop::algorithm_params_setting_status()
{
    return true;
}

bool HVImageRoiCrop::algorithm_init_status()
{
    return true;
}

bool HVImageRoiCrop::save_params_to_json(const std::string& filePath)
{
    try {
        nlohmann::json params_json = nlohmann::json::array();
        add_param(params_json, "input_type", HV_INT, input_type_);

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

bool HVImageRoiCrop::load_params_from_json(const std::string& filePath)
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
            if (param_json["name"].get<std::string>() == "input_type") {
                input_type_ = param_json["value"].get<int>();
            }
        }
        return true;
    }
    catch (...) {
        return false;
    }
}

AlgorithmType HVImageRoiCrop::get_algorithm_type()
{
    return AlgorithmType::ImageProcess;
}

void HVImageRoiCrop::set_language(int language)
{
    if (hvi18n::IsSupportedLanguage(language)) {
        language_ = language;
    }
}

int HVImageRoiCrop::get_language() const
{
    return language_;
}

std::string HVImageRoiCrop::get_algorithm_display_name()
{
    return Tr(language_, "algorithm.display");
}

int HVImageRoiCrop::ApplyParam(int param_id, void* param)
{
    switch (param_id) {
    case 0:
        input_type_ = param == nullptr ? kInputTypeImage2D : *static_cast<int*>(param);
        return SUCCESS;
    case 1:
        input_2d_image_ = param == nullptr ? nullptr : *static_cast<std::shared_ptr<ImageDataInfo2D>*>(param);
        return SUCCESS;
    case 2:
        input_depth_image_ = param == nullptr ? nullptr : *static_cast<std::shared_ptr<ImageDataInfoDepth>*>(param);
        return SUCCESS;
    case 3:
        input_point_cloud_ = param == nullptr ? nullptr : *static_cast<std::shared_ptr<HVPointCloud>*>(param);
        return SUCCESS;
    case 4:
        roi_ = param == nullptr ? HVGeometryInfo() : *static_cast<HVGeometryInfo*>(param);
        return SUCCESS;
    default:
        return INVALID_PARAMS_NUM;
    }
}

int HVImageRoiCrop::RunImageCrop()
{
    if (!input_2d_image_ || input_2d_image_->empty()) {
        SetFailure(ALGORITHM_RUN_ERROR, "msg.image_input_null");
        return ALGORITHM_RUN_ERROR;
    }
    if (!IsImageCropGeometry(roi_)) {
        SetFailure(ALGORITHM_RUN_ERROR, "msg.invalid_image_roi");
        return ALGORITHM_RUN_ERROR;
    }

    ImageDataInfo2D cropped_image;
    const HVGeometryInfo image_roi = NormalizeImageCropGeometry(roi_);
    if (!BuildMaskedImageFromRoi(image_roi, *input_2d_image_, cropped_image)) {
        SetFailure(ALGORITHM_RUN_ERROR, "msg.invalid_image_roi");
        return ALGORITHM_RUN_ERROR;
    }

    cropped_2d_image_ = std::make_shared<ImageDataInfo2D>(std::move(cropped_image));
    execute_status_ = SUCCESS;
    error_message_key_ = "msg.image_success";
    return SUCCESS;
}

int HVImageRoiCrop::RunDepthCrop()
{
    if (!input_depth_image_ || input_depth_image_->empty()) {
        SetFailure(ALGORITHM_RUN_ERROR, "msg.depth_input_null");
        return ALGORITHM_RUN_ERROR;
    }
    if (!IsImageCropGeometry(roi_)) {
        SetFailure(ALGORITHM_RUN_ERROR, "msg.invalid_image_roi");
        return ALGORITHM_RUN_ERROR;
    }

    ImageDataInfoDepth cropped_depth;
    const HVGeometryInfo image_roi = NormalizeImageCropGeometry(roi_);
    if (!BuildMaskedDepthFromRoi(image_roi, *input_depth_image_, cropped_depth)) {
        SetFailure(ALGORITHM_RUN_ERROR, "msg.invalid_image_roi");
        return ALGORITHM_RUN_ERROR;
    }

    cropped_depth_image_ = std::make_shared<ImageDataInfoDepth>(std::move(cropped_depth));
    execute_status_ = SUCCESS;
    error_message_key_ = "msg.depth_success";
    return SUCCESS;
}

int HVImageRoiCrop::RunPointCloudCrop()
{
    if (!input_point_cloud_) {
        SetFailure(ALGORITHM_RUN_ERROR, "msg.cloud_input_null");
        return ALGORITHM_RUN_ERROR;
    }
    if (!IsPointCloudCropGeometry(roi_)) {
        SetFailure(ALGORITHM_RUN_ERROR, "msg.invalid_cloud_roi");
        return ALGORITHM_RUN_ERROR;
    }

    const pcl::PointCloud<pcl::PointXYZ>::Ptr input_cloud = PointCloudConverter::ToPCL(*input_point_cloud_);
    pcl::PointCloud<pcl::PointXYZ> cropped_cloud;
    if (!CropPointCloudByGeometry(roi_, *input_cloud, cropped_cloud)) {
        SetFailure(ALGORITHM_RUN_ERROR, "msg.invalid_cloud_roi");
        return ALGORITHM_RUN_ERROR;
    }

    cropped_point_cloud_ = std::make_shared<HVPointCloud>(PointCloudConverter::FromPCL(cropped_cloud));
    execute_status_ = SUCCESS;
    error_message_key_ = "msg.cloud_success";
    return SUCCESS;
}

void HVImageRoiCrop::ClearOutputs()
{
    cropped_2d_image_.reset();
    cropped_depth_image_.reset();
    cropped_point_cloud_.reset();
}

void HVImageRoiCrop::SetFailure(int status, const std::string& message_key)
{
    execute_status_ = status;
    error_message_key_ = message_key;
    ClearOutputs();
}

NodeEngine* CreateInstance()
{
    return new HVImageRoiCrop();
}

std::string GetInstanceName()
{
    return "Image ROI crop";
}

extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion()
{
    return NODE_ENGINE_ABI_VERSION;
}
