#include "HVVariableCalculate.h"

#include <chrono>
#include <cmath>
#include <sstream>

#include "HVI18n.h"
#include "HVNodeReferenceResolver.h"
#include "tinyexpr.h"

namespace {

const hvi18n::Dictionary kTexts = {
    { "algorithm.display", { "变量计算", "Variable calculate" } },
    { "input.expression_list.name", { "表达式列表", "Expression list" } },
    { "input.expression_list.desc", { "按顺序计算的表达式列表；结果列表会输出每一项的数值结果", "Expression list evaluated in order; result list returns the numeric result of each item" } },
    { "output.result_list.name", { "结果列表", "Result list" } },
    { "output.status.name", { "运行状态", "Execute status" } },
    { "msg.host_services_missing", { "宿主服务不可用", "Host services are unavailable" } },
    { "msg.invalid_token_syntax", { "表达式中的结果引用语法无效", "The result reference syntax in expression is invalid" } },
    { "msg.invalid_token_type", { "表达式中的结果类型标记无效", "The referenced result type tag in expression is invalid" } },
    { "msg.reference_resolve_failed", { "表达式中的结果引用无法解析", "A referenced result in expression cannot be resolved" } },
    { "msg.reference_node_not_found", { "表达式中的节点 ID 无效", "The referenced node id is invalid" } },
    { "msg.reference_not_reachable", { "表达式中的节点结果不在当前节点的可达上游中", "The referenced result is not reachable from the current node" } },
    { "msg.reference_output_not_found", { "表达式中的输出结果索引无效", "The referenced result index is invalid" } },
    { "msg.reference_unavailable", { "表达式中的结果当前没有生成", "The referenced result is currently unavailable" } },
    { "msg.reference_type_mismatch", { "表达式中的结果类型与实际输出类型不匹配", "The referenced result type does not match the actual output type" } },
    { "msg.reference_empty", { "表达式中的结果引用当前没有有效值", "A referenced result in expression does not currently have a value" } },
    { "msg.unsupported_type", { "变量计算仅支持数值类型引用", "Variable calculate only supports numeric references" } },
    { "msg.invalid_expression", { "表达式语法无效", "Expression syntax is invalid" } },
    { "msg.invalid_result", { "表达式计算结果无效", "Expression result is invalid" } },
    { "msg.run_success", { "变量计算成功", "Variable calculation succeeded" } }
};

std::string Tr(int language, const std::string& key)
{
    return hvi18n::Translate(kTexts, key, language);
}

const char* ErrorToMessageKey(hvref::ResolveError error)
{
    switch (error) {
    case hvref::ResolveError::InvalidSyntax:
        return "msg.invalid_token_syntax";
    case hvref::ResolveError::InvalidTypeTag:
        return "msg.invalid_token_type";
    case hvref::ResolveError::ResolveFailed:
        return "msg.reference_resolve_failed";
    case hvref::ResolveError::NodeNotFound:
        return "msg.reference_node_not_found";
    case hvref::ResolveError::NotReachable:
        return "msg.reference_not_reachable";
    case hvref::ResolveError::OutputNotFound:
        return "msg.reference_output_not_found";
    case hvref::ResolveError::Unavailable:
        return "msg.reference_unavailable";
    case hvref::ResolveError::TypeMismatch:
        return "msg.reference_type_mismatch";
    case hvref::ResolveError::EmptyValue:
        return "msg.reference_empty";
    case hvref::ResolveError::UnsupportedType:
        return "msg.unsupported_type";
    case hvref::ResolveError::None:
    default:
        return "msg.reference_resolve_failed";
    }
}

std::string NumberLiteral(double value)
{
    std::ostringstream oss;
    oss.precision(17);
    oss << value;
    return oss.str();
}

}  // namespace

HVVariableCalculate::HVVariableCalculate()
{
    expression_list_
        .SetSchemaName("expression_list")
        .SetStorageKey("expression_list")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.expression_list.name"); })
        .SetDescriptionResolver([this]() { return Tr(current_language(), "input.expression_list.desc"); })
        .SetBindable(true)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_BASIC);

    result_list_
        .SetSchemaName("result_list")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "output.result_list.name"); })
        .SetVisibility(HVOutputVisibility::OnSuccess);

    execute_status_output_
        .SetSchemaName("execute_status")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "output.status.name"); })
        .SetVisibility(HVOutputVisibility::Always)
        .BindExternalValue(execute_status_);

    RegisterInputField(expression_list_);
    RegisterOutputField(result_list_);
    RegisterOutputField(execute_status_output_);
}

int HVVariableCalculate::init()
{
    ResetRuntimeState();
    result_list_.value().values.clear();
    return SUCCESS;
}

