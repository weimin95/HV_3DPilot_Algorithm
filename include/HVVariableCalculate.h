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
    int BuildEvaluableExpression(const std::string& expression_text, std::string& out_expression);
    int EvaluateExpressionText(const std::string& expression_text, double& out_value);
    int FailCalculation(int status, const std::string& message_key);

private:
    HVInputField<HVStringList> expression_list_;

    HVOutputField<HVDoubleList> result_list_;
    HVOutputField<int> execute_status_output_;
};

extern "C" __declspec(dllexport) NodeEngine* CreateInstance();
extern "C" __declspec(dllexport) std::string GetInstanceName();
extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion();
