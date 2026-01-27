#include "HVImageFilter.h"
#include "HVUtils.h"
#include <chrono>


HVImageFilter::HVImageFilter()
	: inputImg()
	, resultImg()
	, run_time(0)
{

}

int HVImageFilter::init()
{
	return SUCCESS;
}

int HVImageFilter::run()
{
    auto start = std::chrono::steady_clock::now();

    // 1. 输入映射
    cv::Mat input = ImageConverter::ToMat(*inputImg);

    cv::Mat output;

    switch (filter_type)
    {
    case 0: // Gaussian
        cv::GaussianBlur(input, output, cv::Size(kernel_size, kernel_size), sigma);
        error_msg = "Run gaussian filter success";
        break;

    case 1: // Median
        cv::medianBlur(input, output, kernel_size);
        error_msg = "Run median filter success";
        break;

    case 2: // Bilateral
        cv::bilateralFilter(input, output, kernel_size, sigma, sigma);
        error_msg = "Run bilateral filter success";
        break;

    default:
        return -2;
    }

    resultImg = std::make_shared<ImageDataInfo2D>(ImageConverter::FromMat(output));

    auto end = std::chrono::steady_clock::now();
    run_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    execute_status = 0;
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
    return { &resultImg };
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
    return { "input image", "filter type", "kernel size", "sigma" };
}

std::vector<std::string> HVImageFilter::get_algorithm_output_params_name()
{
    return { "output image" };
}

std::vector<bool> HVImageFilter::get_algorithm_input_params_bindable()
{
    return { true, false, false, false };
}

int HVImageFilter::get_algorithm_execute_status()
{
	return SUCCESS;
}

std::string HVImageFilter::get_algorithm_error_message()
{
    return error_msg;
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

NodeEngine* CreateInstance() {
    // 每一个 DLL 内部返回自己具体的实现类
    return new HVImageFilter();
}

std::string GetInstanceName() {
    return "Image filter"; // 告知主程序此 DLL 代表的类型
}