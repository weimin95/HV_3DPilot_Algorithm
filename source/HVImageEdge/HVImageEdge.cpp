#include "HVImageEdge.h"
#include <opencv2/opencv.hpp>
#include <chrono>

#include "HVUtils.h"

HVImageEdge::HVImageEdge()
{

}

HVImageEdge::~HVImageEdge()
{

}

int HVImageEdge::init()
{
	return SUCCESS;
}

int HVImageEdge::run()
{
    auto start = std::chrono::steady_clock::now();

    // 1. wrap 输入
    cv::Mat src = wrapToMat(*inputImg);
    cv::Mat gray;

    // 2. 转灰度
    if (src.channels() == 3)
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    else if (src.channels() == 4)
        cv::cvtColor(src, gray, cv::COLOR_BGRA2GRAY);
    else
        gray = src;

    // 3. Canny
    cv::Mat edges;
    cv::Canny(gray, edges, th1, th2);

    resultImg = std::make_shared<ImageDataInfo2D>(matToImageData(edges));

    auto end = std::chrono::steady_clock::now();
    run_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    execute_status = 0;
    error_msg = "Extract edge success";
    return 0;
}

int HVImageEdge::set_algorithm_params(const std::vector<void*>& params, const std::vector<int>& paramID)
{
    if (paramID.size() == 0)
    {
        inputImg = *static_cast<std::shared_ptr<ImageDataInfo2D>*>(params[0]);
        th1 = *static_cast<double*>(params[1]);
        th2 = *static_cast<double*>(params[2]);
    }
    else
    {
        for (size_t i = 0; i < paramID.size(); i++)
        {
            int id = paramID[i];
            switch (id)
            {
            case 0:
                inputImg = *static_cast<std::shared_ptr<ImageDataInfo2D>*>(params[i]);
                break;
            case 1:
                th1 = *static_cast<double*>(params[i]);
                break;
            case 2:
                th2 = *static_cast<double*>(params[i]);
                break;
            default:
                break;
            }
		}
    }

	return SUCCESS;
}

std::vector<void*> HVImageEdge::get_algorithm_result()
{
	return { &resultImg };
}

std::vector<int> HVImageEdge::get_algorithm_input_params_type()
{
    return { HV_IMAGEDATAINFO2D, HV_DOUBLE, HV_DOUBLE };
}

std::vector<int> HVImageEdge::get_algorithm_output_params_type()
{
    return { HV_IMAGEDATAINFO2D };
}

std::vector<std::string> HVImageEdge::get_algorithm_input_params_name()
{
    return { "input image", "low threshold", "high threshold"};
}

std::vector<std::string> HVImageEdge::get_algorithm_output_params_name()
{
    return { "output image" };
}

std::vector<bool> HVImageEdge::get_algorithm_input_params_bindable()
{
    return { true, false, false };
}

int HVImageEdge::get_algorithm_execute_status()
{
	return SUCCESS;
}

std::string HVImageEdge::get_algorithm_error_message()
{
    return error_msg;
}

long HVImageEdge::get_algorithm_use_time()
{
	return run_time;
}

bool HVImageEdge::algorithm_params_setting_status()
{
	return true;
}

bool HVImageEdge::algorithm_init_status()
{
	return true;
}

NodeEngine* CreateInstance() {
    // 每一个 DLL 内部返回自己具体的实现类
    return new HVImageEdge();
}

std::string GetInstanceName() {
    return "Image edge"; // 告知主程序此 DLL 代表的类型
}