#pragma once

#include "3d_pilot_public_def.h"
#include "node_engine.h"
#include "param_meta_data.h"

#include <string>
#include <vector>

class HVRoiCreate : public NodeEngine {
public:
    HVRoiCreate();
    ~HVRoiCreate() override = default;

    int init() override;
    int run() override;
    int set_algorithm_params(const std::vector<void*>& params, const std::vector<int>& paramID = std::vector<int>()) override;
    std::vector<void*> get_current_params() override;
    std::vector<void*> get_algorithm_result() override;
    std::vector<int> get_algorithm_input_params_type() override;
    std::vector<int> get_algorithm_output_params_type() override;
    std::vector<std::string> get_algorithm_input_params_name() override;
    std::vector<std::string> get_algorithm_output_params_name() override;
    std::vector<bool> get_algorithm_input_params_bindable() override;
    std::vector<ParamMetadata> get_algorithm_input_params_metadata() override;
    int get_algorithm_execute_status() override;
    std::string get_algorithm_error_message() override;
    long get_algorithm_use_time() override;
    bool algorithm_params_setting_status() override;
    bool algorithm_init_status() override;
    bool save_params_to_json(const std::string& filePath) override;
    bool load_params_from_json(const std::string& filePath) override;
    AlgorithmType get_algorithm_type() override;
    void set_language(int language) override;
    int get_language() const override;
    std::string get_algorithm_display_name() override;

private:
    int ApplyParam(int param_id, void* param);
    int Fail(const std::string& message_key);

private:
    int shape_type_ = 0;
    int geometry_id_ = -1;
    std::string geometry_name_ = "ROI";
    double point_x_ = 0.0;
    double point_y_ = 0.0;
    double line_start_x_ = 0.0;
    double line_start_y_ = 0.0;
    double line_end_x_ = 0.0;
    double line_end_y_ = 0.0;
    double rect_x_ = 0.0;
    double rect_y_ = 0.0;
    double rect_width_ = 1.0;
    double rect_height_ = 1.0;
    double rotated_rect_center_x_ = 0.0;
    double rotated_rect_center_y_ = 0.0;
    double rotated_rect_width_ = 1.0;
    double rotated_rect_height_ = 1.0;
    double rotated_rect_angle_deg_ = 0.0;
    double box_center_x_ = 0.0;
    double box_center_y_ = 0.0;
    double box_center_z_ = 0.0;
    double box_length_ = 1.0;
    double box_width_ = 1.0;
    double box_height_ = 1.0;
    double rotated_box_center_x_ = 0.0;
    double rotated_box_center_y_ = 0.0;
    double rotated_box_center_z_ = 0.0;
    double rotated_box_length_ = 1.0;
    double rotated_box_width_ = 1.0;
    double rotated_box_height_ = 1.0;
    double rotated_box_roll_deg_ = 0.0;
    double rotated_box_pitch_deg_ = 0.0;
    double rotated_box_yaw_deg_ = 0.0;

    HVGeometryInfo roi_;
    int execute_status_ = NODE_STATUS_NOT_RUN;
    long run_time_ms_ = 0;
    std::string error_message_key_;
    int language_ = static_cast<int>(UIPilotLanguage::ZH_CN);
};

extern "C" __declspec(dllexport) NodeEngine* CreateInstance();
extern "C" __declspec(dllexport) std::string GetInstanceName();
extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion();
