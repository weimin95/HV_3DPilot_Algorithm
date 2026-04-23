#include "HVImageSave.h"

#include "HVI18n.h"
#include "HVUtils.h"
#include "3d_pliot_error.h"
#include "json.hpp"

#include <opencv2/imgcodecs.hpp>
#include <pcl/io/ply_io.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>

namespace {

constexpr int kInputTypeImage2D = 0;
constexpr int kInputTypeDepth = 1;
constexpr int kInputTypePointCloud = 2;

constexpr int kStorageModeOverwrite = 0;
constexpr int kStorageModeStop = 1;

constexpr int kSaveFormatBmp = 0;
constexpr int kSaveFormatJpeg = 1;

const hvi18n::Dictionary kTexts = {
    { "algorithm.display", { "图像保存", "Image save" } },
    { "input.type.name", { "输入类型", "Input type" } },
    { "input.image.name", { "输入2D图像", "Input 2D image" } },
    { "input.depth.name", { "输入深度图", "Input depth image" } },
    { "input.cloud.name", { "输入点云", "Input point cloud" } },
    { "input.output_dir.name", { "保存根目录", "Output root directory" } },
    { "input.prefix.name", { "文件名前缀", "File name prefix" } },
    { "input.create_date_dirs.name", { "按日期建目录", "Create date directories" } },
    { "input.storage_mode.name", { "存储方式", "Storage mode" } },
    { "input.max_count.name", { "最大存储数量", "Maximum storage count" } },
    { "input.min_free_space.name", { "磁盘剩余空间(MB)", "Minimum free disk space (MB)" } },
    { "input.max_days.name", { "最大保存天数", "Maximum retention days" } },
    { "input.format.name", { "保存格式", "Save format" } },
    { "input.jpeg_quality.name", { "JPEG压缩质量", "JPEG quality" } },
    { "output.path.name", { "保存路径", "Saved path" } },
    { "output.saved.name", { "是否已保存", "Saved" } },
    { "output.status.name", { "运行状态", "Execute status" } },
    { "input.type.desc", { "选择本次要保存的数据类型", "Select the data type to save" } },
    { "input.image.desc", { "输入的2D图像，仅在输入类型为2D图像时生效", "2D image used when input type is 2D image" } },
    { "input.depth.desc", { "输入的深度图，仅在输入类型为深度图时生效", "Depth image used when input type is depth image" } },
    { "input.cloud.desc", { "输入的点云，仅在输入类型为点云时生效", "Point cloud used when input type is point cloud" } },
    { "input.output_dir.desc", { "存图/存点云的根目录", "Root directory used to save images or point clouds" } },
    { "input.prefix.desc", { "生成文件名时使用的前缀", "Prefix used when generating output file names" } },
    { "input.create_date_dirs.desc", { "启用后按月份/日期创建子目录", "When enabled, create month/day subdirectories automatically" } },
    { "input.storage_mode.desc", { "达到存储上限或磁盘空间不足时的处理方式", "How to handle storage limit or low free disk space" } },
    { "input.max_count.desc", { "最大允许保留的文件数量，0表示不限制", "Maximum number of files to retain, 0 means unlimited" } },
    { "input.min_free_space.desc", { "磁盘剩余空间低于该值时触发存储策略", "Trigger storage policy when free disk space is below this value" } },
    { "input.max_days.desc", { "超过该天数的旧文件会在运行时自动删除，0表示不限制", "Files older than this many days are deleted automatically during execution, 0 means disabled" } },
    { "input.format.desc", { "图像保存格式，点云固定保存为PLY", "Image save format, point cloud is always saved as PLY" } },
    { "input.jpeg_quality.desc", { "图像保存为JPEG时使用的压缩质量", "JPEG compression quality used when saving images as JPEG" } },
    { "option.type.image", { "2D图像", "2D image" } },
    { "option.type.depth", { "深度图", "Depth image" } },
    { "option.type.cloud", { "点云", "Point cloud" } },
    { "option.storage.overwrite", { "覆盖存储", "Overwrite storage" } },
    { "option.storage.stop", { "停止存储", "Stop storage" } },
    { "option.format.bmp", { "BMP", "BMP" } },
    { "option.format.jpeg", { "JPEG", "JPEG" } },
    { "msg.invalid_output_dir", { "保存目录不合法", "Output directory is invalid" } },
    { "msg.image_input_null", { "输入2D图像为空", "Input 2D image is null" } },
    { "msg.depth_input_null", { "输入深度图为空", "Input depth image is null" } },
    { "msg.cloud_input_null", { "输入点云为空", "Input point cloud is null" } },
    { "msg.unsupported_input_type", { "不支持的输入类型", "Unsupported input type" } },
    { "msg.invalid_storage_mode", { "存储方式不合法", "Storage mode is invalid" } },
    { "msg.invalid_save_format", { "保存格式不合法", "Save format is invalid" } },
    { "msg.invalid_jpeg_quality", { "JPEG压缩质量不合法", "JPEG quality is invalid" } },
    { "msg.create_dir_failed", { "创建保存目录失败", "Failed to create output directory" } },
    { "msg.disk_limit_stop", { "磁盘空间不足，已停止存储", "Free disk space is below the configured threshold, saving skipped" } },
    { "msg.count_limit_stop", { "已达最大存储数量，已停止存储", "Maximum storage count reached, saving skipped" } },
    { "msg.cleanup_failed", { "无法删除旧文件释放空间", "Failed to delete old files to free storage" } },
    { "msg.write_image_failed", { "写入图像文件失败", "Failed to write image file" } },
    { "msg.write_cloud_failed", { "写入PLY点云文件失败", "Failed to write PLY point cloud file" } },
    { "msg.image_saved", { "图像保存成功", "Image saved successfully" } },
    { "msg.depth_saved", { "深度图保存成功", "Depth image saved successfully" } },
    { "msg.cloud_saved", { "点云保存成功", "Point cloud saved successfully" } },
    { "msg.skip_saved", { "当前存储策略下未生成新文件", "No new file was generated under the current storage policy" } }
};

struct StoredFileInfo {
    std::filesystem::path path;
    std::filesystem::file_time_type write_time;
};

std::string Tr(int language, const std::string& key)
{
    return hvi18n::Translate(kTexts, key, language);
}

std::wstring ToWideTimeToken(int value, int width)
{
    std::wstring text = std::to_wstring(value);
    if (static_cast<int>(text.size()) >= width) {
        return text;
    }
    return std::wstring(width - text.size(), L'0') + text;
}

std::tm LocalTimeNow()
{
    std::time_t now = std::time(nullptr);
    std::tm local_tm{};
#ifdef _WIN32
    localtime_s(&local_tm, &now);
#else
    localtime_r(&now, &local_tm);
#endif
    return local_tm;
}

std::wstring BuildTimestampStem()
{
    const auto now = std::chrono::system_clock::now();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    const std::tm local_tm = LocalTimeNow();
    return ToWideTimeToken(local_tm.tm_year + 1900, 4) +
        ToWideTimeToken(local_tm.tm_mon + 1, 2) +
        ToWideTimeToken(local_tm.tm_mday, 2) + L"_" +
        ToWideTimeToken(local_tm.tm_hour, 2) +
        ToWideTimeToken(local_tm.tm_min, 2) +
        ToWideTimeToken(local_tm.tm_sec, 2) + L"_" +
        ToWideTimeToken(static_cast<int>(millis.count()), 3);
}

bool IsManagedStoredFile(const std::filesystem::path& path)
{
    if (!path.has_extension()) {
        return false;
    }
    std::wstring ext = path.extension().wstring();
    std::transform(ext.begin(), ext.end(), ext.begin(), towlower);
    return ext == L".bmp" || ext == L".jpg" || ext == L".jpeg" || ext == L".ply";
}

std::vector<StoredFileInfo> CollectStoredFiles(const std::filesystem::path& root_dir)
{
    std::vector<StoredFileInfo> files;
    if (!std::filesystem::exists(root_dir)) {
        return files;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(root_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (!IsManagedStoredFile(entry.path())) {
            continue;
        }
        files.push_back({ entry.path(), entry.last_write_time() });
    }

    std::sort(files.begin(), files.end(), [](const StoredFileInfo& lhs, const StoredFileInfo& rhs) {
        return lhs.write_time < rhs.write_time;
    });
    return files;
}

std::string PathToUtf8(const std::filesystem::path& path)
{
    return path.u8string();
}

std::vector<int> BuildImageWriteParams(int save_format, int jpeg_quality)
{
    if (save_format == kSaveFormatJpeg) {
        return { cv::IMWRITE_JPEG_QUALITY, jpeg_quality };
    }
    return {};
}

cv::Mat ConvertDepthToSavableMat(const ImageDataInfoDepth& depth_image)
{
    if (depth_image.empty()) {
        return {};
    }

    cv::Mat depth_32f(static_cast<int>(depth_image.height), static_cast<int>(depth_image.width), CV_32FC1);
    float min_value = std::numeric_limits<float>::max();
    float max_value = std::numeric_limits<float>::lowest();

    for (size_t row = 0; row < depth_image.height; ++row) {
        for (size_t col = 0; col < depth_image.width; ++col) {
            // 深度图固定使用每像素 xyz 三个 float，这里取 z 通道做可视化保存。
            const float value = depth_image.getDepthAt(row, col);
            depth_32f.at<float>(static_cast<int>(row), static_cast<int>(col)) = value;
            if (std::isfinite(value) && value > 0.0f) {
                min_value = std::min(min_value, value);
                max_value = std::max(max_value, value);
            }
        }
    }

    cv::Mat output(static_cast<int>(depth_image.height), static_cast<int>(depth_image.width), CV_8UC1, cv::Scalar(0));
    if (!(min_value <= max_value)) {
        return output;
    }

    if (std::abs(max_value - min_value) < 1e-6f) {
        for (int row = 0; row < depth_32f.rows; ++row) {
            for (int col = 0; col < depth_32f.cols; ++col) {
                const float value = depth_32f.at<float>(row, col);
                if (std::isfinite(value) && value > 0.0f) {
                    output.at<unsigned char>(row, col) = 255;
                }
            }
        }
        return output;
    }

    const float scale = 255.0f / (max_value - min_value);
    for (int row = 0; row < depth_32f.rows; ++row) {
        for (int col = 0; col < depth_32f.cols; ++col) {
            const float value = depth_32f.at<float>(row, col);
            if (!std::isfinite(value) || value <= 0.0f) {
                continue;
            }
            const float normalized = (value - min_value) * scale;
            output.at<unsigned char>(row, col) =
                static_cast<unsigned char>(std::clamp(normalized, 0.0f, 255.0f));
        }
    }
    return output;
}

}  // namespace

HVImageSave::HVImageSave() = default;

int HVImageSave::init()
{
    execute_status_ = NODE_STATUS_NOT_RUN;
    run_time_ms_ = 0;
    error_message_key_.clear();
    ResetOutputs();
    return SUCCESS;
}

int HVImageSave::run()
{
    const auto start = std::chrono::steady_clock::now();
    execute_status_ = NODE_STATUS_RUNNING;
    error_message_key_.clear();
    ResetOutputs();

    int result = SUCCESS;
    switch (input_type_) {
    case kInputTypeImage2D:
        result = Save2DImage();
        break;
    case kInputTypeDepth:
        result = SaveDepthImage();
        break;
    case kInputTypePointCloud:
        result = SavePointCloud();
        break;
    default:
        result = Fail(ALGORITHM_RUN_ERROR, "msg.unsupported_input_type");
        break;
    }

    const auto end = std::chrono::steady_clock::now();
    run_time_ms_ = static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
    return result;
}

int HVImageSave::set_algorithm_params(const std::vector<void*>& params, const std::vector<int>& paramID)
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

std::vector<void*> HVImageSave::get_current_params()
{
    return {
        &input_type_,
        &input_2d_image_,
        &input_depth_image_,
        &input_point_cloud_,
        &output_root_dir_,
        &file_name_prefix_,
        &create_date_directories_,
        &storage_mode_,
        &max_storage_count_,
        &min_free_space_mb_,
        &max_save_days_,
        &save_format_,
        &jpeg_quality_
    };
}

std::vector<void*> HVImageSave::get_algorithm_result()
{
    if (execute_status_ == SUCCESS) {
        return { &saved_path_, &saved_, &execute_status_ };
    }
    return { nullptr, &saved_, &execute_status_ };
}

std::vector<int> HVImageSave::get_algorithm_input_params_type()
{
    return {
        HV_INT,
        HV_IMAGEDATAINFO2D,
        HV_IMAGEDATAINFODEPTH,
        HV_POINTCLOUD,
        HV_STRING,
        HV_STRING,
        HV_BOOLEAN,
        HV_INT,
        HV_INT,
        HV_DOUBLE,
        HV_INT,
        HV_INT,
        HV_INT
    };
}

std::vector<int> HVImageSave::get_algorithm_output_params_type()
{
    return { HV_STRING, HV_BOOLEAN, HV_INT };
}

std::vector<std::string> HVImageSave::get_algorithm_input_params_name()
{
    return {
        Tr(language_, "input.type.name"),
        Tr(language_, "input.image.name"),
        Tr(language_, "input.depth.name"),
        Tr(language_, "input.cloud.name"),
        Tr(language_, "input.output_dir.name"),
        Tr(language_, "input.prefix.name"),
        Tr(language_, "input.create_date_dirs.name"),
        Tr(language_, "input.storage_mode.name"),
        Tr(language_, "input.max_count.name"),
        Tr(language_, "input.min_free_space.name"),
        Tr(language_, "input.max_days.name"),
        Tr(language_, "input.format.name"),
        Tr(language_, "input.jpeg_quality.name")
    };
}

std::vector<std::string> HVImageSave::get_algorithm_output_params_name()
{
    return {
        Tr(language_, "output.path.name"),
        Tr(language_, "output.saved.name"),
        Tr(language_, "output.status.name")
    };
}

std::vector<bool> HVImageSave::get_algorithm_input_params_bindable()
{
    // 这里返回 false 只作为默认输入模式提示。
    // 图像/深度图/点云输入仍然可以在画布中手动绑定。
    return std::vector<bool>(13, false);
}

std::vector<ParamMetadata> HVImageSave::get_algorithm_input_params_metadata()
{
    std::vector<ParamMetadata> metadata_list(13);

    metadata_list[0].param_name = Tr(language_, "input.type.name");
    metadata_list[0].param_description = Tr(language_, "input.type.desc");
    metadata_list[0].param_type = HV_INT;
    metadata_list[0].constraint_type = CONSTRAINT_OPTIONS;
    metadata_list[0].options_constraint.AddOption(kInputTypeImage2D, Tr(language_, "option.type.image"));
    metadata_list[0].options_constraint.AddOption(kInputTypeDepth, Tr(language_, "option.type.depth"));
    metadata_list[0].options_constraint.AddOption(kInputTypePointCloud, Tr(language_, "option.type.cloud"));
    metadata_list[0].options_constraint.default_index = 0;

    metadata_list[1].param_name = Tr(language_, "input.image.name");
    metadata_list[1].param_description = Tr(language_, "input.image.desc");
    metadata_list[1].param_type = HV_IMAGEDATAINFO2D;
    metadata_list[1].dependencies.push_back(ParamDependency(0, DEPENDS_ON_EQUALS, { "0" }));

    metadata_list[2].param_name = Tr(language_, "input.depth.name");
    metadata_list[2].param_description = Tr(language_, "input.depth.desc");
    metadata_list[2].param_type = HV_IMAGEDATAINFODEPTH;
    metadata_list[2].dependencies.push_back(ParamDependency(0, DEPENDS_ON_EQUALS, { "1" }));

    metadata_list[3].param_name = Tr(language_, "input.cloud.name");
    metadata_list[3].param_description = Tr(language_, "input.cloud.desc");
    metadata_list[3].param_type = HV_POINTCLOUD;
    metadata_list[3].dependencies.push_back(ParamDependency(0, DEPENDS_ON_EQUALS, { "2" }));

    metadata_list[4].param_name = Tr(language_, "input.output_dir.name");
    metadata_list[4].param_description = Tr(language_, "input.output_dir.desc");
    metadata_list[4].param_type = HV_STRING;

    metadata_list[5].param_name = Tr(language_, "input.prefix.name");
    metadata_list[5].param_description = Tr(language_, "input.prefix.desc");
    metadata_list[5].param_type = HV_STRING;

    metadata_list[6].param_name = Tr(language_, "input.create_date_dirs.name");
    metadata_list[6].param_description = Tr(language_, "input.create_date_dirs.desc");
    metadata_list[6].param_type = HV_BOOLEAN;

    metadata_list[7].param_name = Tr(language_, "input.storage_mode.name");
    metadata_list[7].param_description = Tr(language_, "input.storage_mode.desc");
    metadata_list[7].param_type = HV_INT;
    metadata_list[7].constraint_type = CONSTRAINT_OPTIONS;
    metadata_list[7].options_constraint.AddOption(kStorageModeOverwrite, Tr(language_, "option.storage.overwrite"));
    metadata_list[7].options_constraint.AddOption(kStorageModeStop, Tr(language_, "option.storage.stop"));
    metadata_list[7].options_constraint.default_index = 0;

    metadata_list[8].param_name = Tr(language_, "input.max_count.name");
    metadata_list[8].param_description = Tr(language_, "input.max_count.desc");
    metadata_list[8].param_type = HV_INT;
    metadata_list[8].constraint_type = CONSTRAINT_RANGE;
    metadata_list[8].range_constraint = RangeConstraint(0.0, 1000000.0, 0.0);

    metadata_list[9].param_name = Tr(language_, "input.min_free_space.name");
    metadata_list[9].param_description = Tr(language_, "input.min_free_space.desc");
    metadata_list[9].param_type = HV_DOUBLE;
    metadata_list[9].constraint_type = CONSTRAINT_RANGE;
    metadata_list[9].range_constraint = RangeConstraint(0.0, 1048576.0, 0.0);

    metadata_list[10].param_name = Tr(language_, "input.max_days.name");
    metadata_list[10].param_description = Tr(language_, "input.max_days.desc");
    metadata_list[10].param_type = HV_INT;
    metadata_list[10].constraint_type = CONSTRAINT_RANGE;
    metadata_list[10].range_constraint = RangeConstraint(0.0, 36500.0, 0.0);

    metadata_list[11].param_name = Tr(language_, "input.format.name");
    metadata_list[11].param_description = Tr(language_, "input.format.desc");
    metadata_list[11].param_type = HV_INT;
    metadata_list[11].constraint_type = CONSTRAINT_OPTIONS;
    metadata_list[11].options_constraint.AddOption(kSaveFormatBmp, Tr(language_, "option.format.bmp"));
    metadata_list[11].options_constraint.AddOption(kSaveFormatJpeg, Tr(language_, "option.format.jpeg"));
    metadata_list[11].options_constraint.default_index = 0;
    metadata_list[11].dependencies.push_back(ParamDependency(0, DEPENDS_ON_NOT_EQUALS, { "2" }));

    metadata_list[12].param_name = Tr(language_, "input.jpeg_quality.name");
    metadata_list[12].param_description = Tr(language_, "input.jpeg_quality.desc");
    metadata_list[12].param_type = HV_INT;
    metadata_list[12].constraint_type = CONSTRAINT_RANGE;
    metadata_list[12].range_constraint = RangeConstraint(1.0, 100.0, 95.0);
    metadata_list[12].dependencies.push_back(ParamDependency(11, DEPENDS_ON_EQUALS, { "1" }));
    metadata_list[12].dependencies.push_back(ParamDependency(0, DEPENDS_ON_NOT_EQUALS, { "2" }));

    for (size_t i = 0; i < metadata_list.size(); ++i) {
        metadata_list[i].param_group = (i <= 6) ? PARAM_GROUP_BASIC : PARAM_GROUP_ADVANCED;
    }

    return metadata_list;
}

int HVImageSave::get_algorithm_execute_status()
{
    return execute_status_;
}

std::string HVImageSave::get_algorithm_error_message()
{
    if (error_message_key_.empty()) {
        return "";
    }
    return Tr(language_, error_message_key_);
}

long HVImageSave::get_algorithm_use_time()
{
    return run_time_ms_;
}

bool HVImageSave::algorithm_params_setting_status()
{
    return true;
}

bool HVImageSave::algorithm_init_status()
{
    return true;
}

bool HVImageSave::save_params_to_json(const std::string& filePath)
{
    try {
        nlohmann::json params_json = nlohmann::json::array();
        add_param(params_json, "input_type", HV_INT, input_type_);
        add_param(params_json, "output_root_dir", HV_STRING, output_root_dir_);
        add_param(params_json, "file_name_prefix", HV_STRING, file_name_prefix_);
        add_param(params_json, "create_date_directories", HV_BOOLEAN, create_date_directories_);
        add_param(params_json, "storage_mode", HV_INT, storage_mode_);
        add_param(params_json, "max_storage_count", HV_INT, max_storage_count_);
        add_param(params_json, "min_free_space_mb", HV_DOUBLE, min_free_space_mb_);
        add_param(params_json, "max_save_days", HV_INT, max_save_days_);
        add_param(params_json, "save_format", HV_INT, save_format_);
        add_param(params_json, "jpeg_quality", HV_INT, jpeg_quality_);

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

bool HVImageSave::load_params_from_json(const std::string& filePath)
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

        for (const auto& param : params_json) {
            if (!param.is_object() || !param.contains("name") || !param.contains("value")) {
                continue;
            }

            const std::string name = param["name"].get<std::string>();
            if (name == "input_type") {
                input_type_ = param["value"].get<int>();
            } else if (name == "output_root_dir") {
                output_root_dir_ = param["value"].get<std::string>();
            } else if (name == "file_name_prefix") {
                file_name_prefix_ = param["value"].get<std::string>();
            } else if (name == "create_date_directories") {
                create_date_directories_ = param["value"].get<bool>();
            } else if (name == "storage_mode") {
                storage_mode_ = param["value"].get<int>();
            } else if (name == "max_storage_count") {
                max_storage_count_ = param["value"].get<int>();
            } else if (name == "min_free_space_mb") {
                min_free_space_mb_ = param["value"].get<double>();
            } else if (name == "max_save_days") {
                max_save_days_ = param["value"].get<int>();
            } else if (name == "save_format") {
                save_format_ = param["value"].get<int>();
            } else if (name == "jpeg_quality") {
                jpeg_quality_ = param["value"].get<int>();
            }
        }
        return true;
    }
    catch (...) {
        return false;
    }
}

AlgorithmType HVImageSave::get_algorithm_type()
{
    return AlgorithmType::ImageProcess;
}

void HVImageSave::set_language(int language)
{
    language_ = language;
}

int HVImageSave::get_language() const
{
    return language_;
}

std::string HVImageSave::get_algorithm_display_name()
{
    return Tr(language_, "algorithm.display");
}

int HVImageSave::ApplyParam(int param_id, void* param)
{
    switch (param_id) {
    case 0:
        input_type_ = param == nullptr ? 0 : *static_cast<int*>(param);
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
        output_root_dir_ = param == nullptr ? std::string() : *static_cast<std::string*>(param);
        return SUCCESS;
    case 5:
        file_name_prefix_ = param == nullptr ? std::string() : *static_cast<std::string*>(param);
        return SUCCESS;
    case 6:
        create_date_directories_ = param != nullptr && *static_cast<bool*>(param);
        return SUCCESS;
    case 7:
        storage_mode_ = param == nullptr ? 0 : *static_cast<int*>(param);
        return SUCCESS;
    case 8:
        max_storage_count_ = param == nullptr ? 0 : *static_cast<int*>(param);
        return SUCCESS;
    case 9:
        min_free_space_mb_ = param == nullptr ? 0.0 : *static_cast<double*>(param);
        return SUCCESS;
    case 10:
        max_save_days_ = param == nullptr ? 0 : *static_cast<int*>(param);
        return SUCCESS;
    case 11:
        save_format_ = param == nullptr ? 0 : *static_cast<int*>(param);
        return SUCCESS;
    case 12:
        jpeg_quality_ = param == nullptr ? 95 : *static_cast<int*>(param);
        return SUCCESS;
    default:
        return INVALID_PARAMS_NUM;
    }
}

int HVImageSave::Save2DImage()
{
    if (!input_2d_image_ || input_2d_image_->empty()) {
        return Fail(ALGORITHM_RUN_ERROR, "msg.image_input_null");
    }
    if (save_format_ != kSaveFormatBmp && save_format_ != kSaveFormatJpeg) {
        return Fail(ALGORITHM_RUN_ERROR, "msg.invalid_save_format");
    }
    if (save_format_ == kSaveFormatJpeg && (jpeg_quality_ < 1 || jpeg_quality_ > 100)) {
        return Fail(ALGORITHM_RUN_ERROR, "msg.invalid_jpeg_quality");
    }

    std::filesystem::path output_dir;
    int result = PrepareOutputDirectory(output_dir);
    if (result != SUCCESS) {
        return result;
    }

    std::string skip_message_key;
    result = EnsureStorageCapacity(std::filesystem::u8path(output_root_dir_), skip_message_key);
    if (result != SUCCESS) {
        return result;
    }
    if (!skip_message_key.empty()) {
        return Succeed("", false, skip_message_key);
    }

    cv::Mat image = ImageConverter::ToMat(*input_2d_image_);
    if (image.empty()) {
        return Fail(ALGORITHM_RUN_ERROR, "msg.image_input_null");
    }
    if (image.depth() != CV_8U) {
        image.convertTo(image, CV_8U);
    }

    const std::wstring extension = save_format_ == kSaveFormatJpeg ? L".jpg" : L".bmp";
    const std::wstring prefix = file_name_prefix_.empty() ? L"image" : std::filesystem::u8path(file_name_prefix_).wstring();
    const std::wstring stem = prefix + L"_" + BuildTimestampStem();
    std::filesystem::path output_path = output_dir / (stem + extension);
    int suffix = 1;
    while (std::filesystem::exists(output_path)) {
        output_path = output_dir / (stem + L"_" + std::to_wstring(suffix++) + extension);
    }

    if (!cv::imwrite(output_path.string(), image, BuildImageWriteParams(save_format_, jpeg_quality_))) {
        return Fail(ALGORITHM_RUN_ERROR, "msg.write_image_failed");
    }
    return Succeed(PathToUtf8(output_path), true, "msg.image_saved");
}

int HVImageSave::SaveDepthImage()
{
    if (!input_depth_image_ || input_depth_image_->empty()) {
        return Fail(ALGORITHM_RUN_ERROR, "msg.depth_input_null");
    }
    if (save_format_ != kSaveFormatBmp && save_format_ != kSaveFormatJpeg) {
        return Fail(ALGORITHM_RUN_ERROR, "msg.invalid_save_format");
    }
    if (save_format_ == kSaveFormatJpeg && (jpeg_quality_ < 1 || jpeg_quality_ > 100)) {
        return Fail(ALGORITHM_RUN_ERROR, "msg.invalid_jpeg_quality");
    }

    std::filesystem::path output_dir;
    int result = PrepareOutputDirectory(output_dir);
    if (result != SUCCESS) {
        return result;
    }

    std::string skip_message_key;
    result = EnsureStorageCapacity(std::filesystem::u8path(output_root_dir_), skip_message_key);
    if (result != SUCCESS) {
        return result;
    }
    if (!skip_message_key.empty()) {
        return Succeed("", false, skip_message_key);
    }

    cv::Mat depth_image = ConvertDepthToSavableMat(*input_depth_image_);
    if (depth_image.empty()) {
        return Fail(ALGORITHM_RUN_ERROR, "msg.depth_input_null");
    }

    const std::wstring extension = save_format_ == kSaveFormatJpeg ? L".jpg" : L".bmp";
    const std::wstring prefix = file_name_prefix_.empty() ? L"depth" : std::filesystem::u8path(file_name_prefix_).wstring();
    const std::wstring stem = prefix + L"_" + BuildTimestampStem();
    std::filesystem::path output_path = output_dir / (stem + extension);
    int suffix = 1;
    while (std::filesystem::exists(output_path)) {
        output_path = output_dir / (stem + L"_" + std::to_wstring(suffix++) + extension);
    }

    if (!cv::imwrite(output_path.string(), depth_image, BuildImageWriteParams(save_format_, jpeg_quality_))) {
        return Fail(ALGORITHM_RUN_ERROR, "msg.write_image_failed");
    }
    return Succeed(PathToUtf8(output_path), true, "msg.depth_saved");
}

int HVImageSave::SavePointCloud()
{
    if (!input_point_cloud_ || input_point_cloud_->points.empty()) {
        return Fail(ALGORITHM_RUN_ERROR, "msg.cloud_input_null");
    }

    std::filesystem::path output_dir;
    int result = PrepareOutputDirectory(output_dir);
    if (result != SUCCESS) {
        return result;
    }

    std::string skip_message_key;
    result = EnsureStorageCapacity(std::filesystem::u8path(output_root_dir_), skip_message_key);
    if (result != SUCCESS) {
        return result;
    }
    if (!skip_message_key.empty()) {
        return Succeed("", false, skip_message_key);
    }

    const std::wstring prefix = file_name_prefix_.empty() ? L"cloud" : std::filesystem::u8path(file_name_prefix_).wstring();
    const std::wstring stem = prefix + L"_" + BuildTimestampStem();
    std::filesystem::path output_path = output_dir / (stem + L".ply");
    int suffix = 1;
    while (std::filesystem::exists(output_path)) {
        output_path = output_dir / (stem + L"_" + std::to_wstring(suffix++) + L".ply");
    }

    pcl::PointCloud<pcl::PointXYZ>::Ptr pcl_cloud = PointCloudConverter::ToPCL(*input_point_cloud_);
    if (!pcl_cloud || pcl::io::savePLYFileBinary(output_path.string(), *pcl_cloud) != 0) {
        return Fail(ALGORITHM_RUN_ERROR, "msg.write_cloud_failed");
    }
    return Succeed(PathToUtf8(output_path), true, "msg.cloud_saved");
}

int HVImageSave::PrepareOutputDirectory(std::filesystem::path& output_dir)
{
    if (output_root_dir_.empty()) {
        return Fail(ALGORITHM_RUN_ERROR, "msg.invalid_output_dir");
    }
    if (storage_mode_ != kStorageModeOverwrite && storage_mode_ != kStorageModeStop) {
        return Fail(ALGORITHM_RUN_ERROR, "msg.invalid_storage_mode");
    }
    if (max_storage_count_ < 0 || min_free_space_mb_ < 0.0 || max_save_days_ < 0) {
        return Fail(ALGORITHM_RUN_ERROR, "msg.invalid_output_dir");
    }

    const std::filesystem::path root_dir = std::filesystem::u8path(output_root_dir_);
    output_dir = root_dir;
    if (create_date_directories_) {
        const std::tm local_tm = LocalTimeNow();
        const std::wstring month_dir = ToWideTimeToken(local_tm.tm_year + 1900, 4) + L"-" + ToWideTimeToken(local_tm.tm_mon + 1, 2);
        const std::wstring day_dir = month_dir + L"-" + ToWideTimeToken(local_tm.tm_mday, 2);
        output_dir /= month_dir;
        output_dir /= day_dir;
    }

    std::error_code ec;
    std::filesystem::create_directories(output_dir, ec);
    if (ec) {
        return Fail(ALGORITHM_RUN_ERROR, "msg.create_dir_failed");
    }
    return ApplyRetentionPolicy(root_dir);
}

int HVImageSave::ApplyRetentionPolicy(const std::filesystem::path& root_dir)
{
    if (max_save_days_ <= 0 || !std::filesystem::exists(root_dir)) {
        return SUCCESS;
    }

    const auto now = std::filesystem::file_time_type::clock::now();
    const auto expire_duration = std::chrono::hours(24 * max_save_days_);
    std::error_code ec;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root_dir, ec)) {
        if (ec) {
            break;
        }
        if (!entry.is_regular_file() || !IsManagedStoredFile(entry.path())) {
            continue;
        }
        const auto write_time = entry.last_write_time(ec);
        if (ec) {
            continue;
        }
        if (now - write_time >= expire_duration) {
            std::filesystem::remove(entry.path(), ec);
            ec.clear();
        }
    }
    return SUCCESS;
}

