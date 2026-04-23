#pragma once

#include <memory>
#include <string>

#include "HVSchemaNodeEngine.h"

class HVImageEdge : public HVSchemaNodeEngine {
public:
    HVImageEdge();
    ~HVImageEdge() override = default;

    int init() override;
    int run() override;

    AlgorithmType get_algorithm_type() override;
    std::string get_algorithm_display_name() override;

protected:
    std::string TranslateText(const std::string& key) const override;

private:
    HVInputField<std::shared_ptr<ImageDataInfo2D>> input_image_;
    HVInputField<double> low_threshold_{ 50.0 };
    HVInputField<double> high_threshold_{ 150.0 };

    HVOutputField<std::shared_ptr<ImageDataInfo2D>> edge_image_;
    HVOutputField<int> execute_status_output_;
};

extern "C" __declspec(dllexport) NodeEngine* CreateInstance();
extern "C" __declspec(dllexport) std::string GetInstanceName();
extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion();
