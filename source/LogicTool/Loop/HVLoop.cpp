#include "HVLoop.h"

#include <chrono>

#include "HVI18n.h"

namespace {

constexpr int kLoopStateNotRun = 0;
constexpr int kLoopStateRunning = 1;
constexpr int kLoopStateCompleted = 2;
constexpr int kLoopStateInterrupted = 3;
constexpr int kLoopStateFailed = 4;

constexpr int kDataTypeInt = 0;
constexpr int kDataTypeFloat = 1;
constexpr int kDataTypeDouble = 2;
constexpr int kDataTypeBool = 3;
constexpr int kDataTypeString = 4;

constexpr int kCompareEqual = 0;
constexpr int kCompareNotEqual = 1;
constexpr int kCompareGreater = 2;
constexpr int kCompareGreaterEqual = 3;
constexpr int kCompareLess = 4;
constexpr int kCompareLessEqual = 5;

const hvi18n::Dictionary kTexts = {
    { "algorithm.display", { "循环", "Loop" } },
    { "input.start.name", { "循环起始值", "Loop start value" } },
    { "input.start.desc", { "自定义循环计数起始值，通常设置为0", "Initial loop counter value, usually 0" } },
    { "input.end.name", { "循环结束值", "Loop end value" } },
    { "input.end.desc", { "循环结束值与起始值之差为循环次数", "Loop count is end value minus start value" } },
    { "input.interval.name", { "循环间隔(ms)", "Loop interval (ms)" } },
    { "input.interval.desc", { "单次循环完成后的等待时间", "Delay after each loop iteration" } },
    { "input.target.name", { "目标节点ID", "Target node id" } },
    { "input.target.desc", { "循环体执行到该目标节点为止", "Loop body runs through this target node" } },
    { "input.interrupt_enabled.name", { "中断循环", "Interrupt loop" } },
    { "input.interrupt_enabled.desc", { "开启后按比较条件提前终止循环", "Stop the loop early when the comparison matches" } },
    { "input.data_type.name", { "数据类型", "Data type" } },
    { "input.data_type.desc", { "选择中断比较使用的数据类型", "Data type used by the interrupt comparison" } },
    { "input.compare_operator.name", { "比较条件", "Compare operator" } },
    { "input.compare_operator.desc", { "源比较值与目的比较值之间的比较方式", "Comparison between source and target values" } },
    { "input.source_int.name", { "源比较值(整数)", "Source compare value (int)" } },
    { "input.target_int.name", { "目的比较值(整数)", "Target compare value (int)" } },
    { "input.source_float.name", { "源比较值(浮点)", "Source compare value (float)" } },
    { "input.target_float.name", { "目的比较值(浮点)", "Target compare value (float)" } },
    { "input.source_double.name", { "源比较值(双精度)", "Source compare value (double)" } },
    { "input.target_double.name", { "目的比较值(双精度)", "Target compare value (double)" } },
    { "input.source_bool.name", { "源比较值(布尔)", "Source compare value (bool)" } },
    { "input.target_bool.name", { "目的比较值(布尔)", "Target compare value (bool)" } },
    { "input.source_string.name", { "源比较值(字符串)", "Source compare value (string)" } },
    { "input.target_string.name", { "目的比较值(字符串)", "Target compare value (string)" } },
    { "output.current_count.name", { "当前循环计数", "Current loop count" } },
    { "output.loop_state.name", { "循环状态", "Loop state" } },
    { "output.execute_status.name", { "运行状态", "Execute status" } },
    { "option.data_type.int", { "整数", "Integer" } },
    { "option.data_type.float", { "浮点", "Float" } },
    { "option.data_type.double", { "双精度", "Double" } },
    { "option.data_type.bool", { "布尔", "Boolean" } },
    { "option.data_type.string", { "字符串", "String" } },
    { "option.compare.eq", { "等于", "Equal" } },
    { "option.compare.ne", { "不等于", "Not equal" } },
    { "option.compare.gt", { "大于", "Greater than" } },
    { "option.compare.ge", { "大于等于", "Greater or equal" } },
    { "option.compare.lt", { "小于", "Less than" } },
    { "option.compare.le", { "小于等于", "Less or equal" } },
    { "msg.invalid_range", { "循环结束值不能小于循环起始值", "Loop end value cannot be less than the start value" } },
    { "msg.invalid_interval", { "循环间隔不能小于0", "Loop interval cannot be negative" } },
    { "msg.invalid_target", { "目标节点ID不合法", "Target node id is invalid" } },
    { "msg.invalid_data_type", { "中断循环数据类型不合法", "Interrupt data type is invalid" } },
    { "msg.invalid_compare_operator", { "比较条件不合法", "Compare operator is invalid" } },
    { "msg.run_success", { "循环节点执行成功", "Loop node executed successfully" } }
};

std::string Tr(int language, const std::string& key)
{
    return hvi18n::Translate(kTexts, key, language);
}

OptionsConstraint BuildDataTypeOptions(int language)
{
    OptionsConstraint options;
    options.AddOption(kDataTypeInt, Tr(language, "option.data_type.int"));
    options.AddOption(kDataTypeFloat, Tr(language, "option.data_type.float"));
    options.AddOption(kDataTypeDouble, Tr(language, "option.data_type.double"));
    options.AddOption(kDataTypeBool, Tr(language, "option.data_type.bool"));
    options.AddOption(kDataTypeString, Tr(language, "option.data_type.string"));
    options.default_index = 0;
    return options;
}

OptionsConstraint BuildCompareOptions(int language)
{
    OptionsConstraint options;
    options.AddOption(kCompareEqual, Tr(language, "option.compare.eq"));
    options.AddOption(kCompareNotEqual, Tr(language, "option.compare.ne"));
    options.AddOption(kCompareGreater, Tr(language, "option.compare.gt"));
    options.AddOption(kCompareGreaterEqual, Tr(language, "option.compare.ge"));
    options.AddOption(kCompareLess, Tr(language, "option.compare.lt"));
    options.AddOption(kCompareLessEqual, Tr(language, "option.compare.le"));
    options.default_index = 0;
    return options;
}

ParamDependency InterruptEnabledDependency()
{
    return ParamDependency(4, DEPENDS_ON_EQUALS, { "1" });
}

ParamDependency DataTypeDependency(int data_type)
{
    return ParamDependency(5, DEPENDS_ON_EQUALS, { std::to_string(data_type) });
}

template <typename T>
bool CompareOrdered(T source, T target, int compare_operator)
{
    switch (compare_operator) {
    case kCompareEqual:
        return source == target;
    case kCompareNotEqual:
        return source != target;
    case kCompareGreater:
        return source > target;
    case kCompareGreaterEqual:
        return source >= target;
    case kCompareLess:
        return source < target;
    case kCompareLessEqual:
        return source <= target;
    default:
        return false;
    }
}

bool CompareEquality(bool source, bool target, int compare_operator)
{
    if (compare_operator == kCompareEqual) {
        return source == target;
    }
    if (compare_operator == kCompareNotEqual) {
        return source != target;
    }
    return false;
}

bool CompareEquality(const std::string& source, const std::string& target, int compare_operator)
{
    if (compare_operator == kCompareEqual) {
        return source == target;
    }
    if (compare_operator == kCompareNotEqual) {
        return source != target;
    }
    return false;
}

}  // namespace

