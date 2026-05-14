#include "HVFormat.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>

#include <json.hpp>

#include "HVI18n.h"
#include "HVNodeReferenceResolver.h"

namespace {

constexpr int kFormatTextParamId = 0;
const hvi18n::Dictionary kTexts = {
    { "algorithm.display", { "格式化", "Format" } },
    { "input.format_text.name", { "格式化字符串", "Format text" } },
    { "input.format_text.desc", { "支持在字符串中写入 <node_id.node_alias.result_id.output_name type> 结果引用标记", "Format string with embedded <node_id.node_alias.result_id.output_name type> tokens" } },
    { "output.formatted_text.name", { "格式化结果", "Formatted text" } },
    { "output.status.name", { "运行状态", "Execute status" } },
    { "msg.host_services_missing", { "宿主服务不可用", "Host services are unavailable" } },
    { "msg.invalid_token_syntax", { "格式化字符串中的结果引用语法无效", "The result reference syntax in format text is invalid" } },
    { "msg.invalid_token_type", { "格式化字符串中的结果类型标记无效", "The referenced result type tag in format text is invalid" } },
    { "msg.reference_resolve_failed", { "格式化字符串中的结果引用无法解析", "A referenced result in format text cannot be resolved" } },
    { "msg.reference_node_not_found", { "格式化字符串中的节点 ID 无效", "The referenced node id is invalid" } },
    { "msg.reference_not_reachable", { "格式化字符串中的节点结果不在当前节点的可达上游中", "The referenced result is not reachable from the current node" } },
    { "msg.reference_output_not_found", { "格式化字符串中的输出结果索引无效", "The referenced result index is invalid" } },
    { "msg.reference_unavailable", { "格式化字符串中的结果当前没有生成", "The referenced result is currently unavailable" } },
    { "msg.reference_type_mismatch", { "格式化字符串中的结果类型与实际输出类型不匹配", "The referenced result type does not match the actual output type" } },
    { "msg.reference_empty", { "格式化字符串中的结果引用当前没有有效值", "A referenced result in format text does not currently have a value" } },
    { "msg.unsupported_type", { "格式化节点暂不支持该结果类型", "Format node does not support this result type" } },
    { "msg.run_success", { "格式化成功", "Formatting succeeded" } }
};

std::string Tr(int language, const std::string& key)
{
    return hvi18n::Translate(kTexts, key, language);
}

std::string TrimCopy(const std::string& text)
{
    const size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

bool TryParseIntStrict(const std::string& text, int& out_value)
{
    const std::string trimmed = TrimCopy(text);
    if (trimmed.empty()) {
        return false;
    }

    size_t parsed_length = 0;
    try {
        out_value = std::stoi(trimmed, &parsed_length);
    }
    catch (...) {
        return false;
    }
    return parsed_length == trimmed.size();
}

int MapTypeNameToHvType(const std::string& type_name)
{
    const std::string trimmed = TrimCopy(type_name);
    if (trimmed == "int") {
        return HV_INT;
    }
    if (trimmed == "long") {
        return HV_LONG;
    }
    if (trimmed == "float") {
        return HV_FLOAT;
    }
    if (trimmed == "double") {
        return HV_DOUBLE;
    }
    if (trimmed == "bool") {
        return HV_BOOLEAN;
    }
    if (trimmed == "string") {
        return HV_STRING;
    }
    return -1;
}

std::string FormatFloatingFixed4(double value)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4) << value;
    return oss.str();
}

bool IsGlobalVariableTokenHead(const std::string& token_head)
{
    // 前端可能传 UTF-8，部分手工入口仍按本机 ANSI/GBK 传入，这里同时兼容两种字节序列。
    static const std::string kGlobalVariableUtf8 =
        "\xE5\x85\xA8\xE5\xB1\x80\xE5\x8F\x98\xE9\x87\x8F";
    static const std::string kGlobalVariableGbk = "\xC8\xAB\xBE\xD6\xB1\xE4\xC1\xBF";
    return token_head == kGlobalVariableUtf8 || token_head == kGlobalVariableGbk;
}

const char* ErrorToFormatMessageKey(hvref::ResolveError error)
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

}  // namespace

HVFormat::HVFormat()
{
}

