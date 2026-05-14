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

std::string StripTokenFormatSpec(const std::string& token_text, std::string& out_format_spec)
{
    const auto paren = token_text.rfind("(%");
    if (paren != std::string::npos && !token_text.empty() && token_text.back() == ')') {
        // 提取 %fmt（包含 %），去除首尾空白。
        out_format_spec = TrimCopy(token_text.substr(paren + 1, token_text.size() - paren - 2));
        return token_text.substr(0, paren);
    }
    out_format_spec.clear();
    return token_text;
}

struct FormatSpec {
    char type_char = 0;
    int width = 0;
    int precision = -1;
    bool zero_pad = false;
};

bool ParseFormatSpec(const std::string& spec, FormatSpec& out)
{
    if (spec.size() < 2 || spec[0] != '%') {
        return false;
    }

    size_t pos = 1;

    if (pos < spec.size() && spec[pos] == '0') {
        out.zero_pad = true;
        ++pos;
    }

    out.width = 0;
    while (pos < spec.size() && std::isdigit(static_cast<unsigned char>(spec[pos]))) {
        out.width = out.width * 10 + (spec[pos] - '0');
        ++pos;
    }

    if (pos < spec.size() && spec[pos] == '.') {
        ++pos;
        out.precision = 0;
        while (pos < spec.size() && std::isdigit(static_cast<unsigned char>(spec[pos]))) {
            out.precision = out.precision * 10 + (spec[pos] - '0');
            ++pos;
        }
    }

    if (pos + 1 != spec.size()) {
        return false;
    }
    out.type_char = spec[pos];
    return true;
}

