#pragma once

#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "3d_pliot_error.h"
#include "node_engine.h"
#include "param_meta_data.h"
#include "json.hpp"

// 输出结果的可见性策略。
// 常规结果通常只在节点运行成功时对主库可见，状态类结果则需要始终可见。
enum class HVOutputVisibility {
    OnSuccess = 0,
    Always = 1
};

class HVSchemaNodeEngine;

// C++ 字段类型到 HV 公共类型常量的映射。
// 新增字段类型时，需要补对应的 traits，自动派生逻辑才知道该字段在主库里是什么类型。
template <typename T>
struct HVFieldTypeTraits;

// 字段值与 JSON 的序列化适配。
// 这里只处理当前主库已经稳定支持持久化的值类型，运行时绑定值和复杂对象默认不参与自动保存。
template <typename T>
struct HVFieldJsonTraits {
    static constexpr bool kSupported = false;

    static bool Serialize(const T&, nlohmann::json&)
    {
        return false;
    }

    static bool Deserialize(const nlohmann::json&, T&)
    {
        return false;
    }
};

template <typename T>
struct HVFieldValueCopyTraits {
    static void Copy(const T& source_value, T& target_value)
    {
        target_value = source_value;
    }
};

template <typename TValue>
struct HVFieldValueCopyTraits<HVValueList<TValue>> {
    static void Copy(const HVValueList<TValue>& source_value, HVValueList<TValue>& target_value)
    {
        target_value.values.clear();
        target_value.values.reserve(source_value.values.size());
        for (size_t i = 0; i < source_value.values.size(); ++i) {
            target_value.values.push_back(source_value.values[i]);
        }
    }
};

template <>
struct HVFieldValueCopyTraits<HVStringList> {
    static void Copy(const HVStringList& source_value, HVStringList& target_value)
    {
        target_value.values.clear();
        target_value.values.reserve(source_value.values.size());
        for (size_t i = 0; i < source_value.values.size(); ++i) {
            target_value.values.push_back(
                std::string(source_value.values[i].c_str(), source_value.values[i].size()));
        }
    }
};

template <typename TValue>
struct HVFieldValueCopyTraits<HVSharedPtrList<TValue>> {
    static void Copy(const HVSharedPtrList<TValue>& source_value, HVSharedPtrList<TValue>& target_value)
    {
        target_value.values.clear();
        target_value.values.reserve(source_value.values.size());
        for (size_t i = 0; i < source_value.values.size(); ++i) {
            target_value.values.push_back(source_value.values[i]);
        }
    }
};

template <>
struct HVFieldJsonTraits<int> {
    static constexpr bool kSupported = true;

    static bool Serialize(const int& value, nlohmann::json& out_json)
    {
        out_json = value;
        return true;
    }

    static bool Deserialize(const nlohmann::json& value_json, int& out_value)
    {
        if (!value_json.is_number_integer()) {
            return false;
        }
        out_value = value_json.get<int>();
        return true;
    }
};

template <>
struct HVFieldJsonTraits<long> {
    static constexpr bool kSupported = true;

    static bool Serialize(const long& value, nlohmann::json& out_json)
    {
        out_json = value;
        return true;
    }

    static bool Deserialize(const nlohmann::json& value_json, long& out_value)
    {
        if (!value_json.is_number_integer()) {
            return false;
        }
        out_value = value_json.get<long>();
        return true;
    }
};

template <>
struct HVFieldJsonTraits<float> {
    static constexpr bool kSupported = true;

    static bool Serialize(const float& value, nlohmann::json& out_json)
    {
        out_json = value;
        return true;
    }

    static bool Deserialize(const nlohmann::json& value_json, float& out_value)
    {
        if (!value_json.is_number()) {
            return false;
        }
        out_value = value_json.get<float>();
        return true;
    }
};

template <>
struct HVFieldJsonTraits<double> {
    static constexpr bool kSupported = true;

