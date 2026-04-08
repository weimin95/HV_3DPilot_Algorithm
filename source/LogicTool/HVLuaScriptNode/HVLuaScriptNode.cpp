#include "HVLuaScriptNode.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <stdexcept>
#include <utility>

#include <json.hpp>
#include <sol/sol.hpp>

#include "HVUtils.h"

namespace {

constexpr int kConfigParamCount = 3;

bool IsSupportedScriptType(int type)
{
    return type == HV_INT || type == HV_FLOAT || type == HV_STRING;
}

int FindPortIndex(const std::vector<HVLuaScriptNode::ScriptPortDef>& defs, const std::string& name)
{
    for (int i = 0; i < static_cast<int>(defs.size()); ++i) {
        if (defs[i].name == name) {
            return i;
        }
    }
    return -1;
}

void* PointerFromScriptValue(HVLuaScriptNode::ScriptValue& value)
{
    switch (value.type) {
    case HV_INT:
        return &value.int_value;
    case HV_FLOAT:
        return &value.float_value;
    case HV_STRING:
        return &value.string_value;
    default:
        return nullptr;
    }
}

void* PointerFromOutputValue(HVLuaScriptNode::ScriptValue& value)
{
    return value.has_value ? PointerFromScriptValue(value) : nullptr;
}

sol::object HostValueToLua(sol::state_view lua, const NodeHostValue& value)
{
    if (!value.has_value) {
        return sol::make_object(lua, sol::nil);
    }

    switch (value.type) {
    case HV_INT:
        return sol::make_object(lua, value.int_value);
    case HV_LONG:
        return sol::make_object(lua, static_cast<long long>(value.long_value));
    case HV_FLOAT:
        return sol::make_object(lua, value.float_value);
    case HV_DOUBLE:
        return sol::make_object(lua, value.double_value);
    case HV_STRING:
        return sol::make_object(lua, value.string_value);
    default:
        return sol::make_object(lua, sol::nil);
    }
}

bool LuaObjectToHostValue(const sol::object& object, NodeHostValue& out_value)
{
    out_value = NodeHostValue();
    if (!object.valid() || object == sol::lua_nil) {
        return false;
    }

    if (object.is<int>()) {
        out_value.type = HV_INT;
        out_value.has_value = true;
        out_value.int_value = object.as<int>();
        return true;
    }
    if (object.is<double>()) {
        out_value.type = HV_FLOAT;
        out_value.has_value = true;
        out_value.float_value = static_cast<float>(object.as<double>());
        return true;
    }
    if (object.is<std::string>()) {
        out_value.type = HV_STRING;
        out_value.has_value = true;
        out_value.string_value = object.as<std::string>();
        return true;
    }

    return false;
}

bool ParsePortDefsJson(
    const std::string& json_text,
    std::vector<HVLuaScriptNode::ScriptPortDef>& out_defs,
    std::string& out_error)
{
    out_defs.clear();
    if (json_text.empty()) {
        return true;
    }

    try {
        const nlohmann::json defs_json = nlohmann::json::parse(json_text);
        if (!defs_json.is_array()) {
            out_error = "Port definitions must be a JSON array";
            return false;
        }

        std::vector<std::string> names;
        for (const auto& item : defs_json) {
            if (!item.is_object()) {
                out_error = "Each port definition must be an object";
                return false;
            }
            if (!item.contains("name") || !item["name"].is_string()) {
                out_error = "Port definition is missing a string name";
                return false;
            }
            if (!item.contains("type") || !item["type"].is_number_integer()) {
                out_error = "Port definition is missing an integer type";
                return false;
            }

            HVLuaScriptNode::ScriptPortDef def;
            def.name = item["name"].get<std::string>();
            def.type = item["type"].get<int>();
            if (def.name.empty()) {
                out_error = "Port names must not be empty";
                return false;
            }
            if (!IsSupportedScriptType(def.type)) {
                out_error = "Unsupported script port type";
                return false;
            }
            if (std::find(names.begin(), names.end(), def.name) != names.end()) {
                out_error = "Duplicate script port name";
                return false;
            }

            names.push_back(def.name);
            out_defs.push_back(def);
        }
    }
    catch (const std::exception& e) {
        out_error = e.what();
        return false;
    }

    return true;
}

}  // namespace

