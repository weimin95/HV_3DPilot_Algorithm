#pragma once

#include <string>
#include <vector>

#include "3d_pliot_error.h"
#include "node_engine.h"
#include "param_meta_data.h"

class HVFormat : public NodeEngine {
public:
    HVFormat();
    ~HVFormat() override = default;

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
    void set_host_services(NodeHostServices* host_services) override;

private:
    int ApplyParam(int param_id, void* value_ptr);
    int ParseRowsFromJson(std::vector<std::string>& rows);
    int RenderRow(const std::string& row_template, std::string& rendered_row);
    int ReadInputSlotValue(int slot_index, NodeHostDataView& data_view) const;
    int FormatSlotValue(const NodeHostDataView& data_view, const std::string& format_text, std::string& rendered_value);
    int FormatIntegerValue(long long value, const std::string& format_text, std::string& rendered_value);
    int FormatFloatingValue(double value, const std::string& format_text, std::string& rendered_value);
    int Fail(int status, const std::string& message_key);

private:
    std::string rules_json_;
    std::string segment_separator_ = ";";
    std::string output_ending_ = "\r\n";

    std::string formatted_text_;
    int execute_status_ = NODE_STATUS_NOT_RUN;
    long run_time_ = 0;
    std::string error_message_key_;
    int language_ = static_cast<int>(UIPilotLanguage::ZH_CN);
    NodeHostServices* host_services_ = nullptr;
};

extern "C" __declspec(dllexport) NodeEngine* CreateInstance();
extern "C" __declspec(dllexport) std::string GetInstanceName();
extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion();
