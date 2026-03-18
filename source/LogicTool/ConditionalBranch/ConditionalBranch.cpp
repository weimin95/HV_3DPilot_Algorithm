#include "HVConditionalBranch.h"

#include "HVI18n.h"
#include "HVUtils.h"

#include <chrono>
#include <fstream>

namespace {

constexpr int kConditionTypeInt = 0;
constexpr int kConditionTypeDouble = 1;
constexpr int kConditionTypeBool = 2;
constexpr int kConditionTypeString = 3;

constexpr int kCompareEqual = 0;
constexpr int kCompareNotEqual = 1;
constexpr int kCompareGreater = 2;
constexpr int kCompareGreaterEqual = 3;
constexpr int kCompareLess = 4;
constexpr int kCompareLessEqual = 5;

const hvi18n::Dictionary kTexts = {
    { "algorithm.display", { "\u6761\u4ef6\u5206\u652f", "Conditional branch" } },
    { "input.condition_type.name", { "\u6761\u4ef6\u7c7b\u578b", "Condition type" } },
    { "input.condition_type.desc", { "\u9009\u62e9\u552f\u4e00\u6761\u5224\u65ad\u6761\u4ef6\u4f7f\u7528\u7684\u503c\u7c7b\u578b", "Select the value type used by the single branch condition" } },
    { "input.int_value.name", { "\u6574\u6570\u8f93\u5165", "Integer input" } },
    { "input.int_value.desc", { "\u5f53\u6761\u4ef6\u7c7b\u578b\u4e3a\u6574\u6570\u65f6\u7528\u4e8e\u5224\u65ad\u7684\u8f93\u5165\u503c", "Input value used when condition type is integer" } },
    { "input.double_value.name", { "\u6d6e\u70b9\u8f93\u5165", "Float input" } },
    { "input.double_value.desc", { "\u5f53\u6761\u4ef6\u7c7b\u578b\u4e3a\u6d6e\u70b9\u65f6\u7528\u4e8e\u5224\u65ad\u7684\u8f93\u5165\u503c", "Input value used when condition type is float" } },
    { "input.bool_value.name", { "\u5e03\u5c14\u8f93\u5165", "Boolean input" } },
    { "input.bool_value.desc", { "\u5f53\u6761\u4ef6\u7c7b\u578b\u4e3a\u5e03\u5c14\u65f6\u7528\u4e8e\u5224\u65ad\u7684\u8f93\u5165\u503c", "Input value used when condition type is boolean" } },
    { "input.string_value.name", { "\u5b57\u7b26\u4e32\u8f93\u5165", "String input" } },
    { "input.string_value.desc", { "\u5f53\u6761\u4ef6\u7c7b\u578b\u4e3a\u5b57\u7b26\u4e32\u65f6\u7528\u4e8e\u5224\u65ad\u7684\u8f93\u5165\u503c", "Input value used when condition type is string" } },
    { "input.operator.name", { "\u6bd4\u8f83\u7b26", "Compare operator" } },
    { "input.operator.desc", { "\u9009\u62e9\u552f\u4e00\u6761\u6761\u4ef6\u4f7f\u7528\u7684\u6bd4\u8f83\u8fd0\u7b97\u7b26", "Select the comparison operator used by the single condition" } },
    { "input.int_compare.name", { "\u6574\u6570\u6bd4\u8f83\u503c", "Integer compare value" } },
    { "input.int_compare.desc", { "\u5f53\u6761\u4ef6\u7c7b\u578b\u4e3a\u6574\u6570\u65f6\u7684\u6bd4\u8f83\u503c", "Compare value used when condition type is integer" } },
    { "input.double_compare.name", { "\u6d6e\u70b9\u6bd4\u8f83\u503c", "Float compare value" } },
    { "input.double_compare.desc", { "\u5f53\u6761\u4ef6\u7c7b\u578b\u4e3a\u6d6e\u70b9\u65f6\u7684\u6bd4\u8f83\u503c", "Compare value used when condition type is float" } },
    { "input.bool_compare.name", { "\u5e03\u5c14\u6bd4\u8f83\u503c", "Boolean compare value" } },
    { "input.bool_compare.desc", { "\u5f53\u6761\u4ef6\u7c7b\u578b\u4e3a\u5e03\u5c14\u65f6\u7684\u6bd4\u8f83\u503c", "Compare value used when condition type is boolean" } },
    { "input.string_compare.name", { "\u5b57\u7b26\u4e32\u6bd4\u8f83\u503c", "String compare value" } },
    { "input.string_compare.desc", { "\u5f53\u6761\u4ef6\u7c7b\u578b\u4e3a\u5b57\u7b26\u4e32\u65f6\u7684\u6bd4\u8f83\u503c", "Compare value used when condition type is string" } },
    { "input.target.name", { "\u76ee\u6807\u8282\u70b9ID", "Target node id" } },
    { "input.target.desc", { "\u6761\u4ef6\u6210\u7acb\u65f6\u5141\u8bb8\u7ee7\u7eed\u6267\u884c\u7684\u76f4\u63a5\u4e0b\u6e38\u8282\u70b9ID", "Direct downstream node id to continue when the condition matches" } },
    { "output.target.name", { "\u547d\u4e2d\u76ee\u6807\u8282\u70b9ID", "Matched target node id" } },
    { "option.type.int", { "\u6574\u6570", "Integer" } },
    { "option.type.double", { "\u6d6e\u70b9", "Float" } },
    { "option.type.bool", { "\u5e03\u5c14", "Boolean" } },
    { "option.type.string", { "\u5b57\u7b26\u4e32", "String" } },
    { "msg.invalid_condition_type", { "\u6761\u4ef6\u7c7b\u578b\u4e0d\u5408\u6cd5", "Condition type is invalid" } },
    { "msg.invalid_target", { "\u76ee\u6807\u8282\u70b9ID\u4e0d\u5408\u6cd5", "Target node id is invalid" } },
    { "msg.unsupported_operator", { "\u5f53\u524d\u6761\u4ef6\u7c7b\u578b\u4e0d\u652f\u6301\u8be5\u6bd4\u8f83\u7b26", "The selected condition type does not support this operator" } },
    { "msg.run_success", { "\u6761\u4ef6\u5206\u652f\u6267\u884c\u6210\u529f", "Conditional branch executed successfully" } }
};

std::string Tr(int language, const std::string& key)
{
    return hvi18n::Translate(kTexts, key, language);
}

template <typename T>
bool EvaluateOrderedRule(const T& left_value, int compare_operator, const T& right_value, std::string& error_key)
{
    switch (compare_operator) {
    case kCompareEqual:
        return left_value == right_value;
    case kCompareNotEqual:
        return left_value != right_value;
    case kCompareGreater:
        return left_value > right_value;
    case kCompareGreaterEqual:
        return left_value >= right_value;
    case kCompareLess:
        return left_value < right_value;
    case kCompareLessEqual:
        return left_value <= right_value;
    default:
        error_key = "msg.unsupported_operator";
        return false;
    }
}

bool EvaluateBoolRule(bool left_value, int compare_operator, bool right_value, std::string& error_key)
{
    switch (compare_operator) {
    case kCompareEqual:
        return left_value == right_value;
    case kCompareNotEqual:
        return left_value != right_value;
    default:
        error_key = "msg.unsupported_operator";
        return false;
    }
}

} // namespace

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

    if (target_node_id_ < 0) {
        execute_status_ = ALGORITHM_RUN_ERROR;
        error_msg_ = "msg.invalid_target";
        return ALGORITHM_RUN_ERROR;
    }

    bool matched = false;
    switch (condition_type_) {
    case kConditionTypeInt:
        matched = EvaluateOrderedRule(int_input_, compare_operator_, int_compare_value_, error_msg_);
        break;
    case kConditionTypeDouble:
        matched = EvaluateOrderedRule(double_input_, compare_operator_, double_compare_value_, error_msg_);
        break;
    case kConditionTypeBool:
        matched = EvaluateBoolRule(bool_input_, compare_operator_, bool_compare_value_, error_msg_);
        break;
    case kConditionTypeString:
        matched = EvaluateOrderedRule(string_input_, compare_operator_, string_compare_value_, error_msg_);
        break;
    default:
        execute_status_ = ALGORITHM_RUN_ERROR;
        error_msg_ = "msg.invalid_condition_type";
        return ALGORITHM_RUN_ERROR;
    }

    if (!error_msg_.empty()) {
        execute_status_ = ALGORITHM_RUN_ERROR;
        return ALGORITHM_RUN_ERROR;
    }

    matched_target_node_id_ = matched ? target_node_id_ : -1;

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
        double_input_ = cast_param<double>(params, 2);
        bool_input_ = cast_param<bool>(params, 3);
        string_input_ = cast_param<std::string>(params, 4);
        compare_operator_ = cast_param<int>(params, 5);
        int_compare_value_ = cast_param<int>(params, 6);
        double_compare_value_ = cast_param<double>(params, 7);
        bool_compare_value_ = cast_param<bool>(params, 8);
        string_compare_value_ = cast_param<std::string>(params, 9);
        target_node_id_ = cast_param<int>(params, 10);
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
            double_input_ = cast_param<double>(params, static_cast<int>(i));
            break;
        case 3:
            bool_input_ = cast_param<bool>(params, static_cast<int>(i));
            break;
        case 4:
            string_input_ = cast_param<std::string>(params, static_cast<int>(i));
            break;
        case 5:
            compare_operator_ = cast_param<int>(params, static_cast<int>(i));
            break;
        case 6:
            int_compare_value_ = cast_param<int>(params, static_cast<int>(i));
            break;
        case 7:
            double_compare_value_ = cast_param<double>(params, static_cast<int>(i));
            break;
        case 8:
            bool_compare_value_ = cast_param<bool>(params, static_cast<int>(i));
            break;
        case 9:
            string_compare_value_ = cast_param<std::string>(params, static_cast<int>(i));
            break;
        case 10:
            target_node_id_ = cast_param<int>(params, static_cast<int>(i));
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
        &double_input_,
        &bool_input_,
        &string_input_,
        &compare_operator_,
        &int_compare_value_,
        &double_compare_value_,
        &bool_compare_value_,
        &string_compare_value_,
        &target_node_id_
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
        HV_BOOLEAN,
        HV_STRING,
        HV_INT,
        HV_INT,
        HV_DOUBLE,
        HV_BOOLEAN,
        HV_STRING,
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
        Tr(language_, "input.double_value.name"),
        Tr(language_, "input.bool_value.name"),
        Tr(language_, "input.string_value.name"),
        Tr(language_, "input.operator.name"),
        Tr(language_, "input.int_compare.name"),
        Tr(language_, "input.double_compare.name"),
        Tr(language_, "input.bool_compare.name"),
        Tr(language_, "input.string_compare.name"),
        Tr(language_, "input.target.name")
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
        false,
        false,
        false,
        false,
        false
    };
}

