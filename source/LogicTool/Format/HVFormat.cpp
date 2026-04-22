#include "HVFormat.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>

#include <json.hpp>

#include "HVI18n.h"

namespace {

constexpr int kRulesJsonParamId = 0;
constexpr int kSegmentSeparatorParamId = 1;
constexpr int kOutputEndingParamId = 2;
constexpr int kInput0ParamId = 3;
constexpr int kInput1ParamId = 4;
constexpr int kInput2ParamId = 5;
constexpr int kInputSlotCount = 3;

const hvi18n::Dictionary kTexts = {
    { "algorithm.display", { "格式化", "Format" } },
    { "input.rules_json.name", { "规则配置", "Rules config" } },
    { "input.rules_json.desc", { "格式化规则 JSON 字符串", "Formatting rules JSON string" } },
    { "input.segment_separator.name", { "分隔符", "Segment separator" } },
    { "input.segment_separator.desc", { "多条规则输出之间的分隔符", "Separator between rendered rows" } },
    { "input.output_ending.name", { "结束符", "Output ending" } },
    { "input.output_ending.desc", { "最终输出追加的结束符", "Ending text appended to the final output" } },
    { "input.input0.name", { "输入0", "Input 0" } },
    { "input.input0.desc", { "绑定到占位符 ${0} 的输入", "Input bound to placeholder ${0}" } },
    { "input.input1.name", { "输入1", "Input 1" } },
    { "input.input1.desc", { "绑定到占位符 ${1} 的输入", "Input bound to placeholder ${1}" } },
    { "input.input2.name", { "输入2", "Input 2" } },
    { "input.input2.desc", { "绑定到占位符 ${2} 的输入", "Input bound to placeholder ${2}" } },
    { "output.formatted_text.name", { "格式化结果", "Formatted text" } },
    { "output.status.name", { "运行状态", "Execute status" } },
    { "msg.host_services_missing", { "宿主服务不可用", "Host services are unavailable" } },
    { "msg.invalid_rules_json", { "规则配置 JSON 非法", "Rules config JSON is invalid" } },
    { "msg.invalid_rows_json", { "规则配置缺少字符串数组 rows", "Rules config must contain a string array rows" } },
    { "msg.invalid_placeholder", { "模板占位符语法非法", "Template placeholder syntax is invalid" } },
    { "msg.invalid_slot_index", { "模板引用了无效输入槽位", "Template references an invalid input slot" } },
    { "msg.input_not_bound", { "模板引用的输入槽位未绑定有效数据", "Template references an input slot without a valid bound value" } },
    { "msg.unsupported_type", { "格式化节点暂不支持该输入类型", "Format node does not support this input type yet" } },
    { "msg.invalid_format", { "占位符格式串非法", "Placeholder format string is invalid" } },
    { "msg.invalid_string_format", { "字符串和布尔值不支持数值格式串", "Strings and booleans do not support numeric format strings" } },
    { "msg.run_success", { "格式化成功", "Formatting succeeded" } }
};

std::string Tr(int language, const std::string& key)
{
    return hvi18n::Translate(kTexts, key, language);
}

bool ParseIntegerFormat(const std::string& format_text)
{
    if (format_text.empty()) {
        return true;
    }
    static const std::regex kIntegerPattern(R"(^%[0-9]*[di]$)");
    return std::regex_match(format_text, kIntegerPattern);
}

bool ParseFloatingFormat(const std::string& format_text)
{
    if (format_text.empty()) {
        return true;
    }
    static const std::regex kFloatPattern(R"(^%[0-9]*(?:\.[0-9]+)?f$)");
    return std::regex_match(format_text, kFloatPattern);
}

std::string FormatIntegerDefault(long long value)
{
    return std::to_string(value);
}

std::string FormatFloatingDefault(double value)
{
    std::ostringstream oss;
    oss << value;
    return oss.str();
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

    std::vector<std::string> rows;
    int parse_ret = ParseRowsFromJson(rows);
    if (parse_ret != SUCCESS) {
        return Fail(parse_ret, error_message_key_);
    }

    std::vector<std::string> rendered_rows;
    rendered_rows.reserve(rows.size());
    for (const auto& row : rows) {
        std::string rendered_row;
        const int render_ret = RenderRow(row, rendered_row);
        if (render_ret != SUCCESS) {
            return Fail(render_ret, error_message_key_);
        }
        if (!rendered_row.empty()) {
            rendered_rows.push_back(rendered_row);
        }
    }

    for (size_t i = 0; i < rendered_rows.size(); ++i) {
        if (i > 0) {
            formatted_text_ += segment_separator_;
        }
        formatted_text_ += rendered_rows[i];
    }
    formatted_text_ += output_ending_;

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
    return { &rules_json_, &segment_separator_, &output_ending_, nullptr, nullptr, nullptr };
}

std::vector<void*> HVFormat::get_algorithm_result()
{
    if (execute_status_ == SUCCESS) {
        return { &formatted_text_, &execute_status_ };
    }
    return { nullptr, &execute_status_ };
}

std::vector<int> HVFormat::get_algorithm_input_params_type()
{
    return { HV_STRING, HV_STRING, HV_STRING, HV_ANYINPUT, HV_ANYINPUT, HV_ANYINPUT };
}

std::vector<int> HVFormat::get_algorithm_output_params_type()
{
    return { HV_STRING, HV_INT };
}

std::vector<std::string> HVFormat::get_algorithm_input_params_name()
{
    return {
        Tr(language_, "input.rules_json.name"),
        Tr(language_, "input.segment_separator.name"),
        Tr(language_, "input.output_ending.name"),
        "input0",
        "input1",
        "input2"
    };
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
    return { false, false, false, true, true, true };
}

std::vector<ParamMetadata> HVFormat::get_algorithm_input_params_metadata()
{
    std::vector<ParamMetadata> metadata_list(6);

    metadata_list[0].param_name = "rules_json";
    metadata_list[0].param_description = Tr(language_, "input.rules_json.desc");
    metadata_list[0].param_type = HV_STRING;

    metadata_list[1].param_name = "segment_separator";
    metadata_list[1].param_description = Tr(language_, "input.segment_separator.desc");
    metadata_list[1].param_type = HV_STRING;

    metadata_list[2].param_name = "output_ending";
    metadata_list[2].param_description = Tr(language_, "input.output_ending.desc");
    metadata_list[2].param_type = HV_STRING;

    metadata_list[3].param_name = "input0";
    metadata_list[3].param_description = Tr(language_, "input.input0.desc");
    metadata_list[3].param_type = HV_ANYINPUT;

    metadata_list[4].param_name = "input1";
    metadata_list[4].param_description = Tr(language_, "input.input1.desc");
    metadata_list[4].param_type = HV_ANYINPUT;

    metadata_list[5].param_name = "input2";
    metadata_list[5].param_description = Tr(language_, "input.input2.desc");
    metadata_list[5].param_type = HV_ANYINPUT;

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
        params_json["rules_json"] = rules_json_;
        params_json["segment_separator"] = segment_separator_;
        params_json["output_ending"] = output_ending_;

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
        rules_json_ = params_json.value("rules_json", std::string());
        segment_separator_ = params_json.value("segment_separator", std::string(";"));
        output_ending_ = params_json.value("output_ending", std::string("\r\n"));
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
    switch (param_id) {
    case kRulesJsonParamId:
        if (value_ptr == nullptr) {
            return INVALID_PARAMS_NUM;
        }
        rules_json_ = *static_cast<std::string*>(value_ptr);
        return SUCCESS;
    case kSegmentSeparatorParamId:
        if (value_ptr == nullptr) {
            return INVALID_PARAMS_NUM;
        }
        segment_separator_ = *static_cast<std::string*>(value_ptr);
        return SUCCESS;
    case kOutputEndingParamId:
        if (value_ptr == nullptr) {
            return INVALID_PARAMS_NUM;
        }
        output_ending_ = *static_cast<std::string*>(value_ptr);
        return SUCCESS;
    case kInput0ParamId:
    case kInput1ParamId:
    case kInput2ParamId:
        return SUCCESS;
    default:
        return INVALID_PARAMS_NUM;
    }
}

int HVFormat::ParseRowsFromJson(std::vector<std::string>& rows)
{
    rows.clear();
    try {
        nlohmann::json root_json = nlohmann::json::parse(rules_json_.empty() ? "{\"rows\":[]}" : rules_json_);
        if (!root_json.is_object() || !root_json.contains("rows") || !root_json["rows"].is_array()) {
            error_message_key_ = "msg.invalid_rows_json";
            return ALGORITHM_RUN_ERROR;
        }

        for (const auto& row_json : root_json["rows"]) {
            if (!row_json.is_string()) {
                error_message_key_ = "msg.invalid_rows_json";
                return ALGORITHM_RUN_ERROR;
            }
            rows.push_back(row_json.get<std::string>());
        }
        return SUCCESS;
    }
    catch (...) {
        error_message_key_ = "msg.invalid_rules_json";
        return ALGORITHM_RUN_ERROR;
    }
}

int HVFormat::RenderRow(const std::string& row_template, std::string& rendered_row)
{
    rendered_row.clear();
    size_t cursor = 0;
    while (cursor < row_template.size()) {
        const size_t placeholder_begin = row_template.find("${", cursor);
        if (placeholder_begin == std::string::npos) {
            rendered_row.append(row_template.substr(cursor));
            return SUCCESS;
        }

        rendered_row.append(row_template.substr(cursor, placeholder_begin - cursor));
        const size_t placeholder_end = row_template.find('}', placeholder_begin + 2);
        if (placeholder_end == std::string::npos) {
            error_message_key_ = "msg.invalid_placeholder";
            return ALGORITHM_RUN_ERROR;
        }

        const std::string placeholder_body =
            row_template.substr(placeholder_begin + 2, placeholder_end - (placeholder_begin + 2));
        const size_t format_separator = placeholder_body.find('|');
        const std::string slot_text =
            format_separator == std::string::npos ? placeholder_body : placeholder_body.substr(0, format_separator);
        const std::string format_text =
            format_separator == std::string::npos ? std::string() : placeholder_body.substr(format_separator + 1);

        if (slot_text.size() != 1 || slot_text[0] < '0' || slot_text[0] >= '0' + kInputSlotCount) {
            error_message_key_ = "msg.invalid_slot_index";
            return ALGORITHM_RUN_ERROR;
        }

        NodeHostDataView data_view;
        const int slot_index = slot_text[0] - '0';
        const int read_ret = ReadInputSlotValue(slot_index, data_view);
        if (read_ret != SUCCESS) {
            error_message_key_ = "msg.input_not_bound";
            return ALGORITHM_RUN_ERROR;
        }

        std::string rendered_value;
        const int format_ret = FormatSlotValue(data_view, format_text, rendered_value);
        if (format_ret != SUCCESS) {
            return format_ret;
        }
        rendered_row.append(rendered_value);
        cursor = placeholder_end + 1;
    }

    return SUCCESS;
}

int HVFormat::ReadInputSlotValue(int slot_index, NodeHostDataView& data_view) const
{
    data_view = NodeHostDataView();
    if (host_services_ == nullptr) {
        return INSTANCE_NOT_EXIST;
    }

    const int ret = host_services_->get_current_input_data(kInput0ParamId + slot_index, data_view);
    if (ret != SUCCESS || !data_view.has_value || data_view.data == nullptr) {
        return PARAMS_BIND_ERROR;
    }
    return SUCCESS;
}

int HVFormat::FormatSlotValue(
    const NodeHostDataView& data_view,
    const std::string& format_text,
    std::string& rendered_value)
{
    rendered_value.clear();
    switch (data_view.type) {
    case HV_INT:
        return FormatIntegerValue(*static_cast<const int*>(data_view.data), format_text, rendered_value);
    case HV_LONG:
        return FormatIntegerValue(*static_cast<const long*>(data_view.data), format_text, rendered_value);
    case HV_FLOAT:
        return FormatFloatingValue(*static_cast<const float*>(data_view.data), format_text, rendered_value);
    case HV_DOUBLE:
        return FormatFloatingValue(*static_cast<const double*>(data_view.data), format_text, rendered_value);
    case HV_BOOLEAN:
        if (!format_text.empty()) {
            error_message_key_ = "msg.invalid_string_format";
            return ALGORITHM_RUN_ERROR;
        }
        rendered_value = *static_cast<const bool*>(data_view.data) ? "true" : "false";
        return SUCCESS;
    case HV_STRING:
        if (!format_text.empty()) {
            error_message_key_ = "msg.invalid_string_format";
            return ALGORITHM_RUN_ERROR;
        }
        rendered_value = *static_cast<const std::string*>(data_view.data);
        return SUCCESS;
    default:
        error_message_key_ = "msg.unsupported_type";
        return ALGORITHM_RUN_ERROR;
    }
}

int HVFormat::FormatIntegerValue(long long value, const std::string& format_text, std::string& rendered_value)
{
    if (format_text.empty()) {
        rendered_value = FormatIntegerDefault(value);
        return SUCCESS;
    }
    if (!ParseIntegerFormat(format_text)) {
        error_message_key_ = "msg.invalid_format";
        return ALGORITHM_RUN_ERROR;
    }

    char buffer[128] = { 0 };
    const int written = std::snprintf(buffer, sizeof(buffer), format_text.c_str(), static_cast<int>(value));
    if (written < 0) {
        error_message_key_ = "msg.invalid_format";
        return ALGORITHM_RUN_ERROR;
    }
    rendered_value.assign(buffer, static_cast<size_t>(written));
    return SUCCESS;
}

int HVFormat::FormatFloatingValue(double value, const std::string& format_text, std::string& rendered_value)
{
    if (format_text.empty()) {
        rendered_value = FormatFloatingDefault(value);
        return SUCCESS;
    }
    if (!ParseFloatingFormat(format_text)) {
        error_message_key_ = "msg.invalid_format";
        return ALGORITHM_RUN_ERROR;
    }

    char buffer[128] = { 0 };
    const int written = std::snprintf(buffer, sizeof(buffer), format_text.c_str(), value);
    if (written < 0) {
        error_message_key_ = "msg.invalid_format";
        return ALGORITHM_RUN_ERROR;
    }
    rendered_value.assign(buffer, static_cast<size_t>(written));
    return SUCCESS;
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