    static bool Serialize(const double& value, nlohmann::json& out_json)
    {
        out_json = value;
        return true;
    }

    static bool Deserialize(const nlohmann::json& value_json, double& out_value)
    {
        if (!value_json.is_number()) {
            return false;
        }
        out_value = value_json.get<double>();
        return true;
    }
};

template <>
struct HVFieldJsonTraits<bool> {
    static constexpr bool kSupported = true;

    static bool Serialize(const bool& value, nlohmann::json& out_json)
    {
        out_json = value;
        return true;
    }

    static bool Deserialize(const nlohmann::json& value_json, bool& out_value)
    {
        if (!value_json.is_boolean()) {
            return false;
        }
        out_value = value_json.get<bool>();
        return true;
    }
};

template <>
struct HVFieldJsonTraits<std::string> {
    static constexpr bool kSupported = true;

    static bool Serialize(const std::string& value, nlohmann::json& out_json)
    {
        out_json = value;
        return true;
    }

    static bool Deserialize(const nlohmann::json& value_json, std::string& out_value)
    {
        if (!value_json.is_string()) {
            return false;
        }
        out_value = value_json.get<std::string>();
        return true;
    }
};

template <>
struct HVFieldJsonTraits<HVStringList> {
    static constexpr bool kSupported = true;

    static bool Serialize(const HVStringList& value, nlohmann::json& out_json)
    {
        out_json = nlohmann::json::array();
        for (const auto& str : value.values) {
            out_json.push_back(str);
        }
        return true;
    }

    static bool Deserialize(const nlohmann::json& value_json, HVStringList& out_value)
    {
        if (!value_json.is_array()) {
            return false;
        }
        out_value.values.clear();
        for (const auto& item : value_json) {
            if (!item.is_string()) {
                return false;
            }
            out_value.values.push_back(item.get<std::string>());
        }
        return true;
    }
};

template <>
struct HVFieldJsonTraits<HVGeometryList> {
    static constexpr bool kSupported = true;

    static bool Serialize(const HVGeometryList& value, nlohmann::json& out_json)
    {
        out_json = nlohmann::json::array();
        for (const auto& geometry : value.values) {
            nlohmann::json geom_json;
            geom_json["geometry_id"] = geometry.geometry_id_;
            geom_json["geometry_name"] = geometry.geometry_name_;
            geom_json["shape_type"] = static_cast<int>(geometry.shape_type_);

            nlohmann::json shape_json;
            switch (geometry.shape_type_) {
            case HVGeometryShapeType::Point:
                shape_json["x"] = geometry.AsPoint().x;
                shape_json["y"] = geometry.AsPoint().y;
                break;
            case HVGeometryShapeType::LineSegment:
                shape_json["start_point"]["x"] = geometry.AsLineSegment().start_point_.x;
                shape_json["start_point"]["y"] = geometry.AsLineSegment().start_point_.y;
                shape_json["end_point"]["x"] = geometry.AsLineSegment().end_point_.x;
                shape_json["end_point"]["y"] = geometry.AsLineSegment().end_point_.y;
                break;
            case HVGeometryShapeType::Rectangle:
                shape_json["x"] = geometry.AsRect().x_;
                shape_json["y"] = geometry.AsRect().y_;
                shape_json["width"] = geometry.AsRect().width_;
                shape_json["height"] = geometry.AsRect().height_;
                break;
            case HVGeometryShapeType::RotatedRectangle:
                shape_json["center"]["x"] = geometry.AsRotatedRect().center_.x;
                shape_json["center"]["y"] = geometry.AsRotatedRect().center_.y;
                shape_json["width"] = geometry.AsRotatedRect().width_;
                shape_json["height"] = geometry.AsRotatedRect().height_;
                shape_json["angle_deg"] = geometry.AsRotatedRect().angle_deg_;
                break;
            case HVGeometryShapeType::Box:
                shape_json["center"]["x"] = geometry.AsBox().center_.x;
                shape_json["center"]["y"] = geometry.AsBox().center_.y;
                shape_json["center"]["z"] = geometry.AsBox().center_.z;
                shape_json["length"] = geometry.AsBox().length_;
                shape_json["width"] = geometry.AsBox().width_;
                shape_json["height"] = geometry.AsBox().height_;
                break;
            case HVGeometryShapeType::RotatedBox:
                shape_json["center"]["x"] = geometry.AsRotatedBox().center_.x;
                shape_json["center"]["y"] = geometry.AsRotatedBox().center_.y;
                shape_json["center"]["z"] = geometry.AsRotatedBox().center_.z;
                shape_json["length"] = geometry.AsRotatedBox().length_;
                shape_json["width"] = geometry.AsRotatedBox().width_;
                shape_json["height"] = geometry.AsRotatedBox().height_;
                shape_json["orientation"]["roll_deg"] = geometry.AsRotatedBox().orientation_.roll_deg_;
                shape_json["orientation"]["pitch_deg"] = geometry.AsRotatedBox().orientation_.pitch_deg_;
                shape_json["orientation"]["yaw_deg"] = geometry.AsRotatedBox().orientation_.yaw_deg_;
                break;
            }
            geom_json["shape"] = shape_json;
            out_json.push_back(geom_json);
        }
        return true;
    }