std::vector<ParamMetadata> HVConditionalBranch::get_algorithm_input_params_metadata()
{
    std::vector<ParamMetadata> metadata_list(11);

    metadata_list[0].param_name = Tr(language_, "input.condition_type.name");
    metadata_list[0].param_description = Tr(language_, "input.condition_type.desc");
    metadata_list[0].param_type = HV_INT;
    metadata_list[0].constraint_type = CONSTRAINT_OPTIONS;
    metadata_list[0].options_constraint.AddOption(kConditionTypeInt, Tr(language_, "option.type.int"));
    metadata_list[0].options_constraint.AddOption(kConditionTypeDouble, Tr(language_, "option.type.double"));
    metadata_list[0].options_constraint.AddOption(kConditionTypeBool, Tr(language_, "option.type.bool"));
    metadata_list[0].options_constraint.AddOption(kConditionTypeString, Tr(language_, "option.type.string"));
    metadata_list[0].options_constraint.default_index = 0;

    metadata_list[1].param_name = Tr(language_, "input.int_value.name");
    metadata_list[1].param_description = Tr(language_, "input.int_value.desc");
    metadata_list[1].param_type = HV_INT;
    metadata_list[1].dependencies.push_back(ParamDependency(0, DEPENDS_ON_EQUALS, { "0" }));

    metadata_list[2].param_name = Tr(language_, "input.double_value.name");
    metadata_list[2].param_description = Tr(language_, "input.double_value.desc");
    metadata_list[2].param_type = HV_DOUBLE;
    metadata_list[2].dependencies.push_back(ParamDependency(0, DEPENDS_ON_EQUALS, { "1" }));

    metadata_list[3].param_name = Tr(language_, "input.bool_value.name");
    metadata_list[3].param_description = Tr(language_, "input.bool_value.desc");
    metadata_list[3].param_type = HV_BOOLEAN;
    metadata_list[3].dependencies.push_back(ParamDependency(0, DEPENDS_ON_EQUALS, { "2" }));

    metadata_list[4].param_name = Tr(language_, "input.string_value.name");
    metadata_list[4].param_description = Tr(language_, "input.string_value.desc");
    metadata_list[4].param_type = HV_STRING;
    metadata_list[4].dependencies.push_back(ParamDependency(0, DEPENDS_ON_EQUALS, { "3" }));

    metadata_list[5].param_name = Tr(language_, "input.operator.name");
    metadata_list[5].param_description = Tr(language_, "input.operator.desc");
    metadata_list[5].param_type = HV_INT;
    metadata_list[5].constraint_type = CONSTRAINT_OPTIONS;
    metadata_list[5].options_constraint.AddOption(kCompareEqual, "==");
    metadata_list[5].options_constraint.AddOption(kCompareNotEqual, "!=");
    metadata_list[5].options_constraint.AddOption(kCompareGreater, ">");
    metadata_list[5].options_constraint.AddOption(kCompareGreaterEqual, ">=");
    metadata_list[5].options_constraint.AddOption(kCompareLess, "<");
    metadata_list[5].options_constraint.AddOption(kCompareLessEqual, "<=");
    metadata_list[5].options_constraint.default_index = 0;

    metadata_list[6].param_name = Tr(language_, "input.int_compare.name");
    metadata_list[6].param_description = Tr(language_, "input.int_compare.desc");
    metadata_list[6].param_type = HV_INT;
    metadata_list[6].dependencies.push_back(ParamDependency(0, DEPENDS_ON_EQUALS, { "0" }));

    metadata_list[7].param_name = Tr(language_, "input.double_compare.name");
    metadata_list[7].param_description = Tr(language_, "input.double_compare.desc");
    metadata_list[7].param_type = HV_DOUBLE;
    metadata_list[7].dependencies.push_back(ParamDependency(0, DEPENDS_ON_EQUALS, { "1" }));

    metadata_list[8].param_name = Tr(language_, "input.bool_compare.name");
    metadata_list[8].param_description = Tr(language_, "input.bool_compare.desc");
    metadata_list[8].param_type = HV_BOOLEAN;
    metadata_list[8].dependencies.push_back(ParamDependency(0, DEPENDS_ON_EQUALS, { "2" }));

    metadata_list[9].param_name = Tr(language_, "input.string_compare.name");
    metadata_list[9].param_description = Tr(language_, "input.string_compare.desc");
    metadata_list[9].param_type = HV_STRING;
    metadata_list[9].dependencies.push_back(ParamDependency(0, DEPENDS_ON_EQUALS, { "3" }));

    metadata_list[10].param_name = Tr(language_, "input.target.name");
    metadata_list[10].param_description = Tr(language_, "input.target.desc");
    metadata_list[10].param_type = HV_INT;

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
        add_param(params_json, "double_input", HV_DOUBLE, double_input_);
        add_param(params_json, "bool_input", HV_BOOLEAN, bool_input_);
        add_param(params_json, "string_input", HV_STRING, string_input_);
        add_param(params_json, "compare_operator", HV_INT, compare_operator_);
        add_param(params_json, "int_compare_value", HV_INT, int_compare_value_);
        add_param(params_json, "double_compare_value", HV_DOUBLE, double_compare_value_);
        add_param(params_json, "bool_compare_value", HV_BOOLEAN, bool_compare_value_);
        add_param(params_json, "string_compare_value", HV_STRING, string_compare_value_);
        add_param(params_json, "target_node_id", HV_INT, target_node_id_);

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
            else if (param_name == "double_input" && param_type == HV_DOUBLE && param_json["value"].is_number()) {
                double_input_ = param_json["value"].get<double>();
            }
            else if (param_name == "bool_input" && param_type == HV_BOOLEAN && param_json["value"].is_boolean()) {
                bool_input_ = param_json["value"].get<bool>();
            }
            else if (param_name == "string_input" && param_type == HV_STRING && param_json["value"].is_string()) {
                string_input_ = param_json["value"].get<std::string>();
            }
            else if (param_name == "compare_operator" && param_type == HV_INT && param_json["value"].is_number_integer()) {
                compare_operator_ = param_json["value"].get<int>();
            }
            else if (param_name == "int_compare_value" && param_type == HV_INT && param_json["value"].is_number_integer()) {
                int_compare_value_ = param_json["value"].get<int>();
            }
            else if (param_name == "double_compare_value" && param_type == HV_DOUBLE && param_json["value"].is_number()) {
                double_compare_value_ = param_json["value"].get<double>();
            }
            else if (param_name == "bool_compare_value" && param_type == HV_BOOLEAN && param_json["value"].is_boolean()) {
                bool_compare_value_ = param_json["value"].get<bool>();
            }
            else if (param_name == "string_compare_value" && param_type == HV_STRING && param_json["value"].is_string()) {
                string_compare_value_ = param_json["value"].get<std::string>();
            }
            else if (param_name == "target_node_id" && param_type == HV_INT && param_json["value"].is_number_integer()) {
                target_node_id_ = param_json["value"].get<int>();
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
    if (hvi18n::IsSupportedLanguage(language)) {
        language_ = language;
    }
}

int HVConditionalBranch::get_language() const
{
    return language_;
}

std::string HVConditionalBranch::get_algorithm_display_name()
{
    return Tr(language_, "algorithm.display");
}

NodeEngine* CreateInstance()
{
    return new HVConditionalBranch();
}

std::string GetInstanceName()
{
    return "Conditional branch";
}

extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion()
{
    return NODE_ENGINE_ABI_VERSION;
}
