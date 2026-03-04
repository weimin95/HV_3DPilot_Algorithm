#include "HVImageFilter.h"
#include "HVUtils.h"
#include "HVI18n.h"
#include <chrono>

namespace {

const hvi18n::Dictionary kTexts = {
    { "algorithm.display", { "图像滤波", "Image filter" } },
    { "input.image.name", { "输入图像", "input image" } },
    { "input.filter_type.name", { "滤波类型", "filter type" } },
    { "input.kernel_size.name", { "卷积核大小", "kernel size" } },
    { "input.sigma.name", { "sigma", "sigma" } },
    { "output.image.name", { "输出图像", "output image" } },
    { "input.image.desc", { "输入图像", "Input image" } },
    { "input.filter_type.desc", { "滤波算法类型", "Filter algorithm type" } },
    { "input.kernel_size.desc", { "卷积核大小（必须为奇数）", "Kernel size (must be odd)" } },
    { "input.sigma.desc", { "标准差（对Gaussian和Bilateral有效）", "Sigma (valid for Gaussian/Bilateral)" } },
    { "option.gaussian", { "高斯滤波", "Gaussian" } },
    { "option.median", { "中值滤波", "Median" } },
    { "option.bilateral", { "双边滤波", "Bilateral" } },
    { "msg.input_null", { "输入图像为空", "Input image is null" } },
    { "msg.gaussian_success", { "高斯滤波执行成功", "Run gaussian filter success" } },
    { "msg.median_success", { "中值滤波执行成功", "Run median filter success" } },
    { "msg.bilateral_success", { "双边滤波执行成功", "Run bilateral filter success" } },
    { "msg.unsupported_filter", { "不支持的滤波类型", "Unsupported filter type" } }
};

std::string Tr(int language, const std::string& key) {
    return hvi18n::Translate(kTexts, key, language);
}

}  // namespace


HVImageFilter::HVImageFilter()
	: inputImg()
	, resultImg()
	, run_time(0)
{

}

int HVImageFilter::init()
{
	execute_status = NODE_STATUS_NOT_RUN;
	error_msg.clear();
	return SUCCESS;
}

int HVImageFilter::run()
{
    auto start = std::chrono::steady_clock::now();
    execute_status = NODE_STATUS_RUNNING;
    error_msg.clear();

    if (!inputImg)
    {
        execute_status = ALGORITHM_RUN_ERROR;
        error_msg = "msg.input_null";
        return ALGORITHM_RUN_ERROR;
    }

    // 1. 输入映射
    cv::Mat input = ImageConverter::ToMat(*inputImg);

    cv::Mat output;

    switch (filter_type)
    {
    case 0: // Gaussian
        cv::GaussianBlur(input, output, cv::Size(kernel_size, kernel_size), sigma);
        error_msg = "msg.gaussian_success";
        break;

    case 1: // Median
        cv::medianBlur(input, output, kernel_size);
        error_msg = "msg.median_success";
        break;

    case 2: // Bilateral
        cv::bilateralFilter(input, output, kernel_size, sigma, sigma);
        error_msg = "msg.bilateral_success";
        break;

    default:
        execute_status = ALGORITHM_RUN_ERROR;
        error_msg = "msg.unsupported_filter";
        return ALGORITHM_RUN_ERROR;
    }

    resultImg = std::make_shared<ImageDataInfo2D>(ImageConverter::FromMat(output));

    auto end = std::chrono::steady_clock::now();
    run_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    execute_status = SUCCESS;
    return SUCCESS;
}

int HVImageFilter::set_algorithm_params(const std::vector<void*>& params, const std::vector<int>& paramID)
{
    if (paramID.size() == 0)
    {
        inputImg = cast_param_sharedPtr<ImageDataInfo2D>(params, 0);
        filter_type = cast_param<int>(params, 1);
        kernel_size = cast_param<int>(params, 2);
        sigma = cast_param<double>(params, 3);
    }
    else
    {
        for (size_t i = 0; i < paramID.size(); i++)
        {
            int id = paramID[i];
            switch (id)
            {
            case 0:
                inputImg = cast_param_sharedPtr<ImageDataInfo2D>(params, i);
                break;
            case 1:
                filter_type = cast_param<int>(params, i);
                break;
            case 2:
                kernel_size = cast_param<int>(params, i);
                break;
            case 3:
                sigma = cast_param<double>(params, i);
                break;
            default:
                break;
            }
		}
    }

    if (kernel_size % 2 == 0)
        kernel_size++;

    return SUCCESS;
}

std::vector<void*> HVImageFilter::get_algorithm_result()
{
    if (execute_status == SUCCESS)
        return { &resultImg };
    return { nullptr };
}

std::vector<int> HVImageFilter::get_algorithm_input_params_type()
{
    return { HV_IMAGEDATAINFO2D, HV_INT, HV_INT, HV_DOUBLE };
}

std::vector<int> HVImageFilter::get_algorithm_output_params_type()
{
    return { HV_IMAGEDATAINFO2D };
}

std::vector<std::string> HVImageFilter::get_algorithm_input_params_name()
{
    return {
        Tr(language_, "input.image.name"),
        Tr(language_, "input.filter_type.name"),
        Tr(language_, "input.kernel_size.name"),
        Tr(language_, "input.sigma.name")
    };
}

