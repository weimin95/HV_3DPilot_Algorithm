#pragma once

#include "3d_pilot_data_define.h"

#include <cstddef>
#include <vector>

namespace line_laser_3d_camera {

constexpr int kInvalidHeight = -1000000000;
constexpr double kRawHeightToMm = 1.0 / 100000.0;

double ResolveYPitch(double x_pitch_mm, double y_pitch_mm);
HVPointCloud BuildPointCloud(
    const std::vector<int>& raw_height,
    size_t width,
    size_t height,
    double x_pitch_mm,
    double y_pitch_mm);
ImageDataInfoDepth BuildDepthImage(
    const std::vector<int>& raw_height,
    size_t width,
    size_t height);
ImageDataInfo2D BuildGrayImage(
    const std::vector<unsigned char>& raw_gray,
    size_t width,
    size_t height);

}  // namespace line_laser_3d_camera
