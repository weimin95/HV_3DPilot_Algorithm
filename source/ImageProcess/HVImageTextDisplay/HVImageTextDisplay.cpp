#include "HVImageTextDisplay.h"

#include <chrono>

#include <opencv2/opencv.hpp>

#include "HVI18n.h"
#include "HVUtils.h"

namespace {

const hvi18n::Dictionary kTexts = {
    { "algorithm.display", { "文本显示", "Text display" } },
    { "input.image.name", { "输入图像", "Input image" } },
    { "input.text_list.name", { "文本列表", "Text list" } },
    { "input.roi_list.name", { "ROI 列表", "ROI list" } },
    { "input.text_color.name", { "文本颜色", "Text color" } },
    { "input.font_size.name", { "字体大小", "Font size" } },
    { "output.image.name", { "输出图像", "Output image" } },
    { "output.status.name", { "运行状态", "Execute status" } },
    { "input.image.desc", { "输入图像", "Input image" } },
    { "input.text_list.desc", { "待叠加显示的文本列表", "Text list to overlay on image" } },
    { "input.roi_list.desc", { "待绘制的 ROI 几何列表", "ROI geometry list to draw" } },
    { "input.text_color.desc", { "文本与 ROI 的绘制颜色", "Color for text and ROI drawing" } },
    { "input.font_size.desc", { "文本字体大小", "Font size for text" } },
    { "msg.input_null", { "输入图像为空", "Input image is null" } },
    { "msg.text_roi_count_mismatch", { "文本列表与 ROI 列表数量不一致", "Text list and ROI list count mismatch" } },
    { "msg.roi_3d_not_supported", { "ROI 列表包含不支持的 3D 几何", "ROI list contains unsupported 3D geometry" } },
    { "msg.success", { "文本显示叠加成功", "Text display overlay success" } },
    { "option.color.green", { "绿色", "Green" } },
    { "option.color.red", { "红色", "Red" } },
    { "option.color.blue", { "蓝色", "Blue" } },
    { "option.color.yellow", { "黄色", "Yellow" } },
    { "option.color.cyan", { "青色", "Cyan" } },
    { "option.color.magenta", { "品红", "Magenta" } },
    { "option.color.white", { "白色", "White" } },
    { "option.color.black", { "黑色", "Black" } },
};

std::string Tr(int language, const std::string& key)
{
    return hvi18n::Translate(kTexts, key, language);
}

struct ColorPreset {
    int id;
    const char* label_key;
    cv::Scalar bgr;
    cv::Scalar bgra;
    cv::Scalar gray;
};

const ColorPreset kColorPresets[] = {
    { 0, "option.color.green",   cv::Scalar(0, 255, 0),   cv::Scalar(0, 255, 0, 255),   cv::Scalar(128) },
    { 1, "option.color.red",     cv::Scalar(0, 0, 255),   cv::Scalar(0, 0, 255, 255),   cv::Scalar(76) },
    { 2, "option.color.blue",    cv::Scalar(255, 0, 0),   cv::Scalar(255, 0, 0, 255),   cv::Scalar(29) },
    { 3, "option.color.yellow",  cv::Scalar(0, 255, 255), cv::Scalar(0, 255, 255, 255), cv::Scalar(226) },
    { 4, "option.color.cyan",    cv::Scalar(255, 255, 0), cv::Scalar(255, 255, 0, 255), cv::Scalar(178) },
    { 5, "option.color.magenta", cv::Scalar(255, 0, 255), cv::Scalar(255, 0, 255, 255), cv::Scalar(105) },
    { 6, "option.color.white",   cv::Scalar(255, 255, 255), cv::Scalar(255, 255, 255, 255), cv::Scalar(255) },
    { 7, "option.color.black",   cv::Scalar(0, 0, 0),     cv::Scalar(0, 0, 0, 255),     cv::Scalar(0) },
};

cv::Scalar GetColor(int color_id, int channels)
{
    for (const auto& preset : kColorPresets) {
        if (preset.id == color_id) {
            if (channels == 1) return preset.gray;
            if (channels == 4) return preset.bgra;
            return preset.bgr;
        }
    }
    return (channels == 1) ? cv::Scalar(128) : cv::Scalar(0, 255, 0);
}

void DrawRoiPoint(cv::Mat& image, const HVPoint& pt, const cv::Scalar& color)
{
    cv::Point p(static_cast<int>(pt.x), static_cast<int>(pt.y));
    cv::circle(image, p, 3, color, -1);
    cv::circle(image, p, 5, color, 1);
}

void DrawRoiLineSegment(cv::Mat& image, const HVLineSegment& seg, const cv::Scalar& color)
{
    cv::Point p1(static_cast<int>(seg.start_point_.x), static_cast<int>(seg.start_point_.y));
    cv::Point p2(static_cast<int>(seg.end_point_.x), static_cast<int>(seg.end_point_.y));
    cv::line(image, p1, p2, color, 2);
}

void DrawRoiRectangle(cv::Mat& image, const HVRect& rect, const cv::Scalar& color)
{
    cv::Rect r(static_cast<int>(rect.x_), static_cast<int>(rect.y_),
               static_cast<int>(rect.width_), static_cast<int>(rect.height_));
    cv::rectangle(image, r, color, 2);
}

void DrawRoiRotatedRectangle(cv::Mat& image, const HVRotatedRect& rrect, const cv::Scalar& color)
{
    auto vertices = rrect.VerticesClockwise();
    std::vector<cv::Point> pts;
    pts.reserve(4);
    for (const auto& v : vertices) {
        pts.emplace_back(static_cast<int>(v.x), static_cast<int>(v.y));
    }
    cv::polylines(image, pts, true, color, 2);
}

void DrawRoi(cv::Mat& image, const HVGeometryInfo& geometry, const cv::Scalar& color)
{
    switch (geometry.shape_type_) {
    case HVGeometryShapeType::Point:
        DrawRoiPoint(image, geometry.AsPoint(), color);
        break;
    case HVGeometryShapeType::LineSegment:
        DrawRoiLineSegment(image, geometry.AsLineSegment(), color);
        break;
    case HVGeometryShapeType::Rectangle:
        DrawRoiRectangle(image, geometry.AsRect(), color);
        break;
    case HVGeometryShapeType::RotatedRectangle:
        DrawRoiRotatedRectangle(image, geometry.AsRotatedRect(), color);
        break;
    default:
        break;
    }
}

cv::Point GetTextAnchor(const HVGeometryInfo& geometry, int offset_x, int offset_y)
{
    switch (geometry.shape_type_) {
    case HVGeometryShapeType::Point:
        return cv::Point(static_cast<int>(geometry.AsPoint().x) + offset_x,
                         static_cast<int>(geometry.AsPoint().y) + offset_y);
    case HVGeometryShapeType::LineSegment: {
        const auto& seg = geometry.AsLineSegment();
        int min_x = static_cast<int>(std::min(seg.start_point_.x, seg.end_point_.x));
        int min_y = static_cast<int>(std::min(seg.start_point_.y, seg.end_point_.y));
        return cv::Point(min_x + offset_x, min_y + offset_y);
    }
    case HVGeometryShapeType::Rectangle:
        return cv::Point(static_cast<int>(geometry.AsRect().x_) + offset_x,
                         static_cast<int>(geometry.AsRect().y_) + offset_y);
    case HVGeometryShapeType::RotatedRectangle: {
        auto vertices = geometry.AsRotatedRect().VerticesClockwise();
        int min_x = static_cast<int>(vertices[0].x);
        int min_y = static_cast<int>(vertices[0].y);
        for (int i = 1; i < 4; ++i) {
            min_x = std::min(min_x, static_cast<int>(vertices[i].x));
            min_y = std::min(min_y, static_cast<int>(vertices[i].y));
        }
        return cv::Point(min_x + offset_x, min_y + offset_y);
    }
    default:
        return cv::Point(offset_x, offset_y);
    }
}

bool Is2DShape(HVGeometryShapeType shape_type)
{
    return shape_type == HVGeometryShapeType::Point ||
           shape_type == HVGeometryShapeType::LineSegment ||
           shape_type == HVGeometryShapeType::Rectangle ||
           shape_type == HVGeometryShapeType::RotatedRectangle;
}

void ClampPoint(cv::Point& pt, int img_width, int img_height)
{
    if (pt.x < 0) pt.x = 0;
    if (pt.y < 0) pt.y = 0;
    if (pt.x >= img_width) pt.x = img_width - 1;
    if (pt.y >= img_height) pt.y = img_height - 1;
}

}  // namespace

