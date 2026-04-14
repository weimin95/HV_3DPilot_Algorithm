#include "HVLineLaser3DCameraContract.h"
#include "HVLineLaser3DCameraConversion.h"

#include "3d_pilot_public_def.h"

#include <Windows.h>

#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::string WideToCurrentCodePage(const wchar_t* text)
{
    const int buffer_size = WideCharToMultiByte(CP_ACP, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (buffer_size <= 0) {
        return {};
    }

    std::string result(static_cast<size_t>(buffer_size), '\0');
    WideCharToMultiByte(CP_ACP, 0, text, -1, &result[0], buffer_size, nullptr, nullptr);
    if (!result.empty() && result.back() == '\0') {
        result.pop_back();
    }
    return result;
}

void ExpectContract()
{
    const auto contract = line_laser_3d_camera::BuildContract();
    const std::string zh_display_name = WideToCurrentCodePage(L"\u7EBF\u6FC0\u51493D\u76F8\u673A");

    Require(contract.algorithm_name == "Line Laser 3D Camera", "algorithm name");
    Require(contract.zh_display_name == zh_display_name, "zh display name");
    Require(contract.en_display_name == "Line Laser 3D Camera", "en display name");
    Require(contract.algorithm_type == AlgorithmType::Capture, "algorithm type");

    const std::vector<std::string> input_names = {
        "ip",
        "device_id",
        "batch_points",
        "head_index",
        "connect_timeout_ms",
        "wait_timeout_ms",
        "y_pitch_mm",
    };
    const std::vector<int> input_types = {
        HV_STRING,
        HV_INT,
        HV_INT,
        HV_INT,
        HV_INT,
        HV_INT,
        HV_DOUBLE,
    };
    Require(contract.input_names == input_names, "input names");
    Require(contract.input_types == input_types, "input types");

    const std::vector<std::string> output_names = {
        "point_cloud",
        "depth_image",
        "gray_image",
        "execute_status",
    };
    const std::vector<int> output_types = {
        HV_POINTCLOUD,
        HV_IMAGEDATAINFODEPTH,
        HV_IMAGEDATAINFO2D,
        HV_INT,
    };
    Require(contract.output_names == output_names, "output names");
    Require(contract.output_types == output_types, "output types");
}

void ExpectConversion()
{
    constexpr size_t width = 3;
    constexpr size_t height = 2;
    const std::vector<int> raw_height = {
        100000,
        line_laser_3d_camera::kInvalidHeight,
        250000,
        -50000,
        0,
        300000,
    };
    const std::vector<unsigned char> raw_gray = { 1, 2, 3, 4, 5, 6 };

    const auto cloud = line_laser_3d_camera::BuildPointCloud(raw_height, width, height, 0.2, 0.5);
    Require(cloud.points.size() == 5, "point count");
    Require(std::abs(cloud.points[0].x - 0.0) < 1e-9, "point 0 x");
    Require(std::abs(cloud.points[0].y - 0.0) < 1e-9, "point 0 y");
    Require(std::abs(cloud.points[0].z - 1.0) < 1e-9, "point 0 z");
    Require(std::abs(cloud.points[1].x - 0.4) < 1e-9, "point 1 x");
    Require(std::abs(cloud.points[1].y - 0.0) < 1e-9, "point 1 y");
    Require(std::abs(cloud.points[1].z - 2.5) < 1e-9, "point 1 z");
    Require(std::abs(cloud.points[2].x - 0.0) < 1e-9, "point 2 x");
    Require(std::abs(cloud.points[2].y - 0.5) < 1e-9, "point 2 y");
    Require(std::abs(cloud.points[2].z + 0.5) < 1e-9, "point 2 z");

    const auto depth = line_laser_3d_camera::BuildDepthImage(raw_height, width, height);
    Require(depth.width == width, "depth width");
    Require(depth.height == height, "depth height");
    Require(depth.data_type == DepthDataType::FLOAT32, "depth type");
    Require(std::abs(depth.asFloat()[0] - 1.0f) < 1e-6f, "depth value 0");
    Require(std::isnan(depth.asFloat()[1]), "depth invalid NaN");
    Require(std::abs(depth.asFloat()[5] - 3.0f) < 1e-6f, "depth value 5");

    const auto gray = line_laser_3d_camera::BuildGrayImage(raw_gray, width, height);
    Require(gray.width == width, "gray width");
    Require(gray.height == height, "gray height");
    Require(gray.channels == 1, "gray channels");
    Require(std::memcmp(gray.image_data, raw_gray.data(), raw_gray.size()) == 0, "gray data");
}

}  // namespace

int main()
{
    ExpectContract();
    ExpectConversion();
    return 0;
}