    static bool Deserialize(const nlohmann::json& value_json, HVGeometryList& out_value)
    {
        if (!value_json.is_array()) {
            return false;
        }
        out_value.values.clear();
        for (const auto& item : value_json) {
            HVGeometryInfo geometry;
            geometry.geometry_id_ = item.value("geometry_id", -1);
            geometry.geometry_name_ = item.value("geometry_name", std::string());
            const auto shape_type = static_cast<HVGeometryShapeType>(
                item.value("shape_type", 0));
            const nlohmann::json shape_json = item.value("shape", nlohmann::json::object());

            switch (shape_type) {
            case HVGeometryShapeType::Point:
                geometry.SetPoint(HVPoint(
                    shape_json.value("x", 0.0),
                    shape_json.value("y", 0.0)));
                break;
            case HVGeometryShapeType::LineSegment:
                geometry.SetLineSegment(HVLineSegment(
                    HVPoint(
                        shape_json.value("start_point", nlohmann::json::object()).value("x", 0.0),
                        shape_json.value("start_point", nlohmann::json::object()).value("y", 0.0)),
                    HVPoint(
                        shape_json.value("end_point", nlohmann::json::object()).value("x", 0.0),
                        shape_json.value("end_point", nlohmann::json::object()).value("y", 0.0))));
                break;
            case HVGeometryShapeType::Rectangle:
                geometry.SetRect(HVRect(
                    shape_json.value("x", 0.0),
                    shape_json.value("y", 0.0),
                    shape_json.value("width", 0.0),
                    shape_json.value("height", 0.0)));
                break;
            case HVGeometryShapeType::RotatedRectangle:
                geometry.SetRotatedRect(HVRotatedRect(
                    HVPoint(
                        shape_json.value("center", nlohmann::json::object()).value("x", 0.0),
                        shape_json.value("center", nlohmann::json::object()).value("y", 0.0)),
                    shape_json.value("width", 0.0),
                    shape_json.value("height", 0.0),
                    shape_json.value("angle_deg", 0.0)));
                break;
            case HVGeometryShapeType::Box:
                geometry.SetBox(HVBox(
                    HVPoint3D(
                        shape_json.value("center", nlohmann::json::object()).value("x", 0.0),
                        shape_json.value("center", nlohmann::json::object()).value("y", 0.0),
                        shape_json.value("center", nlohmann::json::object()).value("z", 0.0)),
                    shape_json.value("length", 0.0),
                    shape_json.value("width", 0.0),
                    shape_json.value("height", 0.0)));
                break;
            case HVGeometryShapeType::RotatedBox: {
                const nlohmann::json orientation_json =
                    shape_json.value("orientation", nlohmann::json::object());
                geometry.SetRotatedBox(HVRotatedBox(
                    HVPoint3D(
                        shape_json.value("center", nlohmann::json::object()).value("x", 0.0),
                        shape_json.value("center", nlohmann::json::object()).value("y", 0.0),
                        shape_json.value("center", nlohmann::json::object()).value("z", 0.0)),
                    shape_json.value("length", 0.0),
                    shape_json.value("width", 0.0),
                    shape_json.value("height", 0.0),
                    HVOrientation3D(
                        orientation_json.value("roll_deg", 0.0),
                        orientation_json.value("pitch_deg", 0.0),
                        orientation_json.value("yaw_deg", 0.0))));
                break;
            }
            default:
                return false;
            }
            out_value.values.push_back(geometry);
        }
        return true;
    }
};

