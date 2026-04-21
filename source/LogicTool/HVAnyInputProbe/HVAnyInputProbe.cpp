#include "HVAnyInputProbe.h"

#include <chrono>

HVAnyInputProbe::HVAnyInputProbe()
{
}

int HVAnyInputProbe::init()
{
    host_result_code_ = INSTANCE_NOT_EXIST;
    input_type_ = -1;
    has_value_ = 0;
    int_value_ = 0;
    execute_status_ = NODE_STATUS_NOT_RUN;
    run_time_ = 0;
    error_msg_.clear();
    return SUCCESS;
}

int HVAnyInputProbe::run()
{
    const auto start = std::chrono::steady_clock::now();

    NodeHostDataView data_view;
    host_result_code_ = INSTANCE_NOT_EXIST;
    input_type_ = -1;
    has_value_ = 0;
    int_value_ = 0;
    error_msg_.clear();

    if (host_services_ == nullptr) {
        host_result_code_ = INSTANCE_NOT_EXIST;
        error_msg_ = "host services is null";
    }
    else {
        host_result_code_ = host_services_->get_current_input_data(0, data_view);
        input_type_ = data_view.type;
        has_value_ = data_view.has_value ? 1 : 0;
        if (data_view.has_value && data_view.data != nullptr && data_view.type == HV_INT) {
            int_value_ = *static_cast<const int*>(data_view.data);
        }
    }

    execute_status_ = SUCCESS;
    const auto end = std::chrono::steady_clock::now();
    run_time_ = static_cast<long>(
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
    return SUCCESS;
}

int HVAnyInputProbe::set_algorithm_params(const std::vector<void*>& params, const std::vector<int>& paramID)
{
    (void)params;
    (void)paramID;
    return SUCCESS;
}

std::vector<void*> HVAnyInputProbe::get_current_params()
{
    return { nullptr };
}

std::vector<void*> HVAnyInputProbe::get_algorithm_result()
{
    return { &host_result_code_, &input_type_, &has_value_, &int_value_, &execute_status_ };
}

std::vector<int> HVAnyInputProbe::get_algorithm_input_params_type()
{
    return { HV_ANYINPUT };
}

std::vector<int> HVAnyInputProbe::get_algorithm_output_params_type()
{
    return { HV_INT, HV_INT, HV_INT, HV_INT, HV_INT };
}

std::vector<std::string> HVAnyInputProbe::get_algorithm_input_params_name()
{
    return { "input" };
}

std::vector<std::string> HVAnyInputProbe::get_algorithm_output_params_name()
{
    return {
        "host_result_code",
        "input_type",
        "has_value",
        "int_value",
        "execute_status"
    };
}

std::vector<bool> HVAnyInputProbe::get_algorithm_input_params_bindable()
{
    return { true };
}

std::vector<ParamMetadata> HVAnyInputProbe::get_algorithm_input_params_metadata()
{
    ParamMetadata metadata;
    metadata.param_name = "input";
    metadata.param_description = "Probe host current input data";
    metadata.param_type = HV_ANYINPUT;
    metadata.constraint_type = CONSTRAINT_NONE;
    metadata.param_group = PARAM_GROUP_BASIC;
    return { metadata };
}

int HVAnyInputProbe::get_algorithm_execute_status()
{
    return execute_status_;
}

std::string HVAnyInputProbe::get_algorithm_error_message()
{
    return error_msg_;
}

long HVAnyInputProbe::get_algorithm_use_time()
{
    return run_time_;
}

bool HVAnyInputProbe::algorithm_params_setting_status()
{
    return true;
}

bool HVAnyInputProbe::algorithm_init_status()
{
    return true;
}

bool HVAnyInputProbe::save_params_to_json(const std::string& filePath)
{
    (void)filePath;
    return true;
}

bool HVAnyInputProbe::load_params_from_json(const std::string& filePath)
{
    (void)filePath;
    return true;
}

AlgorithmType HVAnyInputProbe::get_algorithm_type()
{
    return AlgorithmType::LogicTool;
}

void HVAnyInputProbe::set_language(int language)
{
    language_ = language;
}

int HVAnyInputProbe::get_language() const
{
    return language_;
}

std::string HVAnyInputProbe::get_algorithm_display_name()
{
    return "Any input probe";
}

void HVAnyInputProbe::set_host_services(NodeHostServices* host_services)
{
    host_services_ = host_services;
}

extern "C" __declspec(dllexport) NodeEngine* CreateInstance()
{
    return new HVAnyInputProbe();
}

extern "C" __declspec(dllexport) std::string GetInstanceName()
{
    return "Any input probe";
}

extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion()
{
    return NODE_ENGINE_ABI_VERSION;
}
