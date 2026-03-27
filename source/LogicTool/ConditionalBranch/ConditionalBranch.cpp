#include "HVConditionalBranch.h"

#include "HVI18n.h"
#include "HVUtils.h"

#include <chrono>
#include <fstream>

namespace {

constexpr int kConditionTypeInt = 0;
constexpr int kConditionTypeFloat = 1;

const hvi18n::Dictionary kTexts = {
    { "algorithm.display", { "\u6761\u4ef6\u5206\u652f", "Conditional branch" } },
    { "input.condition_type.name", { "\u6761\u4ef6\u7c7b\u578b", "Condition type" } },
    { "input.condition_type.desc", { "\u9009\u62e9\u6761\u4ef6\u5206\u652f\u4f7f\u7528\u7684\u8f93\u5165\u503c\u7c7b\u578b", "Select the input value type used by the conditional branch" } },
    { "input.int_value.name", { "\u6574\u6570\u8f93\u5165", "Integer input" } },
    { "input.int_value.desc", { "\u6761\u4ef6\u7c7b\u578b\u4e3a\u6574\u6570\u65f6\u7528\u4e8e\u5224\u65ad\u7684\u8f93\u5165\u503c", "Input value used when condition type is integer" } },
    { "input.float_value.name", { "\u6d6e\u70b9\u8f93\u5165", "Float input" } },
    { "input.float_value.desc", { "\u6761\u4ef6\u7c7b\u578b\u4e3a\u6d6e\u70b9\u65f6\u7528\u4e8e\u5224\u65ad\u7684\u8f93\u5165\u503c", "Input value used when condition type is float" } },
    { "input.min_value.name", { "\u6700\u5c0f\u503c", "Minimum value" } },
    { "input.min_value.desc", { "\u6761\u4ef6\u5224\u65ad\u6709\u6548\u533a\u95f4\u7684\u6700\u5c0f\u503c", "Minimum value of the valid range" } },
    { "input.max_value.name", { "\u6700\u5927\u503c", "Maximum value" } },
    { "input.max_value.desc", { "\u6761\u4ef6\u5224\u65ad\u6709\u6548\u533a\u95f4\u7684\u6700\u5927\u503c", "Maximum value of the valid range" } },
    { "input.ok_target.name", { "OK\u76ee\u6807\u8282\u70b9ID", "OK target node id" } },
    { "input.ok_target.desc", { "\u6761\u4ef6\u6ee1\u8db3\u65f6\u7ee7\u7eed\u6267\u884c\u7684\u76f4\u63a5\u4e0b\u6e38\u8282\u70b9ID", "Direct downstream node id to continue when the condition matches" } },
    { "input.ng_target.name", { "NG\u76ee\u6807\u8282\u70b9ID", "NG target node id" } },
    { "input.ng_target.desc", { "\u6761\u4ef6\u4e0d\u6ee1\u8db3\u65f6\u7ee7\u7eed\u6267\u884c\u7684\u76f4\u63a5\u4e0b\u6e38\u8282\u70b9ID", "Direct downstream node id to continue when the condition does not match" } },
    { "output.target.name", { "\u547d\u4e2d\u76ee\u6807\u8282\u70b9ID", "Matched target node id" } },
    { "option.type.int", { "\u6574\u6570", "Integer" } },
    { "option.type.float", { "\u6d6e\u70b9", "Float" } },
    { "msg.invalid_condition_type", { "\u6761\u4ef6\u7c7b\u578b\u4e0d\u5408\u6cd5", "Condition type is invalid" } },
    { "msg.invalid_target", { "\u76ee\u6807\u8282\u70b9ID\u4e0d\u5408\u6cd5", "Target node id is invalid" } },
    { "msg.invalid_range", { "\u6761\u4ef6\u533a\u95f4\u65e0\u6548", "Condition range is invalid" } },
    { "msg.run_success", { "\u6761\u4ef6\u5206\u652f\u6267\u884c\u6210\u529f", "Conditional branch executed successfully" } }
};

std::string Tr(int language, const std::string& key)
{
    return hvi18n::Translate(kTexts, key, language);
}

}  // namespace

HVConditionalBranch::HVConditionalBranch()
{
}

int HVConditionalBranch::init()
{
    matched_target_node_id_ = -1;
    execute_status_ = NODE_STATUS_NOT_RUN;
    run_time_ = 0;
    error_msg_.clear();
    return SUCCESS;
}