template <>
struct HVFieldTypeTraits<int> {
    static constexpr int kType = HV_INT;
};

template <>
struct HVFieldTypeTraits<long> {
    static constexpr int kType = HV_LONG;
};

template <>
struct HVFieldTypeTraits<float> {
    static constexpr int kType = HV_FLOAT;
};

template <>
struct HVFieldTypeTraits<double> {
    static constexpr int kType = HV_DOUBLE;
};

template <>
struct HVFieldTypeTraits<bool> {
    static constexpr int kType = HV_BOOLEAN;
};

template <>
struct HVFieldTypeTraits<std::string> {
    static constexpr int kType = HV_STRING;
};

template <>
struct HVFieldTypeTraits<std::shared_ptr<ImageDataInfo2D>> {
    static constexpr int kType = HV_IMAGEDATAINFO2D;
};

template <>
struct HVFieldTypeTraits<std::shared_ptr<HVPointCloud>> {
    static constexpr int kType = HV_POINTCLOUD;
};

template <>
struct HVFieldTypeTraits<HVGeometryInfo> {
    static constexpr int kType = HV_IMAGEROI;
};

template <>
struct HVFieldTypeTraits<HVDoubleList> {
    static constexpr int kType = HV_DOUBLE_LIST;
};

template <>
struct HVFieldTypeTraits<HVStringList> {
    static constexpr int kType = HV_STRING_LIST;
};

template <>
struct HVFieldTypeTraits<HVGeometryList> {
    static constexpr int kType = HV_GEOMETRY_LIST;
};

// 输入字段的公共抽象基类。
// 插件只需要定义具体的 HVInputField<T>，基类负责让 HVSchemaNodeEngine 以统一方式读取字段定义。
class HVInputFieldBase {
public:
    virtual ~HVInputFieldBase() = default;

    const std::string& schema_name() const { return schema_name_; }
    const std::string& storage_key() const { return storage_key_; }
    int hv_type() const { return type_override_ >= 0 ? type_override_ : declared_hv_type(); }
    bool bindable() const { return bindable_; }
    bool editable() const { return editable_; }
    bool persist() const { return persist_; }

    std::string display_name() const
    {
        if (display_name_resolver_) {
            return display_name_resolver_();
        }
        return schema_name_;
    }

    ParamMetadata build_metadata() const
    {
        ParamMetadata metadata;
        metadata.param_name = schema_name_;
        metadata.param_description = description_resolver_ ? description_resolver_() : std::string();
        metadata.param_type = hv_type();
        metadata.param_group = param_group_;
        metadata.constraint_type = constraint_type_;
        metadata.range_constraint = range_constraint_;
        metadata.options_constraint = options_constraint_;
        metadata.regex_pattern = regex_pattern_;
        metadata.file_filter = file_filter_;
        metadata.dependencies = dependencies_;
        if (metadata_customizer_) {
            metadata_customizer_(metadata);
        }
        return metadata;
    }

