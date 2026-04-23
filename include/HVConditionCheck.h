#pragma once

#include <string>

#include "HVSchemaNodeEngine.h"

class HVConditionCheck : public HVSchemaNodeEngine {
public:
    HVConditionCheck();
    ~HVConditionCheck() override = default;

    int init() override;
    int run() override;

    AlgorithmType get_algorithm_type() override;
    std::string get_algorithm_display_name() override;

protected:
    std::string TranslateText(const std::string& key) const override;

private:
    HVInputField<int> judge_mode_{ 1 };

    HVInputField<bool> rule1_enabled_{ true };
    HVInputField<int> rule1_type_{ 0 };
    HVInputField<int> rule1_int_input_;
    HVInputField<float> rule1_float_input_;
    HVInputField<double> rule1_double_input_;
    HVInputField<int> rule1_int_min_{ 0 };
    HVInputField<int> rule1_int_max_{ 0 };
    HVInputField<float> rule1_float_min_{ 0.0f };
    HVInputField<float> rule1_float_max_{ 0.0f };
    HVInputField<double> rule1_double_min_{ 0.0 };
    HVInputField<double> rule1_double_max_{ 0.0 };

    HVInputField<bool> rule2_enabled_{ false };
    HVInputField<int> rule2_type_{ 0 };
    HVInputField<int> rule2_int_input_;
    HVInputField<float> rule2_float_input_;
    HVInputField<double> rule2_double_input_;
    HVInputField<int> rule2_int_min_{ 0 };
    HVInputField<int> rule2_int_max_{ 0 };
    HVInputField<float> rule2_float_min_{ 0.0f };
    HVInputField<float> rule2_float_max_{ 0.0f };
    HVInputField<double> rule2_double_min_{ 0.0 };
    HVInputField<double> rule2_double_max_{ 0.0 };

    HVOutputField<std::string> result_string_output_;
    HVOutputField<int> result_int_output_;
    HVOutputField<int> execute_status_output_;

    std::string result_string_;
    int result_int_ = 0;
};

extern "C" __declspec(dllexport) NodeEngine* CreateInstance();
extern "C" __declspec(dllexport) std::string GetInstanceName();
extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion();
