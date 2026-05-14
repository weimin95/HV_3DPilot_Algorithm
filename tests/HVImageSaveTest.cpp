#include "HVImageSave.h"

#include "3d_pliot_error.h"

#include <chrono>
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

std::filesystem::path MakeTempOutputDir()
{
    const auto unique_name =
        "hv_image_save_test_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    return std::filesystem::temp_directory_path() / unique_name;
}

std::shared_ptr<ImageDataInfo2D> MakeTestImage()
{
    auto image = std::make_shared<ImageDataInfo2D>(2, 2, 1);
    image->pixel(0, 0, 0) = 0;
    image->pixel(0, 1, 0) = 64;
    image->pixel(1, 0, 0) = 128;
    image->pixel(1, 1, 0) = 255;
    return image;
}

}  // namespace

int main()
{
    const std::filesystem::path output_dir = MakeTempOutputDir();

    HVImageSave saver;
    if (!Check(saver.init() == SUCCESS, "save init failed")) {
        return 1;
    }

    int input_type = 0;
    auto image = MakeTestImage();
    std::string output_dir_string = output_dir.u8string();
    std::string prefix = "plain";
    bool create_date_dirs = false;
    int storage_mode = 0;
    int max_count = 0;
    double min_free_space_mb = 0.0;
    int max_days = 0;
    int save_format = 0;
    int jpeg_quality = 95;

    if (!Check(saver.set_algorithm_params({
                    &input_type,
                    &image,
                    &output_dir_string,
                    &prefix,
                    &create_date_dirs,
                    &storage_mode,
                    &max_count,
                    &min_free_space_mb,
                    &max_days,
                    &save_format,
                    &jpeg_quality,
                },
                { 0, 1, 4, 5, 6, 7, 8, 9, 10, 11, 12 }) == SUCCESS,
            "set save params failed")) {
        return 1;
    }

    if (!Check(saver.run() == SUCCESS, "first save run failed")) {
        return 1;
    }

    const std::vector<void*> first_result = saver.get_algorithm_result();
    if (!Check(first_result.size() == 3, "save result should contain path, saved flag, and status")) {
        return 1;
    }
    if (!Check(first_result[0] != nullptr, "saved path result should be present")) {
        return 1;
    }
    const auto first_path = std::filesystem::u8path(*static_cast<std::string*>(first_result[0]));
    if (!Check(first_path.filename() == "plain.bmp", "saved file name should be prefix plus extension")) {
        return 1;
    }
    if (!Check(std::filesystem::exists(first_path), "saved file should exist")) {
        return 1;
    }

    if (!Check(saver.run() == SUCCESS, "second save run failed")) {
        return 1;
    }
    const std::vector<void*> second_result = saver.get_algorithm_result();
    if (!Check(second_result.size() == 3 && second_result[0] != nullptr, "second saved path should be present")) {
        return 1;
    }
    const auto second_path = std::filesystem::u8path(*static_cast<std::string*>(second_result[0]));
    if (!Check(second_path == first_path, "subsequent saves should reuse the same prefix plus extension path")) {
        return 1;
    }

    std::error_code ec;
    std::filesystem::remove_all(output_dir, ec);
    return 0;
}