struct HVLuaScriptNode::RuntimeState {
    std::unique_ptr<sol::state> lua;
    sol::protected_function process;
};

HVLuaScriptNode::HVLuaScriptNode() = default;

int HVLuaScriptNode::init()
{
    execute_status_ = NODE_STATUS_NOT_RUN;
    run_time_ = 0;
    error_msg_.clear();
    console_output_.clear();
    MarkRuntimeDirty();
    return SUCCESS;
}

void HVLuaScriptNode::MarkRuntimeDirty()
{
    runtime_dirty_ = true;
    runtime_ready_ = false;
    runtime_state_.reset();
}

HVLuaScriptNode::ScriptValue HVLuaScriptNode::MakeDefaultValue(int type) const
{
    ScriptValue value;
    value.type = type;
    value.has_value = true;
    switch (type) {
    case HV_INT:
        value.int_value = 0;
        break;
    case HV_FLOAT:
        value.float_value = 0.0f;
        break;
    case HV_STRING:
        value.string_value.clear();
        break;
    default:
        value.has_value = false;
        break;
    }
    return value;
}

bool HVLuaScriptNode::RebuildSchema(std::string& out_error)
{
    std::vector<ScriptPortDef> new_input_defs;
    std::vector<ScriptPortDef> new_output_defs;
    if (!ParsePortDefsJson(input_defs_json_, new_input_defs, out_error)) {
        return false;
    }
    if (!ParsePortDefsJson(output_defs_json_, new_output_defs, out_error)) {
        return false;
    }

    const std::vector<ScriptPortDef> old_input_defs = input_defs_;
    const std::vector<ScriptValue> old_input_values = input_values_;
    const std::vector<ScriptPortDef> old_output_defs = output_defs_;
    const std::vector<ScriptValue> old_output_values = output_values_;

    input_defs_ = std::move(new_input_defs);
    output_defs_ = std::move(new_output_defs);

    input_values_.clear();
    input_values_.reserve(input_defs_.size());
    for (const auto& def : input_defs_) {
        ScriptValue value = MakeDefaultValue(def.type);
        for (size_t i = 0; i < old_input_defs.size() && i < old_input_values.size(); ++i) {
            if (old_input_defs[i].name == def.name && old_input_defs[i].type == def.type) {
                value = old_input_values[i];
                break;
            }
        }
        value.type = def.type;
        input_values_.push_back(value);
    }

    output_values_.clear();
    output_values_.reserve(output_defs_.size());
    for (const auto& def : output_defs_) {
        ScriptValue value = MakeDefaultValue(def.type);
        value.has_value = false;
        for (size_t i = 0; i < old_output_defs.size() && i < old_output_values.size(); ++i) {
            if (old_output_defs[i].name == def.name && old_output_defs[i].type == def.type) {
                value = old_output_values[i];
                break;
            }
        }
        value.type = def.type;
        output_values_.push_back(value);
    }

    MarkRuntimeDirty();
    return true;
}

