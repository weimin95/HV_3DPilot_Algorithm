#include "HVTimestampGenerate.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "HVI18n.h"

namespace {

constexpr int kFormatCount = 7;

const char* kFormatPatterns[] = {
    "%Y-%m-%d %H:%M:%S",        // yyyy-MM-dd HH:mm:ss
    "%Y-%m-%d",                  // yyyy-MM-dd
    "%H:%M:%S",                  // HH:mm:ss
    "%Y%m%d_%H%M%S",            // yyyyMMdd_HHmmss
    nullptr,                     // ISO 8601 with ms+TZ (special handling)
    nullptr,                     // yyyyMMddHHmmssSSS (special handling)
    "%Y/%m/%d %H:%M:%S",        // yyyy/MM/dd HH:mm:ss
};

const char* kFormatDisplayKeys[] = {
    "format.yyyy_mm_dd_hh_mm_ss",
    "format.yyyy_mm_dd",
    "format.hh_mm_ss",
    "format.yyyymmdd_hhmmss",
    "format.iso8601",
    "format.yyyymmddhhmmsssss",
    "format.yyyy_mm_dd_hh_mm_ss_slash",
};

const hvi18n::Dictionary kTexts = {
    { "algorithm.display", { "时间戳生成", "Timestamp generate" } },
    { "input.format_selector.name", { "格式选择", "Format selector" } },
    { "input.format_selector.desc", { "选择时间戳的输出格式", "Select the output format of the timestamp" } },
    { "output.timestamp.name", { "时间戳", "Timestamp" } },
    { "output.status.name", { "运行状态", "Execute status" } },
    { "format.yyyy_mm_dd_hh_mm_ss", { "yyyy-MM-dd HH:mm:ss", "yyyy-MM-dd HH:mm:ss" } },
    { "format.yyyy_mm_dd", { "yyyy-MM-dd", "yyyy-MM-dd" } },
    { "format.hh_mm_ss", { "HH:mm:ss", "HH:mm:ss" } },
    { "format.yyyymmdd_hhmmss", { "yyyyMMdd_HHmmss", "yyyyMMdd_HHmmss" } },
    { "format.iso8601", { "ISO 8601 (dateTtime.ms+TZ)", "ISO 8601 (dateTtime.ms+TZ)" } },
    { "format.yyyymmddhhmmsssss", { "yyyyMMddHHmmssSSS", "yyyyMMddHHmmssSSS" } },
    { "format.yyyy_mm_dd_hh_mm_ss_slash", { "yyyy/MM/dd HH:mm:ss", "yyyy/MM/dd HH:mm:ss" } },
    { "msg.run_error", { "时间戳生成失败", "Timestamp generation failed" } },
    { "msg.run_success", { "时间戳生成成功", "Timestamp generation succeeded" } },
};

std::string Tr(int language, const std::string& key)
{
    return hvi18n::Translate(kTexts, key, language);
}

long long MilliSecondsSinceEpoch(const std::chrono::system_clock::time_point& tp)
{
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        tp.time_since_epoch());
    return ms.count();
}

void FormatUtcOffset(std::ostringstream& oss, const std::tm& local_tm)
{
    long offset_sec = 0;
#ifdef _WIN32
    long timezone_val = 0;
    _get_timezone(&timezone_val);
    offset_sec = -timezone_val;
    if (local_tm.tm_isdst > 0) {
        offset_sec += 3600;
    }
#else
    offset_sec = local_tm.tm_gmtoff;
#endif

    char sign = (offset_sec >= 0) ? '+' : '-';
    if (offset_sec < 0) offset_sec = -offset_sec;
    int hours = static_cast<int>(offset_sec / 3600);
    int minutes = static_cast<int>((offset_sec % 3600) / 60);

    oss << sign
        << std::setw(2) << std::setfill('0') << hours << ':'
        << std::setw(2) << std::setfill('0') << minutes;
}

}  // namespace

