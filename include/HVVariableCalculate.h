#pragma once

#include <string>
#include <vector>

#include "HVSchemaNodeEngine.h"

class HVStringListExpressionField final : public HVInputFieldBase {
public:
    HVStringListExpressionField& SetSchemaName(const std::string& schema_name)
    {
        schema_name_ = schema_name;
        return *this;
    }

    HVStringListExpressionField& SetStorageKey(const std::string& storage_key)
    {
        storage_key_ = storage_key;
        return *this;
    }

    HVStringListExpressionField& SetDisplayNameResolver(std::function<std::string()> resolver)
    {
        display_name_resolver_ = std::move(resolver);
        return *this;
    }

    HVStringListExpressionField& SetDescriptionResolver(std::function<std::string()> resolver)
    {
        description_resolver_ = std::move(resolver);
        return *this;
    }

    HVStringListExpressionField& SetBindable(bool bindable)
    {
        bindable_ = bindable;
        return *this;
    }

    HVStringListExpressionField& SetEditable(bool editable)
    {
        editable_ = editable;
        return *this;
    }

    HVStringListExpressionField& SetPersist(bool persist)
    {
        persist_ = persist;
        return *this;
    }

    HVStringListExpressionField& SetParamGroup(ParamGroupType param_group)
    {
        param_group_ = param_group;
        return *this;
    }

    const std::vector<std::string>& expressions() const
    {
        return expressions_;
    }

    std::vector<std::string>& expressions()
    {
        return expressions_;
    }

    void SyncPublicValue();

    void* current_value_ptr() override;
    int set_from_void(void* value_ptr) override;
    bool save_value(nlohmann::json& out_entry) const override;
    bool load_value(const nlohmann::json& entry_json) override;

protected:
    int declared_hv_type() const override
    {
        return HV_STRING_LIST;
    }

private:
    std::vector<std::string> expressions_;
    mutable HVStringList public_value_;
};

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
    HVStringListExpressionField expression_list_;

    HVOutputField<HVDoubleList> result_list_;
    HVOutputField<int> execute_status_output_;
};

extern "C" __declspec(dllexport) NodeEngine* CreateInstance();
extern "C" __declspec(dllexport) std::string GetInstanceName();
extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion();
