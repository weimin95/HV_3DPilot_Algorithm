#include "HVHandEyeCalibration.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#include <windows.h>

#include <opencv2/opencv.hpp>

#include "HVI18n.h"

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// SDK struct redefinitions — must match HandEyeCalibrationParam.h layout exactly.
// The SDK header cannot be included directly (GBK encoding conflicts with
// /source-charset:utf-8).  Static asserts catch ABI drift at compile time.
// ---------------------------------------------------------------------------
namespace {

struct SdkImageDataInfo2D {
    int width;
    int height;
    int channel;
    unsigned char* image_data;

    SdkImageDataInfo2D() : width(0), height(0), channel(0), image_data(nullptr) {}
};

static_assert(sizeof(SdkImageDataInfo2D) == 24, "SdkImageDataInfo2D size mismatch with SDK");
static_assert(offsetof(SdkImageDataInfo2D, width) == 0, "SdkImageDataInfo2D::width offset mismatch");
static_assert(offsetof(SdkImageDataInfo2D, height) == 4, "SdkImageDataInfo2D::height offset mismatch");
static_assert(offsetof(SdkImageDataInfo2D, channel) == 8, "SdkImageDataInfo2D::channel offset mismatch");
static_assert(offsetof(SdkImageDataInfo2D, image_data) == 16, "SdkImageDataInfo2D::image_data offset mismatch");

struct SdkRobotPose {
    double x;
    double y;
    double z;
    double rx;
    double ry;
    double rz;

    SdkRobotPose() : x(0), y(0), z(0), rx(0), ry(0), rz(0) {}
};

static_assert(sizeof(SdkRobotPose) == 48, "SdkRobotPose size mismatch with SDK");
static_assert(offsetof(SdkRobotPose, x) == 0, "SdkRobotPose::x offset mismatch");
static_assert(offsetof(SdkRobotPose, y) == 8, "SdkRobotPose::y offset mismatch");
static_assert(offsetof(SdkRobotPose, z) == 16, "SdkRobotPose::z offset mismatch");
static_assert(offsetof(SdkRobotPose, rx) == 24, "SdkRobotPose::rx offset mismatch");
static_assert(offsetof(SdkRobotPose, ry) == 32, "SdkRobotPose::ry offset mismatch");
static_assert(offsetof(SdkRobotPose, rz) == 40, "SdkRobotPose::rz offset mismatch");

struct SdkCalibrateData {
    SdkImageDataInfo2D img;
    SdkRobotPose pose;
};

static_assert(sizeof(SdkCalibrateData) == 72, "SdkCalibrateData size mismatch with SDK");
static_assert(offsetof(SdkCalibrateData, img) == 0, "SdkCalibrateData::img offset mismatch");
static_assert(offsetof(SdkCalibrateData, pose) == 24, "SdkCalibrateData::pose offset mismatch");

struct SdkChessboardParam {
    int cornersHorizontal;
    int cornersVertical;
    double squareSize;

