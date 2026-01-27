#pragma once
#include "node_engine.h"
#include "3d_pliot_error.h"
#include "3d_pilot_public_def.h"

#include <string>

class HVImageEdge : public NodeEngine {
public:
    HVImageEdge();
    ~HVImageEdge();

    int init();

    int run();

    // 0: ImageDataInfo2D* 输入图像
	// 1: double* th1 Canny 算法阈值 1
	// 2: double* th2 Canny 算法阈值 2
    int set_algorithm_params(const std::vector<void*>& params, const std::vector<int>& paramID = std::vector<int>());

    std::vector<void*> get_current_params();

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

    bool save_params_to_json(const std::string& filePath);

    bool load_params_from_json(const std::string& filePath);

    AlgorithmType get_algorithm_type();

private:
    std::shared_ptr<ImageDataInfo2D> inputImg;
    std::shared_ptr<ImageDataInfo2D> resultImg;

    double th1 = 50.0;
    double th2 = 150.0;
    int execute_status = 0;
    long run_time = 0;
    std::string error_msg;
};

extern "C" __declspec(dllexport) NodeEngine* CreateInstance();
extern "C" __declspec(dllexport) std::string GetInstanceName();
