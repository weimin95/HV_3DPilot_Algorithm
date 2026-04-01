#include "HVLuaScriptRouting.h"

#include "HVI18n.h"
#include "HVUtils.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <utility>

#include <sol/sol.hpp>

namespace {

const hvi18n::Dictionary kTexts = {
    { "algorithm.display", { "Lua script routing", "Lua script routing" } },
    { "msg.script_path_required", { "Script path is required", "Script path is required" } },
    { "msg.script_not_found", { "Script file does not exist", "Script file does not exist" } },
    { "msg.route_target_required", { "Script did not set a route target node", "Script did not set a route target node" } },
    { "msg.host_missing", { "Host services are unavailable", "Host services are unavailable" } },
    { "msg.timeout", { "Script execution timed out", "Script execution timed out" } },
    { "msg.cancelled", { "Script execution was cancelled", "Script execution was cancelled" } },
    { "msg.lua_error", { "Lua script execution failed", "Lua script execution failed" } },
    { "msg.global_read_failed", { "Failed to read global variable", "Failed to read global variable" } },
    { "msg.global_write_failed", { "Failed to write global variable", "Failed to write global variable" } },
    { "msg.global_define_failed", { "Failed to define global variable", "Failed to define global variable" } }
};

constexpr const char* kLuaTimeoutMarker = "__HV_LUA_TIMEOUT__";
constexpr const char* kLuaCancelledMarker = "__HV_LUA_CANCELLED__";

std::string Tr(int language, const std::string& key)
{
    return hvi18n::Translate(kTexts, key, language);
}

struct LuaHookState {
    NodeHostServices* host_services = nullptr;
    bool timeout_enabled = false;
    std::chrono::steady_clock::time_point deadline;
};

std::runtime_error MakeRuntimeError(const std::string& key, const std::string& detail = std::string())
{
    if (detail.empty()) {
        return std::runtime_error(key);
    }
    return std::runtime_error(key + "|" + detail);
}

std::pair<std::string, std::string> SplitError(const std::string& raw)
{
    const std::size_t sep = raw.find('|');
    if (sep == std::string::npos) {
        return { raw, std::string() };
    }
    return { raw.substr(0, sep), raw.substr(sep + 1) };
}

void LuaInstructionHook(lua_State* state, lua_Debug*)
{
    auto** hook_state_ptr = static_cast<LuaHookState**>(lua_getextraspace(state));
    if (hook_state_ptr == nullptr || *hook_state_ptr == nullptr) {
        return;
    }

    LuaHookState* hook_state = *hook_state_ptr;
    if (hook_state->host_services != nullptr && hook_state->host_services->is_cancelled()) {
        luaL_error(state, kLuaCancelledMarker);
    }

    if (hook_state->timeout_enabled &&
        std::chrono::steady_clock::now() > hook_state->deadline) {
        luaL_error(state, kLuaTimeoutMarker);
    }
}

NodeHostValue LuaObjectToHostValue(const sol::object& object)
{
    NodeHostValue value;
    if (!object.valid() || object.get_type() == sol::type::lua_nil) {
        return value;
    }

    value.has_value = true;
    switch (object.get_type()) {
    case sol::type::boolean:
        value.type = HV_BOOLEAN;
        value.bool_value = object.as<bool>();
        return value;
    case sol::type::string:
        value.type = HV_STRING;
        value.string_value = object.as<std::string>();
        return value;
    case sol::type::number:
        if (object.is<lua_Integer>()) {
            const lua_Integer integer_value = object.as<lua_Integer>();
            if (integer_value >= std::numeric_limits<int>::min() &&
                integer_value <= std::numeric_limits<int>::max()) {
                value.type = HV_INT;
                value.int_value = static_cast<int>(integer_value);
            } else {
                value.type = HV_LONG;
                value.long_value = static_cast<long>(integer_value);
            }
            return value;
        }
        value.type = HV_DOUBLE;
        value.double_value = object.as<double>();
        return value;
    default:
        throw MakeRuntimeError("msg.lua_error", "unsupported Lua value type");
    }
}

sol::object HostValueToLua(sol::state_view lua, const NodeHostValue& value)
{
    if (!value.has_value) {
        return sol::make_object(lua, sol::lua_nil);
    }

    switch (value.type) {
    case HV_INT:
        return sol::make_object(lua, value.int_value);
    case HV_LONG:
        return sol::make_object(lua, value.long_value);
    case HV_FLOAT:
        return sol::make_object(lua, value.float_value);
    case HV_DOUBLE:
        return sol::make_object(lua, value.double_value);
    case HV_BOOLEAN:
        return sol::make_object(lua, value.bool_value);
    case HV_STRING:
        return sol::make_object(lua, value.string_value);
    default:
        return sol::make_object(lua, sol::lua_nil);
    }
}

std::string ResolveScriptPath(NodeHostServices* host_services, const std::string& raw_path)
{
    if (host_services == nullptr) {
        return raw_path;
    }

    const std::string resolved = host_services->resolve_resource_path(raw_path);
    return resolved.empty() ? raw_path : resolved;
}

void ConfigureLuaHook(sol::state& lua, LuaHookState& hook_state)
{
    auto** extra_space = static_cast<LuaHookState**>(lua_getextraspace(lua.lua_state()));
    *extra_space = &hook_state;
    lua_sethook(lua.lua_state(), LuaInstructionHook, LUA_MASKCOUNT, 1000);
}

}  // namespace

