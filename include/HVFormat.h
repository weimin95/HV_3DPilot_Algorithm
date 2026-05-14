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
    int set_algorithm_params(
        const std::vector<void*>& params,
        const std::vector<int>& paramID = std::vector<int>()) override;

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
    enum class ReferenceKind {
        NodeResult,
        GlobalVariable
    };

    struct ParsedReferenceToken {
        ReferenceKind kind = ReferenceKind::NodeResult;
        int node_id = -1;
        std::string node_alias;
        int result_id = -1;
        std::string output_name;
        int global_var_token_id = -1;
        int global_var_id = -1;
        std::string global_var_name;
        int expected_type = -1;
        std::string format_spec;
    };

    int ApplyParam(int param_id, void* value_ptr);
    int RenderFormatText();
    int ParseReferenceToken(const std::string& token_text, ParsedReferenceToken& out_token);
    int ResolveReferenceToken(const ParsedReferenceToken& token, std::string& rendered_value);
    int FormatResolvedValue(const NodeHostDataView& data_view, std::string& rendered_value);
    int FormatHostValue(const NodeHostValue& value, std::string& rendered_value);
    int FormatValue(int type, const void* data, std::string& rendered_value);
    int Fail(int status, const std::string& message_key);

private:
    std::string format_text_;
    std::string separator_;
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