bool HVLuaScriptNode::EnsureRuntimeReady(std::string& out_error)
{
    if (!runtime_dirty_ && runtime_ready_ && runtime_state_ != nullptr) {
        return true;
    }

    runtime_state_ = std::make_unique<RuntimeState>();
    runtime_state_->lua = std::make_unique<sol::state>();
    sol::state& lua = *runtime_state_->lua;
    lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table, sol::lib::utf8);

    lua.set_function("GetIntValue", [this](const std::string& name) {
        const int index = FindPortIndex(input_defs_, name);
        if (index < 0 || input_defs_[index].type != HV_INT) {
            throw std::runtime_error("Unknown int input: " + name);
        }
        return input_values_[index].int_value;
    });
    lua.set_function("GetFloatValue", [this](const std::string& name) {
        const int index = FindPortIndex(input_defs_, name);
        if (index < 0 || input_defs_[index].type != HV_FLOAT) {
            throw std::runtime_error("Unknown float input: " + name);
        }
        return input_values_[index].float_value;
    });
    lua.set_function("GetStringValue", [this](const std::string& name) {
        const int index = FindPortIndex(input_defs_, name);
        if (index < 0 || input_defs_[index].type != HV_STRING) {
            throw std::runtime_error("Unknown string input: " + name);
        }
        return input_values_[index].string_value;
    });

    lua.set_function("SetIntValue", [this](const std::string& name, int value) {
        const int index = FindPortIndex(output_defs_, name);
        if (index < 0 || output_defs_[index].type != HV_INT) {
            throw std::runtime_error("Unknown int output: " + name);
        }
        output_values_[index].type = HV_INT;
        output_values_[index].has_value = true;
        output_values_[index].int_value = value;
    });
    lua.set_function("SetFloatValue", [this](const std::string& name, double value) {
        const int index = FindPortIndex(output_defs_, name);
        if (index < 0 || output_defs_[index].type != HV_FLOAT) {
            throw std::runtime_error("Unknown float output: " + name);
        }
        output_values_[index].type = HV_FLOAT;
        output_values_[index].has_value = true;
        output_values_[index].float_value = static_cast<float>(value);
    });
    lua.set_function("SetStringValue", [this](const std::string& name, const std::string& value) {
        const int index = FindPortIndex(output_defs_, name);
        if (index < 0 || output_defs_[index].type != HV_STRING) {
            throw std::runtime_error("Unknown string output: " + name);
        }
        output_values_[index].type = HV_STRING;
        output_values_[index].has_value = true;
        output_values_[index].string_value = value;
    });

    lua.set_function("GetGlobalValue", [this, &lua](const std::string& name) {
        if (host_services_ == nullptr) {
            throw std::runtime_error("Host services are unavailable");
        }
        NodeHostValue value;
        const int ret = host_services_->get_global_value_by_name(name, value);
        if (ret == GLOBAL_VAR_NOT_EXIST) {
            return sol::make_object(lua, sol::nil);
        }
        if (ret != SUCCESS) {
            throw std::runtime_error("Failed to read global value: " + name);
        }
        return HostValueToLua(lua, value);
    });
    lua.set_function("GetNodeResult", [this, &lua](const std::string& node_alias, const std::string& output_name) {
        if (host_services_ == nullptr) {
            throw std::runtime_error("Host services are unavailable");
        }

        NodeHostValue value;
        const int ret = host_services_->get_upstream_result_by_name(node_alias, output_name, value);
        if (ret == SUCCESS) {
            return HostValueToLua(lua, value);
        }

        const std::string output_label = node_alias + "." + output_name;
        switch (ret) {
        case UPSTREAM_RESULT_NODE_NOT_FOUND:
            throw std::runtime_error("Unknown upstream node alias: " + node_alias);
        case UPSTREAM_RESULT_ALIAS_AMBIGUOUS:
            throw std::runtime_error("Node alias is ambiguous: " + node_alias);
        case UPSTREAM_RESULT_NOT_REACHABLE:
            throw std::runtime_error("Node is not upstream-reachable: " + node_alias);
        case UPSTREAM_RESULT_UNAVAILABLE:
            throw std::runtime_error("Upstream result is unavailable: " + output_label);
        case UPSTREAM_RESULT_OUTPUT_NOT_FOUND:
            throw std::runtime_error("Unknown upstream output: " + output_label);
        case UPSTREAM_RESULT_TYPE_NOT_SUPPORTED:
            throw std::runtime_error("Unsupported upstream output type: " + output_label);
        default:
            throw std::runtime_error("Failed to read upstream result: " + output_label);
        }
    });
    lua.set_function("SetGlobalValue", [this](const std::string& name, const sol::object& object) {
        if (host_services_ == nullptr) {
            throw std::runtime_error("Host services are unavailable");
        }
        NodeHostValue value;
        if (!LuaObjectToHostValue(object, value)) {
            throw std::runtime_error("Unsupported Lua value for global assignment");
        }
        const int ret = host_services_->set_global_value_by_name(name, value, true);
        if (ret < 0) {
            throw std::runtime_error("Failed to write global value: " + name);
        }
    });
    lua.set_function("GetNodeResult", [this, &lua](const std::string& node_alias, const std::string& output_name) {
        if (host_services_ == nullptr) {
            throw std::runtime_error("Host services are unavailable");
        }

        NodeHostValue value;
        const int ret = host_services_->get_upstream_result_by_name(node_alias, output_name, value);
        if (ret == SUCCESS) {
            return HostValueToLua(lua, value);
        }

        switch (ret) {
        case UPSTREAM_RESULT_NODE_NOT_FOUND:
            throw std::runtime_error("Unknown upstream node alias: " + node_alias);
        case UPSTREAM_RESULT_ALIAS_AMBIGUOUS:
            throw std::runtime_error("Node alias is ambiguous: " + node_alias);
        case UPSTREAM_RESULT_NOT_REACHABLE:
            throw std::runtime_error("Node is not upstream-reachable: " + node_alias);
        case UPSTREAM_RESULT_UNAVAILABLE:
            throw std::runtime_error("Upstream result is unavailable: " + node_alias + "." + output_name);
        case UPSTREAM_RESULT_OUTPUT_NOT_FOUND:
            throw std::runtime_error("Unknown upstream output: " + node_alias + "." + output_name);
        case UPSTREAM_RESULT_TYPE_NOT_SUPPORTED:
            throw std::runtime_error("Unsupported upstream output type: " + node_alias + "." + output_name);
        default:
            throw std::runtime_error("Failed to read upstream result: " + node_alias + "." + output_name);
        }
    });

    lua.set_function("ConsoleWrite", [this](const std::string& message) {
        if (!console_output_.empty()) {
            console_output_ += "\n";
        }
        console_output_ += message;
    });

    const sol::load_result loaded = lua.load(script_content_);
    if (!loaded.valid()) {
        sol::error err = loaded;
        out_error = err.what();
        runtime_state_.reset();
        runtime_ready_ = false;
        return false;
    }

    sol::protected_function chunk = loaded;
    const sol::protected_function_result chunk_result = chunk();
    if (!chunk_result.valid()) {
        sol::error err = chunk_result;
        out_error = err.what();
        runtime_state_.reset();
        runtime_ready_ = false;
        return false;
    }

    sol::object init_object = lua["Init"];
    if (init_object.valid() && init_object.get_type() == sol::type::function) {
        sol::protected_function init = init_object.as<sol::protected_function>();
        const sol::protected_function_result init_result = init();
        if (!init_result.valid()) {
            sol::error err = init_result;
            out_error = err.what();
            runtime_state_.reset();
            runtime_ready_ = false;
            return false;
        }
    }

    sol::object process_object = lua["Process"];
    if (!process_object.valid() || process_object.get_type() != sol::type::function) {
        out_error = "Process() is required";
        runtime_state_.reset();
        runtime_ready_ = false;
        return false;
    }

    runtime_state_->process = process_object.as<sol::protected_function>();
    runtime_dirty_ = false;
    runtime_ready_ = true;
    return true;
}

