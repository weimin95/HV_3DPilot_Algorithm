#include "HVImageReader.h"
#include "HVUtils.h"
#include <chrono>

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

	cv::Mat img = cv::imread(path, cv::IMREAD_UNCHANGED);
	if (img.empty()) 
	{
		error_msg = "Read image failed";
		return ALGORITHM_RUN_ERROR;
	}

	ImageDataInfo2D img_info;
	img_info.width = img.cols;
	img_info.height = img.rows;
	img_info.channels = img.channels();
	size_t data_size = img.cols * img.rows * img.channels() * sizeof(unsigned char);
	img_info.image_data = new char[data_size];
	std::memcpy(img_info.image_data, img.data, data_size);
	resultImg = std::make_shared<ImageDataInfo2D>(img_info);

	auto end = std::chrono::high_resolution_clock::now();
	run_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	error_msg = "Read image success";

	return SUCCESS;
}

int HVImageReader::set_algorithm_params(const std::vector<void*>& params, const std::vector<int>& paramID)
{
	if (params.size() <1) 
	{
		return INVALID_PARAMS_NUM;
	}
	path = *static_cast<std::string*>(params[0]);

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

NodeEngine* CreateInstance() {
	// 每一个 DLL 内部返回自己具体的实现类
	return new HVImageReader();
}

std::string GetInstanceName() {
	return "Image Read"; // 告知主程序此 DLL 代表的类型
}