HVImageTextDisplay::HVImageTextDisplay()
{
    input_image_
        .SetSchemaName("input_image")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.image.name"); })
        .SetDescriptionResolver([this]() { return Tr(current_language(), "input.image.desc"); })
        .SetBindable(true)
        .SetEditable(true)
        .SetPersist(false)
        .SetExposeCurrentValue(true)
        .SetParamGroup(PARAM_GROUP_BASIC);

    text_list_
        .SetSchemaName("text_list")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.text_list.name"); })
        .SetDescriptionResolver([this]() { return Tr(current_language(), "input.text_list.desc"); })
        .SetBindable(true)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_BASIC);

    roi_list_
        .SetSchemaName("roi_list")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.roi_list.name"); })
        .SetDescriptionResolver([this]() { return Tr(current_language(), "input.roi_list.desc"); })
        .SetBindable(true)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_BASIC);

    {
        OptionsConstraint color_options;
        for (const auto& preset : kColorPresets) {
            color_options.AddOption(preset.id, preset.label_key);
        }
        color_options.default_index = 0;
        text_color_
            .SetSchemaName("text_color")
            .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.text_color.name"); })
            .SetDescriptionResolver([this]() { return Tr(current_language(), "input.text_color.desc"); })
            .SetBindable(false)
            .SetEditable(true)
            .SetPersist(true)
            .SetOptionsConstraint(color_options)
            .SetMetadataCustomizer([this](ParamMetadata& metadata) {
                auto& labels = metadata.options_constraint.option_labels;
                constexpr int preset_count = sizeof(kColorPresets) / sizeof(kColorPresets[0]);
                for (int i = 0; i < static_cast<int>(labels.size()) && i < preset_count; ++i) {
                    labels[i] = Tr(current_language(), kColorPresets[i].label_key);
                }
            })
            .SetParamGroup(PARAM_GROUP_ADVANCED);
    }

    font_size_
        .SetSchemaName("font_size")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.font_size.name"); })
        .SetDescriptionResolver([this]() { return Tr(current_language(), "input.font_size.desc"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetRangeConstraint(8, 120, 20)
        .SetParamGroup(PARAM_GROUP_ADVANCED);

    output_image_
        .SetSchemaName("output_image")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "output.image.name"); })
        .SetVisibility(HVOutputVisibility::OnSuccess);

    execute_status_output_
        .SetSchemaName("execute_status")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "output.status.name"); })
        .SetVisibility(HVOutputVisibility::Always)
        .BindExternalValue(execute_status_);

    RegisterInputField(input_image_);
    RegisterInputField(text_list_);
    RegisterInputField(roi_list_);
    RegisterInputField(text_color_);
    RegisterInputField(font_size_);
    RegisterOutputField(output_image_);
    RegisterOutputField(execute_status_output_);
}

