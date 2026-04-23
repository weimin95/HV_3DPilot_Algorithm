#include "HVConditionCheck.h"

#include <algorithm>
#include <chrono>

#include "HVI18n.h"

namespace {

constexpr int kJudgeModeAny = 0;
constexpr int kJudgeModeAll = 1;
constexpr int kValueTypeInt = 0;
constexpr int kValueTypeFloat = 1;
constexpr int kValueTypeDouble = 2;

constexpr int kJudgeModeParamId = 0;
constexpr int kRule1EnabledParamId = 1;
constexpr int kRule1TypeParamId = 2;
constexpr int kRule1IntInputParamId = 3;
constexpr int kRule1FloatInputParamId = 4;
constexpr int kRule1DoubleInputParamId = 5;
constexpr int kRule1IntMinParamId = 6;
constexpr int kRule1IntMaxParamId = 7;
constexpr int kRule1FloatMinParamId = 8;
constexpr int kRule1FloatMaxParamId = 9;
constexpr int kRule1DoubleMinParamId = 10;
constexpr int kRule1DoubleMaxParamId = 11;
constexpr int kRule2EnabledParamId = 12;
constexpr int kRule2TypeParamId = 13;
constexpr int kRule2IntInputParamId = 14;
constexpr int kRule2FloatInputParamId = 15;
constexpr int kRule2DoubleInputParamId = 16;
constexpr int kRule2IntMinParamId = 17;
constexpr int kRule2IntMaxParamId = 18;
constexpr int kRule2FloatMinParamId = 19;
constexpr int kRule2FloatMaxParamId = 20;
constexpr int kRule2DoubleMinParamId = 21;
constexpr int kRule2DoubleMaxParamId = 22;

const hvi18n::Dictionary kTexts = {
    { "algorithm.display", { "条件检测", "Condition check" } },
    { "input.judge_mode.name", { "判断方式", "Judge mode" } },
    { "input.rule1_enabled.name", { "条件1启用", "Rule 1 enabled" } },
    { "input.rule1_type.name", { "条件1类型", "Rule 1 type" } },
    { "input.rule1_int_input.name", { "条件1整数输入", "Rule 1 int input" } },
    { "input.rule1_float_input.name", { "条件1浮点输入", "Rule 1 float input" } },
    { "input.rule1_double_input.name", { "条件1双精度输入", "Rule 1 double input" } },
    { "input.rule1_int_min.name", { "条件1整数最小值", "Rule 1 int min" } },
    { "input.rule1_int_max.name", { "条件1整数最大值", "Rule 1 int max" } },
    { "input.rule1_float_min.name", { "条件1浮点最小值", "Rule 1 float min" } },
    { "input.rule1_float_max.name", { "条件1浮点最大值", "Rule 1 float max" } },
    { "input.rule1_double_min.name", { "条件1双精度最小值", "Rule 1 double min" } },
    { "input.rule1_double_max.name", { "条件1双精度最大值", "Rule 1 double max" } },
    { "input.rule2_enabled.name", { "条件2启用", "Rule 2 enabled" } },
    { "input.rule2_type.name", { "条件2类型", "Rule 2 type" } },
    { "input.rule2_int_input.name", { "条件2整数输入", "Rule 2 int input" } },
    { "input.rule2_float_input.name", { "条件2浮点输入", "Rule 2 float input" } },
    { "input.rule2_double_input.name", { "条件2双精度输入", "Rule 2 double input" } },
    { "input.rule2_int_min.name", { "条件2整数最小值", "Rule 2 int min" } },
    { "input.rule2_int_max.name", { "条件2整数最大值", "Rule 2 int max" } },
    { "input.rule2_float_min.name", { "条件2浮点最小值", "Rule 2 float min" } },
    { "input.rule2_float_max.name", { "条件2浮点最大值", "Rule 2 float max" } },
    { "input.rule2_double_min.name", { "条件2双精度最小值", "Rule 2 double min" } },
    { "input.rule2_double_max.name", { "条件2双精度最大值", "Rule 2 double max" } },
    { "output.result_string.name", { "结果", "Result string" } },
    { "output.result_int.name", { "结果数值", "Result int" } },
    { "output.execute_status.name", { "运行状态", "Execute status" } },
    { "option.judge_mode.any", { "任意", "Any" } },
    { "option.judge_mode.all", { "全部", "All" } },
    { "option.value_type.int", { "int", "int" } },
    { "option.value_type.float", { "float", "float" } },
    { "option.value_type.double", { "double", "double" } },
    { "msg.no_enabled_rule", { "至少需要启用一条条件", "At least one rule must be enabled" } },
    { "msg.invalid_judge_mode", { "判断方式无效", "Judge mode is invalid" } },
    { "msg.invalid_rule_type", { "条件类型无效", "Rule type is invalid" } },
    { "msg.invalid_range", { "条件范围无效", "Rule range is invalid" } },
    { "msg.rule1_missing_input", { "条件1缺少输入值", "Rule 1 is missing input value" } },
    { "msg.rule2_missing_input", { "条件2缺少输入值", "Rule 2 is missing input value" } },
    { "msg.run_ok", { "条件检测结果为OK", "Condition check result is OK" } },
    { "msg.run_ng", { "条件检测结果为NG", "Condition check result is NG" } }
};

std::string Tr(const int language, const std::string& key)
{
    return hvi18n::Translate(kTexts, key, language);
}

ParamDependency BuildEnabledDependency(const int enabled_param_id)
{
    return ParamDependency(enabled_param_id, DEPENDS_ON_IN_LIST, { "1", "true" });
}

ParamDependency BuildTypeDependency(const int type_param_id, const int type_value)
{
    return ParamDependency(type_param_id, DEPENDS_ON_EQUALS, { std::to_string(type_value) });
}

template <typename T>
bool IsValueWithinClosedRange(const T value, const T min_value, const T max_value)
{
    return min_value <= value && value <= max_value;
}

}  // namespace

