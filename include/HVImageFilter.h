#pragma once
#include "node_engine.h"
#include "3d_pliot_error.h"
#include "3d_pilot_public_def.h"

class HVImageFilter : public NodeEngine {
public:
    HVImageFilter();
    ~HVImageFilter() {};

    int init();

    int run();

    // 0: ImageDataInfo2D * 输入图像
    // 1: int* 滤波类型（0: Gaussian, 1: Median, 2: Bilateral）
    // 2: int* 核大小
    // 3: double* sigma 值（仅 Gaussian 和 Bilateral 有效）
    int set_algorithm_params(const std::vector<void*>& params, const std::vector<int>& paramID = std::vector<int>());

    std::vector<void*> get_algorithm_result();

    std::vector<int> get_algorithm_input_params_type();

    std::vector<int> get_algorithm_output_params_type();

    std::vector<std::string> get_algorithm_input_params_name();

    std::vector<std::string> get_algorithm_output_params_name();

    std::vector<bool> get_algorithm_input_params_bindable();

    int get_algorithm_execute_status();

    std::string get_algorithm_error_message();

    long get_algorithm_use_time();

	bool algorithm_params_setting_status();

	bool algorithm_init_status();

private:
    std::shared_ptr<ImageDataInfo2D> inputImg;
    std::shared_ptr<ImageDataInfo2D> resultImg;

    int filter_type = 0;   // 0: Gaussian, 1: Median, 2: Bilateral
    int kernel_size = 3;
    double sigma = 1.0;

    int execute_status = 0;
    long run_time = 0;
    std::string error_msg;
};

extern "C" __declspec(dllexport) NodeEngine* CreateInstance();
extern "C" __declspec(dllexport) std::string GetInstanceName();