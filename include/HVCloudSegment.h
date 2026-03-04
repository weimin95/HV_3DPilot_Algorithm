#pragma once
#include "node_engine.h"
#include "3d_pliot_error.h"
#include "3d_pilot_public_def.h"
#include "param_meta_data.h"

#include <string>

class HVCloudSegment : public NodeEngine {
public:
    HVCloudSegment();
    ~HVCloudSegment();

    int init();

    int run();

    // 0: HVPointCloud* 输入点云
    // 1: 分割类型 (0: RANSAC平面分割; 1: 欧式聚类分割; 2: 区域生长分割)
    // 2: 距离阈值 (RANSAC)
    // 3: 最大迭代次数 (RANSAC)
    // 4: 聚类距离容差 (欧式聚类)
    // 5: 最小聚类点数 (欧式聚类/区域生长)
    // 6: 最大聚类点数 (欧式聚类/区域生长)
    // 7: 邻域点数 (区域生长)
    // 8: 平滑度阈值 (区域生长, 角度)
    // 9: 曲率阈值 (区域生长)
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

    void set_language(int language) override;
    int get_language() const override;
    std::string get_algorithm_display_name() override;

private:
    std::shared_ptr<HVPointCloud> inputCloud;
    std::shared_ptr<HVPointCloud> resultCloud;

    int type = 0;                     // 分割类型 (0: RANSAC平面, 1: 欧式聚类, 2: 区域生长)
    double distance_threshold = 0.01; // RANSAC: 点到平面距离阈值
    int max_iterations = 1000;        // RANSAC: 最大迭代次数
    double cluster_tolerance = 0.02;  // 欧式聚类: 聚类距离容差
    int min_cluster_size = 100;       // 欧式聚类/区域生长: 最小聚类点数
    int max_cluster_size = 100000;    // 欧式聚类/区域生长: 最大聚类点数
    int number_of_neighbours = 30;    // 区域生长: KNN邻域点数
    float smoothness_threshold = 3.0; // 区域生长: 平滑度角度阈值 (度)
    float curvature_threshold = 1.0;  // 区域生长: 曲率阈值

    int execute_status = NODE_STATUS_NOT_RUN;
    long run_time = 0;
    std::string error_msg;
    int language_ = static_cast<int>(UIPilotLanguage::ZH_CN);
};

extern "C" __declspec(dllexport) NodeEngine* CreateInstance();
extern "C" __declspec(dllexport) std::string GetInstanceName();