HVTimestampGenerate::HVTimestampGenerate()
{
    {
        OptionsConstraint format_options;
        for (int i = 0; i < kFormatCount; ++i) {
            format_options.AddOption(i, kFormatDisplayKeys[i]);
        }
        format_options.default_index = 0;

        format_selector_
            .SetSchemaName("format_selector")
            .SetStorageKey("format_selector")
            .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.format_selector.name"); })
            .SetDescriptionResolver([this]() { return Tr(current_language(), "input.format_selector.desc"); })
            .SetBindable(false)
            .SetEditable(true)
            .SetPersist(true)
            .SetOptionsConstraint(format_options)
            .SetMetadataCustomizer([this](ParamMetadata& metadata) {
                auto& labels = metadata.options_constraint.option_labels;
                for (int i = 0; i < static_cast<int>(labels.size()) && i < kFormatCount; ++i) {
                    labels[i] = Tr(current_language(), kFormatDisplayKeys[i]);
                }
            })
            .SetParamGroup(PARAM_GROUP_BASIC);
    }

    timestamp_
        .SetSchemaName("timestamp")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "output.timestamp.name"); })
        .SetVisibility(HVOutputVisibility::OnSuccess);

    execute_status_output_
        .SetSchemaName("execute_status")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "output.status.name"); })
        .SetVisibility(HVOutputVisibility::Always)
        .BindExternalValue(execute_status_);

    RegisterInputField(format_selector_);
    RegisterOutputField(timestamp_);
    RegisterOutputField(execute_status_output_);
}

int HVTimestampGenerate::init()
{
    ResetRuntimeState();
    timestamp_.value().clear();
    return SUCCESS;
}

int HVTimestampGenerate::run()
{
    const auto start = std::chrono::steady_clock::now();
    execute_status_ = NODE_STATUS_RUNNING;
    error_message_key_.clear();
    timestamp_.value().clear();

    const int format_index = format_selector_.value();
    if (format_index < 0 || format_index >= kFormatCount) {
        return FailWithMsg(ALGORITHM_RUN_ERROR, "msg.run_error");
    }

    const auto now = std::chrono::system_clock::now();
    const std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);

    std::tm local_tm = {};
#ifdef _WIN32
    localtime_s(&local_tm, &now_time_t);
#else
    localtime_r(&now_time_t, &local_tm);
#endif

    const long long epoch_ms = MilliSecondsSinceEpoch(now);
    const int ms_part = static_cast<int>(epoch_ms % 1000);

    std::ostringstream oss;

    if (format_index == 4) {
        // ISO 8601 with milliseconds and timezone offset
        char buf[32] = {};
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &local_tm);
        oss << buf << '.' << std::setw(3) << std::setfill('0') << ms_part;
        FormatUtcOffset(oss, local_tm);
    } else if (format_index == 5) {
        // yyyyMMddHHmmssSSS
        char buf[32] = {};
        std::strftime(buf, sizeof(buf), "%Y%m%d%H%M%S", &local_tm);
        oss << buf << std::setw(3) << std::setfill('0') << ms_part;
    } else {
        char buf[64] = {};
        std::strftime(buf, sizeof(buf), kFormatPatterns[format_index], &local_tm);
        oss << buf;
    }

    timestamp_.value() = oss.str();

    const auto end = std::chrono::steady_clock::now();
    run_time_ = static_cast<long>(
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
    execute_status_ = SUCCESS;
    error_message_key_ = "msg.run_success";
    return SUCCESS;
}

AlgorithmType HVTimestampGenerate::get_algorithm_type()
{
    return AlgorithmType::LogicTool;
}

std::string HVTimestampGenerate::get_algorithm_display_name()
{
    return Tr(current_language(), "algorithm.display");
}

std::string HVTimestampGenerate::TranslateText(const std::string& key) const
{
    return Tr(current_language(), key);
}

int HVTimestampGenerate::FailWithMsg(int status, const std::string& message_key)
{
    timestamp_.value().clear();
    return FailWithMessage(status, message_key);
}

extern "C" __declspec(dllexport) NodeEngine* CreateInstance()
{
    return new HVTimestampGenerate();
}

extern "C" __declspec(dllexport) std::string GetInstanceName()
{
    return "Timestamp generate";
}

extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion()
{
    return NODE_ENGINE_ABI_VERSION;
}