int HVImageTextDisplay::init()
{
    ResetRuntimeState();
    output_image_.value().reset();
    return SUCCESS;
}

int HVImageTextDisplay::run()
{
    const auto start = std::chrono::steady_clock::now();
    execute_status_ = NODE_STATUS_RUNNING;
    error_message_key_.clear();
    output_image_.value().reset();

    if (!input_image_.value()) {
        return FailWithMessage(ALGORITHM_RUN_ERROR, "msg.input_null");
    }

    const auto& text_values = text_list_.value().values;
    const auto& roi_values = roi_list_.value().values;

    if (!text_values.empty() && !roi_values.empty() &&
        text_values.size() != roi_values.size()) {
        return FailWithMessage(ALGORITHM_RUN_ERROR, "msg.text_roi_count_mismatch");
    }

    for (const auto& roi : roi_values) {
        if (!Is2DShape(roi.shape_type_)) {
            return FailWithMessage(ALGORITHM_RUN_ERROR, "msg.roi_3d_not_supported");
        }
    }

    cv::Mat src = ImageConverter::ToMat(*input_image_.value());
    cv::Mat output = src.clone();
    const cv::Scalar color = GetColor(text_color_.value(), output.channels());
    const int font_size = font_size_.value();
    const double font_scale = static_cast<double>(font_size) / 20.0;

    const int img_width = static_cast<int>(input_image_.value()->width);
    const int img_height = static_cast<int>(input_image_.value()->height);

    if (!roi_values.empty()) {
        for (size_t i = 0; i < roi_values.size(); ++i) {
            DrawRoi(output, roi_values[i], color);

            if (!text_values.empty() && i < text_values.size()) {
                cv::Point anchor = GetTextAnchor(roi_values[i], 6, -6);
                ClampPoint(anchor, img_width, img_height);
                cv::putText(output, text_values[i], anchor,
                            cv::FONT_HERSHEY_SIMPLEX, font_scale, color, 2);
            }
        }
    }
    else if (!text_values.empty()) {
        const int text_margin = 10;
        const int line_spacing = static_cast<int>(font_size * 1.4);
        for (size_t i = 0; i < text_values.size(); ++i) {
            cv::Point anchor(text_margin, text_margin + static_cast<int>(i) * line_spacing + font_size);
            ClampPoint(anchor, img_width, img_height);
            cv::putText(output, text_values[i], anchor,
                        cv::FONT_HERSHEY_SIMPLEX, font_scale, color, 2);
        }
    }

    output_image_.value() = std::make_shared<ImageDataInfo2D>(ImageConverter::FromMat(output));

    const auto end = std::chrono::steady_clock::now();
    run_time_ = static_cast<long>(
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
    execute_status_ = SUCCESS;
    error_message_key_ = "msg.success";
    return SUCCESS;
}

AlgorithmType HVImageTextDisplay::get_algorithm_type()
{
    return AlgorithmType::ImageProcess;
}

std::string HVImageTextDisplay::get_algorithm_display_name()
{
    return Tr(current_language(), "algorithm.display");
}

std::string HVImageTextDisplay::TranslateText(const std::string& key) const
{
    return Tr(current_language(), key);
}

extern "C" __declspec(dllexport) NodeEngine* CreateInstance()
{
    return new HVImageTextDisplay();
}

extern "C" __declspec(dllexport) std::string GetInstanceName()
{
    return "Text display";
}

extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion()
{
    return NODE_ENGINE_ABI_VERSION;
}