HVLoop::HVLoop()
{
    start_value_
        .SetSchemaName("loop_start_value")
        .SetStorageKey("loop_start_value")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.start.name"); })
        .SetDescriptionResolver([this]() { return Tr(current_language(), "input.start.desc"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_BASIC);

    end_value_
        .SetSchemaName("loop_end_value")
        .SetStorageKey("loop_end_value")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.end.name"); })
        .SetDescriptionResolver([this]() { return Tr(current_language(), "input.end.desc"); })
        .SetBindable(true)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_BASIC);

    interval_ms_
        .SetSchemaName("loop_interval_ms")
        .SetStorageKey("loop_interval_ms")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.interval.name"); })
        .SetDescriptionResolver([this]() { return Tr(current_language(), "input.interval.desc"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_BASIC);

    target_node_id_
        .SetSchemaName("target_node_id")
        .SetStorageKey("target_node_id")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.target.name"); })
        .SetDescriptionResolver([this]() { return Tr(current_language(), "input.target.desc"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_BASIC);

    interrupt_enabled_
        .SetSchemaName("interrupt_enabled")
        .SetStorageKey("interrupt_enabled")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.interrupt_enabled.name"); })
        .SetDescriptionResolver([this]() { return Tr(current_language(), "input.interrupt_enabled.desc"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_BASIC);

    interrupt_data_type_
        .SetSchemaName("interrupt_data_type")
        .SetStorageKey("interrupt_data_type")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.data_type.name"); })
        .SetDescriptionResolver([this]() { return Tr(current_language(), "input.data_type.desc"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_BASIC)
        .SetOptionsConstraint(BuildDataTypeOptions(current_language()))
        .AddDependency(InterruptEnabledDependency())
        .SetMetadataCustomizer([this](ParamMetadata& metadata) {
            metadata.options_constraint = BuildDataTypeOptions(current_language());
        });

    compare_operator_
        .SetSchemaName("compare_operator")
        .SetStorageKey("compare_operator")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.compare_operator.name"); })
        .SetDescriptionResolver([this]() { return Tr(current_language(), "input.compare_operator.desc"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_BASIC)
        .SetOptionsConstraint(BuildCompareOptions(current_language()))
        .AddDependency(InterruptEnabledDependency())
        .SetMetadataCustomizer([this](ParamMetadata& metadata) {
            metadata.options_constraint = BuildCompareOptions(current_language());
        });

    source_int_value_
        .SetSchemaName("source_int_value")
        .SetStorageKey("source_int_value")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.source_int.name"); })
        .SetBindable(true)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_BASIC)
        .AddDependency(InterruptEnabledDependency())
        .AddDependency(DataTypeDependency(kDataTypeInt));

    target_int_value_
        .SetSchemaName("target_int_value")
        .SetStorageKey("target_int_value")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.target_int.name"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_BASIC)
        .AddDependency(InterruptEnabledDependency())
        .AddDependency(DataTypeDependency(kDataTypeInt));

    source_float_value_
        .SetSchemaName("source_float_value")
        .SetStorageKey("source_float_value")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.source_float.name"); })
        .SetBindable(true)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_BASIC)
        .AddDependency(InterruptEnabledDependency())
        .AddDependency(DataTypeDependency(kDataTypeFloat));

    target_float_value_
        .SetSchemaName("target_float_value")
        .SetStorageKey("target_float_value")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.target_float.name"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_BASIC)
        .AddDependency(InterruptEnabledDependency())
        .AddDependency(DataTypeDependency(kDataTypeFloat));

    source_double_value_
        .SetSchemaName("source_double_value")
        .SetStorageKey("source_double_value")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.source_double.name"); })
        .SetBindable(true)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_BASIC)
        .AddDependency(InterruptEnabledDependency())
        .AddDependency(DataTypeDependency(kDataTypeDouble));

    target_double_value_
        .SetSchemaName("target_double_value")
        .SetStorageKey("target_double_value")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.target_double.name"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_BASIC)
        .AddDependency(InterruptEnabledDependency())
        .AddDependency(DataTypeDependency(kDataTypeDouble));

    source_bool_value_
        .SetSchemaName("source_bool_value")
        .SetStorageKey("source_bool_value")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.source_bool.name"); })
        .SetBindable(true)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_BASIC)
        .AddDependency(InterruptEnabledDependency())
        .AddDependency(DataTypeDependency(kDataTypeBool));

    target_bool_value_
        .SetSchemaName("target_bool_value")
        .SetStorageKey("target_bool_value")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.target_bool.name"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_BASIC)
        .AddDependency(InterruptEnabledDependency())
        .AddDependency(DataTypeDependency(kDataTypeBool));

    source_string_value_
        .SetSchemaName("source_string_value")
        .SetStorageKey("source_string_value")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.source_string.name"); })
        .SetBindable(true)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_BASIC)
        .AddDependency(InterruptEnabledDependency())
        .AddDependency(DataTypeDependency(kDataTypeString));

    target_string_value_
        .SetSchemaName("target_string_value")
        .SetStorageKey("target_string_value")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.target_string.name"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_BASIC)
        .AddDependency(InterruptEnabledDependency())
        .AddDependency(DataTypeDependency(kDataTypeString));

    current_count_output_
        .SetSchemaName("current_loop_count")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "output.current_count.name"); })
        .SetVisibility(HVOutputVisibility::Always)
        .BindExternalValue(current_count_);

    loop_state_output_
        .SetSchemaName("loop_state")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "output.loop_state.name"); })
        .SetVisibility(HVOutputVisibility::Always)
        .BindExternalValue(loop_state_);

    execute_status_output_
        .SetSchemaName("execute_status")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "output.execute_status.name"); })
        .SetVisibility(HVOutputVisibility::Always)
        .BindExternalValue(execute_status_);

    RegisterInputField(start_value_);
    RegisterInputField(end_value_);
    RegisterInputField(interval_ms_);
    RegisterInputField(target_node_id_);
    RegisterInputField(interrupt_enabled_);
    RegisterInputField(interrupt_data_type_);
    RegisterInputField(compare_operator_);
    RegisterInputField(source_int_value_);
    RegisterInputField(target_int_value_);
    RegisterInputField(source_float_value_);
    RegisterInputField(target_float_value_);
    RegisterInputField(source_double_value_);
    RegisterInputField(target_double_value_);
    RegisterInputField(source_bool_value_);
    RegisterInputField(target_bool_value_);
    RegisterInputField(source_string_value_);
    RegisterInputField(target_string_value_);

    RegisterOutputField(current_count_output_);
    RegisterOutputField(loop_state_output_);
    RegisterOutputField(execute_status_output_);
}

