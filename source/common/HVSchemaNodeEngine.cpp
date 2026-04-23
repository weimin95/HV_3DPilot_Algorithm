#include "HVSchemaNodeEngine.h"

#include <fstream>
#include <unordered_map>

std::vector<void*> HVSchemaNodeEngine::get_current_params()
{
    std::vector<void*> params;
    params.reserve(input_fields_.size());
    for (auto* field : input_fields_) {
        params.push_back(field != nullptr ? field->current_value_ptr() : nullptr);
    }
    return params;
}

std::vector<void*> HVSchemaNodeEngine::get_algorithm_result()
{
    std::vector<void*> results;
    results.reserve(output_fields_.size());
    for (auto* field : output_fields_) {
        results.push_back(field != nullptr ? field->result_value_ptr(execute_status_) : nullptr);
    }
    return results;
}

std::vector<int> HVSchemaNodeEngine::get_algorithm_input_params_type()
{
    std::vector<int> types;
    types.reserve(input_fields_.size());
    for (const auto* field : input_fields_) {
        types.push_back(field != nullptr ? field->hv_type() : -1);
    }
    return types;
}

std::vector<int> HVSchemaNodeEngine::get_algorithm_output_params_type()
{
    std::vector<int> types;
    types.reserve(output_fields_.size());
    for (const auto* field : output_fields_) {
        types.push_back(field != nullptr ? field->hv_type() : -1);
    }
    return types;
}

std::vector<std::string> HVSchemaNodeEngine::get_algorithm_input_params_name()
{
    std::vector<std::string> names;
    names.reserve(input_fields_.size());
    for (const auto* field : input_fields_) {
        names.push_back(field != nullptr ? field->display_name() : std::string());
    }
    return names;
}

std::vector<std::string> HVSchemaNodeEngine::get_algorithm_output_params_name()
{
    std::vector<std::string> names;
    names.reserve(output_fields_.size());
    for (const auto* field : output_fields_) {
        names.push_back(field != nullptr ? field->display_name() : std::string());
    }
    return names;
}

std::vector<bool> HVSchemaNodeEngine::get_algorithm_input_params_bindable()
{
    std::vector<bool> bindable;
    bindable.reserve(input_fields_.size());
    for (const auto* field : input_fields_) {
        bindable.push_back(field != nullptr && field->bindable());
    }
    return bindable;
}

std::vector<ParamMetadata> HVSchemaNodeEngine::get_algorithm_input_params_metadata()
{
    std::vector<ParamMetadata> metadata_list;
    metadata_list.reserve(input_fields_.size());
    for (const auto* field : input_fields_) {
        metadata_list.push_back(field != nullptr ? field->build_metadata() : ParamMetadata());
    }
    return metadata_list;
}

int HVSchemaNodeEngine::set_algorithm_params(
    const std::vector<void*>& params,
    const std::vector<int>& paramID)
{
    if (paramID.empty()) {
        if (params.size() > input_fields_.size()) {
            return INVALID_PARAMS_NUM;
        }
        for (size_t i = 0; i < params.size(); ++i) {
            if (input_fields_[i] == nullptr) {
                return INVALID_PARAMS_NUM;
            }
            const int ret = input_fields_[i]->set_from_void(params[i]);
            if (ret != SUCCESS) {
                return ret;
            }
        }
        return SUCCESS;
    }

    if (params.size() != paramID.size()) {
        return INVALID_PARAMS_NUM;
    }

    for (size_t i = 0; i < paramID.size(); ++i) {
        const int field_index = paramID[i];
        if (field_index < 0 || field_index >= static_cast<int>(input_fields_.size())) {
            return INVALID_PARAMS_NUM;
        }
        if (input_fields_[field_index] == nullptr) {
            return INVALID_PARAMS_NUM;
        }
        const int ret = input_fields_[field_index]->set_from_void(params[i]);
        if (ret != SUCCESS) {
            return ret;
        }
    }
    return SUCCESS;
}

int HVSchemaNodeEngine::get_algorithm_execute_status()
{
    return execute_status_;
}

std::string HVSchemaNodeEngine::get_algorithm_error_message()
{
    if (error_message_key_.empty()) {
        return {};
    }
    return TranslateText(error_message_key_);
}

long HVSchemaNodeEngine::get_algorithm_use_time()
{
    return run_time_;
}

bool HVSchemaNodeEngine::algorithm_params_setting_status()
{
    return true;
}

bool HVSchemaNodeEngine::algorithm_init_status()
{
    return true;
}

bool HVSchemaNodeEngine::save_params_to_json(const std::string& filePath)
{
    try {
        nlohmann::json params_json = nlohmann::json::array();
        for (const auto* field : input_fields_) {
            if (field == nullptr || !field->persist()) {
                continue;
            }
            nlohmann::json entry_json;
            if (!field->save_value(entry_json)) {
                return false;
            }
            params_json.push_back(entry_json);
        }

        std::ofstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }
        file << params_json.dump(4);
        return true;
    }
    catch (...) {
        return false;
    }
}

bool HVSchemaNodeEngine::load_params_from_json(const std::string& filePath)
{
    try {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }

        nlohmann::json params_json;
        file >> params_json;
        if (!params_json.is_array()) {
            return false;
        }

        std::unordered_map<std::string, const nlohmann::json*> entry_map;
        for (const auto& entry_json : params_json) {
            if (!entry_json.is_object() || !entry_json.contains("name")) {
                continue;
            }
            entry_map[entry_json["name"].get<std::string>()] = &entry_json;
        }

        for (auto* field : input_fields_) {
            if (field == nullptr || !field->persist()) {
                continue;
            }
            const std::string key = field->storage_key().empty() ? field->schema_name() : field->storage_key();
            const auto it = entry_map.find(key);
            if (it == entry_map.end()) {
                continue;
            }
            if (!field->load_value(*it->second)) {
                return false;
            }
        }
        return true;
    }
    catch (...) {
        return false;
    }
}

void HVSchemaNodeEngine::set_language(int language)
{
    language_ = language;
}

int HVSchemaNodeEngine::get_language() const
{
    return language_;
}

void HVSchemaNodeEngine::set_host_services(NodeHostServices* host_services)
{
    host_services_ = host_services;
}

void HVSchemaNodeEngine::ClearRegisteredSchema()
{
    input_fields_.clear();
    output_fields_.clear();
}

void HVSchemaNodeEngine::ResetRuntimeState()
{
    execute_status_ = NODE_STATUS_NOT_RUN;
    run_time_ = 0;
    error_message_key_.clear();
}

int HVSchemaNodeEngine::FailWithMessage(int status, const std::string& message_key)
{
    execute_status_ = status;
    error_message_key_ = message_key;
    return status;
}
