#include "HVRoiCreate.h"

#include "3d_pliot_error.h"
#include "HVI18n.h"
#include "HVUtils.h"

#include <chrono>
#include <fstream>

namespace {

enum RoiCreateParamId {
    kParamShapeType = 0,
    kParamGeometryId,
    kParamGeometryName,
    kParamPointX,
    kParamPointY,
    kParamLineStartX,
    kParamLineStartY,
    kParamLineEndX,
    kParamLineEndY,
    kParamRectX,
    kParamRectY,
    kParamRectWidth,
    kParamRectHeight,
    kParamRotatedRectCenterX,
    kParamRotatedRectCenterY,
    kParamRotatedRectWidth,
    kParamRotatedRectHeight,
    kParamRotatedRectAngleDeg,
    kParamBoxCenterX,
    kParamBoxCenterY,
    kParamBoxCenterZ,
    kParamBoxLength,
    kParamBoxWidth,
    kParamBoxHeight,
    kParamRotatedBoxCenterX,
    kParamRotatedBoxCenterY,
    kParamRotatedBoxCenterZ,
    kParamRotatedBoxLength,
    kParamRotatedBoxWidth,
    kParamRotatedBoxHeight,
    kParamRotatedBoxRollDeg,
    kParamRotatedBoxPitchDeg,
    kParamRotatedBoxYawDeg,
    kParamCount
};

constexpr int kShapePoint = 0;
constexpr int kShapeLineSegment = 1;
constexpr int kShapeRectangle = 2;
constexpr int kShapeRotatedRectangle = 3;
constexpr int kShapeBox = 4;
constexpr int kShapeRotatedBox = 5;

const hvi18n::Dictionary kTexts = {
    { "algorithm.display", { "ROI\u521b\u5efa", "ROI create" } },
    { "input.shape.name", { "\u5f62\u72b6\u7c7b\u578b", "shape type" } },
    { "input.shape.desc", { "\u9009\u62e9\u9700\u8981\u521b\u5efa\u7684ROI\u51e0\u4f55\u5f62\u72b6", "Select the ROI geometry shape to create" } },
    { "input.id.name", { "ROI ID", "ROI id" } },
    { "input.id.desc", { "\u524d\u7aef\u900f\u4f20\u7684ROI\u6807\u8bc6", "ROI id passed through from the frontend" } },
    { "input.name.name", { "ROI\u540d\u79f0", "ROI name" } },
    { "input.name.desc", { "\u524d\u7aef\u900f\u4f20\u7684ROI\u540d\u79f0", "ROI name passed through from the frontend" } },
    { "input.point_x.name", { "\u70b9X", "point X" } },
    { "input.point_y.name", { "\u70b9Y", "point Y" } },
    { "input.line_start_x.name", { "\u7ebf\u6bb5\u8d77\u70b9X", "line start X" } },
    { "input.line_start_y.name", { "\u7ebf\u6bb5\u8d77\u70b9Y", "line start Y" } },
    { "input.line_end_x.name", { "\u7ebf\u6bb5\u7ec8\u70b9X", "line end X" } },
    { "input.line_end_y.name", { "\u7ebf\u6bb5\u7ec8\u70b9Y", "line end Y" } },
    { "input.rect_x.name", { "\u77e9\u5f62X", "rect X" } },
    { "input.rect_y.name", { "\u77e9\u5f62Y", "rect Y" } },
    { "input.rect_width.name", { "\u77e9\u5f62\u5bbd\u5ea6", "rect width" } },
    { "input.rect_height.name", { "\u77e9\u5f62\u9ad8\u5ea6", "rect height" } },
    { "input.rot_rect_center_x.name", { "\u65cb\u8f6c\u77e9\u5f62\u4e2d\u5fc3X", "rotated rect center X" } },
    { "input.rot_rect_center_y.name", { "\u65cb\u8f6c\u77e9\u5f62\u4e2d\u5fc3Y", "rotated rect center Y" } },
    { "input.rot_rect_width.name", { "\u65cb\u8f6c\u77e9\u5f62\u5bbd\u5ea6", "rotated rect width" } },
    { "input.rot_rect_height.name", { "\u65cb\u8f6c\u77e9\u5f62\u9ad8\u5ea6", "rotated rect height" } },
    { "input.rot_rect_angle.name", { "\u65cb\u8f6c\u77e9\u5f62\u89d2\u5ea6", "rotated rect angle" } },
    { "input.box_center_x.name", { "\u957f\u65b9\u4f53\u4e2d\u5fc3X", "box center X" } },
    { "input.box_center_y.name", { "\u957f\u65b9\u4f53\u4e2d\u5fc3Y", "box center Y" } },
    { "input.box_center_z.name", { "\u957f\u65b9\u4f53\u4e2d\u5fc3Z", "box center Z" } },
    { "input.box_length.name", { "\u957f\u65b9\u4f53\u957f\u5ea6", "box length" } },
    { "input.box_width.name", { "\u957f\u65b9\u4f53\u5bbd\u5ea6", "box width" } },
    { "input.box_height.name", { "\u957f\u65b9\u4f53\u9ad8\u5ea6", "box height" } },
    { "input.rot_box_center_x.name", { "\u65cb\u8f6c\u957f\u65b9\u4f53\u4e2d\u5fc3X", "rotated box center X" } },
    { "input.rot_box_center_y.name", { "\u65cb\u8f6c\u957f\u65b9\u4f53\u4e2d\u5fc3Y", "rotated box center Y" } },
    { "input.rot_box_center_z.name", { "\u65cb\u8f6c\u957f\u65b9\u4f53\u4e2d\u5fc3Z", "rotated box center Z" } },
    { "input.rot_box_length.name", { "\u65cb\u8f6c\u957f\u65b9\u4f53\u957f\u5ea6", "rotated box length" } },
    { "input.rot_box_width.name", { "\u65cb\u8f6c\u957f\u65b9\u4f53\u5bbd\u5ea6", "rotated box width" } },
    { "input.rot_box_height.name", { "\u65cb\u8f6c\u957f\u65b9\u4f53\u9ad8\u5ea6", "rotated box height" } },
    { "input.rot_box_roll.name", { "\u65cb\u8f6c\u957f\u65b9\u4f53Roll", "rotated box roll" } },
    { "input.rot_box_pitch.name", { "\u65cb\u8f6c\u957f\u65b9\u4f53Pitch", "rotated box pitch" } },
    { "input.rot_box_yaw.name", { "\u65cb\u8f6c\u957f\u65b9\u4f53Yaw", "rotated box yaw" } },
    { "input.coord.desc", { "\u6309\u9009\u4e2d\u5f62\u72b6\u751f\u6548\u7684ROI\u5750\u6807\u6216\u5c3a\u5bf8\u53c2\u6570", "ROI coordinate or size parameter used by the selected shape" } },
    { "output.roi.name", { "ROI", "ROI" } },
    { "output.status.name", { "\u8fd0\u884c\u72b6\u6001", "Execute status" } },
    { "option.point", { "\u70b9", "Point" } },
    { "option.line", { "\u7ebf\u6bb5", "Line segment" } },
    { "option.rect", { "\u77e9\u5f62", "Rectangle" } },
    { "option.rot_rect", { "\u65cb\u8f6c\u77e9\u5f62", "Rotated rectangle" } },
    { "option.box", { "\u957f\u65b9\u4f53", "Box" } },
    { "option.rot_box", { "\u65cb\u8f6c\u957f\u65b9\u4f53", "Rotated box" } },
    { "msg.invalid_shape", { "ROI\u5f62\u72b6\u7c7b\u578b\u4e0d\u5408\u6cd5", "ROI shape type is invalid" } },
    { "msg.invalid_geometry", { "ROI\u51e0\u4f55\u53c2\u6570\u4e0d\u5408\u6cd5", "ROI geometry is invalid" } },
    { "msg.success", { "ROI\u521b\u5efa\u6210\u529f", "ROI create success" } }
};

std::string Tr(int language, const std::string& key)
{
    return hvi18n::Translate(kTexts, key, language);
}

void AddShapeDependency(ParamMetadata& metadata, int shape_type)
{
    metadata.dependencies.push_back(ParamDependency(kParamShapeType, DEPENDS_ON_EQUALS, { std::to_string(shape_type) }));
}

ParamMetadata MakeDoubleMetadata(int language, const std::string& name_key, int shape_type)
{
    ParamMetadata metadata;
    metadata.param_name = Tr(language, name_key);
    metadata.param_description = Tr(language, "input.coord.desc");
    metadata.param_type = HV_DOUBLE;
    metadata.constraint_type = CONSTRAINT_NONE;
    metadata.param_group = PARAM_GROUP_BASIC;
    AddShapeDependency(metadata, shape_type);
    return metadata;
}

bool ReadJsonInt(const nlohmann::json& param_json, const std::string& name, int& value)
{
    if (param_json["name"].get<std::string>() != name ||
        param_json["type"].get<int>() != HV_INT ||
        !param_json["value"].is_number_integer()) {
        return false;
    }
    value = param_json["value"].get<int>();
    return true;
}

bool ReadJsonDouble(const nlohmann::json& param_json, const std::string& name, double& value)
{
    if (param_json["name"].get<std::string>() != name ||
        param_json["type"].get<int>() != HV_DOUBLE ||
        !param_json["value"].is_number()) {
        return false;
    }
    value = param_json["value"].get<double>();
    return true;
}

bool ReadJsonString(const nlohmann::json& param_json, const std::string& name, std::string& value)
{
    if (param_json["name"].get<std::string>() != name ||
        param_json["type"].get<int>() != HV_STRING ||
        !param_json["value"].is_string()) {
        return false;
    }
    value = param_json["value"].get<std::string>();
    return true;
}

}  // namespace