HVLuaScriptRouting::HVLuaScriptRouting()
{
}

int HVLuaScriptRouting::init()
{
    route_target_node_id_ = -1;
    execute_status_ = NODE_STATUS_NOT_RUN;
    run_time_ = 0;
    error_msg_.clear();
    return SUCCESS;
}

int HVLuaScriptRouting::run()
{
    const auto start = std::chrono::steady_clock::now();
    execute_status_ = NODE_STATUS_RUNNING;
    route_target_node_id_ = -1;
    error_msg_.clear();

    if (script_path_.empty()) {
        execute_status_ = ALGORITHM_RUN_ERROR;
        error_msg_ = "msg.script_path_required";
        return ALGORITHM_RUN_ERROR;
    }

    const std::string resolved_script_path = ResolveScriptPath(host_services_, script_path_);
    const std::filesystem::path script_file = std::filesystem::u8path(resolved_script_path);
    if (!std::filesystem::exists(script_file)) {
        execute_status_ = ALGORITHM_RUN_ERROR;
        error_msg_ = "msg.script_not_found";
        return ALGORITHM_RUN_ERROR;
    }

    try {
        sol::state lua;
        lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table, sol::lib::utf8);

        LuaHookState hook_state;
        hook_state.host_services = host_services_;
        hook_state.timeout_enabled = timeout_ms_ > 0;
        if (hook_state.timeout_enabled) {
            hook_state.deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms_);
        }
        ConfigureLuaHook(lua, hook_state);

        sol::table host = lua.create_named_table("host");
        host.set_function("set_route_target", [this](int node_id) {
            route_target_node_id_ = node_id;
        });
        host.set_function("check_cancel", [this]() {
            if (host_services_ != nullptr && host_services_->is_cancelled()) {
                throw MakeRuntimeError("msg.cancelled");
            }
        });
        host.set_function("get_global", [this, &lua](int var_id) {
            if (host_services_ == nullptr) {
                throw MakeRuntimeError("msg.host_missing");
            }
            NodeHostValue value;
            const int ret = host_services_->get_global_value(var_id, value);
            if (ret != SUCCESS) {
                throw MakeRuntimeError("msg.global_read_failed", std::to_string(ret));
            }
            return HostValueToLua(lua, value);
        });
        host.set_function("set_global", [this](int var_id, const sol::object& object) {
            if (host_services_ == nullptr) {
                throw MakeRuntimeError("msg.host_missing");
            }
            const NodeHostValue value = LuaObjectToHostValue(object);
            const int ret = host_services_->set_global_value(var_id, value);
            if (ret != SUCCESS) {
                throw MakeRuntimeError("msg.global_write_failed", std::to_string(ret));
            }
        });
        host.set_function("define_global", [this](const std::string& name, int type, const sol::object& object) {
            if (host_services_ == nullptr) {
                throw MakeRuntimeError("msg.host_missing");
            }
            const NodeHostValue value = LuaObjectToHostValue(object);
            const int ret = host_services_->define_global_value(name, type, value, value.has_value, std::string());
            if (ret < 0) {
                throw MakeRuntimeError("msg.global_define_failed", std::to_string(ret));
            }
            return ret;
        });
        host.set_function("get_route_candidates", [this, &lua]() {
            sol::table result = lua.create_table();
            if (host_services_ == nullptr) {
                return result;
            }
            const auto candidates = host_services_->get_route_target_candidates();
            for (std::size_t i = 0; i < candidates.size(); ++i) {
                sol::table entry = lua.create_table();
                entry["target_node_id"] = candidates[i].target_node_id_;
                entry["target_node_name"] = candidates[i].target_node_name_;
                entry["target_node_alias"] = candidates[i].target_node_alias_;
                result[i + 1] = entry;
            }
            return result;
        });
        host["HV_INT"] = HV_INT;
        host["HV_LONG"] = HV_LONG;
        host["HV_FLOAT"] = HV_FLOAT;
        host["HV_DOUBLE"] = HV_DOUBLE;
        host["HV_BOOLEAN"] = HV_BOOLEAN;
        host["HV_STRING"] = HV_STRING;

        const sol::load_result loaded = lua.load_file(script_file.string());
        if (!loaded.valid()) {
            sol::error err = loaded;
            throw MakeRuntimeError("msg.lua_error", err.what());
        }

        sol::protected_function script = loaded;
        const sol::protected_function_result result = script();
        if (!result.valid()) {
            sol::error err = result;
            const std::string raw_error = err.what();
            if (raw_error.find(kLuaTimeoutMarker) != std::string::npos) {
                throw MakeRuntimeError("msg.timeout");
            }
            if (raw_error.find(kLuaCancelledMarker) != std::string::npos) {
                throw MakeRuntimeError("msg.cancelled");
            }
            throw MakeRuntimeError("msg.lua_error", raw_error);
        }
    }
    catch (const std::runtime_error& e) {
        const auto [message_key, detail] = SplitError(e.what());
        execute_status_ = ALGORITHM_RUN_ERROR;
        error_msg_ = message_key.empty() ? "msg.lua_error" : message_key;
        if (error_msg_ == "msg.lua_error" && !detail.empty()) {
            error_msg_ = detail;
        }
        run_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        return ALGORITHM_RUN_ERROR;
    }
    catch (const std::exception& e) {
        execute_status_ = ALGORITHM_RUN_ERROR;
        error_msg_ = e.what();
        run_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        return ALGORITHM_RUN_ERROR;
    }

    if (route_target_node_id_ < 0) {
        execute_status_ = ALGORITHM_RUN_ERROR;
        error_msg_ = "msg.route_target_required";
        run_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        return ALGORITHM_RUN_ERROR;
    }

    run_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    execute_status_ = SUCCESS;
    return SUCCESS;
}