int HVConditionalBranch::run()
{
    const auto start = std::chrono::steady_clock::now();
    execute_status_ = NODE_STATUS_RUNNING;
    matched_target_node_id_ = -1;
    error_msg_.clear();

    if (ok_target_node_id_ < 0 || ng_target_node_id_ < 0) {
        execute_status_ = ALGORITHM_RUN_ERROR;
        error_msg_ = "msg.invalid_target";
        return ALGORITHM_RUN_ERROR;
    }

    if (min_value_ > max_value_) {
        execute_status_ = ALGORITHM_RUN_ERROR;
        error_msg_ = "msg.invalid_range";
        return ALGORITHM_RUN_ERROR;
    }

    bool matched = false;
    switch (condition_type_) {
    case kConditionTypeInt:
        matched = static_cast<double>(int_input_) >= min_value_ &&
            static_cast<double>(int_input_) <= max_value_;
        break;
    case kConditionTypeFloat:
        matched = float_input_ >= min_value_ && float_input_ <= max_value_;
        break;
    default:
        execute_status_ = ALGORITHM_RUN_ERROR;
        error_msg_ = "msg.invalid_condition_type";
        return ALGORITHM_RUN_ERROR;
    }

    matched_target_node_id_ = matched ? ok_target_node_id_ : ng_target_node_id_;

    const auto end = std::chrono::steady_clock::now();
    run_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    execute_status_ = SUCCESS;
    error_msg_ = "msg.run_success";
    return SUCCESS;
}

int HVConditionalBranch::set_algorithm_params(const std::vector<void*>& params, const std::vector<int>& paramID)
{
    if (paramID.empty()) {
        condition_type_ = cast_param<int>(params, 0);
        int_input_ = cast_param<int>(params, 1);
        float_input_ = cast_param<double>(params, 2);
        min_value_ = cast_param<double>(params, 3);
        max_value_ = cast_param<double>(params, 4);
        ok_target_node_id_ = cast_param<int>(params, 5);
        ng_target_node_id_ = cast_param<int>(params, 6);
        return SUCCESS;
    }

    for (size_t i = 0; i < paramID.size(); ++i) {
        switch (paramID[i]) {
        case 0:
            condition_type_ = cast_param<int>(params, static_cast<int>(i));
            break;
        case 1:
            int_input_ = cast_param<int>(params, static_cast<int>(i));
            break;
        case 2:
            float_input_ = cast_param<double>(params, static_cast<int>(i));
            break;
        case 3:
            min_value_ = cast_param<double>(params, static_cast<int>(i));
            break;
        case 4:
            max_value_ = cast_param<double>(params, static_cast<int>(i));
            break;
        case 5:
            ok_target_node_id_ = cast_param<int>(params, static_cast<int>(i));
            break;
        case 6:
            ng_target_node_id_ = cast_param<int>(params, static_cast<int>(i));
            break;
        default:
            break;
        }
    }

    return SUCCESS;
}

std::vector<void*> HVConditionalBranch::get_current_params()
{
    return {
        &condition_type_,
        &int_input_,
        &float_input_,
        &min_value_,
        &max_value_,
        &ok_target_node_id_,
        &ng_target_node_id_
    };
}

std::vector<void*> HVConditionalBranch::get_algorithm_result()
{
    if (execute_status_ == SUCCESS) {
        return { &matched_target_node_id_ };
    }
    return { nullptr };
}

std::vector<int> HVConditionalBranch::get_algorithm_input_params_type()
{
    return {
        HV_INT,
        HV_INT,
        HV_DOUBLE,
        HV_DOUBLE,
        HV_DOUBLE,
        HV_INT,
        HV_INT
    };
}

std::vector<int> HVConditionalBranch::get_algorithm_output_params_type()
{
    return { HV_INT };
}

std::vector<std::string> HVConditionalBranch::get_algorithm_input_params_name()
{
    return {
        Tr(language_, "input.condition_type.name"),
        Tr(language_, "input.int_value.name"),
        Tr(language_, "input.float_value.name"),
        Tr(language_, "input.min_value.name"),
        Tr(language_, "input.max_value.name"),
        Tr(language_, "input.ok_target.name"),
        Tr(language_, "input.ng_target.name")
    };
}

std::vector<std::string> HVConditionalBranch::get_algorithm_output_params_name()
{
    return { Tr(language_, "output.target.name") };
}

std::vector<bool> HVConditionalBranch::get_algorithm_input_params_bindable()
{
    return {
        false,
        false,
        false,
        false,
        false,
        false,
        false
    };
}