HVRoiCreate::HVRoiCreate() = default;

int HVRoiCreate::init()
{
    execute_status_ = NODE_STATUS_NOT_RUN;
    run_time_ms_ = 0;
    error_message_key_.clear();
    roi_ = HVGeometryInfo();
    return SUCCESS;
}

int HVRoiCreate::run()
{
    const auto start = std::chrono::steady_clock::now();
    execute_status_ = NODE_STATUS_RUNNING;
    run_time_ms_ = 0;
    error_message_key_.clear();

    roi_.geometry_id_ = geometry_id_;
    roi_.geometry_name_ = geometry_name_;

    switch (shape_type_) {
    case kShapePoint:
        roi_.SetPoint(HVPoint(point_x_, point_y_));
        break;
    case kShapeLineSegment:
        roi_.SetLineSegment(HVLineSegment(HVPoint(line_start_x_, line_start_y_), HVPoint(line_end_x_, line_end_y_)));
        break;
    case kShapeRectangle: {
        const HVRect rect(rect_x_, rect_y_, rect_width_, rect_height_);
        if (!rect.IsValid()) {
            return Fail("msg.invalid_geometry");
        }
        roi_.SetRect(rect);
        break;
    }
    case kShapeRotatedRectangle: {
        const HVRotatedRect rect(
            HVPoint(rotated_rect_center_x_, rotated_rect_center_y_),
            rotated_rect_width_,
            rotated_rect_height_,
            rotated_rect_angle_deg_);
        if (!rect.IsValid()) {
            return Fail("msg.invalid_geometry");
        }
        roi_.SetRotatedRect(rect);
        break;
    }
    case kShapeBox: {
        const HVBox box(HVPoint3D(box_center_x_, box_center_y_, box_center_z_), box_length_, box_width_, box_height_);
        if (!box.IsValid()) {
            return Fail("msg.invalid_geometry");
        }
        roi_.SetBox(box);
        break;
    }
    case kShapeRotatedBox: {
        const HVRotatedBox box(
            HVPoint3D(rotated_box_center_x_, rotated_box_center_y_, rotated_box_center_z_),
            rotated_box_length_,
            rotated_box_width_,
            rotated_box_height_,
            HVOrientation3D(rotated_box_roll_deg_, rotated_box_pitch_deg_, rotated_box_yaw_deg_));
        if (!box.IsValid()) {
            return Fail("msg.invalid_geometry");
        }
        roi_.SetRotatedBox(box);
        break;
    }
    default:
        return Fail("msg.invalid_shape");
    }

    const auto end = std::chrono::steady_clock::now();
    run_time_ms_ = static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
    execute_status_ = SUCCESS;
    error_message_key_ = "msg.success";
    return SUCCESS;
}