HVConditionCheck::HVConditionCheck()
{
    judge_mode_
        .SetSchemaName("judge_mode")
        .SetStorageKey("judge_mode")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.judge_mode.name"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_BASIC)
        .SetMetadataCustomizer([this](ParamMetadata& metadata) {
            metadata.constraint_type = CONSTRAINT_OPTIONS;
            metadata.options_constraint = OptionsConstraint();
            metadata.options_constraint.AddOption(kJudgeModeAny, Tr(current_language(), "option.judge_mode.any"));
            metadata.options_constraint.AddOption(kJudgeModeAll, Tr(current_language(), "option.judge_mode.all"));
            metadata.options_constraint.default_index = 1;
        });

    rule1_enabled_
        .SetSchemaName("rule1_enabled")
        .SetStorageKey("rule1_enabled")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.rule1_enabled.name"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_BASIC);

    rule1_type_
        .SetSchemaName("rule1_type")
        .SetStorageKey("rule1_type")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.rule1_type.name"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_BASIC)
        .AddDependency(BuildEnabledDependency(kRule1EnabledParamId))
        .SetMetadataCustomizer([this](ParamMetadata& metadata) {
            metadata.constraint_type = CONSTRAINT_OPTIONS;
            metadata.options_constraint = OptionsConstraint();
            metadata.options_constraint.AddOption(kValueTypeInt, Tr(current_language(), "option.value_type.int"));
            metadata.options_constraint.AddOption(kValueTypeFloat, Tr(current_language(), "option.value_type.float"));
            metadata.options_constraint.AddOption(kValueTypeDouble, Tr(current_language(), "option.value_type.double"));
        });

    rule1_int_input_
        .SetSchemaName("rule1_int_input")
        .SetStorageKey("rule1_int_input")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.rule1_int_input.name"); })
        .SetBindable(true)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_BASIC)
        .AddDependency(BuildEnabledDependency(kRule1EnabledParamId))
        .AddDependency(BuildTypeDependency(kRule1TypeParamId, kValueTypeInt));

    rule1_float_input_
        .SetSchemaName("rule1_float_input")
        .SetStorageKey("rule1_float_input")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.rule1_float_input.name"); })
        .SetBindable(true)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_BASIC)
        .AddDependency(BuildEnabledDependency(kRule1EnabledParamId))
        .AddDependency(BuildTypeDependency(kRule1TypeParamId, kValueTypeFloat));

    rule1_double_input_
        .SetSchemaName("rule1_double_input")
        .SetStorageKey("rule1_double_input")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.rule1_double_input.name"); })
        .SetBindable(true)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_BASIC)
        .AddDependency(BuildEnabledDependency(kRule1EnabledParamId))
        .AddDependency(BuildTypeDependency(kRule1TypeParamId, kValueTypeDouble));

    rule1_int_min_
        .SetSchemaName("rule1_int_min")
        .SetStorageKey("rule1_int_min")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.rule1_int_min.name"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_ADVANCED)
        .AddDependency(BuildEnabledDependency(kRule1EnabledParamId))
        .AddDependency(BuildTypeDependency(kRule1TypeParamId, kValueTypeInt));

    rule1_int_max_
        .SetSchemaName("rule1_int_max")
        .SetStorageKey("rule1_int_max")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.rule1_int_max.name"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_ADVANCED)
        .AddDependency(BuildEnabledDependency(kRule1EnabledParamId))
        .AddDependency(BuildTypeDependency(kRule1TypeParamId, kValueTypeInt));

    rule1_float_min_
        .SetSchemaName("rule1_float_min")
        .SetStorageKey("rule1_float_min")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.rule1_float_min.name"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_ADVANCED)
        .AddDependency(BuildEnabledDependency(kRule1EnabledParamId))
        .AddDependency(BuildTypeDependency(kRule1TypeParamId, kValueTypeFloat));

    rule1_float_max_
        .SetSchemaName("rule1_float_max")
        .SetStorageKey("rule1_float_max")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.rule1_float_max.name"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_ADVANCED)
        .AddDependency(BuildEnabledDependency(kRule1EnabledParamId))
        .AddDependency(BuildTypeDependency(kRule1TypeParamId, kValueTypeFloat));

    rule1_double_min_
        .SetSchemaName("rule1_double_min")
        .SetStorageKey("rule1_double_min")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.rule1_double_min.name"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_ADVANCED)
        .AddDependency(BuildEnabledDependency(kRule1EnabledParamId))
        .AddDependency(BuildTypeDependency(kRule1TypeParamId, kValueTypeDouble));

    rule1_double_max_
        .SetSchemaName("rule1_double_max")
        .SetStorageKey("rule1_double_max")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.rule1_double_max.name"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_ADVANCED)
        .AddDependency(BuildEnabledDependency(kRule1EnabledParamId))
        .AddDependency(BuildTypeDependency(kRule1TypeParamId, kValueTypeDouble));

    rule2_enabled_
        .SetSchemaName("rule2_enabled")
        .SetStorageKey("rule2_enabled")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.rule2_enabled.name"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_BASIC);

    rule2_type_
        .SetSchemaName("rule2_type")
        .SetStorageKey("rule2_type")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.rule2_type.name"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_BASIC)
        .AddDependency(BuildEnabledDependency(kRule2EnabledParamId))
        .SetMetadataCustomizer([this](ParamMetadata& metadata) {
            metadata.constraint_type = CONSTRAINT_OPTIONS;
            metadata.options_constraint = OptionsConstraint();
            metadata.options_constraint.AddOption(kValueTypeInt, Tr(current_language(), "option.value_type.int"));
            metadata.options_constraint.AddOption(kValueTypeFloat, Tr(current_language(), "option.value_type.float"));
            metadata.options_constraint.AddOption(kValueTypeDouble, Tr(current_language(), "option.value_type.double"));
        });

    rule2_int_input_
        .SetSchemaName("rule2_int_input")
        .SetStorageKey("rule2_int_input")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.rule2_int_input.name"); })
        .SetBindable(true)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_BASIC)
        .AddDependency(BuildEnabledDependency(kRule2EnabledParamId))
        .AddDependency(BuildTypeDependency(kRule2TypeParamId, kValueTypeInt));

    rule2_float_input_
        .SetSchemaName("rule2_float_input")
        .SetStorageKey("rule2_float_input")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.rule2_float_input.name"); })
        .SetBindable(true)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_BASIC)
        .AddDependency(BuildEnabledDependency(kRule2EnabledParamId))
        .AddDependency(BuildTypeDependency(kRule2TypeParamId, kValueTypeFloat));

    rule2_double_input_
        .SetSchemaName("rule2_double_input")
        .SetStorageKey("rule2_double_input")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.rule2_double_input.name"); })
        .SetBindable(true)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_BASIC)
        .AddDependency(BuildEnabledDependency(kRule2EnabledParamId))
        .AddDependency(BuildTypeDependency(kRule2TypeParamId, kValueTypeDouble));

    rule2_int_min_
        .SetSchemaName("rule2_int_min")
        .SetStorageKey("rule2_int_min")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.rule2_int_min.name"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_ADVANCED)
        .AddDependency(BuildEnabledDependency(kRule2EnabledParamId))
        .AddDependency(BuildTypeDependency(kRule2TypeParamId, kValueTypeInt));

    rule2_int_max_
        .SetSchemaName("rule2_int_max")
        .SetStorageKey("rule2_int_max")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.rule2_int_max.name"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_ADVANCED)
        .AddDependency(BuildEnabledDependency(kRule2EnabledParamId))
        .AddDependency(BuildTypeDependency(kRule2TypeParamId, kValueTypeInt));

    rule2_float_min_
        .SetSchemaName("rule2_float_min")
        .SetStorageKey("rule2_float_min")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.rule2_float_min.name"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_ADVANCED)
        .AddDependency(BuildEnabledDependency(kRule2EnabledParamId))
        .AddDependency(BuildTypeDependency(kRule2TypeParamId, kValueTypeFloat));

    rule2_float_max_
        .SetSchemaName("rule2_float_max")
        .SetStorageKey("rule2_float_max")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.rule2_float_max.name"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_ADVANCED)
        .AddDependency(BuildEnabledDependency(kRule2EnabledParamId))
        .AddDependency(BuildTypeDependency(kRule2TypeParamId, kValueTypeFloat));

    rule2_double_min_
        .SetSchemaName("rule2_double_min")
        .SetStorageKey("rule2_double_min")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.rule2_double_min.name"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_ADVANCED)
        .AddDependency(BuildEnabledDependency(kRule2EnabledParamId))
        .AddDependency(BuildTypeDependency(kRule2TypeParamId, kValueTypeDouble));

    rule2_double_max_
        .SetSchemaName("rule2_double_max")
        .SetStorageKey("rule2_double_max")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.rule2_double_max.name"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_ADVANCED)
        .AddDependency(BuildEnabledDependency(kRule2EnabledParamId))
        .AddDependency(BuildTypeDependency(kRule2TypeParamId, kValueTypeDouble));

    result_string_output_
        .SetSchemaName("result_string")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "output.result_string.name"); })
        .SetVisibility(HVOutputVisibility::Always)
        .BindExternalValue(result_string_);

    result_int_output_
        .SetSchemaName("result_int")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "output.result_int.name"); })
        .SetVisibility(HVOutputVisibility::Always)
        .BindExternalValue(result_int_);

    execute_status_output_
        .SetSchemaName("execute_status")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "output.execute_status.name"); })
        .SetVisibility(HVOutputVisibility::Always)
        .BindExternalValue(execute_status_);

    RegisterInputField(judge_mode_);
    RegisterInputField(rule1_enabled_);
    RegisterInputField(rule1_type_);
    RegisterInputField(rule1_int_input_);
    RegisterInputField(rule1_float_input_);
    RegisterInputField(rule1_double_input_);
    RegisterInputField(rule1_int_min_);
    RegisterInputField(rule1_int_max_);
    RegisterInputField(rule1_float_min_);
    RegisterInputField(rule1_float_max_);
    RegisterInputField(rule1_double_min_);
    RegisterInputField(rule1_double_max_);
    RegisterInputField(rule2_enabled_);
    RegisterInputField(rule2_type_);
    RegisterInputField(rule2_int_input_);
    RegisterInputField(rule2_float_input_);
    RegisterInputField(rule2_double_input_);
    RegisterInputField(rule2_int_min_);
    RegisterInputField(rule2_int_max_);
    RegisterInputField(rule2_float_min_);
    RegisterInputField(rule2_float_max_);
    RegisterInputField(rule2_double_min_);
    RegisterInputField(rule2_double_max_);

    RegisterOutputField(result_string_output_);
    RegisterOutputField(result_int_output_);
    RegisterOutputField(execute_status_output_);
}

