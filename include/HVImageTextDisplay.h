#pragma once

#include <memory>
#include <string>

#include "HVSchemaNodeEngine.h"

class HVImageTextDisplay : public HVSchemaNodeEngine {
public:
    HVImageTextDisplay();
    ~HVImageTextDisplay() override = default;

    int init() override;
    int run() override;

    AlgorithmType get_algorithm_type() override;
    std::string get_algorithm_display_name() override;

protected:
    std::string TranslateText(const std::string& key) const override;

private:
    HVInputField<std::shared_ptr<ImageDataInfo2D>> input_image_;
    HVInputField<HVStringList> text_list_;
    HVInputField<HVGeometryList> roi_list_;
    HVInputField<int> text_color_{ 0 };
    HVInputField<int> font_size_{ 20 };

    HVOutputField<std::shared_ptr<ImageDataInfo2D>> output_image_;
    HVOutputField<int> execute_status_output_;
};

extern "C" __declspec(dllexport) NodeEngine* CreateInstance();
extern "C" __declspec(dllexport) std::string GetInstanceName();
extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion();