int HVRoiCreate::set_algorithm_params(const std::vector<void*>& params, const std::vector<int>& paramID)
{
    if (paramID.empty()) {
        for (int param_id = 0; param_id < static_cast<int>(params.size()); ++param_id) {
            const int result = ApplyParam(param_id, params[param_id]);
            if (result != SUCCESS) {
                return result;
            }
        }
        return SUCCESS;
    }

    if (params.size() != paramID.size()) {
        return INVALID_PARAMS_NUM;
    }

    for (size_t i = 0; i < paramID.size(); ++i) {
        const int result = ApplyParam(paramID[i], params[i]);
        if (result != SUCCESS) {
            return result;
        }
    }
    return SUCCESS;
}

std::vector<void*> HVRoiCreate::get_current_params()
{
    return {
        &shape_type_,
        &geometry_id_,
        &geometry_name_,
        &point_x_,
        &point_y_,
        &line_start_x_,
        &line_start_y_,
        &line_end_x_,
        &line_end_y_,
        &rect_x_,
        &rect_y_,
        &rect_width_,
        &rect_height_,
        &rotated_rect_center_x_,
        &rotated_rect_center_y_,
        &rotated_rect_width_,
        &rotated_rect_height_,
        &rotated_rect_angle_deg_,
        &box_center_x_,
        &box_center_y_,
        &box_center_z_,
        &box_length_,
        &box_width_,
        &box_height_,
        &rotated_box_center_x_,
        &rotated_box_center_y_,
        &rotated_box_center_z_,
        &rotated_box_length_,
        &rotated_box_width_,
        &rotated_box_height_,
        &rotated_box_roll_deg_,
        &rotated_box_pitch_deg_,
        &rotated_box_yaw_deg_
    };
}

