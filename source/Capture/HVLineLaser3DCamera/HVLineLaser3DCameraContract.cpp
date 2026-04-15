#include "HVLineLaser3DCameraContract.h"

#include <utility>

namespace line_laser_3d_camera {
namespace {

ParamMetadata MakeRangeMetadata(
    const std::string& name,
    int type,
    double min_value,
    double max_value,
    double default_value)
{
    ParamMetadata metadata;
    metadata.param_name = name;
    metadata.param_description = name;
    metadata.param_type = type;
    metadata.param_group = PARAM_GROUP_BASIC;
    metadata.constraint_type = CONSTRAINT_RANGE;
    metadata.range_constraint = RangeConstraint(min_value, max_value, default_value);
    return metadata;
}

ParamMetadata MakeStringMetadata(const std::string& name, const std::string& pattern)
{
    ParamMetadata metadata;
    metadata.param_name = name;
    metadata.param_description = name;
    metadata.param_type = HV_STRING;
    metadata.param_group = PARAM_GROUP_BASIC;
    metadata.constraint_type = CONSTRAINT_REGEX;
    metadata.regex_pattern = pattern;
    return metadata;
}

ParamMetadata MakeOptionsMetadata(
    const std::string& name,
    int type,
    const std::vector<std::pair<int, std::string>>& options,
    int default_index)
{
    ParamMetadata metadata;
    metadata.param_name = name;
    metadata.param_description = name;
    metadata.param_type = type;
    metadata.param_group = PARAM_GROUP_BASIC;
    metadata.constraint_type = CONSTRAINT_OPTIONS;
    metadata.options_constraint.default_index = default_index;
    for (const auto& option : options) {
        metadata.options_constraint.AddOption(option.first, option.second);
    }
    return metadata;
}

}  // namespace

Contract BuildContract()
{
    Contract contract;
    contract.algorithm_name = "Line Laser 3D Camera";
    contract.zh_display_name = u8"\u7EBF\u6FC0\u51493D\u76F8\u673A";
    contract.en_display_name = "Line Laser 3D Camera";
    contract.algorithm_type = AlgorithmType::Capture;

    contract.input_names = {
        "ip",
        "device_id",
        "batch_points",
        "head_index",
        "connect_timeout_ms",
        "wait_timeout_ms",
        "y_pitch_mm",
    };
    contract.input_types = {
        HV_STRING,
        HV_INT,
        HV_INT,
        HV_INT,
        HV_INT,
        HV_INT,
        HV_DOUBLE,
    };
    contract.input_bindable_hints.assign(contract.input_types.size(), false);
    contract.input_metadata = {
        MakeStringMetadata(
            "ip",
            R"(^((25[0-5]|2[0-4][0-9]|1?[0-9][0-9]?)\.){3}(25[0-5]|2[0-4][0-9]|1?[0-9][0-9]?)$)"),
        MakeRangeMetadata("device_id", HV_INT, 0.0, 63.0, 0.0),
        MakeRangeMetadata("batch_points", HV_INT, 1.0, 65535.0, 1000.0),
        MakeOptionsMetadata("head_index", HV_INT, { { 0, "A" }, { 1, "B" } }, 0),
        MakeRangeMetadata("connect_timeout_ms", HV_INT, 100.0, 60000.0, 2000.0),
        MakeRangeMetadata("wait_timeout_ms", HV_INT, 1.0, 600000.0, 50000.0),
        MakeRangeMetadata("y_pitch_mm", HV_DOUBLE, 0.0, 1000.0, 0.0),
    };

    contract.output_names = {
        "point_cloud",
        "depth_image",
        "gray_image",
        "execute_status",
    };
    contract.output_types = {
        HV_POINTCLOUD,
        HV_IMAGEDATAINFODEPTH,
        HV_IMAGEDATAINFO2D,
        HV_INT,
    };
    return contract;
}

}  // namespace line_laser_3d_camera