int HVImageSave::EnsureStorageCapacity(const std::filesystem::path& root_dir, std::string& skip_message_key)
{
    skip_message_key.clear();
    if (!std::filesystem::exists(root_dir)) {
        return SUCCESS;
    }

    if (max_storage_count_ > 0) {
        auto files = CollectStoredFiles(root_dir);
        if (static_cast<int>(files.size()) >= max_storage_count_) {
            if (storage_mode_ == kStorageModeStop) {
                skip_message_key = "msg.count_limit_stop";
                return SUCCESS;
            }

            const int delete_count = static_cast<int>(files.size()) - max_storage_count_ + 1;
            std::error_code ec;
            for (int i = 0; i < delete_count && i < static_cast<int>(files.size()); ++i) {
                std::filesystem::remove(files[static_cast<size_t>(i)].path, ec);
                if (ec) {
                    return Fail(ALGORITHM_RUN_ERROR, "msg.cleanup_failed");
                }
            }
        }
    }

    if (min_free_space_mb_ <= 0.0) {
        return SUCCESS;
    }

    const std::uintmax_t min_free_bytes =
        static_cast<std::uintmax_t>(std::max(0.0, min_free_space_mb_) * 1024.0 * 1024.0);
    std::error_code ec;
    auto space_info = std::filesystem::space(root_dir, ec);
    if (ec) {
        return SUCCESS;
    }
    if (space_info.available >= min_free_bytes) {
        return SUCCESS;
    }
    if (storage_mode_ == kStorageModeStop) {
        skip_message_key = "msg.disk_limit_stop";
        return SUCCESS;
    }

    auto files = CollectStoredFiles(root_dir);
    for (const auto& file : files) {
        std::filesystem::remove(file.path, ec);
        if (ec) {
            return Fail(ALGORITHM_RUN_ERROR, "msg.cleanup_failed");
        }
        space_info = std::filesystem::space(root_dir, ec);
        if (ec) {
            return SUCCESS;
        }
        if (space_info.available >= min_free_bytes) {
            return SUCCESS;
        }
    }

    return Fail(ALGORITHM_RUN_ERROR, "msg.cleanup_failed");
}

void HVImageSave::ResetOutputs()
{
    saved_path_.clear();
    saved_ = false;
}

int HVImageSave::Fail(int status, const std::string& message_key)
{
    execute_status_ = status;
    error_message_key_ = message_key;
    ResetOutputs();
    return status;
}

int HVImageSave::Succeed(const std::string& saved_path, bool saved, const std::string& message_key)
{
    execute_status_ = SUCCESS;
    error_message_key_ = message_key;
    saved_path_ = saved_path;
    saved_ = saved;
    return SUCCESS;
}

extern "C" __declspec(dllexport) NodeEngine* CreateInstance()
{
    return new HVImageSave();
}

extern "C" __declspec(dllexport) std::string GetInstanceName()
{
    return "Image save";
}

extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion()
{
    return NODE_ENGINE_ABI_VERSION;
}
