#pragma once
#include "node_engine.h"
#include "3d_pliot_error.h"
#include "3d_pilot_public_def.h"
#include "param_meta_data.h"

#include <string>

class HVCloudRegister : public NodeEngine {
public:
    HVCloudRegister();
    ~HVCloudRegister();

    int init();

    int run();

    // 0: HVPointCloud* source点云（待配准）
    // 1: HVPointCloud* target点云（参考）
    // 2: 配准类型 (0: ICP; 1: NDT; 2: GICP; 3: FPFH+SAC-IA)
    // --- ICP / GICP 共用 ---
    // 3: 最大迭代次数 (ICP/GICP)
    // 4: 最大对应点距离 (ICP/GICP)
    // 5: 变换收敛阈值 transformation epsilon (ICP/GICP)
    // 6: 欧式适应度收敛阈值 euclidean fitness epsilon (ICP)
    // --- NDT ---
    // 7: NDT体素分辨率 ndt resolution
    // 8: NDT步长 ndt step size
    // 9: NDT变换收敛阈值 ndt transformation epsilon
    // 10: NDT最大迭代次数
    // --- GICP ---
    // 11: 旋转收敛阈值 gicp rotation epsilon
    // --- FPFH + SAC-IA ---
    // 12: 法线估计搜索半径 normal radius search
    // 13: FPFH特征搜索半径 fpfh radius search
    // 14: SAC-IA最大迭代次数
    // 15: SAC-IA最小采样距离 sacia min sample distance
    // 16: SAC-IA最大对应点距离 sacia max correspondence distance
    int set_algorithm_params(const std::vector<void*>& params, const std::vector<int>& paramID = std::vector<int>());

    std::vector<void*> get_current_params();

    std::vector<void*> get_algorithm_result();

    std::vector<int> get_algorithm_input_params_type();

    std::vector<int> get_algorithm_output_params_type();

    std::vector<std::string> get_algorithm_input_params_name();

    std::vector<std::string> get_algorithm_output_params_name();

    std::vector<bool> get_algorithm_input_params_bindable();

    std::vector<ParamMetadata> get_algorithm_input_params_metadata() override;

    int get_algorithm_execute_status();

    std::string get_algorithm_error_message();

    long get_algorithm_use_time();

    bool algorithm_params_setting_status();

    bool algorithm_init_status();

    bool save_params_to_json(const std::string& filePath);

    bool load_params_from_json(const std::string& filePath);

    AlgorithmType get_algorithm_type();

private:
    std::shared_ptr<HVPointCloud> sourceCloud;
    std::shared_ptr<HVPointCloud> targetCloud;
    std::shared_ptr<HVPointCloud> resultCloud;
    std::string transformMatrix;

    int type = 0;                              // 配准类型

    // ICP / GICP 共用参数
    int max_iterations = 50;                   // 最大迭代次数
    double max_correspondence_distance = 1.0;  // 最大对应点距离
    double transformation_epsilon = 1e-8;      // 变换收敛阈值
    double euclidean_fitness_epsilon = 1.0;    // 欧式适应度收敛阈值 (仅ICP)

    // NDT 参数
    double ndt_resolution = 1.0;              // 体素分辨率
    double ndt_step_size = 0.1;              // 步长
    double ndt_transformation_epsilon = 0.01; // 变换收敛阈值
    int ndt_max_iterations = 35;             // 最大迭代次数

    // GICP 参数
    double gicp_rotation_epsilon = 2e-3;      // 旋转收敛阈值

    // FPFH + SAC-IA 参数
    double normal_radius_search = 0.1;         // 法线估计搜索半径
    double fpfh_radius_search = 0.25;          // FPFH特征搜索半径
    int sacia_max_iterations = 500;            // SAC-IA最大迭代次数
    double sacia_min_sample_distance = 0.05;   // 最小采样距离
    double sacia_max_correspondence_distance = 0.5; // 最大对应点距离

    int execute_status = NODE_STATUS_NOT_RUN;
    long run_time = 0;
    std::string error_msg;
};

extern "C" __declspec(dllexport) NodeEngine* CreateInstance();
extern "C" __declspec(dllexport) std::string GetInstanceName();
