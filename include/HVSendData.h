#pragma once

#include <string>

#include "HVSchemaNodeEngine.h"

class HVSendData : public HVSchemaNodeEngine {
public:
    HVSendData();
    ~HVSendData() override = default;

    int init() override;
    int run() override;

    AlgorithmType get_algorithm_type() override;
    std::string get_algorithm_display_name() override;

protected:
    std::string TranslateText(const std::string& key) const override;

private:
    HVInputField<int> send_target_{ 0 };
    HVInputField<int> select_variable_{ 0 };
    HVInputField<int> select_comm_device_{ 0 };
    HVInputField<int> data_source_binding_;

    HVOutputField<int> execute_status_output_;
};

extern "C" __declspec(dllexport) NodeEngine* CreateInstance();
extern "C" __declspec(dllexport) std::string GetInstanceName();
extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion();