std::vector<ParamMetadata> HVConditionalBranch::get_algorithm_input_params_metadata()
{
    std::vector<ParamMetadata> metadata_list(7);

    metadata_list[0].param_name = Tr(language_, "input.condition_type.name");
    metadata_list[0].param_description = Tr(language_, "input.condition_type.desc");
    metadata_list[0].param_type = HV_INT;
    metadata_list[0].constraint_type = CONSTRAINT_OPTIONS;
    metadata_list[0].options_constraint.AddOption(kConditionTypeInt, Tr(language_, "option.type.int"));
    metadata_list[0].options_constraint.AddOption(kConditionTypeFloat, Tr(language_, "option.type.float"));
    metadata_list[0].options_constraint.default_index = 0;

    metadata_list[1].param_name = Tr(language_, "input.int_value.name");
    metadata_list[1].param_description = Tr(language_, "input.int_value.desc");
    metadata_list[1].param_type = HV_INT;
    metadata_list[1].dependencies.push_back(ParamDependency(0, DEPENDS_ON_EQUALS, { "0" }));

    metadata_list[2].param_name = Tr(language_, "input.float_value.name");
    metadata_list[2].param_description = Tr(language_, "input.float_value.desc");
    metadata_list[2].param_type = HV_DOUBLE;
    metadata_list[2].dependencies.push_back(ParamDependency(0, DEPENDS_ON_EQUALS, { "1" }));

    metadata_list[3].param_name = Tr(language_, "input.min_value.name");
    metadata_list[3].param_description = Tr(language_, "input.min_value.desc");
    metadata_list[3].param_type = HV_DOUBLE;

    metadata_list[4].param_name = Tr(language_, "input.max_value.name");
    metadata_list[4].param_description = Tr(language_, "input.max_value.desc");
    metadata_list[4].param_type = HV_DOUBLE;

    metadata_list[5].param_name = Tr(language_, "input.ok_target.name");
    metadata_list[5].param_description = Tr(language_, "input.ok_target.desc");
    metadata_list[5].param_type = HV_INT;

    metadata_list[6].param_name = Tr(language_, "input.ng_target.name");
    metadata_list[6].param_description = Tr(language_, "input.ng_target.desc");
    metadata_list[6].param_type = HV_INT;

    return metadata_list;
}

int HVConditionalBranch::get_algorithm_execute_status()
{
    return execute_status_;
}

std::string HVConditionalBranch::get_algorithm_error_message()
{
    if (error_msg_.empty()) {
        return "";
    }
    return Tr(language_, error_msg_);
}

long HVConditionalBranch::get_algorithm_use_time()
{
    return run_time_;
}

bool HVConditionalBranch::algorithm_params_setting_status()
{
    return true;
}

bool HVConditionalBranch::algorithm_init_status()
{
    return true;
}

bool HVConditionalBranch::save_params_to_json(const std::string& filePath)
{
    try {
        nlohmann::json params_json = nlohmann::json::array();
        add_param(params_json, "condition_type", HV_INT, condition_type_);
        add_param(params_json, "int_input", HV_INT, int_input_);
        add_param(params_json, "float_input", HV_DOUBLE, float_input_);
        add_param(params_json, "min_value", HV_DOUBLE, min_value_);
        add_param(params_json, "max_value", HV_DOUBLE, max_value_);
        add_param(params_json, "ok_target_node_id", HV_INT, ok_target_node_id_);
        add_param(params_json, "ng_target_node_id", HV_INT, ng_target_node_id_);

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

bool HVConditionalBranch::load_params_from_json(const std::string& filePath)
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

            if (param_name == "condition_type" && param_type == HV_INT && param_json["value"].is_number_integer()) {
                condition_type_ = param_json["value"].get<int>();
            }
            else if (param_name == "int_input" && param_type == HV_INT && param_json["value"].is_number_integer()) {
                int_input_ = param_json["value"].get<int>();
            }
            else if (param_name == "float_input" && param_type == HV_DOUBLE && param_json["value"].is_number()) {
                float_input_ = param_json["value"].get<double>();
            }
            else if (param_name == "min_value" && param_type == HV_DOUBLE && param_json["value"].is_number()) {
                min_value_ = param_json["value"].get<double>();
            }
            else if (param_name == "max_value" && param_type == HV_DOUBLE && param_json["value"].is_number()) {
                max_value_ = param_json["value"].get<double>();
            }
            else if (param_name == "ok_target_node_id" && param_type == HV_INT && param_json["value"].is_number_integer()) {
                ok_target_node_id_ = param_json["value"].get<int>();
            }
            else if (param_name == "ng_target_node_id" && param_type == HV_INT && param_json["value"].is_number_integer()) {
                ng_target_node_id_ = param_json["value"].get<int>();
            }
        }
        return true;
    }
    catch (...) {
        return false;
    }
}

AlgorithmType HVConditionalBranch::get_algorithm_type()
{
    return AlgorithmType::LogicTool;
}

void HVConditionalBranch::set_language(int language)
{
    language_ = language;
}

int HVConditionalBranch::get_language() const
{
    return language_;
}

std::string HVConditionalBranch::get_algorithm_display_name()
{
    return Tr(language_, "algorithm.display");
}

extern "C" __declspec(dllexport) NodeEngine* CreateInstance()
{
    return new HVConditionalBranch();
}

extern "C" __declspec(dllexport) std::string GetInstanceName()
{
    return "Conditional branch";
}

extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion()
{
    return NODE_ENGINE_ABI_VERSION;
}