std::string ApplyFormatSpec(int hv_type, const void* data, const std::string& format_spec)
{
    FormatSpec spec;
    if (!ParseFormatSpec(format_spec, spec)) {
        return {};
    }

    switch (spec.type_char) {
    case 'd': {
        long long value = 0;
        switch (hv_type) {
        case HV_INT:    value = *static_cast<const int*>(data); break;
        case HV_LONG:   value = *static_cast<const long*>(data); break;
        default: return {};
        }
        std::ostringstream oss;
        if (spec.width > 0) {
            oss << std::setfill('0') << std::setw(spec.width);
        }
        oss << value;
        return oss.str();
    }
    case 'f': {
        double value = 0.0;
        switch (hv_type) {
        case HV_INT:    value = static_cast<double>(*static_cast<const int*>(data)); break;
        case HV_LONG:   value = static_cast<double>(*static_cast<const long*>(data)); break;
        case HV_FLOAT:  value = static_cast<double>(*static_cast<const float*>(data)); break;
        case HV_DOUBLE: value = *static_cast<const double*>(data); break;
        default: return {};
        }
        const int prec = (spec.precision >= 0) ? spec.precision : 6;
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(prec);
        if (spec.width > 0) {
            oss << std::setfill('0') << std::setw(spec.width);
        }
        oss << value;
        return oss.str();
    }
    case 's': {
        if (hv_type == HV_STRING) {
            return *static_cast<const std::string*>(data);
        }
        return {};
    }
    default:
        return {};
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
    if (trimmed == "intList") {
        return HV_INT_LIST;
    }
    if (trimmed == "longList") {
        return HV_LONG_LIST;
    }
    if (trimmed == "floatList") {
        return HV_FLOAT_LIST;
    }
    if (trimmed == "doubleList") {
        return HV_DOUBLE_LIST;
    }
    if (trimmed == "boolList") {
        return HV_BOOLEAN_LIST;
    }
    if (trimmed == "stringList") {
        return HV_STRING_LIST;
    }
    return -1;
}

// 从列表类型推导单值类型，非列表类型返回 -1
static int SingleTypeFromListType(int list_type)
{
    switch (list_type) {
    case HV_INT_LIST:     return HV_INT;
    case HV_LONG_LIST:    return HV_LONG;
    case HV_FLOAT_LIST:   return HV_FLOAT;
    case HV_DOUBLE_LIST:  return HV_DOUBLE;
    case HV_BOOLEAN_LIST: return HV_BOOLEAN;
    case HV_STRING_LIST:  return HV_STRING;
    default:              return -1;
    }
}

// 检查是否为列表类型
static bool IsListType(int hv_type)
{
    return SingleTypeFromListType(hv_type) >= 0;
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

    // 先把末尾的 (%fmt) 提取出来，避免格式串内的 '.' 干扰按点拆分。
    const std::string clean_text = StripTokenFormatSpec(token_text, out_token.format_spec);
    const std::vector<std::string> parts = SplitByDot(clean_text);
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

    // 列表项标记提取：从末尾类型段中解析 typeName[N]
    auto extract_item_index = [](std::string& type_part) -> int {
        const auto bracket = type_part.rfind('[');
        if (bracket == std::string::npos || type_part.empty() || type_part.back() != ']') {
            return -1;
        }
        const std::string index_text = type_part.substr(bracket + 1, type_part.size() - bracket - 2);
        int idx = -1;
        if (!TryParseIntStrict(index_text, idx) || idx < 0) {
            return -1;
        }
        type_part = type_part.substr(0, bracket);
        return idx;
    };

    if (parts.size() == 3) {
        if (!TryParseIntStrict(parts[0], out_token.node_id) ||
            !TryParseIntStrict(parts[1], out_token.result_id)) {
            return ResolveError::InvalidSyntax;
        }
        std::string type_part = parts[2];
        out_token.item_index = extract_item_index(type_part);
        out_token.kind = ReferenceKind::NodeResult;
        out_token.expected_type = MapTypeNameToHvType(type_part);
        return out_token.expected_type < 0 ? ResolveError::InvalidTypeTag : ResolveError::None;
    }

    if (parts.size() == 5) {
        // 兼容旧 Format 语法：<node_id.node_alias.result_id.output_name.type>。
        if (!TryParseIntStrict(parts[0], out_token.node_id) ||
            !TryParseIntStrict(parts[2], out_token.result_id)) {
            return ResolveError::InvalidSyntax;
        }
        std::string type_part = parts[4];
        out_token.item_index = extract_item_index(type_part);
        out_token.kind = ReferenceKind::NodeResult;
        out_token.expected_type = MapTypeNameToHvType(type_part);
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

    // 对列表项引用，先按列表类型请求上游数据，再按索引提取单值。
    const int request_type = (token.item_index >= 0) ? token.expected_type : token.expected_type;

    NodeHostDataView data_view;
    const int ret = host_services.get_upstream_result_by_id(
        token.node_id,
        token.result_id,
        std::string(),
        std::string(),
        request_type,
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

    if (token.item_index < 0 || !IsListType(data_view.type)) {
        out_value.type = data_view.type;
        out_value.has_value = data_view.has_value;
        out_value.data = data_view.data;
        return ResolveError::None;
    }

    // 列表项提取：从列表数据中取出指定索引的元素，转为单值。
    const int item_type = SingleTypeFromListType(data_view.type);
    if (item_type < 0) {
        return ResolveError::UnsupportedType;
    }

    auto extract_list_item = [&](const void* list_data, int index) -> const void* {
        switch (data_view.type) {
        case HV_INT_LIST: {
            const auto* list = static_cast<const HVIntList*>(list_data);
            if (index < 0 || index >= static_cast<int>(list->values.size())) return nullptr;
            return &list->values[index];
        }
        case HV_LONG_LIST: {
            const auto* list = static_cast<const HVLongList*>(list_data);
            if (index < 0 || index >= static_cast<int>(list->values.size())) return nullptr;
            return &list->values[index];
        }
        case HV_FLOAT_LIST: {
            const auto* list = static_cast<const HVFloatList*>(list_data);
            if (index < 0 || index >= static_cast<int>(list->values.size())) return nullptr;
            return &list->values[index];
        }
        case HV_DOUBLE_LIST: {
            const auto* list = static_cast<const HVDoubleList*>(list_data);
            if (index < 0 || index >= static_cast<int>(list->values.size())) return nullptr;
            return &list->values[index];
        }
        case HV_BOOLEAN_LIST: {
            const auto* list = static_cast<const HVBooleanList*>(list_data);
            if (index < 0 || index >= static_cast<int>(list->values.size())) return nullptr;
            return &list->values[index];
        }
        case HV_STRING_LIST: {
            const auto* list = static_cast<const HVStringList*>(list_data);
            if (index < 0 || index >= static_cast<int>(list->values.size())) return nullptr;
            out_value.owned_item_string = std::string(list->values[index].c_str());
            return &out_value.owned_item_string;
        }
        default:
            return nullptr;
        }
    };

    const void* item_ptr = extract_list_item(data_view.data, token.item_index);
    if (item_ptr == nullptr) {
        return ResolveError::EmptyValue;
    }

    out_value.type = item_type;
    out_value.has_value = true;
    out_value.data = item_ptr;
    return ResolveError::None;
}

ResolveError FormatReferenceValue(const ResolvedReferenceValue& value, std::string& out_text, const std::string& format_spec)
{
    out_text.clear();
    if (!value.has_value || value.data == nullptr) {
        return ResolveError::EmptyValue;
    }

    if (!format_spec.empty()) {
        const std::string formatted = ApplyFormatSpec(value.type, value.data, format_spec);
        if (!formatted.empty()) {
            out_text = formatted;
            return ResolveError::None;
        }
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

ResolveError FormatListReferenceValue(const ResolvedReferenceValue& value, std::string& out_text, const std::string& separator, const std::string& format_spec)
{
    out_text.clear();
    if (!value.has_value || value.data == nullptr) {
        return ResolveError::EmptyValue;
    }
    if (!IsListType(value.type)) {
        return FormatReferenceValue(value, out_text, format_spec);
    }

    auto format_single_item = [](int item_type, const void* item_data, const std::string& fmt_spec) -> std::string {
        if (item_data == nullptr) {
            return {};
        }
        if (!fmt_spec.empty()) {
            const std::string formatted = ApplyFormatSpec(item_type, item_data, fmt_spec);
            if (!formatted.empty()) {
                return formatted;
            }
        }
        switch (item_type) {
        case HV_INT:    return std::to_string(*static_cast<const int*>(item_data));
        case HV_LONG:   return std::to_string(*static_cast<const long*>(item_data));
        case HV_FLOAT:  return FormatFixed4(*static_cast<const float*>(item_data));
        case HV_DOUBLE: return FormatFixed4(*static_cast<const double*>(item_data));
        case HV_BOOLEAN:return *static_cast<const bool*>(item_data) ? "true" : "false";
        case HV_STRING: return *static_cast<const std::string*>(item_data);
        default:        return {};
        }
    };

    const int item_type = SingleTypeFromListType(value.type);

    // 用于浮点/整数型列表的模板 lambda
    auto format_value_list = [&](auto* typed_data) {
        bool first = true;
        for (size_t i = 0; i < typed_data->values.size(); ++i) {
            const void* item_ptr = static_cast<const void*>(&typed_data->values[i]);
            const std::string item_str = format_single_item(item_type, item_ptr, format_spec);
            if (!first) {
                out_text += separator;
            }
            out_text += item_str;
            first = false;
        }
    };

    switch (value.type) {
    case HV_INT_LIST:     format_value_list(static_cast<const HVIntList*>(value.data));     break;
    case HV_LONG_LIST:    format_value_list(static_cast<const HVLongList*>(value.data));    break;
    case HV_FLOAT_LIST:   format_value_list(static_cast<const HVFloatList*>(value.data));   break;
    case HV_DOUBLE_LIST:  format_value_list(static_cast<const HVDoubleList*>(value.data));  break;
    case HV_BOOLEAN_LIST: format_value_list(static_cast<const HVBooleanList*>(value.data)); break;
    case HV_STRING_LIST: {
        const auto* list = static_cast<const HVStringList*>(value.data);
        bool first = true;
        for (size_t i = 0; i < list->values.size(); ++i) {
            const std::string item_str_value = std::string(list->values[i].c_str());
            const void* item_ptr = static_cast<const void*>(&item_str_value);
            const std::string item_str = format_single_item(HV_STRING, item_ptr, format_spec);
            if (!first) {
                out_text += separator;
            }
            out_text += item_str;
            first = false;
        }
        break;
    }
    default:
        return ResolveError::UnsupportedType;
    }
    return ResolveError::None;
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