    SdkChessboardParam() : cornersHorizontal(0), cornersVertical(0), squareSize(0) {}
};

static_assert(sizeof(SdkChessboardParam) == 16, "SdkChessboardParam size mismatch with SDK");
static_assert(offsetof(SdkChessboardParam, cornersHorizontal) == 0, "SdkChessboardParam::cornersHorizontal offset mismatch");
static_assert(offsetof(SdkChessboardParam, cornersVertical) == 4, "SdkChessboardParam::cornersVertical offset mismatch");
static_assert(offsetof(SdkChessboardParam, squareSize) == 8, "SdkChessboardParam::squareSize offset mismatch");

// ---------------------------------------------------------------------------
// SDK function pointer types
// ---------------------------------------------------------------------------
using FnSetCalibrateData = bool (*)(const SdkCalibrateData*, int, bool, int);
using FnSetChessboardParams = void (*)(const SdkChessboardParam*);
using FnReadCameraIntrinsics = bool (*)(const char*, bool);
using FnCalibrateHandEye = bool (*)(int, bool);
using FnSaveHandEyeMatrix = bool (*)(const char*);
using FnGetCalibrateError = bool (*)(float*, int);

// ---------------------------------------------------------------------------
// i18n
// ---------------------------------------------------------------------------
const hvi18n::Dictionary kTexts = {
    { "algorithm.display", { "手眼标定", "Hand-eye calibration" } },

    { "input.image_folder.name", { "图像文件夹", "Image folder" } },
    { "input.robot_pose_txt.name", { "机器人位姿文件", "Robot pose file" } },
    { "input.camera_intrinsics_yml.name", { "相机内参文件", "Camera intrinsics file" } },
    { "input.output_matrix_path.name", { "矩阵输出路径", "Matrix output path" } },
    { "input.corners_horizontal.name", { "水平角点数", "Corners horizontal" } },
    { "input.corners_vertical.name", { "竖直角点数", "Corners vertical" } },
    { "input.square_size_mm.name", { "棋盘格尺寸(mm)", "Square size (mm)" } },
    { "input.is_degree.name", { "角度制", "Is degree" } },
    { "input.rotation_type.name", { "旋转类型", "Rotation type" } },
    { "input.calibration_type.name", { "标定类型", "Calibration type" } },
    { "input.optimize.name", { "开启优化", "Optimize" } },
    { "input.recalibrate_intrinsics.name", { "重新标定内参", "Recalibrate intrinsics" } },

    { "input.image_folder.desc", { "包含棋盘图像样本的文件夹", "Folder containing chessboard image samples" } },
    { "input.robot_pose_txt.desc", { "机器人位姿文本文件，每行 x y z rx ry rz", "Robot pose text file, one line per sample: x y z rx ry rz" } },
    { "input.camera_intrinsics_yml.desc", { "相机内参 YML 文件", "Camera intrinsics YML file" } },
    { "input.output_matrix_path.desc", { "手眼矩阵输出文件路径 (.dat / .ini)", "Hand-eye matrix output file path (.dat / .ini)" } },
    { "input.corners_horizontal.desc", { "棋盘水平方向角点数量", "Number of chessboard corners horizontally" } },
    { "input.corners_vertical.desc", { "棋盘竖直方向角点数量", "Number of chessboard corners vertically" } },
    { "input.square_size_mm.desc", { "棋盘格尺寸，单位毫米", "Chessboard square size in mm" } },
    { "input.is_degree.desc", { "位姿旋转量是否使用角度制", "Whether pose rotation uses degrees" } },
    { "input.rotation_type.desc", { "位姿旋转表示方式", "Pose rotation representation" } },
    { "input.calibration_type.desc", { "手眼标定类型", "Hand-eye calibration type" } },
    { "input.optimize.desc", { "是否开启 SDK 优化过程", "Whether to enable SDK optimization" } },
    { "input.recalibrate_intrinsics.desc", { "是否重新标定相机内参", "Whether to recalibrate camera intrinsics" } },

    { "output.translation_error_x.name", { "平移误差 X", "Translation error X" } },
    { "output.translation_error_y.name", { "平移误差 Y", "Translation error Y" } },
    { "output.translation_error_z.name", { "平移误差 Z", "Translation error Z" } },
    { "output.rotation_error_rx.name", { "旋转误差 RX", "Rotation error RX" } },
    { "output.rotation_error_ry.name", { "旋转误差 RY", "Rotation error RY" } },
    { "output.rotation_error_rz.name", { "旋转误差 RZ", "Rotation error RZ" } },
    { "output.status.name", { "运行状态", "Execute status" } },

    { "option.rotation.euler", { "欧拉角", "Euler angles" } },
    { "option.rotation.vector", { "旋转向量", "Rotation vector" } },
    { "option.calibration.eye_in_hand", { "眼在手上", "Eye in hand" } },
    { "option.calibration.eye_to_hand", { "眼在手外", "Eye to hand" } },

    { "msg.image_folder_empty", { "图像文件夹为空", "Image folder is empty" } },
    { "msg.no_supported_images", { "未找到支持的图像文件", "No supported image files found" } },
    { "msg.failed_to_read_image", { "读取图像失败: ", "Failed to read image: " } },
    { "msg.robot_pose_not_found", { "机器人位姿文件不存在", "Robot pose file not found" } },
    { "msg.pose_line_invalid", { "机器人位姿行非法: 行 ", "Robot pose line is invalid: line " } },
    { "msg.count_mismatch", { "图像数量与位姿数量不一致", "Image count does not match pose count" } },
    { "msg.too_few_samples", { "标定样本数不得少于 3", "Calibration sample count must be at least 3" } },
    { "msg.intrinsics_not_found", { "相机内参文件不存在", "Camera intrinsics file not found" } },
    { "msg.intrinsics_read_failed", { "读取相机内参失败", "Failed to read camera intrinsics" } },
    { "msg.set_calibrate_data_failed", { "设置标定数据失败", "Failed to set calibration data" } },
    { "msg.calibrate_failed", { "手眼标定失败", "Hand-eye calibration failed" } },
    { "msg.save_matrix_failed", { "保存手眼矩阵失败", "Failed to save hand-eye matrix" } },
    { "msg.get_error_failed", { "获取标定误差失败", "Failed to get calibration error" } },
    { "msg.sdk_load_failed", { "加载手眼标定 SDK 失败", "Failed to load hand-eye calibration SDK" } },
    { "msg.success", { "手眼标定成功", "Hand-eye calibration success" } },
};

std::string Tr(int language, const std::string& key)
{
    return hvi18n::Translate(kTexts, key, language);
}

// ---------------------------------------------------------------------------
// Image file extensions
// ---------------------------------------------------------------------------
const std::vector<std::wstring> kSupportedImageExts = {
    L".bmp", L".png", L".jpg", L".jpeg", L".tif", L".tiff"
};

bool IsSupportedImageExt(const std::wstring& ext)
{
    std::wstring lower = ext;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
    for (const auto& supported : kSupportedImageExts) {
        if (lower == supported) {
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// SDK DLL loader
// ---------------------------------------------------------------------------
class SdkLoader {
public:
    SdkLoader() = default;

    bool Load()
    {
        if (loaded_) {
            return true;
        }

        dll_ = LoadLibraryW(L"HandEyeCalibration.dll");
        if (!dll_) {
            DWORD err1 = GetLastError();
            diag_ += "  [1] LoadLibraryW(\"HandEyeCalibration.dll\") failed: 0x";
            diag_ += Hex(err1);
            diag_ += " (" + WinErrorStr(err1) + ")\n";
            dll_ = TryLoadFromPluginDir();
        } else {
            diag_ += "  [1] Loaded from process search path\n";
        }

        if (!dll_) {
            return false;
        }

        setCalibrateData = reinterpret_cast<FnSetCalibrateData>(GetProcAddress(dll_, "setCalibrateData"));
        if (!setCalibrateData) { return false; }
        setChessboardParams = reinterpret_cast<FnSetChessboardParams>(GetProcAddress(dll_, "setChessboardParams"));
        if (!setChessboardParams) { return false; }
        readCameraIntrinsics = reinterpret_cast<FnReadCameraIntrinsics>(GetProcAddress(dll_, "readCameraIntrinsics"));
        if (!readCameraIntrinsics) { return false; }
        calibrateHandEye = reinterpret_cast<FnCalibrateHandEye>(GetProcAddress(dll_, "calibrateHandEye"));
        if (!calibrateHandEye) { return false; }
        saveHandEyeMatrix = reinterpret_cast<FnSaveHandEyeMatrix>(GetProcAddress(dll_, "saveHandEyeMatrix"));
        if (!saveHandEyeMatrix) { return false; }
        getCalibrateError = reinterpret_cast<FnGetCalibrateError>(GetProcAddress(dll_, "getCalibrateError"));
        if (!getCalibrateError) { return false; }

        loaded_ = true;
        return true;
    }

    ~SdkLoader()
    {
        if (dll_) {
            FreeLibrary(dll_);
        }
    }

    const std::string& GetDiag() const { return diag_; }

    FnSetCalibrateData setCalibrateData = nullptr;
    FnSetChessboardParams setChessboardParams = nullptr;
    FnReadCameraIntrinsics readCameraIntrinsics = nullptr;
    FnCalibrateHandEye calibrateHandEye = nullptr;
    FnSaveHandEyeMatrix saveHandEyeMatrix = nullptr;
    FnGetCalibrateError getCalibrateError = nullptr;

private:
    static std::string Hex(DWORD v)
    {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%08lX", v);
        return buf;
    }

    static std::string WinErrorStr(DWORD err)
    {
        char* msg = nullptr;
        FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPSTR>(&msg), 0, nullptr);
        std::string result(msg ? msg : "unknown");
        if (msg) LocalFree(msg);
        // Trim trailing CRLF
        while (!result.empty() && (result.back() == '\r' || result.back() == '\n')) {
            result.pop_back();
        }
        return result;
    }

    HMODULE TryLoadFromPluginDir()
    {
        HMODULE hModule = nullptr;
        GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&CreateInstance),
            &hModule);
        if (!hModule) {
            diag_ += "  [2] GetModuleHandleExA failed\n";
            return nullptr;
        }

        char dllPath[MAX_PATH];
        GetModuleFileNameA(hModule, dllPath, MAX_PATH);
        fs::path pluginDir = fs::path(dllPath).parent_path();
        fs::path sdkPath = pluginDir / "HandEyeCalibration.dll";

        diag_ += "  [2] Plugin dir: " + pluginDir.string() + "\n";
        diag_ += "  [2] Looking for: " + sdkPath.string() + "\n";

        std::error_code ec;
        if (fs::exists(sdkPath, ec) && !ec) {
            // LOAD_WITH_ALTERED_SEARCH_PATH: also searches SDK DLL's directory for its dependencies
            HMODULE hSdk = LoadLibraryExW(sdkPath.wstring().c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
            if (!hSdk) {
                DWORD err = GetLastError();
                diag_ += "  [2] Found, but LoadLibraryExW failed: 0x";
                diag_ += Hex(err);
                diag_ += " (" + WinErrorStr(err) + ")\n";
            } else {
                diag_ += "  [2] Found, loaded successfully\n";
            }
            return hSdk;
        }

        diag_ += "  [2] Not found (";
        diag_ += ec ? ec.message() : "file missing";
        diag_ += ")\n";
        return nullptr;
    }

    HMODULE dll_ = nullptr;
    bool loaded_ = false;
    std::string diag_;
};

}  // namespace

// ---------------------------------------------------------------------------
// HVHandEyeCalibration
// ---------------------------------------------------------------------------
HVHandEyeCalibration::HVHandEyeCalibration()
{
    image_folder_
        .SetSchemaName("image_folder")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.image_folder.name"); })
        .SetDescriptionResolver([this]() { return Tr(current_language(), "input.image_folder.desc"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetDirectoryPathConstraint()
        .SetParamGroup(PARAM_GROUP_BASIC);

    robot_pose_txt_
        .SetSchemaName("robot_pose_txt")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.robot_pose_txt.name"); })
        .SetDescriptionResolver([this]() { return Tr(current_language(), "input.robot_pose_txt.desc"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetFilePathConstraint("*.txt")
        .SetParamGroup(PARAM_GROUP_BASIC);

    camera_intrinsics_yml_
        .SetSchemaName("camera_intrinsics_yml")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.camera_intrinsics_yml.name"); })
        .SetDescriptionResolver([this]() { return Tr(current_language(), "input.camera_intrinsics_yml.desc"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetFilePathConstraint("*.yml")
        .SetParamGroup(PARAM_GROUP_BASIC);

    output_matrix_path_
        .SetSchemaName("output_matrix_path")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.output_matrix_path.name"); })
        .SetDescriptionResolver([this]() { return Tr(current_language(), "input.output_matrix_path.desc"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetFilePathConstraint("*.dat;*.ini")
        .SetParamGroup(PARAM_GROUP_BASIC);

    corners_horizontal_
        .SetSchemaName("corners_horizontal")
        .SetStorageKey("corners_h")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.corners_horizontal.name"); })
        .SetDescriptionResolver([this]() { return Tr(current_language(), "input.corners_horizontal.desc"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetRangeConstraint(1, 100, 11)
        .SetParamGroup(PARAM_GROUP_BASIC);

    corners_vertical_
        .SetSchemaName("corners_vertical")
        .SetStorageKey("corners_v")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.corners_vertical.name"); })
        .SetDescriptionResolver([this]() { return Tr(current_language(), "input.corners_vertical.desc"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetRangeConstraint(1, 100, 8)
        .SetParamGroup(PARAM_GROUP_BASIC);

    square_size_mm_
        .SetSchemaName("square_size_mm")
        .SetStorageKey("sq_size")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.square_size_mm.name"); })
        .SetDescriptionResolver([this]() { return Tr(current_language(), "input.square_size_mm.desc"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetRangeConstraint(0.001, 1000, 30.0)
        .SetParamGroup(PARAM_GROUP_BASIC);

    is_degree_
        .SetSchemaName("is_degree")
        .SetStorageKey("is_deg")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.is_degree.name"); })
        .SetDescriptionResolver([this]() { return Tr(current_language(), "input.is_degree.desc"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_BASIC);

    {
        OptionsConstraint calib_opts;
        calib_opts.AddOption(0, "option.calibration.eye_in_hand");
        calib_opts.AddOption(1, "option.calibration.eye_to_hand");
        calib_opts.default_index = 0;
        calibration_type_
            .SetSchemaName("calibration_type")
            .SetStorageKey("calib_type")
            .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.calibration_type.name"); })
            .SetDescriptionResolver([this]() { return Tr(current_language(), "input.calibration_type.desc"); })
            .SetBindable(false)
            .SetEditable(true)
            .SetPersist(true)
            .SetOptionsConstraint(calib_opts)
            .SetMetadataCustomizer([this](ParamMetadata& metadata) {
                auto& labels = metadata.options_constraint.option_labels;
                if (labels.size() >= 2) {
                    labels[0] = Tr(current_language(), "option.calibration.eye_in_hand");
                    labels[1] = Tr(current_language(), "option.calibration.eye_to_hand");
                }
            })
            .SetParamGroup(PARAM_GROUP_BASIC);
    }

    {
        OptionsConstraint rot_opts;
        rot_opts.AddOption(0, "option.rotation.euler");
        rot_opts.AddOption(1, "option.rotation.vector");
        rot_opts.default_index = 0;
        rotation_type_
            .SetSchemaName("rotation_type")
            .SetStorageKey("rot_type")
            .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.rotation_type.name"); })
            .SetDescriptionResolver([this]() { return Tr(current_language(), "input.rotation_type.desc"); })
            .SetBindable(false)
            .SetEditable(true)
            .SetPersist(true)
            .SetOptionsConstraint(rot_opts)
            .SetMetadataCustomizer([this](ParamMetadata& metadata) {
                auto& labels = metadata.options_constraint.option_labels;
                if (labels.size() >= 2) {
                    labels[0] = Tr(current_language(), "option.rotation.euler");
                    labels[1] = Tr(current_language(), "option.rotation.vector");
                }
            })
            .SetParamGroup(PARAM_GROUP_ADVANCED);
    }

    optimize_
        .SetSchemaName("optimize")
        .SetStorageKey("opt")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.optimize.name"); })
        .SetDescriptionResolver([this]() { return Tr(current_language(), "input.optimize.desc"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_ADVANCED);

    recalibrate_intrinsics_
        .SetSchemaName("recalibrate_intrinsics")
        .SetStorageKey("recalib")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "input.recalibrate_intrinsics.name"); })
        .SetDescriptionResolver([this]() { return Tr(current_language(), "input.recalibrate_intrinsics.desc"); })
        .SetBindable(false)
        .SetEditable(true)
        .SetPersist(true)
        .SetParamGroup(PARAM_GROUP_ADVANCED);

    translation_error_x_
        .SetSchemaName("translation_error_x")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "output.translation_error_x.name"); })
        .SetVisibility(HVOutputVisibility::OnSuccess);

    translation_error_y_
        .SetSchemaName("translation_error_y")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "output.translation_error_y.name"); })
        .SetVisibility(HVOutputVisibility::OnSuccess);

    translation_error_z_
        .SetSchemaName("translation_error_z")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "output.translation_error_z.name"); })
        .SetVisibility(HVOutputVisibility::OnSuccess);

    rotation_error_rx_
        .SetSchemaName("rotation_error_rx")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "output.rotation_error_rx.name"); })
        .SetVisibility(HVOutputVisibility::OnSuccess);

    rotation_error_ry_
        .SetSchemaName("rotation_error_ry")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "output.rotation_error_ry.name"); })
        .SetVisibility(HVOutputVisibility::OnSuccess);

    rotation_error_rz_
        .SetSchemaName("rotation_error_rz")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "output.rotation_error_rz.name"); })
        .SetVisibility(HVOutputVisibility::OnSuccess);

    execute_status_output_
        .SetSchemaName("execute_status")
        .SetDisplayNameResolver([this]() { return Tr(current_language(), "output.status.name"); })
        .SetVisibility(HVOutputVisibility::Always)
        .BindExternalValue(execute_status_);

    RegisterInputField(image_folder_);
    RegisterInputField(robot_pose_txt_);
    RegisterInputField(camera_intrinsics_yml_);
    RegisterInputField(output_matrix_path_);
    RegisterInputField(corners_horizontal_);
    RegisterInputField(corners_vertical_);
    RegisterInputField(square_size_mm_);
    RegisterInputField(is_degree_);
    RegisterInputField(calibration_type_);
    RegisterInputField(rotation_type_);
    RegisterInputField(optimize_);
    RegisterInputField(recalibrate_intrinsics_);
    RegisterOutputField(translation_error_x_);
    RegisterOutputField(translation_error_y_);
    RegisterOutputField(translation_error_z_);
    RegisterOutputField(rotation_error_rx_);
    RegisterOutputField(rotation_error_ry_);
    RegisterOutputField(rotation_error_rz_);
    RegisterOutputField(execute_status_output_);
}

int HVHandEyeCalibration::init()
{
    ResetRuntimeState();
    translation_error_x_.value() = 0.0;
    translation_error_y_.value() = 0.0;
    translation_error_z_.value() = 0.0;
    rotation_error_rx_.value() = 0.0;
    rotation_error_ry_.value() = 0.0;
    rotation_error_rz_.value() = 0.0;
    return SUCCESS;
}

int HVHandEyeCalibration::run()
{
    const auto start = std::chrono::steady_clock::now();
    execute_status_ = NODE_STATUS_RUNNING;
    error_message_key_.clear();
    translation_error_x_.value() = 0.0;
    translation_error_y_.value() = 0.0;
    translation_error_z_.value() = 0.0;
    rotation_error_rx_.value() = 0.0;
    rotation_error_ry_.value() = 0.0;
    rotation_error_rz_.value() = 0.0;

    const std::string& imageFolder = image_folder_.value();
    const std::string& poseTxt = robot_pose_txt_.value();
    const std::string& intrinsicsYml = camera_intrinsics_yml_.value();
    const std::string& outputMatrixPath = output_matrix_path_.value();

    // 1. Validate params
    if (imageFolder.empty()) {
        return FailWithMessage(ALGORITHM_RUN_ERROR, "msg.image_folder_empty");
    }
    {
        std::error_code ec;
        if (!fs::exists(fs::u8path(imageFolder), ec) || ec ||
            !fs::is_directory(fs::u8path(imageFolder), ec) || ec) {
            return FailWithMessage(ALGORITHM_RUN_ERROR, "msg.image_folder_empty");
        }
    }

    // 2. Scan and load images
    std::vector<fs::path> imagePaths;
    {
        std::error_code ec;
        for (auto iter = fs::directory_iterator(fs::u8path(imageFolder), ec);
             iter != fs::directory_iterator() && !ec; ++iter) {
            const auto& entry = *iter;
            if (!entry.is_regular_file()) continue;
            std::wstring ext = entry.path().extension().wstring();
            if (IsSupportedImageExt(ext)) {
                imagePaths.push_back(entry.path());
            }
        }
        if (ec) {
            return FailWithMessage(ALGORITHM_RUN_ERROR, "msg.image_folder_empty");
        }
    }
    if (imagePaths.empty()) {
        return FailWithMessage(ALGORITHM_RUN_ERROR, "msg.no_supported_images");
    }
    std::sort(imagePaths.begin(), imagePaths.end());

    std::vector<cv::Mat> images;
    images.reserve(imagePaths.size());
    for (const auto& imgPath : imagePaths) {
        cv::Mat img = cv::imread(imgPath.string(), cv::IMREAD_UNCHANGED);
        if (img.empty()) {
            error_message_key_ = Tr(current_language(), "msg.failed_to_read_image") + imgPath.string();
            execute_status_ = ALGORITHM_RUN_ERROR;
            return ALGORITHM_RUN_ERROR;
        }
        images.push_back(img);
    }

    // 3. Parse robot poses
    {
        std::error_code ec;
        if (!fs::exists(fs::u8path(poseTxt), ec) || ec) {
            return FailWithMessage(ALGORITHM_RUN_ERROR, "msg.robot_pose_not_found");
        }
    }

    std::vector<SdkRobotPose> poses;
    std::ifstream poseFile(fs::u8path(poseTxt));
    if (!poseFile.is_open()) {
        return FailWithMessage(ALGORITHM_RUN_ERROR, "msg.robot_pose_not_found");
    }

    std::string line;
    int lineNo = 0;
    while (std::getline(poseFile, line)) {
        ++lineNo;
        if (line.empty()) continue;

        // Accept comma as field separator (e.g. exported CSV), normalise to spaces
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream iss(line);
        SdkRobotPose pose;
        if (!(iss >> pose.x >> pose.y >> pose.z >> pose.rx >> pose.ry >> pose.rz)) {
            error_message_key_ = Tr(current_language(), "msg.pose_line_invalid") + std::to_string(lineNo);
            execute_status_ = ALGORITHM_RUN_ERROR;
            return ALGORITHM_RUN_ERROR;
        }
        // Reject lines with extra trailing tokens (contract: exactly 6 columns)
        std::string extra;
        if (iss >> extra) {
            error_message_key_ = Tr(current_language(), "msg.pose_line_invalid") + std::to_string(lineNo);
            execute_status_ = ALGORITHM_RUN_ERROR;
            return ALGORITHM_RUN_ERROR;
        }
        poses.push_back(pose);
    }

    // 4. Validate sample counts
    if (images.size() != poses.size()) {
        return FailWithMessage(ALGORITHM_RUN_ERROR, "msg.count_mismatch");
    }
    if (images.size() < 3) {
        return FailWithMessage(ALGORITHM_RUN_ERROR, "msg.too_few_samples");
    }

    // 5. Build CalibrateData array
    std::vector<SdkCalibrateData> calibDatas(images.size());
    for (size_t i = 0; i < images.size(); ++i) {
        calibDatas[i].img.width = images[i].cols;
        calibDatas[i].img.height = images[i].rows;
        calibDatas[i].img.channel = images[i].channels();
        calibDatas[i].img.image_data = images[i].data;
        calibDatas[i].pose = poses[i];
    }

    // 6. Load SDK
    SdkLoader sdk;
    if (!sdk.Load()) {
        error_message_key_ = Tr(current_language(), "msg.sdk_load_failed") + "\n" + sdk.GetDiag();
        execute_status_ = ALGORITHM_RUN_ERROR;
        return ALGORITHM_RUN_ERROR;
    }

    // 7. Set chessboard params
    SdkChessboardParam chessboard;
    chessboard.cornersHorizontal = corners_horizontal_.value();
    chessboard.cornersVertical = corners_vertical_.value();
    chessboard.squareSize = square_size_mm_.value();
    sdk.setChessboardParams(&chessboard);

    // 8. Set calibrate data
    int dataNum = static_cast<int>(calibDatas.size());
    if (!sdk.setCalibrateData(calibDatas.data(), dataNum, is_degree_.value(), rotation_type_.value())) {
        return FailWithMessage(ALGORITHM_RUN_ERROR, "msg.set_calibrate_data_failed");
    }

    // 9. Read camera intrinsics
    {
        std::error_code ec;
        if (!fs::exists(fs::u8path(intrinsicsYml), ec) || ec) {
            return FailWithMessage(ALGORITHM_RUN_ERROR, "msg.intrinsics_not_found");
        }
    }
    if (!sdk.readCameraIntrinsics(intrinsicsYml.c_str(), recalibrate_intrinsics_.value())) {
        return FailWithMessage(ALGORITHM_RUN_ERROR, "msg.intrinsics_read_failed");
    }

    // 10. Calibrate
    if (!sdk.calibrateHandEye(calibration_type_.value(), optimize_.value())) {
        return FailWithMessage(ALGORITHM_RUN_ERROR, "msg.calibrate_failed");
    }

    // 11. Get errors — must be called before save (SDK may clear state on save)
    //     Array layout: [x, y, z, rx, ry, rz]
    //     errorType 0 = translation, errorType 1 = rotation
    float errorData[6] = {};
    if (!sdk.getCalibrateError(errorData, 0)) {
        return FailWithMessage(ALGORITHM_RUN_ERROR, "msg.get_error_failed");
    }

    const double tx = static_cast<double>(errorData[0]);
    const double ty = static_cast<double>(errorData[1]);
    const double tz = static_cast<double>(errorData[2]);
    const double rx = static_cast<double>(errorData[3]);
    const double ry = static_cast<double>(errorData[4]);
    const double rz = static_cast<double>(errorData[5]);

    translation_error_x_.value() = tx;
    translation_error_y_.value() = ty;
    translation_error_z_.value() = tz;
    rotation_error_rx_.value() = rx;
    rotation_error_ry_.value() = ry;
    rotation_error_rz_.value() = rz;


    // 12. Save matrix — ensure output directory exists
    {
        fs::path outPath = fs::u8path(outputMatrixPath);
        std::error_code ec;
        fs::create_directories(outPath.parent_path(), ec);
        if (!sdk.saveHandEyeMatrix(outputMatrixPath.c_str())) {
            return FailWithMessage(ALGORITHM_RUN_ERROR, "msg.save_matrix_failed");
        }
    }

    const auto end = std::chrono::steady_clock::now();
    run_time_ = static_cast<long>(
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
    execute_status_ = SUCCESS;
    error_message_key_ = "msg.success";
    return SUCCESS;
}

AlgorithmType HVHandEyeCalibration::get_algorithm_type()
{
    return AlgorithmType::Calibration;
}

std::string HVHandEyeCalibration::get_algorithm_display_name()
{
    return Tr(current_language(), "algorithm.display");
}

std::string HVHandEyeCalibration::TranslateText(const std::string& key) const
{
    return Tr(current_language(), key);
}

extern "C" __declspec(dllexport) NodeEngine* CreateInstance()
{
    return new HVHandEyeCalibration();
}

extern "C" __declspec(dllexport) std::string GetInstanceName()
{
    return "HandEyeCalibration";
}

extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion()
{
    return NODE_ENGINE_ABI_VERSION;
}