int HVFormat::init()
{
    formatted_text_.clear();
    execute_status_ = NODE_STATUS_NOT_RUN;
    run_time_ = 0;
    error_message_key_.clear();
    return SUCCESS;
}

int HVFormat::run()
{
    const auto start = std::chrono::steady_clock::now();
    formatted_text_.clear();
    error_message_key_.clear();
    execute_status_ = NODE_STATUS_RUNNING;

    if (host_services_ == nullptr) {
        return Fail(ALGORITHM_RUN_ERROR, "msg.host_services_missing");
    }

    const int render_ret = RenderFormatText();
    if (render_ret != SUCCESS) {
        return Fail(render_ret, error_message_key_);
    }

    const auto end = std::chrono::steady_clock::now();
    run_time_ = static_cast<long>(
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
    execute_status_ = SUCCESS;
    error_message_key_ = "msg.run_success";
    return SUCCESS;
}

int HVFormat::set_algorithm_params(const std::vector<void*>& params, const std::vector<int>& paramID)
{
    if (paramID.empty()) {
        for (int i = 0; i < static_cast<int>(params.size()); ++i) {
            const int ret = ApplyParam(i, params[i]);
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
        const int ret = ApplyParam(paramID[i], params[i]);
        if (ret != SUCCESS) {
            return ret;
        }
    }
    return SUCCESS;
}

std::vector<void*> HVFormat::get_current_params()
{
    return { &format_text_ };
}

std::vector<void*> HVFormat::get_algorithm_result()
{
    if (execute_status_ == SUCCESS || execute_status_ == NODE_STATUS_NOT_RUN) {
        return { &formatted_text_, &execute_status_ };
    }
    return { nullptr, &execute_status_ };
}

std::vector<int> HVFormat::get_algorithm_input_params_type()
{
    return { HV_STRING };
}

std::vector<int> HVFormat::get_algorithm_output_params_type()
{
    return { HV_STRING, HV_INT };
}

std::vector<std::string> HVFormat::get_algorithm_input_params_name()
{
    return { Tr(language_, "input.format_text.name") };
}

std::vector<std::string> HVFormat::get_algorithm_output_params_name()
{
    return {
        Tr(language_, "output.formatted_text.name"),
        Tr(language_, "output.status.name")
    };
}

std::vector<bool> HVFormat::get_algorithm_input_params_bindable()
{
    return { false };
}

std::vector<ParamMetadata> HVFormat::get_algorithm_input_params_metadata()
{
    std::vector<ParamMetadata> metadata_list(1);
    metadata_list[0].param_name = "format_text";
    metadata_list[0].param_description = Tr(language_, "input.format_text.desc");
    metadata_list[0].param_type = HV_STRING;
    return metadata_list;
}

int HVFormat::get_algorithm_execute_status()
{
    return execute_status_;
}

std::string HVFormat::get_algorithm_error_message()
{
    if (error_message_key_.empty()) {
        return {};
    }
    return Tr(language_, error_message_key_);
}

long HVFormat::get_algorithm_use_time()
{
    return run_time_;
}

bool HVFormat::algorithm_params_setting_status()
{
    return true;
}

bool HVFormat::algorithm_init_status()
{
    return true;
}

bool HVFormat::save_params_to_json(const std::string& filePath)
{
    try {
        nlohmann::json params_json;
        params_json["format_text"] = format_text_;

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

bool HVFormat::load_params_from_json(const std::string& filePath)
{
    try {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }
        nlohmann::json params_json;
        file >> params_json;
        format_text_ = params_json.value("format_text", std::string());
        return true;
    }
    catch (...) {
        return false;
    }
}

AlgorithmType HVFormat::get_algorithm_type()
{
    return AlgorithmType::LogicTool;
}

void HVFormat::set_language(int language)
{
    language_ = language;
}

int HVFormat::get_language() const
{
    return language_;
}

std::string HVFormat::get_algorithm_display_name()
{
    return Tr(language_, "algorithm.display");
}

void HVFormat::set_host_services(NodeHostServices* host_services)
{
    host_services_ = host_services;
}

int HVFormat::ApplyParam(int param_id, void* value_ptr)
{
    if (param_id != kFormatTextParamId || value_ptr == nullptr) {
        return INVALID_PARAMS_NUM;
    }
    format_text_ = *static_cast<std::string*>(value_ptr);
    return SUCCESS;
}

int HVFormat::RenderFormatText()
{
    formatted_text_.clear();

    size_t current_pos = 0;
    while (current_pos < format_text_.size()) {
        const size_t token_begin = format_text_.find('<', current_pos);
        if (token_begin == std::string::npos) {
            formatted_text_ += format_text_.substr(current_pos);
            break;
        }

        formatted_text_ += format_text_.substr(current_pos, token_begin - current_pos);

        const size_t token_end = format_text_.find('>', token_begin + 1);
        if (token_end == std::string::npos) {
            error_message_key_ = "msg.invalid_token_syntax";
            return ALGORITHM_RUN_ERROR;
        }

        const std::string token_text =
            format_text_.substr(token_begin + 1, token_end - token_begin - 1);
        hvref::ParsedReferenceToken token;
        hvref::ResolveError parse_error = hvref::ParseReferenceToken(token_text, token);
        if (parse_error != hvref::ResolveError::None) {
            error_message_key_ = ErrorToFormatMessageKey(parse_error);
            return ALGORITHM_RUN_ERROR;
        }

        hvref::ResolvedReferenceValue resolved_value;
        hvref::ResolveError resolve_error =
            hvref::ResolveReferenceValue(*host_services_, token, resolved_value);
        if (resolve_error != hvref::ResolveError::None) {
            error_message_key_ = ErrorToFormatMessageKey(resolve_error);
            return ALGORITHM_RUN_ERROR;
        }

        std::string rendered_value;
        hvref::ResolveError format_error =
            hvref::FormatReferenceValue(resolved_value, rendered_value, token.format_spec);
        if (format_error != hvref::ResolveError::None) {
            error_message_key_ = ErrorToFormatMessageKey(format_error);
            return ALGORITHM_RUN_ERROR;
        }
        formatted_text_ += rendered_value;
        current_pos = token_end + 1;
    }

    return SUCCESS;
}

int HVFormat::ParseReferenceToken(const std::string& token_text, ParsedReferenceToken& out_token)
{
    out_token = ParsedReferenceToken();

    // 先把末尾的 (%fmt) 提取出来，避免格式串内的 '.' 干扰点位置查找。
    std::string clean_text = token_text;
    {
        const auto paren = token_text.rfind("(%");
        if (paren != std::string::npos && !token_text.empty() && token_text.back() == ')') {
            out_token.format_spec = TrimCopy(token_text.substr(paren + 1, token_text.size() - paren - 2));
            clean_text = TrimCopy(token_text.substr(0, paren));
        }
    }

    const size_t first_dot = clean_text.find('.');
    const size_t second_dot = (first_dot == std::string::npos)
        ? std::string::npos
        : clean_text.find('.', first_dot + 1);
    const size_t third_dot = (second_dot == std::string::npos)
        ? std::string::npos
        : clean_text.find('.', second_dot + 1);
    const size_t last_dot = clean_text.find_last_of('.');

    if (first_dot != std::string::npos && IsGlobalVariableTokenHead(clean_text.substr(0, first_dot))) {
        if (second_dot == std::string::npos ||
            third_dot == std::string::npos ||
            clean_text.find('.', third_dot + 1) != std::string::npos) {
            error_message_key_ = "msg.invalid_token_syntax";
            return ALGORITHM_RUN_ERROR;
        }

        int global_var_display_id = -1;
        const std::string global_var_id_text = clean_text.substr(first_dot + 1, second_dot - first_dot - 1);
        const std::string global_var_name_text = clean_text.substr(second_dot + 1, third_dot - second_dot - 1);
        const std::string type_text = clean_text.substr(third_dot + 1);
        if (!TryParseIntStrict(global_var_id_text, global_var_display_id) || global_var_display_id <= 0) {
            error_message_key_ = "msg.invalid_token_syntax";
            return ALGORITHM_RUN_ERROR;
        }

        out_token.kind = ReferenceKind::GlobalVariable;
        out_token.global_var_token_id = global_var_display_id;
        out_token.global_var_id = global_var_display_id;
        out_token.global_var_name = TrimCopy(global_var_name_text);
        out_token.expected_type = MapTypeNameToHvType(type_text);
        if (out_token.global_var_name.empty()) {
            error_message_key_ = "msg.invalid_token_syntax";
            return ALGORITHM_RUN_ERROR;
        }
        if (out_token.expected_type < 0) {
            error_message_key_ = "msg.invalid_token_type";
            return ALGORITHM_RUN_ERROR;
        }
        return SUCCESS;
    }

    if (first_dot == std::string::npos ||
        second_dot == std::string::npos ||
        third_dot == std::string::npos ||
        last_dot == std::string::npos ||
        third_dot >= last_dot) {
        error_message_key_ = "msg.invalid_token_syntax";
        return ALGORITHM_RUN_ERROR;
    }

    const std::string node_id_text = clean_text.substr(0, first_dot);
    const std::string node_alias_text = clean_text.substr(first_dot + 1, second_dot - first_dot - 1);
    const std::string result_id_text = clean_text.substr(second_dot + 1, third_dot - second_dot - 1);
    const std::string output_name_text = clean_text.substr(third_dot + 1, last_dot - third_dot - 1);
    const std::string type_text = clean_text.substr(last_dot + 1);

    if (!TryParseIntStrict(node_id_text, out_token.node_id) ||
        !TryParseIntStrict(result_id_text, out_token.result_id)) {
        error_message_key_ = "msg.invalid_token_syntax";
        return ALGORITHM_RUN_ERROR;
    }

    out_token.node_alias = TrimCopy(node_alias_text);
    out_token.output_name = TrimCopy(output_name_text);
    out_token.expected_type = MapTypeNameToHvType(type_text);
    if (out_token.node_alias.empty() || out_token.output_name.empty()) {
        error_message_key_ = "msg.invalid_token_syntax";
        return ALGORITHM_RUN_ERROR;
    }
    if (out_token.expected_type < 0) {
        error_message_key_ = "msg.invalid_token_type";
        return ALGORITHM_RUN_ERROR;
    }

    return SUCCESS;
}

int HVFormat::ResolveReferenceToken(const ParsedReferenceToken& token, std::string& rendered_value)
{
    rendered_value.clear();

    if (token.kind == ReferenceKind::GlobalVariable) {
        const std::vector<NodeHostGlobalVariableInfo> global_infos =
            host_services_->get_global_variable_infos();
        auto info_it = std::find_if(
            global_infos.begin(),
            global_infos.end(),
            [&token](const NodeHostGlobalVariableInfo& info) {
                return info.var_id == token.global_var_token_id && info.var_name == token.global_var_name;
            });
        if (info_it == global_infos.end() && token.global_var_token_id > 0) {
            const int legacy_var_id = token.global_var_token_id - 1;
            info_it = std::find_if(
                global_infos.begin(),
                global_infos.end(),
                [&token, legacy_var_id](const NodeHostGlobalVariableInfo& info) {
                    return info.var_id == legacy_var_id && info.var_name == token.global_var_name;
                });
        }
        if (info_it == global_infos.end()) {
            error_message_key_ = "msg.reference_resolve_failed";
            return ALGORITHM_RUN_ERROR;
        }
        if (info_it->var_type != token.expected_type) {
            error_message_key_ = "msg.reference_type_mismatch";
            return ALGORITHM_RUN_ERROR;
        }

        NodeHostValue global_value;
        const int resolve_ret = host_services_->get_global_value(info_it->var_id, global_value);
        if (resolve_ret == GLOBAL_VAR_NOT_EXIST) {
            error_message_key_ = "msg.reference_resolve_failed";
            return ALGORITHM_RUN_ERROR;
        }
        if (resolve_ret != SUCCESS) {
            error_message_key_ = "msg.reference_empty";
            return ALGORITHM_RUN_ERROR;
        }
        if (!global_value.has_value || global_value.type != token.expected_type) {
            error_message_key_ = (!global_value.has_value)
                ? "msg.reference_empty"
                : "msg.reference_type_mismatch";
            return ALGORITHM_RUN_ERROR;
        }

        return FormatHostValue(global_value, rendered_value);
    }

    NodeHostDataView data_view;
    const int resolve_ret = host_services_->get_upstream_result_by_id(
        token.node_id,
        token.result_id,
        std::string(),
        std::string(),
        token.expected_type,
        data_view);
    if (resolve_ret == UPSTREAM_RESULT_NODE_NOT_FOUND) {
        error_message_key_ = "msg.reference_node_not_found";
        return ALGORITHM_RUN_ERROR;
    }
    if (resolve_ret == UPSTREAM_RESULT_NOT_REACHABLE) {
        error_message_key_ = "msg.reference_not_reachable";
        return ALGORITHM_RUN_ERROR;
    }
    if (resolve_ret == UPSTREAM_RESULT_OUTPUT_NOT_FOUND) {
        error_message_key_ = "msg.reference_output_not_found";
        return ALGORITHM_RUN_ERROR;
    }
    if (resolve_ret == UPSTREAM_RESULT_UNAVAILABLE) {
        error_message_key_ = "msg.reference_unavailable";
        return ALGORITHM_RUN_ERROR;
    }
    if (resolve_ret == UPSTREAM_RESULT_TYPE_NOT_SUPPORTED) {
        error_message_key_ = "msg.reference_type_mismatch";
        return ALGORITHM_RUN_ERROR;
    }
    if (resolve_ret != SUCCESS) {
        error_message_key_ = "msg.reference_resolve_failed";
        return ALGORITHM_RUN_ERROR;
    }
    if (!data_view.has_value || data_view.data == nullptr) {
        error_message_key_ = "msg.reference_empty";
        return ALGORITHM_RUN_ERROR;
    }

    return FormatResolvedValue(data_view, rendered_value);
}

int HVFormat::FormatResolvedValue(const NodeHostDataView& data_view, std::string& rendered_value)
{
    return FormatValue(data_view.type, data_view.data, rendered_value);
}

int HVFormat::FormatHostValue(const NodeHostValue& value, std::string& rendered_value)
{
    switch (value.type) {
    case HV_INT:
        return FormatValue(value.type, &value.int_value, rendered_value);
    case HV_LONG:
        return FormatValue(value.type, &value.long_value, rendered_value);
    case HV_FLOAT:
        return FormatValue(value.type, &value.float_value, rendered_value);
    case HV_DOUBLE:
        return FormatValue(value.type, &value.double_value, rendered_value);
    case HV_BOOLEAN:
        return FormatValue(value.type, &value.bool_value, rendered_value);
    case HV_STRING:
        return FormatValue(value.type, &value.string_value, rendered_value);
    default:
        error_message_key_ = "msg.unsupported_type";
        return ALGORITHM_RUN_ERROR;
    }
}

int HVFormat::FormatValue(int type, const void* data, std::string& rendered_value)
{
    rendered_value.clear();
    if (data == nullptr) {
        error_message_key_ = "msg.reference_empty";
        return ALGORITHM_RUN_ERROR;
    }

    switch (type) {
    case HV_INT:
        rendered_value = std::to_string(*static_cast<const int*>(data));
        return SUCCESS;
    case HV_LONG:
        rendered_value = std::to_string(*static_cast<const long*>(data));
        return SUCCESS;
    case HV_FLOAT:
        rendered_value = FormatFloatingFixed4(*static_cast<const float*>(data));
        return SUCCESS;
    case HV_DOUBLE:
        rendered_value = FormatFloatingFixed4(*static_cast<const double*>(data));
        return SUCCESS;
    case HV_BOOLEAN:
        rendered_value = *static_cast<const bool*>(data) ? "true" : "false";
        return SUCCESS;
    case HV_STRING:
        rendered_value = *static_cast<const std::string*>(data);
        return SUCCESS;
    default:
        error_message_key_ = "msg.unsupported_type";
        return ALGORITHM_RUN_ERROR;
    }
}

int HVFormat::Fail(int status, const std::string& message_key)
{
    execute_status_ = status;
    error_message_key_ = message_key;
    formatted_text_.clear();
    return status;
}

extern "C" __declspec(dllexport) NodeEngine* CreateInstance()
{
    return new HVFormat();
}

extern "C" __declspec(dllexport) std::string GetInstanceName()
{
    return "Format";
}

extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion()
{
    return NODE_ENGINE_ABI_VERSION;
}
