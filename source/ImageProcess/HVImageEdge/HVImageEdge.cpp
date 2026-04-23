#include "HVImageEdge.h"

#include <chrono>

#include <opencv2/opencv.hpp>

#include "HVI18n.h"
#include "HVUtils.h"

namespace {

const hvi18n::Dictionary kTexts = {
    { "algorithm.display", { "图像边缘提取", "Image edge" } },
    { "input.image.name", { "输入图像", "Input image" } },
    { "input.low_threshold.name", { "低阈值", "Low threshold" } },
    { "input.high_threshold.name", { "高阈值", "High threshold" } },
    { "output.edge.name", { "边缘图像", "Edge image" } },
    { "output.status.name", { "运行状态", "Execute status" } },
    { "input.image.desc", { "输入图像", "Input image" } },
    { "input.low_threshold.desc", { "Canny 算法低阈值", "Canny low threshold" } },
    { "input.high_threshold.desc", { "Canny 算法高阈值", "Canny high threshold" } },
    { "msg.input_null", { "输入图像为空", "Input image is null" } },
    { "msg.edge_success", { "边缘提取成功", "Extract edge success" } }
};

std::string Tr(int language, const std::string& key)
{
    return hvi18n::Translate(kTexts, key, language);
}

}  // namespace

HVImageEdge::HVImageEdge()
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

    low_threshold_
        .SetSchemaName("low_threshold")
        .SetStorageKey("th1")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.low_threshold.name"); })
        .SetDescriptionResolver([this]() { return Tr(current_language(), "input.low_threshold.desc"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_ADVANCED)
        .SetRangeConstraint(0.0, 500.0, 50.0);

    high_threshold_
        .SetSchemaName("high_threshold")
        .SetStorageKey("th2")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.high_threshold.name"); })
        .SetDescriptionResolver([this]() { return Tr(current_language(), "input.high_threshold.desc"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_ADVANCED)
        .SetRangeConstraint(0.0, 500.0, 150.0);

    edge_image_
        .SetSchemaName("edge_image")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "output.edge.name"); })
        .SetVisibility(HVOutputVisibility::OnSuccess);

    execute_status_output_
        .SetSchemaName("execute_status")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "output.status.name"); })
        .SetVisibility(HVOutputVisibility::Always)
        .BindExternalValue(execute_status_);

    RegisterInputField(input_image_);
    RegisterInputField(low_threshold_);
    RegisterInputField(high_threshold_);
    RegisterOutputField(edge_image_);
    RegisterOutputField(execute_status_output_);
}

int HVImageEdge::init()
{
    ResetRuntimeState();
    edge_image_.value().reset();
    return SUCCESS;
}

int HVImageEdge::run()
{
    const auto start = std::chrono::steady_clock::now();
    execute_status_ = NODE_STATUS_RUNNING;
    error_message_key_.clear();
    edge_image_.value().reset();

    if (!input_image_.value()) {
        return FailWithMessage(ALGORITHM_RUN_ERROR, "msg.input_null");
    }

    cv::Mat src = ImageConverter::ToMat(*input_image_.value());
    cv::Mat gray;

    if (src.channels() == 3) {
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    }
    else if (src.channels() == 4) {
        cv::cvtColor(src, gray, cv::COLOR_BGRA2GRAY);
    }
    else {
        gray = src;
    }

    cv::Mat edges;
    cv::Canny(gray, edges, low_threshold_.value(), high_threshold_.value());
    edge_image_.value() = std::make_shared<ImageDataInfo2D>(ImageConverter::FromMat(edges));

    const auto end = std::chrono::steady_clock::now();
    run_time_ = static_cast<long>(
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
    execute_status_ = SUCCESS;
    error_message_key_ = "msg.edge_success";
    return SUCCESS;
}

AlgorithmType HVImageEdge::get_algorithm_type()
{
    return AlgorithmType::ImageProcess;
}

std::string HVImageEdge::get_algorithm_display_name()
{
    return Tr(current_language(), "algorithm.display");
}

std::string HVImageEdge::TranslateText(const std::string& key) const
{
    return Tr(current_language(), key);
}

extern "C" __declspec(dllexport) NodeEngine* CreateInstance()
{
    return new HVImageEdge();
}

extern "C" __declspec(dllexport) std::string GetInstanceName()
{
    return "Image edge";
}

extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion()
{
    return NODE_ENGINE_ABI_VERSION;
}
