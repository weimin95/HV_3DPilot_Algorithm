#pragma once

#include "node_engine.h"
#include "3d_pliot_error.h"
#include "3d_pilot_public_def.h"
#include "param_meta_data.h"

#include <string>

class HVConditionalBranch : public NodeEngine {
public:
    HVConditionalBranch();
    ~HVConditionalBranch() override = default;

    int init() override;
    int run() override;
    int set_algorithm_params(const std::vector<void*>& params, const std::vector<int>& paramID = std::vector<int>()) override;

    std::vector<void*> get_current_params() override;
    std::vector<void*> get_algorithm_result() override;

    std::vector<int> get_algorithm_input_params_type() override;
    std::vector<int> get_algorithm_output_params_type() override;

    std::vector<std::string> get_algorithm_input_params_name() override;
    std::vector<std::string> get_algorithm_output_params_name() override;
    std::vector<bool> get_algorithm_input_params_bindable() override;
    std::vector<ParamMetadata> get_algorithm_input_params_metadata() override;

    int get_algorithm_execute_status() override;
    std::string get_algorithm_error_message() override;
    long get_algorithm_use_time() override;

    bool algorithm_params_setting_status() override;
    bool algorithm_init_status() override;

    bool save_params_to_json(const std::string& filePath) override;
    bool load_params_from_json(const std::string& filePath) override;

    AlgorithmType get_algorithm_type() override;

    void set_language(int language) override;
    int get_language() const override;
    std::string get_algorithm_display_name() override;

private:
    int condition_type_ = 0;
    int int_input_ = 0;
    double float_input_ = 0.0;
    double min_value_ = 0.0;
    double max_value_ = 0.0;
    int ok_target_node_id_ = -1;
    int ng_target_node_id_ = -1;

    int matched_target_node_id_ = -1;
    int execute_status = NODE_STATUS_NOT_RUN;
    long run_time_ = 0;
    std::string error_msg_;
    int language_ = static_cast<int>(UIPilotLanguage::ZH_CN);
};

extern "C" __declspec(dllexport) NodeEngine* CreateInstance();
extern "C" __declspec(dllexport) std::string GetInstanceName();