int HVLuaScriptNode::run()
{
    const auto start = std::chrono::steady_clock::now();
    execute_status_ = NODE_STATUS_RUNNING;
    error_msg_.clear();
    console_output_.clear();
    for (size_t i = 0; i < output_defs_.size(); ++i) {
        output_values_[i] = MakeDefaultValue(output_defs_[i].type);
        output_values_[i].has_value = false;
    }

    if (script_content_.empty()) {
        execute_status_ = ALGORITHM_RUN_ERROR;
        error_msg_ = "script_content is required";
        return ALGORITHM_RUN_ERROR;
    }

    std::string runtime_error;
    if (!EnsureRuntimeReady(runtime_error)) {
        execute_status_ = ALGORITHM_RUN_ERROR;
        error_msg_ = runtime_error;
        run_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        return ALGORITHM_RUN_ERROR;
    }

    const sol::protected_function_result result = runtime_state_->process();
    if (!result.valid()) {
        sol::error err = result;
        execute_status_ = ALGORITHM_RUN_ERROR;
        error_msg_ = err.what();
        run_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        return ALGORITHM_RUN_ERROR;
    }

    run_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    execute_status_ = SUCCESS;
    return SUCCESS;
}

int HVLuaScriptNode::set_algorithm_params(const std::vector<void*>& params, const std::vector<int>& paramID)
{
    std::vector<int> effective_param_ids = paramID;
    if (effective_param_ids.empty()) {
        effective_param_ids.reserve(params.size());
        for (size_t i = 0; i < params.size(); ++i) {
            effective_param_ids.push_back(static_cast<int>(i));
        }
    }
    if (effective_param_ids.size() != params.size()) {
        return INVALID_PARAMS_NUM;
    }

    bool schema_changed = false;
    for (size_t i = 0; i < effective_param_ids.size(); ++i) {
        const int param_id = effective_param_ids[i];
        if (param_id < 0) {
            return INVALID_PARAMS_NUM;
        }
        if (param_id == 0) {
            script_content_ = cast_param<std::string>(params, static_cast<int>(i));
            MarkRuntimeDirty();
            continue;
        }
        if (param_id == 1) {
            input_defs_json_ = cast_param<std::string>(params, static_cast<int>(i));
            schema_changed = true;
            continue;
        }
        if (param_id == 2) {
            output_defs_json_ = cast_param<std::string>(params, static_cast<int>(i));
            schema_changed = true;
            continue;
        }
    }

    if (schema_changed) {
        std::string schema_error;
        if (!RebuildSchema(schema_error)) {
            error_msg_ = schema_error;
            return INVALID_PARAMS_NUM;
        }
    }

    for (size_t i = 0; i < effective_param_ids.size(); ++i) {
        const int param_id = effective_param_ids[i];
        if (param_id < kConfigParamCount) {
            continue;
        }

        const int input_index = param_id - kConfigParamCount;
        if (input_index < 0 || input_index >= static_cast<int>(input_values_.size())) {
            return INVALID_PARAMS_NUM;
        }

        ScriptValue& value = input_values_[input_index];
        value = MakeDefaultValue(value.type);
        switch (value.type) {
        case HV_INT:
            value.int_value = cast_param<int>(params, static_cast<int>(i));
            break;
        case HV_FLOAT:
            value.float_value = cast_param<float>(params, static_cast<int>(i));
            break;
        case HV_STRING:
            value.string_value = cast_param<std::string>(params, static_cast<int>(i));
            break;
        default:
            return INVALID_PARAMS_NUM;
        }
    }

    return SUCCESS;
}

