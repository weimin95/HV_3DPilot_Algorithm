#include "HVNodeReferenceResolver.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <vector>

namespace hvref {
namespace {

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

std::vector<std::string> SplitByDot(const std::string& text)
{
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= text.size()) {
        const size_t dot = text.find('.', start);
        if (dot == std::string::npos) {
            parts.push_back(text.substr(start));
            break;
        }
        parts.push_back(text.substr(start, dot - start));
        start = dot + 1;
    }
    return parts;
}

bool IsGlobalVariableTokenHead(const std::string& token_head)
{
    // 兼容前端可能按 UTF-8 或本机 ANSI/GBK 传入的“全局变量”前缀。
    static const std::string kGlobalVariableUtf8 =
        "\xE5\x85\xA8\xE5\xB1\x80\xE5\x8F\x98\xE9\x87\x8F";
    static const std::string kGlobalVariableGbk = "\xC8\xAB\xBE\xD6\xB1\xE4\xC1\xBF";
    return token_head == kGlobalVariableUtf8 || token_head == kGlobalVariableGbk;
}

const void* PointerFromHostValue(const NodeHostValue& value)
{
    switch (value.type) {
    case HV_INT:
        return &value.int_value;
    case HV_LONG:
        return &value.long_value;
    case HV_FLOAT:
        return &value.float_value;
    case HV_DOUBLE:
        return &value.double_value;
    case HV_BOOLEAN:
        return &value.bool_value;
    case HV_STRING:
        return &value.string_value;
    default:
        return nullptr;
    }
}

}  // namespace

std::string TrimCopy(const std::string& text)
{
    const size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
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

std::string FormatFixed4(double value)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4) << value;
    return oss.str();
}

ResolveError ParseReferenceToken(const std::string& token_text, ParsedReferenceToken& out_token)
{
    out_token = ParsedReferenceToken();

    const std::vector<std::string> parts = SplitByDot(token_text);
    if (parts.empty()) {
        return ResolveError::InvalidSyntax;
    }

    if (IsGlobalVariableTokenHead(parts[0])) {
        if (parts.size() != 4) {
            return ResolveError::InvalidSyntax;
        }

        int global_var_display_id = -1;
        if (!TryParseIntStrict(parts[1], global_var_display_id) || global_var_display_id <= 0) {
            return ResolveError::InvalidSyntax;
        }

        out_token.kind = ReferenceKind::GlobalVariable;
        out_token.global_var_token_id = global_var_display_id;
        out_token.global_var_name = TrimCopy(parts[2]);
        out_token.expected_type = MapTypeNameToHvType(parts[3]);
        if (out_token.global_var_name.empty()) {
            return ResolveError::InvalidSyntax;
        }
        return out_token.expected_type < 0 ? ResolveError::InvalidTypeTag : ResolveError::None;
    }

    if (parts.size() == 3) {
        if (!TryParseIntStrict(parts[0], out_token.node_id) ||
            !TryParseIntStrict(parts[1], out_token.result_id)) {
            return ResolveError::InvalidSyntax;
        }
        out_token.kind = ReferenceKind::NodeResult;
        out_token.expected_type = MapTypeNameToHvType(parts[2]);
        return out_token.expected_type < 0 ? ResolveError::InvalidTypeTag : ResolveError::None;
    }

    if (parts.size() == 5) {
        // 兼容旧 Format 语法：<node_id.node_alias.result_id.output_name.type>。
        if (!TryParseIntStrict(parts[0], out_token.node_id) ||
            !TryParseIntStrict(parts[2], out_token.result_id)) {
            return ResolveError::InvalidSyntax;
        }
        out_token.kind = ReferenceKind::NodeResult;
        out_token.expected_type = MapTypeNameToHvType(parts[4]);
        return out_token.expected_type < 0 ? ResolveError::InvalidTypeTag : ResolveError::None;
    }

    return ResolveError::InvalidSyntax;
}