    virtual void* current_value_ptr() = 0;
    virtual int set_from_void(void* value_ptr) = 0;
    virtual bool save_value(nlohmann::json& out_entry) const = 0;
    virtual bool load_value(const nlohmann::json& entry_json) = 0;

protected:
    friend class HVSchemaNodeEngine;
    // 默认类型由模板参数决定，少数场景例如 HV_ANYINPUT 会通过 type_override_ 覆盖。
    virtual int declared_hv_type() const = 0;

    std::string schema_name_;
    std::string storage_key_;
    std::function<std::string()> display_name_resolver_;
    std::function<std::string()> description_resolver_;
    int type_override_ = -1;
    bool bindable_ = false;
    bool editable_ = true;
    bool persist_ = true;
    bool expose_current_value_ = true;
    ParamGroupType param_group_ = PARAM_GROUP_BASIC;
    ParamConstraintType constraint_type_ = CONSTRAINT_NONE;
    RangeConstraint range_constraint_;
    OptionsConstraint options_constraint_;
    std::string regex_pattern_;
    std::string file_filter_;
    std::vector<ParamDependency> dependencies_;
    std::function<void(ParamMetadata&)> metadata_customizer_;
};

// 输出字段的公共抽象基类。
// 输出字段不参与 set_algorithm_params，只负责把插件内部结果暴露给主库。
class HVOutputFieldBase {
public:
    virtual ~HVOutputFieldBase() = default;

    int hv_type() const { return type_override_ >= 0 ? type_override_ : declared_hv_type(); }

    std::string display_name() const
    {
        if (display_name_resolver_) {
            return display_name_resolver_();
        }
        return schema_name_;
    }

    virtual void* result_value_ptr(int execute_status) = 0;

protected:
    friend class HVSchemaNodeEngine;

    virtual int declared_hv_type() const = 0;

    std::string schema_name_;
    std::function<std::string()> display_name_resolver_;
    int type_override_ = -1;
    HVOutputVisibility visibility_ = HVOutputVisibility::OnSuccess;
};

// 结构化输入字段。
// 一个字段对象同时承载名称、类型、约束、可绑定性、持久化策略和值本身，避免在多个 getter 中重复维护。
template <typename T>
class HVInputField final : public HVInputFieldBase {
public:
    HVInputField() = default;

    explicit HVInputField(const T& initial_value)
        : value_(initial_value)
    {
    }

    T& value()
    {
        return value_;
    }

    const T& value() const
    {
        return value_;
    }

    HVInputField& SetSchemaName(const std::string& schema_name)
    {
        schema_name_ = schema_name;
        return *this;
    }

    HVInputField& SetStorageKey(const std::string& storage_key)
    {
        storage_key_ = storage_key;
        return *this;
    }

    HVInputField& SetDisplayNameResolver(std::function<std::string()> resolver)
    {
        display_name_resolver_ = std::move(resolver);
        return *this;
    }

    HVInputField& SetDescriptionResolver(std::function<std::string()> resolver)
    {
        description_resolver_ = std::move(resolver);
        return *this;
    }

    HVInputField& SetTypeOverride(int type_override)
    {
        type_override_ = type_override;
        return *this;
    }

    HVInputField& SetBindable(bool bindable)
    {
        bindable_ = bindable;
        return *this;
    }

    HVInputField& SetEditable(bool editable)
    {
        editable_ = editable;
        return *this;
    }

    HVInputField& SetPersist(bool persist)
    {
        persist_ = persist;
        return *this;
    }

    HVInputField& SetExposeCurrentValue(bool expose_current_value)
    {
        expose_current_value_ = expose_current_value;
        return *this;
    }

    HVInputField& SetParamGroup(ParamGroupType param_group)
    {
        param_group_ = param_group;
        return *this;
    }

    HVInputField& SetRangeConstraint(double min_value, double max_value, double default_value)
    {
        constraint_type_ = CONSTRAINT_RANGE;
        range_constraint_ = RangeConstraint(min_value, max_value, default_value);
        return *this;
    }

