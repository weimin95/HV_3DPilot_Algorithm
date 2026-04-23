#include "HVSendData.h"

#include <chrono>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "HVI18n.h"

namespace cp_loader {

static HMODULE hModule = nullptr;

using fn_get_device_count = int(*)();
using fn_get_device_list = int(*)(char*, int);
using fn_send_string = int(*)(const char*, const char*);

static fn_get_device_count pGetDeviceCount = nullptr;
static fn_get_device_list pGetDeviceList = nullptr;
static fn_send_string pSendString = nullptr;

static bool EnsureLoaded()
{
    if (hModule != nullptr) {
        return true;
    }

    hModule = ::LoadLibraryA("CommunicatePlugin.dll");
    if (hModule == nullptr) {
        return false;
    }

    pGetDeviceCount = reinterpret_cast<fn_get_device_count>(::GetProcAddress(hModule, "cp_get_device_count"));
    pGetDeviceList = reinterpret_cast<fn_get_device_list>(::GetProcAddress(hModule, "cp_get_device_list"));
    pSendString = reinterpret_cast<fn_send_string>(::GetProcAddress(hModule, "cp_send_string"));
    return pGetDeviceCount != nullptr && pGetDeviceList != nullptr && pSendString != nullptr;
}

static std::vector<std::string> GetDeviceList()
{
    std::vector<std::string> devices;
    if (!EnsureLoaded() || pGetDeviceCount == nullptr || pGetDeviceList == nullptr) {
        return devices;
    }

    const int count = pGetDeviceCount();
    if (count <= 0) {
        return devices;
    }

    std::vector<char> buffer(count * 256, '\0');
    const int written = pGetDeviceList(buffer.data(), static_cast<int>(buffer.size()));
    const char* cursor = buffer.data();
    for (int i = 0; i < written; ++i) {
        std::string device_id(cursor);
        cursor += device_id.size() + 1;
        if (!device_id.empty()) {
            devices.push_back(device_id);
        }
    }
    return devices;
}

static int SendString(const std::string& device_id, const std::string& payload)
{
    if (!EnsureLoaded() || pSendString == nullptr) {
        return -1;
    }
    return pSendString(device_id.c_str(), payload.c_str());
}

}  // namespace cp_loader

namespace {

constexpr int kSendTargetParamId = 0;
constexpr int kSelectVariableParamId = 1;
constexpr int kSelectCommDeviceParamId = 2;
constexpr int kDataSourceParamId = 3;

const hvi18n::Dictionary kTexts = {
    { "algorithm.display", { "发送数据", "SendData" } },
    { "input.send_target.name", { "发送目标", "Send target" } },
    { "input.select_variable.name", { "选择变量", "Select variable" } },
    { "input.select_comm_device.name", { "通信设备", "Communication device" } },
    { "input.data_source.name", { "数据来源", "Data source" } },
    { "output.status.name", { "运行状态", "Execute status" } },
    { "option.global_variable", { "全局变量", "Global variable" } },
    { "option.communication_device", { "通信设备", "Communication device" } },
    { "msg.send_data_failed", { "发送数据失败", "Send data failed" } },
    { "msg.send_data_success", { "发送数据成功", "Send data success" } }
};

std::string Tr(int language, const std::string& key)
{
    return hvi18n::Translate(kTexts, key, language);
}

}  // namespace

HVSendData::HVSendData()
{
    send_target_
        .SetSchemaName("send_target")
        .SetStorageKey("send_target")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.send_target.name"); })
        .SetDescriptionResolver([]() { return std::string(); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_BASIC)
        .SetMetadataCustomizer([this](ParamMetadata& metadata) {
            metadata.constraint_type = CONSTRAINT_OPTIONS;
            metadata.options_constraint = OptionsConstraint();
            metadata.options_constraint.AddOption(0, Tr(current_language(), "option.global_variable"));
            metadata.options_constraint.AddOption(1, Tr(current_language(), "option.communication_device"));
        });

    select_variable_
        .SetSchemaName("select_variable")
        .SetStorageKey("select_variable")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.select_variable.name"); })
        .SetDescriptionResolver([]() { return std::string(); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_BASIC)
        .SetMetadataCustomizer([this](ParamMetadata& metadata) {
            metadata.constraint_type = CONSTRAINT_OPTIONS;
            metadata.options_constraint = OptionsConstraint();
            if (host_services() != nullptr) {
                const auto variable_infos = host_services()->get_global_variable_infos_by_type(HV_STRING);
                for (const auto& info : variable_infos) {
                    metadata.options_constraint.AddOption(info.var_id, info.var_name);
                }
            }
            metadata.dependencies.clear();
            metadata.dependencies.push_back(ParamDependency(
                kSendTargetParamId,
                DEPENDS_ON_EQUALS,
                { "0" }));
        });

    select_comm_device_
        .SetSchemaName("select_comm_device")
        .SetStorageKey("select_comm_device")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.select_comm_device.name"); })
        .SetDescriptionResolver([]() { return std::string(); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_BASIC)
        .SetMetadataCustomizer([](ParamMetadata& metadata) {
            metadata.constraint_type = CONSTRAINT_OPTIONS;
            metadata.options_constraint = OptionsConstraint();
            const auto devices = cp_loader::GetDeviceList();
            for (int i = 0; i < static_cast<int>(devices.size()); ++i) {
                metadata.options_constraint.AddOption(i, devices[i]);
            }
            metadata.dependencies.clear();
            metadata.dependencies.push_back(ParamDependency(
                kSendTargetParamId,
                DEPENDS_ON_EQUALS,
                { "1" }));
        });

    data_source_binding_
        .SetSchemaName("data_source")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.data_source.name"); })
        .SetDescriptionResolver([]() { return std::string(); })
        .SetTypeOverride(HV_ANYINPUT)
        .SetBindable(true)
        .SetEditable(false)
        .SetPersist(false)
        .SetExposeCurrentValue(false)
        .SetParamGroup(PARAM_GROUP_BASIC);

    execute_status_output_
        .SetSchemaName("execute_status")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "output.status.name"); })
        .SetVisibility(HVOutputVisibility::Always)
        .BindExternalValue(execute_status_);

    RegisterInputField(send_target_);
    RegisterInputField(select_variable_);
    RegisterInputField(select_comm_device_);
    RegisterInputField(data_source_binding_);
    RegisterOutputField(execute_status_output_);
}

