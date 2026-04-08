#pragma once

#include <memory>
#include <string>
#include <vector>

#include "node_engine.h"
#include "3d_pliot_error.h"
#include "param_meta_data.h"

class HVLuaScriptNode : public NodeEngine {
public:
    HVLuaScriptNode();
    ~HVLuaScriptNode() override = default;

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

public:
    struct ScriptPortDef {
        std::string name;
        int type = -1;
    };

    struct ScriptValue {
        int type = -1;
        bool has_value = false;
        int int_value = 0;
        float float_value = 0.0f;
        std::string string_value;
    };

private:
    bool RebuildSchema(std::string& out_error);
    bool EnsureRuntimeReady(std::string& out_error);
    void MarkRuntimeDirty();
    ScriptValue MakeDefaultValue(int type) const;

    std::string script_content_;
    std::string input_defs_json_;
    std::string output_defs_json_;
    std::vector<ScriptPortDef> input_defs_;
    std::vector<ScriptPortDef> output_defs_;
    std::vector<ScriptValue> input_values_;
    std::vector<ScriptValue> output_values_;

    int execute_status_ = NODE_STATUS_NOT_RUN;
    long run_time_ = 0;
    std::string error_msg_;
    std::string console_output_;
    int language_ = static_cast<int>(UIPilotLanguage::ZH_CN);
    NodeHostServices* host_services_ = nullptr;
    bool runtime_dirty_ = true;
    bool runtime_ready_ = false;

    struct RuntimeState;
    std::unique_ptr<RuntimeState> runtime_state_;
};

extern "C" __declspec(dllexport) NodeEngine* CreateInstance();
extern "C" __declspec(dllexport) std::string GetInstanceName();
