#pragma once

#include <string>

#include "HVSchemaNodeEngine.h"

class HVLoop : public HVSchemaNodeEngine {
public:
    HVLoop();
    ~HVLoop() override = default;

    int init() override;
    int run() override;

    AlgorithmType get_algorithm_type() override;
    std::string get_algorithm_display_name() override;
    NodeControlMode get_control_mode() const override;

protected:
    std::string TranslateText(const std::string& key) const override;

private:
    bool IsInterruptConditionMet() const;
    int FailLoop(int status, const std::string& message_key);

private:
    HVInputField<int> start_value_{ 0 };
    HVInputField<int> end_value_{ 1 };
    HVInputField<int> interval_ms_{ 0 };
    HVInputField<int> target_node_id_{ -1 };
    HVInputField<bool> interrupt_enabled_{ false };
    HVInputField<int> interrupt_data_type_{ 0 };
    HVInputField<int> compare_operator_{ 0 };
    HVInputField<int> source_int_value_{ 0 };
    HVInputField<int> target_int_value_{ 0 };
    HVInputField<float> source_float_value_{ 0.0f };
    HVInputField<float> target_float_value_{ 0.0f };
    HVInputField<double> source_double_value_{ 0.0 };
    HVInputField<double> target_double_value_{ 0.0 };
    HVInputField<bool> source_bool_value_{ false };
    HVInputField<bool> target_bool_value_{ false };
    HVInputField<std::string> source_string_value_;
    HVInputField<std::string> target_string_value_;

    HVOutputField<int> current_count_output_;
    HVOutputField<int> loop_state_output_;
    HVOutputField<int> execute_status_output_;

    int current_count_ = 0;
    int loop_state_ = 0;
    int next_count_ = 0;
};

extern "C" __declspec(dllexport) NodeEngine* CreateInstance();
extern "C" __declspec(dllexport) std::string GetInstanceName();
extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion();
