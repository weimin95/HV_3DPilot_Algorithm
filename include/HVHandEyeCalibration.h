#pragma once

#include <memory>
#include <string>

#include "HVSchemaNodeEngine.h"

class HVHandEyeCalibration : public HVSchemaNodeEngine {
public:
    HVHandEyeCalibration();
    ~HVHandEyeCalibration() override = default;

    int init() override;
    int run() override;

    AlgorithmType get_algorithm_type() override;
    std::string get_algorithm_display_name() override;

protected:
    std::string TranslateText(const std::string& key) const override;

private:
    HVInputField<std::string> image_folder_;
    HVInputField<std::string> robot_pose_txt_;
    HVInputField<std::string> camera_intrinsics_yml_;
    HVInputField<std::string> output_matrix_path_;
    HVInputField<int> corners_horizontal_{ 11 };
    HVInputField<int> corners_vertical_{ 8 };
    HVInputField<double> square_size_mm_{ 30.0 };
    HVInputField<bool> is_degree_{ true };
    HVInputField<int> rotation_type_{ 0 };
    HVInputField<int> calibration_type_{ 0 };
    HVInputField<bool> optimize_{ false };
    HVInputField<bool> recalibrate_intrinsics_{ false };

    HVOutputField<double> translation_error_x_;
    HVOutputField<double> translation_error_y_;
    HVOutputField<double> translation_error_z_;
    HVOutputField<double> rotation_error_rx_;
    HVOutputField<double> rotation_error_ry_;
    HVOutputField<double> rotation_error_rz_;
    HVOutputField<int> execute_status_output_;
};

extern "C" __declspec(dllexport) NodeEngine* CreateInstance();
extern "C" __declspec(dllexport) std::string GetInstanceName();
extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion();