    HVInputField& SetOptionsConstraint(const OptionsConstraint& options_constraint)
    {
        constraint_type_ = CONSTRAINT_OPTIONS;
        options_constraint_ = options_constraint;
        return *this;
    }

    HVInputField& SetRegexConstraint(const std::string& regex_pattern)
    {
        constraint_type_ = CONSTRAINT_REGEX;
        regex_pattern_ = regex_pattern;
        return *this;
    }

    HVInputField& SetFilePathConstraint(const std::string& file_filter)
    {
        constraint_type_ = CONSTRAINT_FILE_PATH;
        file_filter_ = file_filter;
        return *this;
    }

    HVInputField& SetDirectoryPathConstraint()
    {
        constraint_type_ = CONSTRAINT_DIRECTORY_PATH;
        return *this;
    }

    HVInputField& AddDependency(const ParamDependency& dependency)
    {
        dependencies_.push_back(dependency);
        return *this;
    }

    HVInputField& SetMetadataCustomizer(std::function<void(ParamMetadata&)> customizer)
    {
        metadata_customizer_ = std::move(customizer);
        return *this;
    }

    HVInputField& SetNullResetValue(const T& null_reset_value)
    {
        null_reset_value_ = null_reset_value;
        return *this;
    }

    void* current_value_ptr() override
    {
        // 运行时绑定专用字段（例如 HV_ANYINPUT）不向主库暴露静态值指针。
        if (!editable_ || !expose_current_value_) {
            return nullptr;
        }
        return &value_;
    }

    int set_from_void(void* value_ptr) override
    {
        if (!editable_) {
            // 不可值编辑的字段允许通过 nullptr 做清空或重置，但不接受真实值写入。
            return value_ptr == nullptr ? SUCCESS : INVALID_PARAMS_NUM;
        }

        if (value_ptr == nullptr) {
            value_ = null_reset_value_;
            return SUCCESS;
        }

        HVFieldValueCopyTraits<T>::Copy(*static_cast<T*>(value_ptr), value_);
        return SUCCESS;
    }

    bool save_value(nlohmann::json& out_entry) const override
    {
        if (!persist_) {
            return true;
        }
        if constexpr (!HVFieldJsonTraits<T>::kSupported) {
            return false;
        }

        nlohmann::json value_json;
        if (!HVFieldJsonTraits<T>::Serialize(value_, value_json)) {
            return false;
        }
        out_entry["name"] = storage_key_.empty() ? schema_name_ : storage_key_;
        out_entry["type"] = hv_type();
        out_entry["value"] = value_json;
        return true;
    }

    bool load_value(const nlohmann::json& entry_json) override
    {
        if (!persist_) {
            return true;
        }
        if constexpr (!HVFieldJsonTraits<T>::kSupported) {
            return false;
        }

        if (!entry_json.contains("value")) {
            return false;
        }
        return HVFieldJsonTraits<T>::Deserialize(entry_json["value"], value_);
    }

protected:
    int declared_hv_type() const override
    {
        return HVFieldTypeTraits<T>::kType;
    }

private:
    T value_{};
    T null_reset_value_{};
};

// 结构化输出字段。
// 默认持有自己的结果值，也可以绑定到插件已有成员变量，避免双份状态。
template <typename T>
class HVOutputField final : public HVOutputFieldBase {
public:
    HVOutputField() = default;

    explicit HVOutputField(const T& initial_value)
        : value_(initial_value)
    {
    }

    T& value()
    {
        return external_value_ != nullptr ? *external_value_ : value_;
    }

    const T& value() const
    {
        return external_value_ != nullptr ? *external_value_ : value_;
    }

    HVOutputField& SetSchemaName(const std::string& schema_name)
    {
        schema_name_ = schema_name;
        return *this;
    }

