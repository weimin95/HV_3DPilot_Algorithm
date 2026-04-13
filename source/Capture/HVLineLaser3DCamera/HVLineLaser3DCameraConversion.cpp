#include "HVLineLaser3DCameraConversion.h"

#include <cstring>
#include <limits>

namespace line_laser_3d_camera {
namespace {

double RawHeightToMillimeters(int raw_height)
{
    return static_cast<double>(raw_height) * kRawHeightToMm;
}

size_t ExpectedCount(size_t width, size_t height)
{
    return width * height;
}

}  // namespace

double ResolveYPitch(double x_pitch_mm, double y_pitch_mm)
{
    return y_pitch_mm > 0.0 ? y_pitch_mm : x_pitch_mm;
}

HVPointCloud BuildPointCloud(
    const std::vector<int>& raw_height,
    size_t width,
    size_t height,
    double x_pitch_mm,
    double y_pitch_mm)
{
    HVPointCloud cloud;
    const size_t expected_count = ExpectedCount(width, height);
    if (width == 0 || height == 0 || raw_height.size() < expected_count) {
        return cloud;
    }

    const double resolved_y_pitch = ResolveYPitch(x_pitch_mm, y_pitch_mm);
    cloud.points.reserve(expected_count);
    for (size_t row = 0; row < height; ++row) {
        for (size_t col = 0; col < width; ++col) {
            const size_t index = row * width + col;
            const int height_value = raw_height[index];
            if (height_value == kInvalidHeight) {
                continue;
            }
            cloud.points.emplace_back(
                static_cast<double>(col) * x_pitch_mm,
                static_cast<double>(row) * resolved_y_pitch,
                RawHeightToMillimeters(height_value));
        }
    }
    return cloud;
}

ImageDataInfoDepth BuildDepthImage(
    const std::vector<int>& raw_height,
    size_t width,
    size_t height)
{
    if (width == 0 || height == 0) {
        return ImageDataInfoDepth();
    }

    ImageDataInfoDepth depth(width, height, DepthDataType::FLOAT32, 1.0F, 0.0F);
    float* values = depth.asFloat();
    const size_t expected_count = ExpectedCount(width, height);
    for (size_t index = 0; index < expected_count; ++index) {
        if (index >= raw_height.size() || raw_height[index] == kInvalidHeight) {
            values[index] = std::numeric_limits<float>::quiet_NaN();
            continue;
        }
        values[index] = static_cast<float>(RawHeightToMillimeters(raw_height[index]));
    }
    return depth;
}

ImageDataInfo2D BuildGrayImage(
    const std::vector<unsigned char>& raw_gray,
    size_t width,
    size_t height)
{
    if (width == 0 || height == 0) {
        return ImageDataInfo2D();
    }

    ImageDataInfo2D gray(width, height, 1);
    const size_t expected_count = ExpectedCount(width, height);
    if (raw_gray.size() >= expected_count) {
        std::memcpy(gray.image_data, raw_gray.data(), expected_count);
    }
    return gray;
}

}  // namespace line_laser_3d_camera