int HVLoop::init()
{
    ResetRuntimeState();
    current_count_ = start_value_.value();
    next_count_ = start_value_.value();
    loop_state_ = kLoopStateNotRun;
    return SUCCESS;
}

int HVLoop::run()
{
    const auto start = std::chrono::steady_clock::now();
    execute_status_ = NODE_STATUS_RUNNING;
    error_message_key_.clear();

    auto finish_timing = [this, &start]() {
        const auto end = std::chrono::steady_clock::now();
        run_time_ = static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
    };

    if (end_value_.value() < start_value_.value()) {
        finish_timing();
        return FailLoop(INVALID_PARAMS_NUM, "msg.invalid_range");
    }
    if (interval_ms_.value() < 0) {
        finish_timing();
        return FailLoop(INVALID_PARAMS_NUM, "msg.invalid_interval");
    }
    if (target_node_id_.value() < 0) {
        finish_timing();
        return FailLoop(INVALID_PARAMS_NUM, "msg.invalid_target");
    }
    if (interrupt_data_type_.value() < kDataTypeInt || interrupt_data_type_.value() > kDataTypeString) {
        finish_timing();
        return FailLoop(INVALID_PARAMS_NUM, "msg.invalid_data_type");
    }
    if (compare_operator_.value() < kCompareEqual || compare_operator_.value() > kCompareLessEqual) {
        finish_timing();
        return FailLoop(INVALID_PARAMS_NUM, "msg.invalid_compare_operator");
    }

    if (loop_state_ != kLoopStateRunning) {
        next_count_ = start_value_.value();
    }

    current_count_ = next_count_;
    if (current_count_ >= end_value_.value()) {
        loop_state_ = kLoopStateCompleted;
        execute_status_ = SUCCESS;
        error_message_key_ = "msg.run_success";
        finish_timing();
        return SUCCESS;
    }

    if (interrupt_enabled_.value() && IsInterruptConditionMet()) {
        loop_state_ = kLoopStateInterrupted;
        execute_status_ = SUCCESS;
        error_message_key_ = "msg.run_success";
        finish_timing();
        return SUCCESS;
    }

    ++next_count_;
    loop_state_ = next_count_ >= end_value_.value() ? kLoopStateCompleted : kLoopStateRunning;
    execute_status_ = SUCCESS;
    error_message_key_ = "msg.run_success";
    finish_timing();
    return SUCCESS;
}