int HVConditionCheck::init()
{
    ResetRuntimeState();
    result_string_.clear();
    result_int_ = 0;
    return SUCCESS;
}

int HVConditionCheck::run()
{
    const auto start = std::chrono::steady_clock::now();
    execute_status_ = NODE_STATUS_RUNNING;
    error_message_key_.clear();
    result_string_.clear();
    result_int_ = 0;

    auto fail = [this, &start](const int status, const std::string& key) {
        const auto end = std::chrono::steady_clock::now();
        run_time_ = static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
        return FailWithMessage(status, key);
    };

    if (judge_mode_.value() != kJudgeModeAny && judge_mode_.value() != kJudgeModeAll) {
        return fail(INVALID_PARAMS_NUM, "msg.invalid_judge_mode");
    }

    std::vector<bool> rule_results;
    rule_results.reserve(2);

    auto evaluate_rule = [&fail, &rule_results](
                             const bool enabled,
                             const int type,
                             const int int_input,
                             const float float_input,
                             const double double_input,
                             const int int_min,
                             const int int_max,
                             const float float_min,
                             const float float_max,
                             const double double_min,
                             const double double_max,
                             const std::string&) -> int {
        if (!enabled) {
            return SUCCESS;
        }

        bool rule_ok = false;
        switch (type) {
        case kValueTypeInt:
            if (int_min > int_max) {
                return fail(INVALID_PARAMS_NUM, "msg.invalid_range");
            }
            rule_ok = IsValueWithinClosedRange(int_input, int_min, int_max);
            break;
        case kValueTypeFloat:
            if (float_min > float_max) {
                return fail(INVALID_PARAMS_NUM, "msg.invalid_range");
            }
            rule_ok = IsValueWithinClosedRange(float_input, float_min, float_max);
            break;
        case kValueTypeDouble:
            if (double_min > double_max) {
                return fail(INVALID_PARAMS_NUM, "msg.invalid_range");
            }
            rule_ok = IsValueWithinClosedRange(double_input, double_min, double_max);
            break;
        default:
            return fail(INVALID_PARAMS_NUM, "msg.invalid_rule_type");
        }

        rule_results.push_back(rule_ok);
        return SUCCESS;
    };

    const int active_rule_count = static_cast<int>(rule1_enabled_.value()) + static_cast<int>(rule2_enabled_.value());
    if (active_rule_count <= 0) {
        return fail(INVALID_PARAMS_NUM, "msg.no_enabled_rule");
    }

    int ret = evaluate_rule(
        rule1_enabled_.value(),
        rule1_type_.value(),
        rule1_int_input_.value(),
        rule1_float_input_.value(),
        rule1_double_input_.value(),
        rule1_int_min_.value(),
        rule1_int_max_.value(),
        rule1_float_min_.value(),
        rule1_float_max_.value(),
        rule1_double_min_.value(),
        rule1_double_max_.value(),
        "msg.rule1_missing_input");
    if (ret != SUCCESS) {
        return ret;
    }

    ret = evaluate_rule(
        rule2_enabled_.value(),
        rule2_type_.value(),
        rule2_int_input_.value(),
        rule2_float_input_.value(),
        rule2_double_input_.value(),
        rule2_int_min_.value(),
        rule2_int_max_.value(),
        rule2_float_min_.value(),
        rule2_float_max_.value(),
        rule2_double_min_.value(),
        rule2_double_max_.value(),
        "msg.rule2_missing_input");
    if (ret != SUCCESS) {
        return ret;
    }

    bool final_ok = false;
    if (judge_mode_.value() == kJudgeModeAll) {
        final_ok = std::all_of(rule_results.begin(), rule_results.end(), [](const bool value) { return value; });
    }
    else {
        final_ok = std::any_of(rule_results.begin(), rule_results.end(), [](const bool value) { return value; });
    }

    result_int_ = final_ok ? 1 : 0;
    result_string_ = final_ok ? "OK" : "NG";

    const auto end = std::chrono::steady_clock::now();
    run_time_ = static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
    execute_status_ = SUCCESS;
    error_message_key_ = final_ok ? "msg.run_ok" : "msg.run_ng";
    return SUCCESS;
}

AlgorithmType HVConditionCheck::get_algorithm_type()
{
    return AlgorithmType::LogicTool;
}

std::string HVConditionCheck::get_algorithm_display_name()
{
    return Tr(current_language(), "algorithm.display");
}

std::string HVConditionCheck::TranslateText(const std::string& key) const
{
    return Tr(current_language(), key);
}

extern "C" __declspec(dllexport) NodeEngine* CreateInstance()
{
    return new HVConditionCheck();
}

extern "C" __declspec(dllexport) std::string GetInstanceName()
{
    return "Condition check";
}

extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion()
{
    return NODE_ENGINE_ABI_VERSION;
}