int HVVariableCalculate::run()
{
    const auto start = std::chrono::steady_clock::now();
    execute_status_ = NODE_STATUS_RUNNING;
    error_message_key_.clear();
    result_list_.value().values.clear();

    if (host_services() == nullptr) {
        return FailCalculation(ALGORITHM_RUN_ERROR, "msg.host_services_missing");
    }

    if (expression_list_.value().values.empty()) {
        return FailCalculation(ALGORITHM_RUN_ERROR, "msg.invalid_expression");
    }

    for (size_t i = 0; i < expression_list_.value().values.size(); ++i) {
        const std::string expression_text = expression_list_.value().values[i].c_str();
        double value = 0.0;
        const int eval_ret = EvaluateExpressionText(expression_text, value);
        if (eval_ret != SUCCESS) {
            return eval_ret;
        }
        result_list_.value().values.push_back(value);
    }

    const auto end = std::chrono::steady_clock::now();
    run_time_ = static_cast<long>(
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
    execute_status_ = SUCCESS;
    error_message_key_ = "msg.run_success";
    return SUCCESS;
}

AlgorithmType HVVariableCalculate::get_algorithm_type()
{
    return AlgorithmType::LogicTool;
}

std::string HVVariableCalculate::get_algorithm_display_name()
{
    return Tr(current_language(), "algorithm.display");
}

std::string HVVariableCalculate::TranslateText(const std::string& key) const
{
    return Tr(current_language(), key);
}

int HVVariableCalculate::BuildEvaluableExpression(
    const std::string& expression_text,
    std::string& out_expression)
{
    out_expression.clear();

    size_t current_pos = 0;
    while (current_pos < expression_text.size()) {
        const size_t token_begin = expression_text.find('<', current_pos);
        if (token_begin == std::string::npos) {
            out_expression += expression_text.substr(current_pos);
            break;
        }

        out_expression += expression_text.substr(current_pos, token_begin - current_pos);

        const size_t token_end = expression_text.find('>', token_begin + 1);
        if (token_end == std::string::npos) {
            return FailCalculation(ALGORITHM_RUN_ERROR, "msg.invalid_token_syntax");
        }

        const std::string token_text =
            expression_text.substr(token_begin + 1, token_end - token_begin - 1);

        hvref::ParsedReferenceToken token;
        hvref::ResolveError parse_error = hvref::ParseReferenceToken(token_text, token);
        if (parse_error != hvref::ResolveError::None) {
            return FailCalculation(ALGORITHM_RUN_ERROR, ErrorToMessageKey(parse_error));
        }

        hvref::ResolvedReferenceValue resolved_value;
        hvref::ResolveError resolve_error =
            hvref::ResolveReferenceValue(*host_services(), token, resolved_value);
        if (resolve_error != hvref::ResolveError::None) {
            return FailCalculation(ALGORITHM_RUN_ERROR, ErrorToMessageKey(resolve_error));
        }

        double numeric_value = 0.0;
        hvref::ResolveError convert_error =
            hvref::ConvertReferenceValueToDouble(resolved_value, numeric_value);
        if (convert_error != hvref::ResolveError::None) {
            return FailCalculation(ALGORITHM_RUN_ERROR, ErrorToMessageKey(convert_error));
        }
        out_expression += NumberLiteral(numeric_value);
        current_pos = token_end + 1;
    }

    return SUCCESS;
}

int HVVariableCalculate::EvaluateExpressionText(
    const std::string& expression_text,
    double& out_value)
{
    out_value = 0.0;

    std::string evaluable_expression;
    const int build_ret = BuildEvaluableExpression(expression_text, evaluable_expression);
    if (build_ret != SUCCESS) {
        return build_ret;
    }

    int error_pos = 0;
    const double value = te_interp(evaluable_expression.c_str(), &error_pos);
    if (error_pos != 0) {
        return FailCalculation(ALGORITHM_RUN_ERROR, "msg.invalid_expression");
    }
    if (!std::isfinite(value)) {
        return FailCalculation(ALGORITHM_RUN_ERROR, "msg.invalid_result");
    }

    out_value = value;
    return SUCCESS;
}

int HVVariableCalculate::FailCalculation(int status, const std::string& message_key)
{
    result_list_.value().values.clear();
    return FailWithMessage(status, message_key);
}

extern "C" __declspec(dllexport) NodeEngine* CreateInstance()
{
    return new HVVariableCalculate();
}

extern "C" __declspec(dllexport) std::string GetInstanceName()
{
    return "Variable calculate";
}

extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion()
{
    return NODE_ENGINE_ABI_VERSION;
}