std::vector<void*> HVLuaScriptNode::get_current_params()
{
    std::vector<void*> params;
    params.reserve(kConfigParamCount + input_values_.size());
    params.push_back(&script_content_);
    params.push_back(&input_defs_json_);
    params.push_back(&output_defs_json_);
    for (auto& value : input_values_) {
        params.push_back(PointerFromScriptValue(value));
    }
    return params;
}

std::vector<void*> HVLuaScriptNode::get_algorithm_result()
{
    std::vector<void*> results;
    results.reserve(output_values_.size() + 1);
    for (auto& value : output_values_) {
        results.push_back(PointerFromOutputValue(value));
    }
    results.push_back(&execute_status_);
    return results;
}

std::vector<int> HVLuaScriptNode::get_algorithm_input_params_type()
{
    std::vector<int> types = { HV_STRING, HV_STRING, HV_STRING };
    for (const auto& def : input_defs_) {
        types.push_back(def.type);
    }
    return types;
}

std::vector<int> HVLuaScriptNode::get_algorithm_output_params_type()
{
    std::vector<int> types;
    types.reserve(output_defs_.size() + 1);
    for (const auto& def : output_defs_) {
        types.push_back(def.type);
    }
    types.push_back(HV_INT);
    return types;
}

std::vector<std::string> HVLuaScriptNode::get_algorithm_input_params_name()
{
    std::vector<std::string> names = { "script_content", "input_defs_json", "output_defs_json" };
    for (const auto& def : input_defs_) {
        names.push_back(def.name);
    }
    return names;
}

std::vector<std::string> HVLuaScriptNode::get_algorithm_output_params_name()
{
    std::vector<std::string> names;
    names.reserve(output_defs_.size() + 1);
    for (const auto& def : output_defs_) {
        names.push_back(def.name);
    }
    names.push_back("execute_status");
    return names;
}

