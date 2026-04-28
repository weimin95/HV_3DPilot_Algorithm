#pragma once

#include <string>

#include "HVSchemaNodeEngine.h"

class HVVariableCalculate : public HVSchemaNodeEngine {
public:
    HVVariableCalculate();
    ~HVVariableCalculate() override = default;

    int init() override;
    int run() override;

    AlgorithmType get_algorithm_type() override;
    std::string get_algorithm_display_name() override;

protected:
    std::string TranslateText(const std::string& key) const override;

private:
    int BuildEvaluableExpression(std::string& out_expression);
    int FailCalculation(int status, const std::string& message_key);

private:
    HVInputField<std::string> expression_{ "0" };

    HVOutputField<double> result_value_;
    HVOutputField<std::string> result_text_;
    HVOutputField<int> execute_status_output_;
};

extern "C" __declspec(dllexport) NodeEngine* CreateInstance();
extern "C" __declspec(dllexport) std::string GetInstanceName();
extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion();