std::vector<void*> HVRoiCreate::get_algorithm_result()
{
    if (execute_status_ == SUCCESS) {
        return { &roi_, &execute_status_ };
    }
    return { nullptr, &execute_status_ };
}

std::vector<int> HVRoiCreate::get_algorithm_input_params_type()
{
    std::vector<int> types(kParamCount, HV_DOUBLE);
    types[kParamShapeType] = HV_INT;
    types[kParamGeometryId] = HV_INT;
    types[kParamGeometryName] = HV_STRING;
    return types;
}

std::vector<int> HVRoiCreate::get_algorithm_output_params_type()
{
    return { HV_IMAGEROI, HV_INT };
}

std::vector<std::string> HVRoiCreate::get_algorithm_input_params_name()
{
    return {
        Tr(language_, "input.shape.name"),
        Tr(language_, "input.id.name"),
        Tr(language_, "input.name.name"),
        Tr(language_, "input.point_x.name"),
        Tr(language_, "input.point_y.name"),
        Tr(language_, "input.line_start_x.name"),
        Tr(language_, "input.line_start_y.name"),
        Tr(language_, "input.line_end_x.name"),
        Tr(language_, "input.line_end_y.name"),
        Tr(language_, "input.rect_x.name"),
        Tr(language_, "input.rect_y.name"),
        Tr(language_, "input.rect_width.name"),
        Tr(language_, "input.rect_height.name"),
        Tr(language_, "input.rot_rect_center_x.name"),
        Tr(language_, "input.rot_rect_center_y.name"),
        Tr(language_, "input.rot_rect_width.name"),
        Tr(language_, "input.rot_rect_height.name"),
        Tr(language_, "input.rot_rect_angle.name"),
        Tr(language_, "input.box_center_x.name"),
        Tr(language_, "input.box_center_y.name"),
        Tr(language_, "input.box_center_z.name"),
        Tr(language_, "input.box_length.name"),
        Tr(language_, "input.box_width.name"),
        Tr(language_, "input.box_height.name"),
        Tr(language_, "input.rot_box_center_x.name"),
        Tr(language_, "input.rot_box_center_y.name"),
        Tr(language_, "input.rot_box_center_z.name"),
        Tr(language_, "input.rot_box_length.name"),
        Tr(language_, "input.rot_box_width.name"),
        Tr(language_, "input.rot_box_height.name"),
        Tr(language_, "input.rot_box_roll.name"),
        Tr(language_, "input.rot_box_pitch.name"),
        Tr(language_, "input.rot_box_yaw.name")
    };
}

std::vector<std::string> HVRoiCreate::get_algorithm_output_params_name()
{
    return {
        Tr(language_, "output.roi.name"),
        Tr(language_, "output.status.name")
    };
}

std::vector<bool> HVRoiCreate::get_algorithm_input_params_bindable()
{
    return std::vector<bool>(kParamCount, false);
}

