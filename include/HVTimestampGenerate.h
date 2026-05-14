#pragma once

#include <string>

#include "HVSchemaNodeEngine.h"

class HVTimestampGenerate : public HVSchemaNodeEngine {
public:
    HVTimestampGenerate();
    ~HVTimestampGenerate() override = default;

    int init() override;
    int run() override;

    AlgorithmType get_algorithm_type() override;
    std::string get_algorithm_display_name() override;

protected:
    std::string TranslateText(const std::string& key) const override;

private:
    int FailWithMsg(int status, const std::string& message_key);

    HVInputField<int> format_selector_{ 0 };

    HVOutputField<std::string> timestamp_;
    HVOutputField<int> execute_status_output_;
};

extern "C" __declspec(dllexport) NodeEngine* CreateInstance();
extern "C" __declspec(dllexport) std::string GetInstanceName();
extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion();