int HVSendData::init()
{
    ResetRuntimeState();
    return SUCCESS;
}

int HVSendData::run()
{
    const auto start = std::chrono::steady_clock::now();
    execute_status_ = NODE_STATUS_RUNNING;
    error_message_key_.clear();

    NodeHostDataView data_view;
    int data_ret = INSTANCE_NOT_EXIST;
    if (host_services() != nullptr) {
        data_ret = host_services()->get_current_input_data(kDataSourceParamId, data_view);
    }

    std::string send_payload;
    if (data_view.has_value && data_view.data != nullptr) {
        switch (data_view.type) {
        case HV_STRING:
            send_payload = *static_cast<const std::string*>(data_view.data);
            break;
        case HV_INT:
            send_payload = std::to_string(*static_cast<const int*>(data_view.data));
            break;
        case HV_FLOAT:
            send_payload = std::to_string(*static_cast<const float*>(data_view.data));
            break;
        case HV_DOUBLE:
            send_payload = std::to_string(*static_cast<const double*>(data_view.data));
            break;
        case HV_LONG:
            send_payload = std::to_string(*static_cast<const long*>(data_view.data));
            break;
        case HV_BOOLEAN:
            send_payload = *static_cast<const bool*>(data_view.data) ? "true" : "false";
            break;
        default:
            send_payload.clear();
            break;
        }
    }

    if (data_ret != SUCCESS || !data_view.has_value || data_view.data == nullptr || send_payload.empty()) {
        const auto end = std::chrono::steady_clock::now();
        run_time_ = static_cast<long>(
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
        return FailWithMessage(ALGORITHM_RUN_ERROR, "msg.send_data_failed");
    }

    if (send_target_.value() == 0) {
        if (host_services() != nullptr) {
            NodeHostValue value;
            value.type = HV_STRING;
            value.has_value = true;
            value.string_value = send_payload;
            if (host_services()->set_global_value(select_variable_.value(), value) != SUCCESS) {
                const auto end = std::chrono::steady_clock::now();
                run_time_ = static_cast<long>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
                return FailWithMessage(ALGORITHM_RUN_ERROR, "msg.send_data_failed");
            }
        }
    }
    else if (send_target_.value() == 1) {
        const auto devices = cp_loader::GetDeviceList();
        if (select_comm_device_.value() < 0 ||
            select_comm_device_.value() >= static_cast<int>(devices.size()) ||
            cp_loader::SendString(devices[select_comm_device_.value()], send_payload) != 0) {
            const auto end = std::chrono::steady_clock::now();
            run_time_ = static_cast<long>(
                std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
            return FailWithMessage(ALGORITHM_RUN_ERROR, "msg.send_data_failed");
        }
    }

    const auto end = std::chrono::steady_clock::now();
    run_time_ = static_cast<long>(
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
    execute_status_ = SUCCESS;
    error_message_key_ = "msg.send_data_success";
    return SUCCESS;
}

AlgorithmType HVSendData::get_algorithm_type()
{
    return AlgorithmType::Capture;
}

std::string HVSendData::get_algorithm_display_name()
{
    return Tr(current_language(), "algorithm.display");
}

std::string HVSendData::TranslateText(const std::string& key) const
{
    return Tr(current_language(), key);
}

extern "C" __declspec(dllexport) NodeEngine* CreateInstance()
{
    return new HVSendData();
}

extern "C" __declspec(dllexport) std::string GetInstanceName()
{
    return "SendData";
}

extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion()
{
    return NODE_ENGINE_ABI_VERSION;
}
