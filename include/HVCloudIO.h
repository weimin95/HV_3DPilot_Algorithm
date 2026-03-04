#pragma once
#include "node_engine.h"
#include "3d_pliot_error.h"
#include "3d_pilot_public_def.h"
#include "param_meta_data.h"

#include <string>

class HVCloudIO : public NodeEngine {
public:
    HVCloudIO();
    ~HVCloudIO();

    int init();

    int run();

    // 0: string* 点云文件路径
    int set_algorithm_params(const std::vector<void*>& params, const std::vector<int>& paramID = std::vector<int>());

    std::vector<void*> get_algorithm_result();

    std::vector<void*> get_current_params();

    std::vector<int> get_algorithm_input_params_type();

    std::vector<int> get_algorithm_output_params_type();

    std::vector<std::string> get_algorithm_input_params_name();

    std::vector<std::string> get_algorithm_output_params_name();

    std::vector<bool> get_algorithm_input_params_bindable();

    std::vector<ParamMetadata> get_algorithm_input_params_metadata() override;

    int get_algorithm_execute_status();

    std::string get_algorithm_error_message();

    long get_algorithm_use_time();

	bool algorithm_params_setting_status();

	bool algorithm_init_status();

    bool save_params_to_json(const std::string& filePath);

    bool load_params_from_json(const std::string& filePath);

    AlgorithmType get_algorithm_type();

    void set_language(int language) override;
    int get_language() const override;
    std::string get_algorithm_display_name() override;

private:
    std::string cloud_path;
    std::shared_ptr<HVPointCloud> cloudout;

    int execute_status = NODE_STATUS_NOT_RUN;
    long run_time = 0;
    std::string error_msg;
    int language_ = static_cast<int>(UIPilotLanguage::ZH_CN);
};

extern "C" __declspec(dllexport) NodeEngine* CreateInstance();
extern "C" __declspec(dllexport) std::string GetInstanceName();