int HVLuaScriptRouting::set_algorithm_params(const std::vector<void*>& params, const std::vector<int>& paramID)
{
    if (paramID.empty()) {
        script_path_ = cast_param<std::string>(params, 0);
        timeout_ms_ = cast_param<int>(params, 1);
        return SUCCESS;
    }

    for (size_t i = 0; i < paramID.size(); ++i) {
        switch (paramID[i]) {
        case 0:
            script_path_ = cast_param<std::string>(params, static_cast<int>(i));
            break;
        case 1:
            timeout_ms_ = cast_param<int>(params, static_cast<int>(i));
            break;
        default:
            break;
        }
    }

    return SUCCESS;
}

std::vector<void*> HVLuaScriptRouting::get_current_params()
{
    return { &script_path_, &timeout_ms_ };
}

std::vector<void*> HVLuaScriptRouting::get_algorithm_result()
{
    if (execute_status_ == SUCCESS) {
        return { &route_target_node_id_, &execute_status_ };
    }
    return { nullptr, &execute_status_ };
}

std::vector<int> HVLuaScriptRouting::get_algorithm_input_params_type()
{
    return { HV_STRING, HV_INT };
}

std::vector<int> HVLuaScriptRouting::get_algorithm_output_params_type()
{
    return { HV_INT, HV_INT };
}