std::vector<ParamMetadata> HVRoiCreate::get_algorithm_input_params_metadata()
{
    std::vector<ParamMetadata> metadata_list(kParamCount);

    metadata_list[kParamShapeType].param_name = Tr(language_, "input.shape.name");
    metadata_list[kParamShapeType].param_description = Tr(language_, "input.shape.desc");
    metadata_list[kParamShapeType].param_type = HV_INT;
    metadata_list[kParamShapeType].constraint_type = CONSTRAINT_OPTIONS;
    metadata_list[kParamShapeType].options_constraint.AddOption(kShapePoint, Tr(language_, "option.point"));
    metadata_list[kParamShapeType].options_constraint.AddOption(kShapeLineSegment, Tr(language_, "option.line"));
    metadata_list[kParamShapeType].options_constraint.AddOption(kShapeRectangle, Tr(language_, "option.rect"));
    metadata_list[kParamShapeType].options_constraint.AddOption(kShapeRotatedRectangle, Tr(language_, "option.rot_rect"));
    metadata_list[kParamShapeType].options_constraint.AddOption(kShapeBox, Tr(language_, "option.box"));
    metadata_list[kParamShapeType].options_constraint.AddOption(kShapeRotatedBox, Tr(language_, "option.rot_box"));
    metadata_list[kParamShapeType].options_constraint.default_index = 0;

    metadata_list[kParamGeometryId].param_name = Tr(language_, "input.id.name");
    metadata_list[kParamGeometryId].param_description = Tr(language_, "input.id.desc");
    metadata_list[kParamGeometryId].param_type = HV_INT;
    metadata_list[kParamGeometryId].constraint_type = CONSTRAINT_NONE;

    metadata_list[kParamGeometryName].param_name = Tr(language_, "input.name.name");
    metadata_list[kParamGeometryName].param_description = Tr(language_, "input.name.desc");
    metadata_list[kParamGeometryName].param_type = HV_STRING;
    metadata_list[kParamGeometryName].constraint_type = CONSTRAINT_NONE;

    metadata_list[kParamPointX] = MakeDoubleMetadata(language_, "input.point_x.name", kShapePoint);
    metadata_list[kParamPointY] = MakeDoubleMetadata(language_, "input.point_y.name", kShapePoint);
    metadata_list[kParamLineStartX] = MakeDoubleMetadata(language_, "input.line_start_x.name", kShapeLineSegment);
    metadata_list[kParamLineStartY] = MakeDoubleMetadata(language_, "input.line_start_y.name", kShapeLineSegment);
    metadata_list[kParamLineEndX] = MakeDoubleMetadata(language_, "input.line_end_x.name", kShapeLineSegment);
    metadata_list[kParamLineEndY] = MakeDoubleMetadata(language_, "input.line_end_y.name", kShapeLineSegment);
    metadata_list[kParamRectX] = MakeDoubleMetadata(language_, "input.rect_x.name", kShapeRectangle);
    metadata_list[kParamRectY] = MakeDoubleMetadata(language_, "input.rect_y.name", kShapeRectangle);
    metadata_list[kParamRectWidth] = MakeDoubleMetadata(language_, "input.rect_width.name", kShapeRectangle);
    metadata_list[kParamRectHeight] = MakeDoubleMetadata(language_, "input.rect_height.name", kShapeRectangle);
    metadata_list[kParamRotatedRectCenterX] = MakeDoubleMetadata(language_, "input.rot_rect_center_x.name", kShapeRotatedRectangle);
    metadata_list[kParamRotatedRectCenterY] = MakeDoubleMetadata(language_, "input.rot_rect_center_y.name", kShapeRotatedRectangle);
    metadata_list[kParamRotatedRectWidth] = MakeDoubleMetadata(language_, "input.rot_rect_width.name", kShapeRotatedRectangle);
    metadata_list[kParamRotatedRectHeight] = MakeDoubleMetadata(language_, "input.rot_rect_height.name", kShapeRotatedRectangle);
    metadata_list[kParamRotatedRectAngleDeg] = MakeDoubleMetadata(language_, "input.rot_rect_angle.name", kShapeRotatedRectangle);
    metadata_list[kParamBoxCenterX] = MakeDoubleMetadata(language_, "input.box_center_x.name", kShapeBox);
    metadata_list[kParamBoxCenterY] = MakeDoubleMetadata(language_, "input.box_center_y.name", kShapeBox);
    metadata_list[kParamBoxCenterZ] = MakeDoubleMetadata(language_, "input.box_center_z.name", kShapeBox);
    metadata_list[kParamBoxLength] = MakeDoubleMetadata(language_, "input.box_length.name", kShapeBox);
    metadata_list[kParamBoxWidth] = MakeDoubleMetadata(language_, "input.box_width.name", kShapeBox);
    metadata_list[kParamBoxHeight] = MakeDoubleMetadata(language_, "input.box_height.name", kShapeBox);
    metadata_list[kParamRotatedBoxCenterX] = MakeDoubleMetadata(language_, "input.rot_box_center_x.name", kShapeRotatedBox);
    metadata_list[kParamRotatedBoxCenterY] = MakeDoubleMetadata(language_, "input.rot_box_center_y.name", kShapeRotatedBox);
    metadata_list[kParamRotatedBoxCenterZ] = MakeDoubleMetadata(language_, "input.rot_box_center_z.name", kShapeRotatedBox);
    metadata_list[kParamRotatedBoxLength] = MakeDoubleMetadata(language_, "input.rot_box_length.name", kShapeRotatedBox);
    metadata_list[kParamRotatedBoxWidth] = MakeDoubleMetadata(language_, "input.rot_box_width.name", kShapeRotatedBox);
    metadata_list[kParamRotatedBoxHeight] = MakeDoubleMetadata(language_, "input.rot_box_height.name", kShapeRotatedBox);
    metadata_list[kParamRotatedBoxRollDeg] = MakeDoubleMetadata(language_, "input.rot_box_roll.name", kShapeRotatedBox);
    metadata_list[kParamRotatedBoxPitchDeg] = MakeDoubleMetadata(language_, "input.rot_box_pitch.name", kShapeRotatedBox);
    metadata_list[kParamRotatedBoxYawDeg] = MakeDoubleMetadata(language_, "input.rot_box_yaw.name", kShapeRotatedBox);

    return metadata_list;
}