    HVOutputField& SetDisplayNameResolver(std::function<std::string()> resolver)
    {
        display_name_resolver_ = std::move(resolver);
        return *this;
    }

    HVOutputField& SetTypeOverride(int type_override)
    {
        type_override_ = type_override;
        return *this;
    }

    HVOutputField& SetVisibility(HVOutputVisibility visibility)
    {
        visibility_ = visibility;
        return *this;
    }

    HVOutputField& BindExternalValue(T& external_value)
    {
        external_value_ = &external_value;
        return *this;
    }

    void* result_value_ptr(int execute_status) override
    {
        // 常规输出失败时返回空指针，主库就不会继续把旧结果当成当前有效输出使用。
        if (visibility_ == HVOutputVisibility::OnSuccess && execute_status != SUCCESS && execute_status != NODE_STATUS_NOT_RUN) {
            return nullptr;
        }
        return external_value_ != nullptr ? static_cast<void*>(external_value_) : static_cast<void*>(&value_);
    }

protected:
    int declared_hv_type() const override
    {
        return HVFieldTypeTraits<T>::kType;
    }

private:
    T value_{};
    T* external_value_ = nullptr;
};

// 结构化字段驱动的 NodeEngine 基类。
// 插件在构造阶段集中注册输入输出字段后，类型、名称、结果、元数据、保存加载等接口由基类自动派生。
class HVSchemaNodeEngine : public NodeEngine {
public:
    std::vector<void*> get_current_params() override;
    std::vector<void*> get_algorithm_result() override;

    std::vector<int> get_algorithm_input_params_type() override;
    std::vector<int> get_algorithm_output_params_type() override;
    std::vector<std::string> get_algorithm_input_params_name() override;
    std::vector<std::string> get_algorithm_output_params_name() override;
    std::vector<bool> get_algorithm_input_params_bindable() override;
    std::vector<ParamMetadata> get_algorithm_input_params_metadata() override;

    int set_algorithm_params(
        const std::vector<void*>& params,
        const std::vector<int>& paramID = std::vector<int>()) override;

    int get_algorithm_execute_status() override;
    std::string get_algorithm_error_message() override;
    long get_algorithm_use_time() override;

    bool algorithm_params_setting_status() override;
    bool algorithm_init_status() override;

    bool save_params_to_json(const std::string& filePath) override;
    bool load_params_from_json(const std::string& filePath) override;

    void set_language(int language) override;
    int get_language() const override;
    void set_host_services(NodeHostServices* host_services) override;

protected:
    void RegisterInputField(HVInputFieldBase& field)
    {
        input_fields_.push_back(&field);
    }

    template <typename T>
    void RegisterInputField(HVInputField<T>& field)
    {
        RegisterInputField(static_cast<HVInputFieldBase&>(field));
    }

    template <typename T>
    void RegisterOutputField(HVOutputField<T>& field)
    {
        output_fields_.push_back(&field);
    }

    void ClearRegisteredSchema();
    // 重置每次运行前的公共执行态，不触碰插件自己的业务字段值。
    void ResetRuntimeState();
    // 设置失败状态并记录可本地化的错误消息 key，供 get_algorithm_error_message() 统一翻译。
    int FailWithMessage(int status, const std::string& message_key);

    NodeHostServices* host_services() const { return host_services_; }
    int current_language() const { return language_; }
    int current_execute_status() const { return execute_status_; }
    // 插件自己提供文本翻译入口，基类只负责在统一位置调用。
    virtual std::string TranslateText(const std::string& key) const = 0;

protected:
    int execute_status_ = NODE_STATUS_NOT_RUN;
    long run_time_ = 0;
    std::string error_message_key_;
    int language_ = static_cast<int>(UIPilotLanguage::ZH_CN);
    NodeHostServices* host_services_ = nullptr;

private:
    std::vector<HVInputFieldBase*> input_fields_;
    std::vector<HVOutputFieldBase*> output_fields_;
};
