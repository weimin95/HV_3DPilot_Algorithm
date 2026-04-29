#include "HVImageReader.h"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

bool Check(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << std::endl;
        return false;
    }
    return true;
}

bool Near(float actual, float expected)
{
    return std::fabs(actual - expected) < 1e-5f;
}

}  // namespace

int main()
{
    const std::filesystem::path depth_path =
        std::filesystem::temp_directory_path() / "hv_image_reader_depth_xyz.tiff";

    cv::Mat xyz(2, 2, CV_32FC3);
    xyz.at<cv::Vec3f>(0, 0) = cv::Vec3f(1.0f, 2.0f, 3.0f);
    xyz.at<cv::Vec3f>(0, 1) = cv::Vec3f(4.0f, 5.0f, 6.0f);
    xyz.at<cv::Vec3f>(1, 0) = cv::Vec3f(7.0f, 8.0f, 9.0f);
    xyz.at<cv::Vec3f>(1, 1) = cv::Vec3f(10.0f, 11.0f, 12.0f);

    if (!Check(cv::imwrite(depth_path.string(), xyz), "failed to write test depth TIFF")) {
        return 1;
    }
    const cv::Mat reloaded = cv::imread(depth_path.string(), cv::IMREAD_UNCHANGED);
    if (!Check(!reloaded.empty(), "failed to reload test depth TIFF")) {
        return 1;
    }
    if (!Check(reloaded.type() == CV_32FC3, "test depth TIFF should reload as CV_32FC3")) {
        return 1;
    }

    HVImageReader reader;
    if (!Check(reader.init() == SUCCESS, "reader init failed")) {
        return 1;
    }

    const std::vector<int> output_types = reader.get_algorithm_output_params_type();
    if (!Check(output_types.size() == 3, "reader should expose 2D image, depth image, and status outputs")) {
        return 1;
    }
    if (!Check(output_types[0] == HV_IMAGEDATAINFO2D, "output[0] should be 2D image")) {
        return 1;
    }
    if (!Check(output_types[1] == HV_IMAGEDATAINFODEPTH, "output[1] should be depth image")) {
        return 1;
    }
    if (!Check(output_types[2] == HV_INT, "output[2] should be execute status")) {
        return 1;
    }

    std::string depth_path_string = depth_path.string();
    if (!Check(reader.set_algorithm_params({ &depth_path_string }, { 0 }) == SUCCESS, "set path failed")) {
        return 1;
    }
    if (!Check(reader.run() == SUCCESS, "reader run failed")) {
        return 1;
    }

    const std::vector<void*> result = reader.get_algorithm_result();
    if (!Check(result.size() == 3, "reader result should contain 2D image, depth image, and status")) {
        return 1;
    }
    if (!Check(result[0] != nullptr, "2D image result holder should be present")) {
        return 1;
    }
    if (!Check(result[1] != nullptr, "depth image result holder should be present")) {
        return 1;
    }
    if (!Check(result[2] != nullptr, "status result should be present")) {
        return 1;
    }

    const auto image_result = *static_cast<std::shared_ptr<ImageDataInfo2D>*>(result[0]);
    if (!Check(image_result == nullptr, "depth TIFF should not populate the 2D image output")) {
        return 1;
    }

    const auto depth_result = *static_cast<std::shared_ptr<ImageDataInfoDepth>*>(result[1]);
    if (!Check(depth_result != nullptr, "depth TIFF should populate the depth image output")) {
        return 1;
    }
    if (!Check(depth_result->width == 2 && depth_result->height == 2, "depth dimensions mismatch")) {
        return 1;
    }

    const HVPoint3D p00 = depth_result->getPointAt(0, 0);
    const HVPoint3D p11 = depth_result->getPointAt(1, 1);
    const cv::Vec3f reloaded_p00 = reloaded.at<cv::Vec3f>(0, 0);
    const cv::Vec3f reloaded_p11 = reloaded.at<cv::Vec3f>(1, 1);
    if (!Check(Near(static_cast<float>(p00.x), reloaded_p00[0]) &&
               Near(static_cast<float>(p00.y), reloaded_p00[1]) &&
               Near(static_cast<float>(p00.z), reloaded_p00[2]),
            "first XYZ point mismatch")) {
        return 1;
    }
    if (!Check(Near(static_cast<float>(p11.x), reloaded_p11[0]) &&
               Near(static_cast<float>(p11.y), reloaded_p11[1]) &&
               Near(static_cast<float>(p11.z), reloaded_p11[2]),
            "last XYZ point mismatch")) {
        return 1;
    }

    const int status = *static_cast<int*>(result[2]);
    if (!Check(status == SUCCESS, "execute status should be SUCCESS")) {
        return 1;
    }

    return 0;
}
