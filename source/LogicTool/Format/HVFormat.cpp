#include "HVFormat.h"

#include <chrono>
#include <fstream>
#include <sstream>

#include <json.hpp>

#include "HVI18n.h"

namespace {

constexpr int kSlot0LabelParamId = 0;
constexpr int kSlot1LabelParamId = 1;
constexpr int kSlot2LabelParamId = 2;
constexpr int kSegmentSeparatorParamId = 3;
constexpr int kOutputEndingParamId = 4;
constexpr int kInput0ParamId = 5;
constexpr int kInput1ParamId = 6;
constexpr int kInput2ParamId = 7;
constexpr int kInputSlotCount = 3;

const hvi18n::Dictionary kTexts = {
    { "algorithm.display", { "格式化", "Format" } },
    { "input.slot0_label.name", { "名称0", "Label 0" } },
    { "input.slot0_label.desc", { "第 0 个输入槽输出前缀", "Prefix text for input slot 0" } },
    { "input.slot1_label.name", { "名称1", "Label 1" } },
    { "input.slot1_label.desc", { "第 1 个输入槽输出前缀", "Prefix text for input slot 1" } },
    { "input.slot2_label.name", { "名称2", "Label 2" } },
    { "input.slot2_label.desc", { "第 2 个输入槽输出前缀", "Prefix text for input slot 2" } },
    { "input.segment_separator.name", { "分隔符", "Segment separator" } },
    { "input.segment_separator.desc", { "多条格式化结果之间的分隔符", "Separator between rendered segments" } },
    { "input.output_ending.name", { "结束符", "Output ending" } },
    { "input.output_ending.desc", { "最终输出追加的结束符", "Ending text appended to the final output" } },
    { "input.input0.name", { "输入0", "Input 0" } },
    { "input.input0.desc", { "绑定到第 0 个格式化槽位的数据", "Data bound to formatting slot 0" } },
    { "input.input1.name", { "输入1", "Input 1" } },
    { "input.input1.desc", { "绑定到第 1 个格式化槽位的数据", "Data bound to formatting slot 1" } },
    { "input.input2.name", { "输入2", "Input 2" } },
    { "input.input2.desc", { "绑定到第 2 个格式化槽位的数据", "Data bound to formatting slot 2" } },
    { "output.formatted_text.name", { "格式化结果", "Formatted text" } },
    { "output.status.name", { "运行状态", "Execute status" } },
    { "msg.host_services_missing", { "宿主服务不可用", "Host services are unavailable" } },
    { "msg.input_not_bound", { "存在已启用但未绑定有效数据的格式化槽位", "An enabled formatting slot does not have a valid bound value" } },
    { "msg.unsupported_type", { "格式化节点暂不支持该输入类型", "Format node does not support this input type yet" } },
    { "msg.run_success", { "格式化成功", "Formatting succeeded" } }
};

std::string Tr(int language, const std::string& key)
{
    return hvi18n::Translate(kTexts, key, language);
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

    std::vector<std::string> rendered_segments;
    rendered_segments.reserve(kInputSlotCount);
    for (int slot_index = 0; slot_index < kInputSlotCount; ++slot_index) {
        std::string rendered_text;
        const int render_ret = RenderSlot(slot_index, rendered_text);
        if (render_ret != SUCCESS) {
            return Fail(render_ret, error_message_key_);
        }
        if (!rendered_text.empty()) {
            rendered_segments.push_back(rendered_text);
        }
    }

    for (size_t i = 0; i < rendered_segments.size(); ++i) {
        if (i > 0) {
            formatted_text_ += segment_separator_;
        }
        formatted_text_ += rendered_segments[i];
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
    return {
        &slot0_label_,
        &slot1_label_,
        &slot2_label_,
        &segment_separator_,
        &output_ending_,
        nullptr,
        nullptr,
        nullptr
    };
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
    return { HV_STRING, HV_STRING, HV_STRING, HV_STRING, HV_STRING, HV_ANYINPUT, HV_ANYINPUT, HV_ANYINPUT };
}

std::vector<int> HVFormat::get_algorithm_output_params_type()
{
    return { HV_STRING, HV_INT };
}

std::vector<std::string> HVFormat::get_algorithm_input_params_name()
{
    return {
        Tr(language_, "input.slot0_label.name"),
        Tr(language_, "input.slot1_label.name"),
        Tr(language_, "input.slot2_label.name"),
        Tr(language_, "input.segment_separator.name"),
        Tr(language_, "input.output_ending.name"),
        Tr(language_, "input.input0.name"),
        Tr(language_, "input.input1.name"),
        Tr(language_, "input.input2.name")
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
    return { false, false, false, false, false, true, true, true };
}

std::vector<ParamMetadata> HVFormat::get_algorithm_input_params_metadata()
{
    std::vector<ParamMetadata> metadata_list(8);

    metadata_list[0].param_name = "slot0_label";
    metadata_list[0].param_description = Tr(language_, "input.slot0_label.desc");
    metadata_list[0].param_type = HV_STRING;

    metadata_list[1].param_name = "slot1_label";
    metadata_list[1].param_description = Tr(language_, "input.slot1_label.desc");
    metadata_list[1].param_type = HV_STRING;

    metadata_list[2].param_name = "slot2_label";
    metadata_list[2].param_description = Tr(language_, "input.slot2_label.desc");
    metadata_list[2].param_type = HV_STRING;

    metadata_list[3].param_name = "segment_separator";
    metadata_list[3].param_description = Tr(language_, "input.segment_separator.desc");
    metadata_list[3].param_type = HV_STRING;

    metadata_list[4].param_name = "output_ending";
    metadata_list[4].param_description = Tr(language_, "input.output_ending.desc");
    metadata_list[4].param_type = HV_STRING;

    metadata_list[5].param_name = "input0";
    metadata_list[5].param_description = Tr(language_, "input.input0.desc");
    metadata_list[5].param_type = HV_ANYINPUT;

    metadata_list[6].param_name = "input1";
    metadata_list[6].param_description = Tr(language_, "input.input1.desc");
    metadata_list[6].param_type = HV_ANYINPUT;

    metadata_list[7].param_name = "input2";
    metadata_list[7].param_description = Tr(language_, "input.input2.desc");
    metadata_list[7].param_type = HV_ANYINPUT;

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
        params_json["slot0_label"] = slot0_label_;
        params_json["slot1_label"] = slot1_label_;
        params_json["slot2_label"] = slot2_label_;
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
        slot0_label_ = params_json.value("slot0_label", std::string());
        slot1_label_ = params_json.value("slot1_label", std::string());
        slot2_label_ = params_json.value("slot2_label", std::string());
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
    case kSlot0LabelParamId:
        if (value_ptr == nullptr) {
            return INVALID_PARAMS_NUM;
        }
        slot0_label_ = *static_cast<std::string*>(value_ptr);
        return SUCCESS;
    case kSlot1LabelParamId:
        if (value_ptr == nullptr) {
            return INVALID_PARAMS_NUM;
        }
        slot1_label_ = *static_cast<std::string*>(value_ptr);
        return SUCCESS;
    case kSlot2LabelParamId:
        if (value_ptr == nullptr) {
            return INVALID_PARAMS_NUM;
        }
        slot2_label_ = *static_cast<std::string*>(value_ptr);
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

int HVFormat::RenderSlot(int slot_index, std::string& rendered_text)
{
    rendered_text.clear();

    const std::string* label = nullptr;
    switch (slot_index) {
    case 0:
        label = &slot0_label_;
        break;
    case 1:
        label = &slot1_label_;
        break;
    case 2:
        label = &slot2_label_;
        break;
    default:
        return INVALID_PARAMS_NUM;
    }

    if (label == nullptr || label->empty()) {
        return SUCCESS;
    }

    NodeHostDataView data_view;
    const int read_ret = ReadInputSlotValue(slot_index, data_view);
    if (read_ret != SUCCESS) {
        error_message_key_ = "msg.input_not_bound";
        return ALGORITHM_RUN_ERROR;
    }

    std::string rendered_value;
    const int format_ret = FormatSlotValue(data_view, rendered_value);
    if (format_ret != SUCCESS) {
        return format_ret;
    }

    rendered_text = *label + rendered_value;
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

int HVFormat::FormatSlotValue(const NodeHostDataView& data_view, std::string& rendered_value)
{
    rendered_value.clear();
    switch (data_view.type) {
    case HV_INT:
        rendered_value = std::to_string(*static_cast<const int*>(data_view.data));
        return SUCCESS;
    case HV_LONG:
        rendered_value = std::to_string(*static_cast<const long*>(data_view.data));
        return SUCCESS;
    case HV_FLOAT:
        rendered_value = FormatFloatingDefault(*static_cast<const float*>(data_view.data));
        return SUCCESS;
    case HV_DOUBLE:
        rendered_value = FormatFloatingDefault(*static_cast<const double*>(data_view.data));
        return SUCCESS;
    case HV_BOOLEAN:
        rendered_value = *static_cast<const bool*>(data_view.data) ? "true" : "false";
        return SUCCESS;
    case HV_STRING:
        rendered_value = *static_cast<const std::string*>(data_view.data);
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