AlgorithmType HVLoop::get_algorithm_type()
{
    return AlgorithmType::LogicTool;
}

std::string HVLoop::get_algorithm_display_name()
{
    return Tr(current_language(), "algorithm.display");
}

NodeControlMode HVLoop::get_control_mode() const
{
    return NodeControlMode::LoopControl;
}

std::string HVLoop::TranslateText(const std::string& key) const
{
    return Tr(current_language(), key);
}

bool HVLoop::IsInterruptConditionMet() const
{
    switch (interrupt_data_type_.value()) {
    case kDataTypeInt:
        return CompareOrdered(source_int_value_.value(), target_int_value_.value(), compare_operator_.value());
    case kDataTypeFloat:
        return CompareOrdered(source_float_value_.value(), target_float_value_.value(), compare_operator_.value());
    case kDataTypeDouble:
        return CompareOrdered(source_double_value_.value(), target_double_value_.value(), compare_operator_.value());
    case kDataTypeBool:
        return CompareEquality(source_bool_value_.value(), target_bool_value_.value(), compare_operator_.value());
    case kDataTypeString:
        return CompareEquality(source_string_value_.value(), target_string_value_.value(), compare_operator_.value());
    default:
        return false;
    }
}

int HVLoop::FailLoop(int status, const std::string& message_key)
{
    loop_state_ = kLoopStateFailed;
    return FailWithMessage(status, message_key);
}

extern "C" __declspec(dllexport) NodeEngine* CreateInstance()
{
    return new HVLoop();
}

extern "C" __declspec(dllexport) std::string GetInstanceName()
{
    return "Loop";
}

extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion()
{
    return NODE_ENGINE_ABI_VERSION;
}