std::vector<std::string> HVImageFilter::get_algorithm_output_params_name()
{
    return { Tr(language_, "output.image.name") };
}

std::vector<bool> HVImageFilter::get_algorithm_input_params_bindable()
{
    return std::vector<bool>(get_algorithm_input_params_type().size(), true);
}

std::vector<ParamMetadata> HVImageFilter::get_algorithm_input_params_metadata()
{
    std::vector<ParamMetadata> metadata_list;

    // 参数0: 输入图像 (可绑定，无约束)
    ParamMetadata meta0;
    meta0.param_name = Tr(language_, "input.image.name");
    meta0.param_description = Tr(language_, "input.image.desc");
    meta0.param_type = HV_IMAGEDATAINFO2D;
    meta0.constraint_type = CONSTRAINT_NONE;
    metadata_list.push_back(meta0);

    // 参数1: 滤波类型 (选项约束)
    ParamMetadata meta1;
    meta1.param_name = Tr(language_, "input.filter_type.name");
    meta1.param_description = Tr(language_, "input.filter_type.desc");
    meta1.param_type = HV_INT;
    meta1.constraint_type = CONSTRAINT_OPTIONS;
    meta1.options_constraint.AddOption(0, Tr(language_, "option.gaussian"));
    meta1.options_constraint.AddOption(1, Tr(language_, "option.median"));
    meta1.options_constraint.AddOption(2, Tr(language_, "option.bilateral"));
    meta1.options_constraint.default_index = 0;
    metadata_list.push_back(meta1);

    // 参数2: 卷积核大小 (范围约束)
    ParamMetadata meta2;
    meta2.param_name = Tr(language_, "input.kernel_size.name");
    meta2.param_description = Tr(language_, "input.kernel_size.desc");
    meta2.param_type = HV_INT;
    meta2.constraint_type = CONSTRAINT_RANGE;
    meta2.range_constraint = RangeConstraint(1, 31, 3);
    metadata_list.push_back(meta2);

    // 参数3: sigma值 (范围约束，依赖filter_type为Gaussian或Bilateral)
    ParamMetadata meta3;
    meta3.param_name = Tr(language_, "input.sigma.name");
    meta3.param_description = Tr(language_, "input.sigma.desc");
    meta3.param_type = HV_DOUBLE;
    meta3.constraint_type = CONSTRAINT_RANGE;
    meta3.range_constraint = RangeConstraint(0.1, 100.0, 1.0);
    meta3.dependencies.push_back(ParamDependency(1, DEPENDS_ON_IN_LIST, {"0", "2"}));
    metadata_list.push_back(meta3);

    for (size_t i = 0; i < metadata_list.size(); ++i) {
        metadata_list[i].param_group = (i < 2) ? PARAM_GROUP_BASIC : PARAM_GROUP_ADVANCED;
    }

    return metadata_list;
}

int HVImageFilter::get_algorithm_execute_status()
{
	return execute_status;
}

std::string HVImageFilter::get_algorithm_error_message()
{
    if (error_msg.empty()) {
        return "";
    }
    return Tr(language_, error_msg);
}

long HVImageFilter::get_algorithm_use_time()
{
	return run_time;
}

bool HVImageFilter::algorithm_params_setting_status()
{
	return true;
}

bool HVImageFilter::algorithm_init_status()
{
	return true;
}

bool HVImageFilter::save_params_to_json(const std::string& filePath)
{
    try {
        nlohmann::json params_json = nlohmann::json::array();

        add_param(params_json, "filter_type", HV_DOUBLE, this->filter_type);
        add_param(params_json, "kernel_size", HV_DOUBLE, this->kernel_size);
        add_param(params_json, "sigma", HV_DOUBLE, this->sigma);

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

bool HVImageFilter::load_params_from_json(const std::string& filePath)
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
            if (param_name == "filter_type") {
                // 设置到类成员变量
                this->filter_type = param_json["value"];
            }

            if (param_name == "kernel_size") {
                // 设置到类成员变量
                this->kernel_size = param_json["value"];
            }

            if (param_name == "sigma") {
                // 设置到类成员变量
                this->sigma = param_json["value"];
            }
        }

        return true;
    }
    catch (...) {
        return false;
    }
}

std::vector<void*> HVImageFilter::get_current_params()
{
	return { &inputImg, &filter_type, &kernel_size, &sigma };
}

AlgorithmType HVImageFilter::get_algorithm_type()
{
    return AlgorithmType::ImageProcess;
}

void HVImageFilter::set_language(int language)
{
    if (hvi18n::IsSupportedLanguage(language)) {
        language_ = language;
    }
}

int HVImageFilter::get_language() const
{
    return language_;
}

std::string HVImageFilter::get_algorithm_display_name()
{
    return Tr(language_, "algorithm.display");
}

NodeEngine* CreateInstance() {
    // 每一个 DLL 内部返回自己具体的实现类
    return new HVImageFilter();
}

std::string GetInstanceName() {
    return "Image filter"; // 告知主程序此 DLL 代表的类型
}

extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion() {
    return NODE_ENGINE_ABI_VERSION;
}