std::vector<bool> HVLuaScriptNode::get_algorithm_input_params_bindable()
{
    std::vector<bool> bindable(kConfigParamCount + input_defs_.size(), false);
    for (size_t i = 0; i < input_defs_.size(); ++i) {
        bindable[kConfigParamCount + i] = true;
    }
    return bindable;
}

std::vector<ParamMetadata> HVLuaScriptNode::get_algorithm_input_params_metadata()
{
    std::vector<ParamMetadata> metadata;
    metadata.resize(kConfigParamCount + input_defs_.size());

    metadata[0].param_name = "script_content";
    metadata[0].param_description = "Embedded Lua script source";
    metadata[0].param_type = HV_STRING;

    metadata[1].param_name = "input_defs_json";
    metadata[1].param_description = "JSON array of input port definitions";
    metadata[1].param_type = HV_STRING;

    metadata[2].param_name = "output_defs_json";
    metadata[2].param_description = "JSON array of output port definitions";
    metadata[2].param_type = HV_STRING;

    for (size_t i = 0; i < input_defs_.size(); ++i) {
        metadata[kConfigParamCount + i].param_name = input_defs_[i].name;
        metadata[kConfigParamCount + i].param_description = "Lua script input";
        metadata[kConfigParamCount + i].param_type = input_defs_[i].type;
    }

    return metadata;
}

int HVLuaScriptNode::get_algorithm_execute_status()
{
    return execute_status_;
}

std::string HVLuaScriptNode::get_algorithm_error_message()
{
    if (execute_status_ == SUCCESS && !console_output_.empty()) {
        return console_output_;
    }
    return error_msg_;
}

long HVLuaScriptNode::get_algorithm_use_time()
{
    return run_time_;
}

bool HVLuaScriptNode::algorithm_params_setting_status()
{
    return true;
}

bool HVLuaScriptNode::algorithm_init_status()
{
    return true;
}

bool HVLuaScriptNode::save_params_to_json(const std::string& filePath)
{
    try {
        nlohmann::json params_json = nlohmann::json::array();
        add_param(params_json, "script_content", HV_STRING, script_content_);
        add_param(params_json, "input_defs_json", HV_STRING, input_defs_json_);
        add_param(params_json, "output_defs_json", HV_STRING, output_defs_json_);

        std::ofstream file(filePath);
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

bool HVLuaScriptNode::load_params_from_json(const std::string& filePath)
{
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            return false;
        }

        nlohmann::json params_json;
        file >> params_json;
        if (!params_json.is_array()) {
            return false;
        }

        for (const auto& param_json : params_json) {
            if (!param_json.is_object() ||
                !param_json.contains("name") ||
                !param_json.contains("type") ||
                !param_json.contains("value")) {
                continue;
            }

            const std::string param_name = param_json["name"].get<std::string>();
            if (param_name == "script_content" && param_json["value"].is_string()) {
                script_content_ = param_json["value"].get<std::string>();
            }
            else if (param_name == "input_defs_json" && param_json["value"].is_string()) {
                input_defs_json_ = param_json["value"].get<std::string>();
            }
            else if (param_name == "output_defs_json" && param_json["value"].is_string()) {
                output_defs_json_ = param_json["value"].get<std::string>();
            }
        }

        std::string schema_error;
        if (!RebuildSchema(schema_error)) {
            error_msg_ = schema_error;
            return false;
        }
        MarkRuntimeDirty();
        return true;
    }
    catch (...) {
        return false;
    }
}

AlgorithmType HVLuaScriptNode::get_algorithm_type()
{
    return AlgorithmType::LogicTool;
}

void HVLuaScriptNode::set_language(int language)
{
    language_ = language;
}

int HVLuaScriptNode::get_language() const
{
    return language_;
}

std::string HVLuaScriptNode::get_algorithm_display_name()
{
    return "Lua script";
}

void HVLuaScriptNode::set_host_services(NodeHostServices* host_services)
{
    host_services_ = host_services;
}

extern "C" __declspec(dllexport) NodeEngine* CreateInstance()
{
    return new HVLuaScriptNode();
}

extern "C" __declspec(dllexport) std::string GetInstanceName()
{
    return "Lua script";
}

extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion()
{
    return NODE_ENGINE_ABI_VERSION;
}