ResolveError ResolveReferenceValue(
    NodeHostServices& host_services,
    const ParsedReferenceToken& token,
    ResolvedReferenceValue& out_value)
{
    out_value = ResolvedReferenceValue();

    if (token.kind == ReferenceKind::GlobalVariable) {
        const std::vector<NodeHostGlobalVariableInfo> global_infos =
            host_services.get_global_variable_infos();
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
            return ResolveError::ResolveFailed;
        }
        if (info_it->var_type != token.expected_type) {
            return ResolveError::TypeMismatch;
        }

        const int ret = host_services.get_global_value(info_it->var_id, out_value.owned_global_value);
        if (ret == GLOBAL_VAR_NOT_EXIST) {
            return ResolveError::ResolveFailed;
        }
        if (ret != SUCCESS || !out_value.owned_global_value.has_value) {
            return ResolveError::EmptyValue;
        }
        if (out_value.owned_global_value.type != token.expected_type) {
            return ResolveError::TypeMismatch;
        }

        out_value.type = out_value.owned_global_value.type;
        out_value.has_value = out_value.owned_global_value.has_value;
        out_value.data = PointerFromHostValue(out_value.owned_global_value);
        return out_value.data == nullptr ? ResolveError::UnsupportedType : ResolveError::None;
    }

    NodeHostDataView data_view;
    const int ret = host_services.get_upstream_result_by_id(
        token.node_id,
        token.result_id,
        std::string(),
        std::string(),
        token.expected_type,
        data_view);
    if (ret == UPSTREAM_RESULT_NODE_NOT_FOUND) {
        return ResolveError::NodeNotFound;
    }
    if (ret == UPSTREAM_RESULT_NOT_REACHABLE) {
        return ResolveError::NotReachable;
    }
    if (ret == UPSTREAM_RESULT_OUTPUT_NOT_FOUND) {
        return ResolveError::OutputNotFound;
    }
    if (ret == UPSTREAM_RESULT_UNAVAILABLE) {
        return ResolveError::Unavailable;
    }
    if (ret == UPSTREAM_RESULT_TYPE_NOT_SUPPORTED) {
        return ResolveError::TypeMismatch;
    }
    if (ret != SUCCESS) {
        return ResolveError::ResolveFailed;
    }
    if (!data_view.has_value || data_view.data == nullptr) {
        return ResolveError::EmptyValue;
    }

    out_value.type = data_view.type;
    out_value.has_value = data_view.has_value;
    out_value.data = data_view.data;
    return ResolveError::None;
}

ResolveError FormatReferenceValue(const ResolvedReferenceValue& value, std::string& out_text)
{
    out_text.clear();
    if (!value.has_value || value.data == nullptr) {
        return ResolveError::EmptyValue;
    }

    switch (value.type) {
    case HV_INT:
        out_text = std::to_string(*static_cast<const int*>(value.data));
        return ResolveError::None;
    case HV_LONG:
        out_text = std::to_string(*static_cast<const long*>(value.data));
        return ResolveError::None;
    case HV_FLOAT:
        out_text = FormatFixed4(*static_cast<const float*>(value.data));
        return ResolveError::None;
    case HV_DOUBLE:
        out_text = FormatFixed4(*static_cast<const double*>(value.data));
        return ResolveError::None;
    case HV_BOOLEAN:
        out_text = *static_cast<const bool*>(value.data) ? "true" : "false";
        return ResolveError::None;
    case HV_STRING:
        out_text = *static_cast<const std::string*>(value.data);
        return ResolveError::None;
    default:
        return ResolveError::UnsupportedType;
    }
}

ResolveError ConvertReferenceValueToDouble(const ResolvedReferenceValue& value, double& out_value)
{
    out_value = 0.0;
    if (!value.has_value || value.data == nullptr) {
        return ResolveError::EmptyValue;
    }

    switch (value.type) {
    case HV_INT:
        out_value = static_cast<double>(*static_cast<const int*>(value.data));
        return ResolveError::None;
    case HV_LONG:
        out_value = static_cast<double>(*static_cast<const long*>(value.data));
        return ResolveError::None;
    case HV_FLOAT:
        out_value = static_cast<double>(*static_cast<const float*>(value.data));
        return ResolveError::None;
    case HV_DOUBLE:
        out_value = *static_cast<const double*>(value.data);
        return ResolveError::None;
    case HV_BOOLEAN:
        out_value = *static_cast<const bool*>(value.data) ? 1.0 : 0.0;
        return ResolveError::None;
    default:
        return ResolveError::UnsupportedType;
    }
}

}  // namespace hvref