int HVRoiCreate::get_algorithm_execute_status()
{
    return execute_status_;
}

std::string HVRoiCreate::get_algorithm_error_message()
{
    if (error_message_key_.empty()) {
        return "";
    }
    return Tr(language_, error_message_key_);
}

long HVRoiCreate::get_algorithm_use_time()
{
    return run_time_ms_;
}

bool HVRoiCreate::algorithm_params_setting_status()
{
    return true;
}

bool HVRoiCreate::algorithm_init_status()
{
    return true;
}

bool HVRoiCreate::save_params_to_json(const std::string& filePath)
{
    try {
        nlohmann::json params_json = nlohmann::json::array();
        add_param(params_json, "shape_type", HV_INT, shape_type_);
        add_param(params_json, "geometry_id", HV_INT, geometry_id_);
        add_param(params_json, "geometry_name", HV_STRING, geometry_name_);
        add_param(params_json, "point_x", HV_DOUBLE, point_x_);
        add_param(params_json, "point_y", HV_DOUBLE, point_y_);
        add_param(params_json, "line_start_x", HV_DOUBLE, line_start_x_);
        add_param(params_json, "line_start_y", HV_DOUBLE, line_start_y_);
        add_param(params_json, "line_end_x", HV_DOUBLE, line_end_x_);
        add_param(params_json, "line_end_y", HV_DOUBLE, line_end_y_);
        add_param(params_json, "rect_x", HV_DOUBLE, rect_x_);
        add_param(params_json, "rect_y", HV_DOUBLE, rect_y_);
        add_param(params_json, "rect_width", HV_DOUBLE, rect_width_);
        add_param(params_json, "rect_height", HV_DOUBLE, rect_height_);
        add_param(params_json, "rotated_rect_center_x", HV_DOUBLE, rotated_rect_center_x_);
        add_param(params_json, "rotated_rect_center_y", HV_DOUBLE, rotated_rect_center_y_);
        add_param(params_json, "rotated_rect_width", HV_DOUBLE, rotated_rect_width_);
        add_param(params_json, "rotated_rect_height", HV_DOUBLE, rotated_rect_height_);
        add_param(params_json, "rotated_rect_angle_deg", HV_DOUBLE, rotated_rect_angle_deg_);
        add_param(params_json, "box_center_x", HV_DOUBLE, box_center_x_);
        add_param(params_json, "box_center_y", HV_DOUBLE, box_center_y_);
        add_param(params_json, "box_center_z", HV_DOUBLE, box_center_z_);
        add_param(params_json, "box_length", HV_DOUBLE, box_length_);
        add_param(params_json, "box_width", HV_DOUBLE, box_width_);
        add_param(params_json, "box_height", HV_DOUBLE, box_height_);
        add_param(params_json, "rotated_box_center_x", HV_DOUBLE, rotated_box_center_x_);
        add_param(params_json, "rotated_box_center_y", HV_DOUBLE, rotated_box_center_y_);
        add_param(params_json, "rotated_box_center_z", HV_DOUBLE, rotated_box_center_z_);
        add_param(params_json, "rotated_box_length", HV_DOUBLE, rotated_box_length_);
        add_param(params_json, "rotated_box_width", HV_DOUBLE, rotated_box_width_);
        add_param(params_json, "rotated_box_height", HV_DOUBLE, rotated_box_height_);
        add_param(params_json, "rotated_box_roll_deg", HV_DOUBLE, rotated_box_roll_deg_);
        add_param(params_json, "rotated_box_pitch_deg", HV_DOUBLE, rotated_box_pitch_deg_);
        add_param(params_json, "rotated_box_yaw_deg", HV_DOUBLE, rotated_box_yaw_deg_);

        std::ofstream file(filePath);
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

bool HVRoiCreate::load_params_from_json(const std::string& filePath)
{
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            return false;
        }

        nlohmann::json params_json;
        file >> params_json;
        if (!params_json.is_array()) {
            return false;
        }

        for (const auto& param_json : params_json) {
            if (!param_json.is_object() ||
                !param_json.contains("name") ||
                !param_json.contains("type") ||
                !param_json.contains("value")) {
                continue;
            }

            ReadJsonInt(param_json, "shape_type", shape_type_) ||
                ReadJsonInt(param_json, "geometry_id", geometry_id_) ||
                ReadJsonString(param_json, "geometry_name", geometry_name_) ||
                ReadJsonDouble(param_json, "point_x", point_x_) ||
                ReadJsonDouble(param_json, "point_y", point_y_) ||
                ReadJsonDouble(param_json, "line_start_x", line_start_x_) ||
                ReadJsonDouble(param_json, "line_start_y", line_start_y_) ||
                ReadJsonDouble(param_json, "line_end_x", line_end_x_) ||
                ReadJsonDouble(param_json, "line_end_y", line_end_y_) ||
                ReadJsonDouble(param_json, "rect_x", rect_x_) ||
                ReadJsonDouble(param_json, "rect_y", rect_y_) ||
                ReadJsonDouble(param_json, "rect_width", rect_width_) ||
                ReadJsonDouble(param_json, "rect_height", rect_height_) ||
                ReadJsonDouble(param_json, "rotated_rect_center_x", rotated_rect_center_x_) ||
                ReadJsonDouble(param_json, "rotated_rect_center_y", rotated_rect_center_y_) ||
                ReadJsonDouble(param_json, "rotated_rect_width", rotated_rect_width_) ||
                ReadJsonDouble(param_json, "rotated_rect_height", rotated_rect_height_) ||
                ReadJsonDouble(param_json, "rotated_rect_angle_deg", rotated_rect_angle_deg_) ||
                ReadJsonDouble(param_json, "box_center_x", box_center_x_) ||
                ReadJsonDouble(param_json, "box_center_y", box_center_y_) ||
                ReadJsonDouble(param_json, "box_center_z", box_center_z_) ||
                ReadJsonDouble(param_json, "box_length", box_length_) ||
                ReadJsonDouble(param_json, "box_width", box_width_) ||
                ReadJsonDouble(param_json, "box_height", box_height_) ||
                ReadJsonDouble(param_json, "rotated_box_center_x", rotated_box_center_x_) ||
                ReadJsonDouble(param_json, "rotated_box_center_y", rotated_box_center_y_) ||
                ReadJsonDouble(param_json, "rotated_box_center_z", rotated_box_center_z_) ||
                ReadJsonDouble(param_json, "rotated_box_length", rotated_box_length_) ||
                ReadJsonDouble(param_json, "rotated_box_width", rotated_box_width_) ||
                ReadJsonDouble(param_json, "rotated_box_height", rotated_box_height_) ||
                ReadJsonDouble(param_json, "rotated_box_roll_deg", rotated_box_roll_deg_) ||
                ReadJsonDouble(param_json, "rotated_box_pitch_deg", rotated_box_pitch_deg_) ||
                ReadJsonDouble(param_json, "rotated_box_yaw_deg", rotated_box_yaw_deg_);
        }
        return true;
    }
    catch (...) {
        return false;
    }
}