std::vector<std::string> HVLuaScriptRouting::get_algorithm_input_params_name()
{
    return { "script_path", "timeout_ms" };
}

std::vector<std::string> HVLuaScriptRouting::get_algorithm_output_params_name()
{
    return { "route_target_node_id", "execute_status" };
}

std::vector<bool> HVLuaScriptRouting::get_algorithm_input_params_bindable()
{
    return { false, false };
}

std::vector<ParamMetadata> HVLuaScriptRouting::get_algorithm_input_params_metadata()
{
    std::vector<ParamMetadata> metadata_list(2);
    metadata_list[0].param_name = "script_path";
    metadata_list[0].param_description = "Lua script file path";
    metadata_list[0].param_type = HV_STRING;
    metadata_list[0].constraint_type = CONSTRAINT_FILE_PATH;
    metadata_list[0].file_filter = "*.lua";

    metadata_list[1].param_name = "timeout_ms";
    metadata_list[1].param_description = "Soft timeout for Lua execution";
    metadata_list[1].param_type = HV_INT;
    metadata_list[1].constraint_type = CONSTRAINT_RANGE;
    metadata_list[1].range_constraint.min_value = 0.0;
    metadata_list[1].range_constraint.max_value = 600000.0;
    metadata_list[1].range_constraint.default_value = 5000.0;

    return metadata_list;
}

int HVLuaScriptRouting::get_algorithm_execute_status()
{
    return execute_status_;
}

std::string HVLuaScriptRouting::get_algorithm_error_message()
{
    if (error_msg_.empty()) {
        return "";
    }
    if (kTexts.find(error_msg_) != kTexts.end()) {
        return Tr(language_, error_msg_);
    }
    return error_msg_;
}

long HVLuaScriptRouting::get_algorithm_use_time()
{
    return run_time_;
}

bool HVLuaScriptRouting::algorithm_params_setting_status()
{
    return true;
}

bool HVLuaScriptRouting::algorithm_init_status()
{
    return true;
}

bool HVLuaScriptRouting::save_params_to_json(const std::string& filePath)
{
    try {
        nlohmann::json params_json = nlohmann::json::array();
        add_param(params_json, "script_path", HV_STRING, script_path_);
        add_param(params_json, "timeout_ms", HV_INT, timeout_ms_);

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

bool HVLuaScriptRouting::load_params_from_json(const std::string& filePath)
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
            const int param_type = param_json["type"].get<int>();

            if (param_name == "script_path" && param_type == HV_STRING && param_json["value"].is_string()) {
                script_path_ = param_json["value"].get<std::string>();
            } else if (param_name == "timeout_ms" && param_type == HV_INT && param_json["value"].is_number_integer()) {
                timeout_ms_ = param_json["value"].get<int>();
            }
        }
        return true;
    }
    catch (...) {
        return false;
    }
}

AlgorithmType HVLuaScriptRouting::get_algorithm_type()
{
    return AlgorithmType::LogicTool;
}

void HVLuaScriptRouting::set_language(int language)
{
    language_ = language;
}

int HVLuaScriptRouting::get_language() const
{
    return language_;
}

std::string HVLuaScriptRouting::get_algorithm_display_name()
{
    return Tr(language_, "algorithm.display");
}

void HVLuaScriptRouting::set_host_services(NodeHostServices* host_services)
{
    host_services_ = host_services;
}

NodeControlMode HVLuaScriptRouting::get_control_mode() const
{
    return NodeControlMode::TargetNodeRouting;
}

extern "C" __declspec(dllexport) NodeEngine* CreateInstance()
{
    return new HVLuaScriptRouting();
}

extern "C" __declspec(dllexport) std::string GetInstanceName()
{
    return "Lua script routing";
}

extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion()
{
    return NODE_ENGINE_ABI_VERSION;
}
