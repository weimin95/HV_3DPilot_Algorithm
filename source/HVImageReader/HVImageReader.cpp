#include "HVImageReader.h"
#include "HVUtils.h"
#include "json.hpp"

#include <chrono>
#include <fstream>

HVImageReader::HVImageReader()
{
	
}

HVImageReader::~HVImageReader()
{

}

int HVImageReader::init()
{
	return SUCCESS;
}

int HVImageReader::run()
{
	auto start = std::chrono::high_resolution_clock::now();

	cv::Mat img = cv::imread(image_path, cv::IMREAD_UNCHANGED);
	if (img.empty()) 
	{
		error_msg = "Read image failed";
		return ALGORITHM_RUN_ERROR;
	}

	resultImg = std::make_shared<ImageDataInfo2D>(ImageConverter::FromMat(img));

	auto end = std::chrono::high_resolution_clock::now();
	run_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	error_msg = "Read image success";

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
	return { &resultImg };
}

std::vector<int> HVImageReader::get_algorithm_input_params_type()
{
	return { HV_STRING };
}

std::vector<int> HVImageReader::get_algorithm_output_params_type()
{
	return { HV_IMAGEDATAINFO2D };
}

std::vector<std::string> HVImageReader::get_algorithm_input_params_name()
{
	return { "image path" };
}

std::vector<std::string> HVImageReader::get_algorithm_output_params_name()
{
	return { "output image" };
}

std::vector<bool> HVImageReader::get_algorithm_input_params_bindable()
{
	return { false };
}

int HVImageReader::get_algorithm_execute_status()
{
	return SUCCESS;
}

std::string HVImageReader::get_algorithm_error_message()
{
	return error_msg;
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

NodeEngine* CreateInstance() {
	// 每一个 DLL 内部返回自己具体的实现类
	return new HVImageReader();
}

std::string GetInstanceName() {
	return "Image read"; // 告知主程序此 DLL 代表的类型
}