AlgorithmType HVRoiCreate::get_algorithm_type()
{
    return AlgorithmType::LogicTool;
}

void HVRoiCreate::set_language(int language)
{
    if (hvi18n::IsSupportedLanguage(language)) {
        language_ = language;
    }
}

int HVRoiCreate::get_language() const
{
    return language_;
}

std::string HVRoiCreate::get_algorithm_display_name()
{
    return Tr(language_, "algorithm.display");
}

int HVRoiCreate::ApplyParam(int param_id, void* param)
{
    if (param == nullptr) {
        return SUCCESS;
    }

    switch (param_id) {
    case kParamShapeType:
        shape_type_ = *static_cast<int*>(param);
        return SUCCESS;
    case kParamGeometryId:
        geometry_id_ = *static_cast<int*>(param);
        return SUCCESS;
    case kParamGeometryName:
        geometry_name_ = *static_cast<std::string*>(param);
        return SUCCESS;
    case kParamPointX:
        point_x_ = *static_cast<double*>(param);
        return SUCCESS;
    case kParamPointY:
        point_y_ = *static_cast<double*>(param);
        return SUCCESS;
    case kParamLineStartX:
        line_start_x_ = *static_cast<double*>(param);
        return SUCCESS;
    case kParamLineStartY:
        line_start_y_ = *static_cast<double*>(param);
        return SUCCESS;
    case kParamLineEndX:
        line_end_x_ = *static_cast<double*>(param);
        return SUCCESS;
    case kParamLineEndY:
        line_end_y_ = *static_cast<double*>(param);
        return SUCCESS;
    case kParamRectX:
        rect_x_ = *static_cast<double*>(param);
        return SUCCESS;
    case kParamRectY:
        rect_y_ = *static_cast<double*>(param);
        return SUCCESS;
    case kParamRectWidth:
        rect_width_ = *static_cast<double*>(param);
        return SUCCESS;
    case kParamRectHeight:
        rect_height_ = *static_cast<double*>(param);
        return SUCCESS;
    case kParamRotatedRectCenterX:
        rotated_rect_center_x_ = *static_cast<double*>(param);
        return SUCCESS;
    case kParamRotatedRectCenterY:
        rotated_rect_center_y_ = *static_cast<double*>(param);
        return SUCCESS;
    case kParamRotatedRectWidth:
        rotated_rect_width_ = *static_cast<double*>(param);
        return SUCCESS;
    case kParamRotatedRectHeight:
        rotated_rect_height_ = *static_cast<double*>(param);
        return SUCCESS;
    case kParamRotatedRectAngleDeg:
        rotated_rect_angle_deg_ = *static_cast<double*>(param);
        return SUCCESS;
    case kParamBoxCenterX:
        box_center_x_ = *static_cast<double*>(param);
        return SUCCESS;
    case kParamBoxCenterY:
        box_center_y_ = *static_cast<double*>(param);
        return SUCCESS;
    case kParamBoxCenterZ:
        box_center_z_ = *static_cast<double*>(param);
        return SUCCESS;
    case kParamBoxLength:
        box_length_ = *static_cast<double*>(param);
        return SUCCESS;
    case kParamBoxWidth:
        box_width_ = *static_cast<double*>(param);
        return SUCCESS;
    case kParamBoxHeight:
        box_height_ = *static_cast<double*>(param);
        return SUCCESS;
    case kParamRotatedBoxCenterX:
        rotated_box_center_x_ = *static_cast<double*>(param);
        return SUCCESS;
    case kParamRotatedBoxCenterY:
        rotated_box_center_y_ = *static_cast<double*>(param);
        return SUCCESS;
    case kParamRotatedBoxCenterZ:
        rotated_box_center_z_ = *static_cast<double*>(param);
        return SUCCESS;
    case kParamRotatedBoxLength:
        rotated_box_length_ = *static_cast<double*>(param);
        return SUCCESS;
    case kParamRotatedBoxWidth:
        rotated_box_width_ = *static_cast<double*>(param);
        return SUCCESS;
    case kParamRotatedBoxHeight:
        rotated_box_height_ = *static_cast<double*>(param);
        return SUCCESS;
    case kParamRotatedBoxRollDeg:
        rotated_box_roll_deg_ = *static_cast<double*>(param);
        return SUCCESS;
    case kParamRotatedBoxPitchDeg:
        rotated_box_pitch_deg_ = *static_cast<double*>(param);
        return SUCCESS;
    case kParamRotatedBoxYawDeg:
        rotated_box_yaw_deg_ = *static_cast<double*>(param);
        return SUCCESS;
    default:
        return INVALID_PARAMS_NUM;
    }
}

int HVRoiCreate::Fail(const std::string& message_key)
{
    execute_status_ = ALGORITHM_RUN_ERROR;
    error_message_key_ = message_key;
    return ALGORITHM_RUN_ERROR;
}

extern "C" __declspec(dllexport) NodeEngine* CreateInstance()
{
    return new HVRoiCreate();
}

extern "C" __declspec(dllexport) std::string GetInstanceName()
{
    return "ROI create";
}

extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion()
{
    return NODE_ENGINE_ABI_VERSION;
}
