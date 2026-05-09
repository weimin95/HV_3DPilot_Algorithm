#include "HVHandEyeCalibration.h"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void Check(bool cond, const char* msg)
{
    if (!cond) {
        std::printf("FAIL: %s\n", msg);
        ++g_failures;
    }
}

// Helper: set all 12 input fields by param index (order must match constructor registration)
int SetAllParams(HVHandEyeCalibration& node,
                 const std::string& imageFolder,
                 const std::string& poseTxt,
                 const std::string& intrinsicsYml = "dummy.xml",
                 const std::string& outputPath = "dummy.dat")
{
    int cornersH = 11;
    int cornersV = 8;
    double squareSize = 30.0;
    bool isDegree = true;
    int calibType = 0;
    int rotType = 0;
    bool optimize = true;
    bool recalib = false;

    // Non-const copies for void* pointers
    std::string img = imageFolder;
    std::string pose = poseTxt;
    std::string intr = intrinsicsYml;
    std::string out = outputPath;

    return node.set_algorithm_params(
        { &img, &pose, &intr, &out,
          &cornersH, &cornersV, &squareSize, &isDegree,
          &calibType, &rotType, &optimize, &recalib },
        { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 });
}

void CreateTestImages(const fs::path& dir, int count)
{
    fs::create_directories(dir);
    cv::Mat img(20, 20, CV_8UC3, cv::Scalar(100, 150, 200));
    for (int i = 0; i < count; ++i) {
        char name[64];
        std::snprintf(name, sizeof(name), "img_%02d.png", i);
        cv::imwrite((dir / name).string(), img);
    }
}

void WritePoseFile(const fs::path& path, const std::vector<std::string>& lines)
{
    std::ofstream f(path);
    for (const auto& line : lines) {
        f << line << "\n";
    }
}

std::vector<std::string> ValidPoses(int n)
{
    std::vector<std::string> lines;
    for (int i = 0; i < n; ++i) {
        double v = static_cast<double>(i);
        char buf[256];
        std::snprintf(buf, sizeof(buf), "%.1f %.1f %.1f %.2f %.2f %.2f",
                      v, v + 1.0, v + 2.0, v * 0.1, v * 0.2, v * 0.3);
        lines.push_back(buf);
    }
    return lines;
}

}  // namespace

static void TestPoseExtraColumnRejected()
{
    std::printf("  Pose extra column rejected ... ");

    const fs::path dir = fs::temp_directory_path() / "hv_he_test_extra";
    fs::remove_all(dir);
    CreateTestImages(dir, 3);

    const fs::path posePath = dir / "poses.txt";
    {
        auto lines = ValidPoses(3);
        lines[1] += " extra_token";
        WritePoseFile(posePath, lines);
    }

    HVHandEyeCalibration node;
    Check(node.init() == SUCCESS, "init");
    SetAllParams(node, dir.string(), posePath.string());
    const int rc = node.run();
    Check(rc != SUCCESS, "should reject extra column");
    Check(rc == ALGORITHM_RUN_ERROR, "should return ALGORITHM_RUN_ERROR");

    fs::remove_all(dir);
    std::printf("done\n");
}

static void TestPoseValidPassesParsing()
{
    std::printf("  Pose valid passes parsing ... ");

    const fs::path dir = fs::temp_directory_path() / "hv_he_test_valid";
    fs::remove_all(dir);
    CreateTestImages(dir, 3);

    const fs::path posePath = dir / "poses.txt";
    WritePoseFile(posePath, ValidPoses(3));

    HVHandEyeCalibration node;
    Check(node.init() == SUCCESS, "init");
    SetAllParams(node, dir.string(), posePath.string());
    const int rc = node.run();
    // Will fail at SDK load (step 6), NOT at pose parsing (step 3)
    Check(rc != SUCCESS, "should fail (no SDK in test env)");
    const int status = node.get_algorithm_execute_status();
    Check(status == ALGORITHM_RUN_ERROR, "status should be ALGORITHM_RUN_ERROR");

    fs::remove_all(dir);
    std::printf("done\n");
}

static void TestCountMismatchRejected()
{
    std::printf("  Count mismatch rejected ... ");

    const fs::path dir = fs::temp_directory_path() / "hv_he_test_mismatch";
    fs::remove_all(dir);
    CreateTestImages(dir, 3);  // 3 images

    const fs::path posePath = dir / "poses.txt";
    WritePoseFile(posePath, ValidPoses(5));  // 5 poses → mismatch

    HVHandEyeCalibration node;
    Check(node.init() == SUCCESS, "init");
    SetAllParams(node, dir.string(), posePath.string());
    const int rc = node.run();
    Check(rc != SUCCESS, "should reject count mismatch");
    Check(rc == ALGORITHM_RUN_ERROR, "should return ALGORITHM_RUN_ERROR");

    fs::remove_all(dir);
    std::printf("done\n");
}

static void TestImageFolderEmptyRejected()
{
    std::printf("  Empty image folder rejected ... ");

    const fs::path dir = fs::temp_directory_path() / "hv_he_test_empty";
    fs::remove_all(dir);
    fs::create_directories(dir);  // empty dir, no images

    const fs::path posePath = dir / "poses.txt";
    WritePoseFile(posePath, ValidPoses(3));

    HVHandEyeCalibration node;
    Check(node.init() == SUCCESS, "init");
    SetAllParams(node, dir.string(), posePath.string());
    const int rc = node.run();
    Check(rc != SUCCESS, "should reject empty image folder");
    Check(rc == ALGORITHM_RUN_ERROR, "should return ALGORITHM_RUN_ERROR");

    fs::remove_all(dir);
    std::printf("done\n");
}

static void TestImageFolderNotExistsRejected()
{
    std::printf("  Non-existent image folder rejected ... ");

    HVHandEyeCalibration node;
    Check(node.init() == SUCCESS, "init");
    SetAllParams(node, "/nonexistent/path/12345", "/nonexistent/pose.txt");
    const int rc = node.run();
    Check(rc != SUCCESS, "should reject non-existent image folder");
    Check(rc == ALGORITHM_RUN_ERROR, "should return ALGORITHM_RUN_ERROR");

    std::printf("done\n");
}

static void TestTooFewSamplesRejected()
{
    std::printf("  Too few samples rejected ... ");

    const fs::path dir = fs::temp_directory_path() / "hv_he_test_few";
    fs::remove_all(dir);
    CreateTestImages(dir, 2);  // only 2 samples

    const fs::path posePath = dir / "poses.txt";
    WritePoseFile(posePath, ValidPoses(2));

    HVHandEyeCalibration node;
    Check(node.init() == SUCCESS, "init");
    SetAllParams(node, dir.string(), posePath.string());
    const int rc = node.run();
    Check(rc != SUCCESS, "should reject < 3 samples");
    Check(rc == ALGORITHM_RUN_ERROR, "should return ALGORITHM_RUN_ERROR");

    fs::remove_all(dir);
    std::printf("done\n");
}

int main()
{
    std::printf("=== HVHandEyeCalibration tests ===\n");

    TestPoseExtraColumnRejected();
    TestPoseValidPassesParsing();
    TestCountMismatchRejected();
    TestImageFolderEmptyRejected();
    TestImageFolderNotExistsRejected();
    TestTooFewSamplesRejected();

    if (g_failures == 0) {
        std::printf("ALL TESTS PASSED\n");
        return 0;
    }
    std::printf("%d TEST(S) FAILED\n", g_failures);
    return 1;
}
