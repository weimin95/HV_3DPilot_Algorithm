#pragma once
#include "node_engine.h"
#include "3d_pliot_error.h"
#include "3d_pilot_public_def.h"
#include "param_meta_data.h"

#include <string>

class HVCloudFit : public NodeEngine {
public:
    HVCloudFit();
    ~HVCloudFit();

    int init();

    int run();

    // 0: HVPointCloud* 输入点云
    // 1: 拟合类型 (0: 平面拟合; 1: 圆柱拟合)
    // --- 共用参数 ---
    // 2: RANSAC距离阈值 distance threshold
    // 3: RANSAC最大迭代次数 max iterations
    // --- 圆柱拟合参数 ---
    // 4: 法线估计搜索半径 normal radius search
    // 5: 法线距离权重 normal distance weight
    // 6: 最小半径 radius min
    // 7: 最大半径 radius max
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
    std::shared_ptr<HVPointCloud> inlierCloud;
    std::string modelCoefficients;

    int type = 0;                        // 拟合类型 (0: 平面, 1: 圆柱)

    // 共用参数
    double distance_threshold = 0.01;   // RANSAC点到模型的距离阈值
    int max_iterations = 1000;          // RANSAC最大迭代次数

    // 圆柱拟合参数
    double normal_radius_search = 0.1;  // 法线估计搜索半径
    double normal_distance_weight = 0.1; // 法线方向距离权重 [0,1]
    double radius_min = 0.0;            // 圆柱半径下限
    double radius_max = 1.0;            // 圆柱半径上限

    int execute_status = NODE_STATUS_NOT_RUN;
    long run_time = 0;
    std::string error_msg;
    int language_ = static_cast<int>(UIPilotLanguage::ZH_CN);
};

extern "C" __declspec(dllexport) NodeEngine* CreateInstance();
extern "C" __declspec(dllexport) std::string GetInstanceName();
