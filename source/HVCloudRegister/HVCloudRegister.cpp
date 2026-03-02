#include "HVCloudRegister.h"
#include "HVUtils.h"

#include <chrono>
#include <sstream>
#include <iomanip>

#include <pcl/registration/icp.h>
#include <pcl/registration/ndt.h>
#include <pcl/registration/gicp.h>
#include <pcl/registration/ia_ransac.h>
#include <pcl/features/normal_3d.h>
#include <pcl/features/fpfh.h>
#include <pcl/search/kdtree.h>

// 将 Eigen 4x4 矩阵序列化为 JSON 二维数组字符串
static std::string matrixToJsonString(const Eigen::Matrix4f& mat)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(8);
    oss << "[";
    for (int i = 0; i < 4; ++i) {
        oss << "[";
        for (int j = 0; j < 4; ++j) {
            oss << mat(i, j);
            if (j < 3) oss << ",";
        }
        oss << "]";
        if (i < 3) oss << ",";
    }
    oss << "]";
    return oss.str();
}

HVCloudRegister::HVCloudRegister()
{
}

HVCloudRegister::~HVCloudRegister()
{
}

int HVCloudRegister::init()
{
    execute_status = NODE_STATUS_NOT_RUN;
    error_msg.clear();
    return SUCCESS;
}

int HVCloudRegister::run()
{
    auto start = std::chrono::steady_clock::now();
    execute_status = NODE_STATUS_RUNNING;
    error_msg.clear();

    if (!sourceCloud || !targetCloud) {
        error_msg = "Source or target cloud is null";
        execute_status = ALGORITHM_RUN_ERROR;
        return ALGORITHM_RUN_ERROR;
    }

    pcl::PointCloud<pcl::PointXYZ>::Ptr cloudSrc = PointCloudConverter::ToPCL(*sourceCloud);
    pcl::PointCloud<pcl::PointXYZ>::Ptr cloudTgt = PointCloudConverter::ToPCL(*targetCloud);
    pcl::PointCloud<pcl::PointXYZ> cloudAligned;
    Eigen::Matrix4f finalTransform = Eigen::Matrix4f::Identity();

    switch (type)
    {
    case 0:
    {
        // ICP 迭代最近点
        pcl::IterativeClosestPoint<pcl::PointXYZ, pcl::PointXYZ> icp;
        icp.setMaximumIterations(max_iterations);
        icp.setMaxCorrespondenceDistance(max_correspondence_distance);
        icp.setTransformationEpsilon(transformation_epsilon);
        icp.setEuclideanFitnessEpsilon(euclidean_fitness_epsilon);
        icp.setInputSource(cloudSrc);
        icp.setInputTarget(cloudTgt);
        icp.align(cloudAligned);

        if (!icp.hasConverged()) {
            error_msg = "ICP did not converge";
            execute_status = ALGORITHM_RUN_ERROR;
            return ALGORITHM_RUN_ERROR;
        }

        finalTransform = icp.getFinalTransformation();
        error_msg = "ICP converged, fitness score: " + std::to_string(icp.getFitnessScore());
    }
    break;
    case 1:
    {
        // NDT 正态分布变换
        pcl::NormalDistributionsTransform<pcl::PointXYZ, pcl::PointXYZ> ndt;
        ndt.setResolution(static_cast<float>(ndt_resolution));
        ndt.setStepSize(ndt_step_size);
        ndt.setTransformationEpsilon(ndt_transformation_epsilon);
        ndt.setMaximumIterations(ndt_max_iterations);
        ndt.setInputSource(cloudSrc);
        ndt.setInputTarget(cloudTgt);
        ndt.align(cloudAligned);

        if (!ndt.hasConverged()) {
            error_msg = "NDT did not converge";
            execute_status = ALGORITHM_RUN_ERROR;
            return ALGORITHM_RUN_ERROR;
        }

        finalTransform = ndt.getFinalTransformation();
        error_msg = "NDT converged, fitness score: " + std::to_string(ndt.getFitnessScore());
    }
    break;
    case 2:
    {
        // GICP 广义迭代最近点
        pcl::GeneralizedIterativeClosestPoint<pcl::PointXYZ, pcl::PointXYZ> gicp;
        gicp.setMaximumIterations(max_iterations);
        gicp.setMaxCorrespondenceDistance(max_correspondence_distance);
        gicp.setTransformationEpsilon(transformation_epsilon);
        gicp.setRotationEpsilon(gicp_rotation_epsilon);
        gicp.setInputSource(cloudSrc);
        gicp.setInputTarget(cloudTgt);
        gicp.align(cloudAligned);

        if (!gicp.hasConverged()) {
            error_msg = "GICP did not converge";
            execute_status = ALGORITHM_RUN_ERROR;
            return ALGORITHM_RUN_ERROR;
        }

        finalTransform = gicp.getFinalTransformation();
        error_msg = "GICP converged, fitness score: " + std::to_string(gicp.getFitnessScore());
    }
    break;
    case 3:
    {
        // FPFH + SAC-IA 基于特征的全局初始配准
        // 1. 估计法线
        pcl::search::KdTree<pcl::PointXYZ>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZ>);

        pcl::PointCloud<pcl::Normal>::Ptr normalsSrc(new pcl::PointCloud<pcl::Normal>);
        pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> ne_src;
        ne_src.setSearchMethod(tree);
        ne_src.setRadiusSearch(normal_radius_search);
        ne_src.setInputCloud(cloudSrc);
        ne_src.compute(*normalsSrc);

        pcl::PointCloud<pcl::Normal>::Ptr normalsTgt(new pcl::PointCloud<pcl::Normal>);
        pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> ne_tgt;
        ne_tgt.setSearchMethod(tree);
        ne_tgt.setRadiusSearch(normal_radius_search);
        ne_tgt.setInputCloud(cloudTgt);
        ne_tgt.compute(*normalsTgt);

        // 2. 计算 FPFH 特征
        pcl::PointCloud<pcl::FPFHSignature33>::Ptr fpfhSrc(new pcl::PointCloud<pcl::FPFHSignature33>);
        pcl::FPFHEstimation<pcl::PointXYZ, pcl::Normal, pcl::FPFHSignature33> fpfh_src;
        fpfh_src.setSearchMethod(tree);
        fpfh_src.setRadiusSearch(fpfh_radius_search);
        fpfh_src.setInputCloud(cloudSrc);
        fpfh_src.setInputNormals(normalsSrc);
        fpfh_src.compute(*fpfhSrc);

        pcl::PointCloud<pcl::FPFHSignature33>::Ptr fpfhTgt(new pcl::PointCloud<pcl::FPFHSignature33>);
        pcl::FPFHEstimation<pcl::PointXYZ, pcl::Normal, pcl::FPFHSignature33> fpfh_tgt;
        fpfh_tgt.setSearchMethod(tree);
        fpfh_tgt.setRadiusSearch(fpfh_radius_search);
        fpfh_tgt.setInputCloud(cloudTgt);
        fpfh_tgt.setInputNormals(normalsTgt);
        fpfh_tgt.compute(*fpfhTgt);

        // 3. SAC-IA 初始配准
        pcl::SampleConsensusInitialAlignment<pcl::PointXYZ, pcl::PointXYZ, pcl::FPFHSignature33> sac_ia;
        sac_ia.setMinSampleDistance(static_cast<float>(sacia_min_sample_distance));
        sac_ia.setMaxCorrespondenceDistance(sacia_max_correspondence_distance);
        sac_ia.setMaximumIterations(sacia_max_iterations);
        sac_ia.setInputSource(cloudSrc);
        sac_ia.setSourceFeatures(fpfhSrc);
        sac_ia.setInputTarget(cloudTgt);
        sac_ia.setTargetFeatures(fpfhTgt);
        sac_ia.align(cloudAligned);

        if (!sac_ia.hasConverged()) {
            error_msg = "FPFH+SAC-IA did not converge";
            execute_status = ALGORITHM_RUN_ERROR;
            return ALGORITHM_RUN_ERROR;
        }

        finalTransform = sac_ia.getFinalTransformation();
        error_msg = "FPFH+SAC-IA converged, fitness score: " + std::to_string(sac_ia.getFitnessScore());
    }
    break;
    default:
    {
        error_msg = "Unsupported registration type";
        execute_status = ALGORITHM_RUN_ERROR;
        return ALGORITHM_RUN_ERROR;
    }
    }

    resultCloud = std::make_shared<HVPointCloud>(PointCloudConverter::FromPCL(cloudAligned));
    transformMatrix = matrixToJsonString(finalTransform);

    auto end = std::chrono::steady_clock::now();
    run_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    execute_status = SUCCESS;
    return SUCCESS;
}

int HVCloudRegister::set_algorithm_params(const std::vector<void*>& params, const std::vector<int>& paramID)
{
    if (paramID.size() == 0)
    {
        sourceCloud                    = cast_param_sharedPtr<HVPointCloud>(params, 0);
        targetCloud                    = cast_param_sharedPtr<HVPointCloud>(params, 1);
        type                           = cast_param<int>(params, 2);
        max_iterations                 = cast_param<int>(params, 3);
        max_correspondence_distance    = cast_param<double>(params, 4);
        transformation_epsilon         = cast_param<double>(params, 5);
        euclidean_fitness_epsilon      = cast_param<double>(params, 6);
        ndt_resolution                 = cast_param<double>(params, 7);
        ndt_step_size                  = cast_param<double>(params, 8);
        ndt_transformation_epsilon     = cast_param<double>(params, 9);
        ndt_max_iterations             = cast_param<int>(params, 10);
        gicp_rotation_epsilon          = cast_param<double>(params, 11);
        normal_radius_search           = cast_param<double>(params, 12);
        fpfh_radius_search             = cast_param<double>(params, 13);
        sacia_max_iterations           = cast_param<int>(params, 14);
        sacia_min_sample_distance      = cast_param<double>(params, 15);
        sacia_max_correspondence_distance = cast_param<double>(params, 16);
    }
    else
    {
        for (size_t i = 0; i < paramID.size(); i++)
        {
            int id = paramID[i];
            switch (id)
            {
            case 0:  sourceCloud                       = cast_param_sharedPtr<HVPointCloud>(params, i); break;
            case 1:  targetCloud                       = cast_param_sharedPtr<HVPointCloud>(params, i); break;
            case 2:  type                              = cast_param<int>(params, i); break;
            case 3:  max_iterations                    = cast_param<int>(params, i); break;
            case 4:  max_correspondence_distance       = cast_param<double>(params, i); break;
            case 5:  transformation_epsilon            = cast_param<double>(params, i); break;
            case 6:  euclidean_fitness_epsilon         = cast_param<double>(params, i); break;
            case 7:  ndt_resolution                    = cast_param<double>(params, i); break;
            case 8:  ndt_step_size                     = cast_param<double>(params, i); break;
            case 9:  ndt_transformation_epsilon        = cast_param<double>(params, i); break;
            case 10: ndt_max_iterations                = cast_param<int>(params, i); break;
            case 11: gicp_rotation_epsilon             = cast_param<double>(params, i); break;
            case 12: normal_radius_search              = cast_param<double>(params, i); break;
            case 13: fpfh_radius_search                = cast_param<double>(params, i); break;
            case 14: sacia_max_iterations              = cast_param<int>(params, i); break;
            case 15: sacia_min_sample_distance         = cast_param<double>(params, i); break;
            case 16: sacia_max_correspondence_distance = cast_param<double>(params, i); break;
            default: break;
            }
        }
    }

    return SUCCESS;
}

std::vector<void*> HVCloudRegister::get_current_params()
{
    return {
        &sourceCloud,
        &targetCloud,
        &type,
        &max_iterations,
        &max_correspondence_distance,
        &transformation_epsilon,
        &euclidean_fitness_epsilon,
        &ndt_resolution,
        &ndt_step_size,
        &ndt_transformation_epsilon,
        &ndt_max_iterations,
        &gicp_rotation_epsilon,
        &normal_radius_search,
        &fpfh_radius_search,
        &sacia_max_iterations,
        &sacia_min_sample_distance,
        &sacia_max_correspondence_distance
    };
}

std::vector<void*> HVCloudRegister::get_algorithm_result()
{
    if (execute_status == SUCCESS)
        return { &resultCloud, &transformMatrix };
    return { nullptr, nullptr };
}

std::vector<int> HVCloudRegister::get_algorithm_input_params_type()
{
    return {
        HV_POINTCLOUD, HV_POINTCLOUD, HV_INT,
        HV_INT, HV_DOUBLE, HV_DOUBLE, HV_DOUBLE,
        HV_DOUBLE, HV_DOUBLE, HV_DOUBLE, HV_INT,
        HV_DOUBLE,
        HV_DOUBLE, HV_DOUBLE, HV_INT, HV_DOUBLE, HV_DOUBLE
    };
}

std::vector<int> HVCloudRegister::get_algorithm_output_params_type()
{
    return { HV_POINTCLOUD, HV_STRING };
}

std::vector<std::string> HVCloudRegister::get_algorithm_input_params_name()
{
    return {
        "source cloud",
        "target cloud",
        "registration type",
        "max iterations",
        "max correspondence distance",
        "transformation epsilon",
        "euclidean fitness epsilon",
        "ndt resolution",
        "ndt step size",
        "ndt transformation epsilon",
        "ndt max iterations",
        "gicp rotation epsilon",
        "normal radius search",
        "fpfh radius search",
        "sacia max iterations",
        "sacia min sample distance",
        "sacia max correspondence distance"
    };
}

std::vector<std::string> HVCloudRegister::get_algorithm_output_params_name()
{
    return { "aligned cloud", "transform matrix" };
}

std::vector<bool> HVCloudRegister::get_algorithm_input_params_bindable()
{
    return {
        true, true, false,
        false, false, false, false,
        false, false, false, false,
        false,
        false, false, false, false, false
    };
}

std::vector<ParamMetadata> HVCloudRegister::get_algorithm_input_params_metadata()
{
    std::vector<ParamMetadata> metadata_list;

    // 参数0: source点云 (可绑定，无约束)
    ParamMetadata meta0;
    meta0.param_name = "source cloud";
    meta0.param_description = "Source point cloud to be registered";
    meta0.param_type = HV_POINTCLOUD;
    meta0.constraint_type = CONSTRAINT_NONE;
    metadata_list.push_back(meta0);

    // 参数1: target点云 (可绑定，无约束)
    ParamMetadata meta1;
    meta1.param_name = "target cloud";
    meta1.param_description = "Target reference point cloud";
    meta1.param_type = HV_POINTCLOUD;
    meta1.constraint_type = CONSTRAINT_NONE;
    metadata_list.push_back(meta1);

    // 参数2: 配准类型 (选项约束)
    ParamMetadata meta2;
    meta2.param_name = "registration type";
    meta2.param_description = "Point cloud registration algorithm type";
    meta2.param_type = HV_INT;
    meta2.constraint_type = CONSTRAINT_OPTIONS;
    meta2.options_constraint.AddOption(0, "ICP (Iterative Closest Point)");
    meta2.options_constraint.AddOption(1, "NDT (Normal Distributions Transform)");
    meta2.options_constraint.AddOption(2, "GICP (Generalized ICP)");
    meta2.options_constraint.AddOption(3, "FPFH+SAC-IA (Feature-based Initial Alignment)");
    meta2.options_constraint.default_index = 0;
    metadata_list.push_back(meta2);

    // 参数3: 最大迭代次数 (ICP/GICP, 依赖type IN [0,2])
    ParamMetadata meta3;
    meta3.param_name = "max iterations";
    meta3.param_description = "Maximum number of iterations for ICP/GICP";
    meta3.param_type = HV_INT;
    meta3.constraint_type = CONSTRAINT_RANGE;
    meta3.range_constraint = RangeConstraint(1, 10000, 50);
    meta3.dependencies.push_back(ParamDependency(2, DEPENDS_ON_IN_LIST, {"0", "2"}));
    metadata_list.push_back(meta3);

    // 参数4: 最大对应点距离 (ICP/GICP, 依赖type IN [0,2])
    ParamMetadata meta4;
    meta4.param_name = "max correspondence distance";
    meta4.param_description = "Maximum distance threshold for correspondence search in ICP/GICP";
    meta4.param_type = HV_DOUBLE;
    meta4.constraint_type = CONSTRAINT_RANGE;
    meta4.range_constraint = RangeConstraint(0.001, 100.0, 1.0);
    meta4.dependencies.push_back(ParamDependency(2, DEPENDS_ON_IN_LIST, {"0", "2"}));
    metadata_list.push_back(meta4);

    // 参数5: 变换收敛阈值 (ICP/GICP, 依赖type IN [0,2])
    ParamMetadata meta5;
    meta5.param_name = "transformation epsilon";
    meta5.param_description = "Convergence threshold for transformation in ICP/GICP (smaller means higher precision)";
    meta5.param_type = HV_DOUBLE;
    meta5.constraint_type = CONSTRAINT_RANGE;
    meta5.range_constraint = RangeConstraint(1e-10, 1.0, 1e-8);
    meta5.dependencies.push_back(ParamDependency(2, DEPENDS_ON_IN_LIST, {"0", "2"}));
    metadata_list.push_back(meta5);

    // 参数6: 欧式适应度收敛阈值 (仅ICP, 依赖type=0)
    ParamMetadata meta6;
    meta6.param_name = "euclidean fitness epsilon";
    meta6.param_description = "Euclidean fitness score convergence threshold for ICP";
    meta6.param_type = HV_DOUBLE;
    meta6.constraint_type = CONSTRAINT_RANGE;
    meta6.range_constraint = RangeConstraint(1e-10, 100.0, 1.0);
    meta6.dependencies.push_back(ParamDependency(2, DEPENDS_ON_EQUALS, {"0"}));
    metadata_list.push_back(meta6);

    // 参数7: NDT体素分辨率 (依赖type=1)
    ParamMetadata meta7;
    meta7.param_name = "ndt resolution";
    meta7.param_description = "Voxel grid resolution for NDT (meters)";
    meta7.param_type = HV_DOUBLE;
    meta7.constraint_type = CONSTRAINT_RANGE;
    meta7.range_constraint = RangeConstraint(0.01, 100.0, 1.0);
    meta7.dependencies.push_back(ParamDependency(2, DEPENDS_ON_EQUALS, {"1"}));
    metadata_list.push_back(meta7);

    // 参数8: NDT步长 (依赖type=1)
    ParamMetadata meta8;
    meta8.param_name = "ndt step size";
    meta8.param_description = "Maximum step size for Newton's method in NDT";
    meta8.param_type = HV_DOUBLE;
    meta8.constraint_type = CONSTRAINT_RANGE;
    meta8.range_constraint = RangeConstraint(0.001, 10.0, 0.1);
    meta8.dependencies.push_back(ParamDependency(2, DEPENDS_ON_EQUALS, {"1"}));
    metadata_list.push_back(meta8);

    // 参数9: NDT变换收敛阈值 (依赖type=1)
    ParamMetadata meta9;
    meta9.param_name = "ndt transformation epsilon";
    meta9.param_description = "Convergence threshold for transformation in NDT";
    meta9.param_type = HV_DOUBLE;
    meta9.constraint_type = CONSTRAINT_RANGE;
    meta9.range_constraint = RangeConstraint(1e-10, 1.0, 0.01);
    meta9.dependencies.push_back(ParamDependency(2, DEPENDS_ON_EQUALS, {"1"}));
    metadata_list.push_back(meta9);

    // 参数10: NDT最大迭代次数 (依赖type=1)
    ParamMetadata meta10;
    meta10.param_name = "ndt max iterations";
    meta10.param_description = "Maximum number of iterations for NDT";
    meta10.param_type = HV_INT;
    meta10.constraint_type = CONSTRAINT_RANGE;
    meta10.range_constraint = RangeConstraint(1, 10000, 35);
    meta10.dependencies.push_back(ParamDependency(2, DEPENDS_ON_EQUALS, {"1"}));
    metadata_list.push_back(meta10);

    // 参数11: GICP旋转收敛阈值 (依赖type=2)
    ParamMetadata meta11;
    meta11.param_name = "gicp rotation epsilon";
    meta11.param_description = "Rotation convergence threshold for GICP";
    meta11.param_type = HV_DOUBLE;
    meta11.constraint_type = CONSTRAINT_RANGE;
    meta11.range_constraint = RangeConstraint(1e-10, 1.0, 2e-3);
    meta11.dependencies.push_back(ParamDependency(2, DEPENDS_ON_EQUALS, {"2"}));
    metadata_list.push_back(meta11);

    // 参数12: 法线估计搜索半径 (依赖type=3)
    ParamMetadata meta12;
    meta12.param_name = "normal radius search";
    meta12.param_description = "Search radius for normal estimation used in FPFH (meters)";
    meta12.param_type = HV_DOUBLE;
    meta12.constraint_type = CONSTRAINT_RANGE;
    meta12.range_constraint = RangeConstraint(0.001, 100.0, 0.1);
    meta12.dependencies.push_back(ParamDependency(2, DEPENDS_ON_EQUALS, {"3"}));
    metadata_list.push_back(meta12);

    // 参数13: FPFH特征搜索半径 (依赖type=3)
    ParamMetadata meta13;
    meta13.param_name = "fpfh radius search";
    meta13.param_description = "Search radius for FPFH feature computation (must be larger than normal radius)";
    meta13.param_type = HV_DOUBLE;
    meta13.constraint_type = CONSTRAINT_RANGE;
    meta13.range_constraint = RangeConstraint(0.001, 100.0, 0.25);
    meta13.dependencies.push_back(ParamDependency(2, DEPENDS_ON_EQUALS, {"3"}));
    metadata_list.push_back(meta13);

    // 参数14: SAC-IA最大迭代次数 (依赖type=3)
    ParamMetadata meta14;
    meta14.param_name = "sacia max iterations";
    meta14.param_description = "Maximum number of iterations for SAC-IA";
    meta14.param_type = HV_INT;
    meta14.constraint_type = CONSTRAINT_RANGE;
    meta14.range_constraint = RangeConstraint(1, 10000, 500);
    meta14.dependencies.push_back(ParamDependency(2, DEPENDS_ON_EQUALS, {"3"}));
    metadata_list.push_back(meta14);

    // 参数15: SAC-IA最小采样距离 (依赖type=3)
    ParamMetadata meta15;
    meta15.param_name = "sacia min sample distance";
    meta15.param_description = "Minimum distance between sampled points in SAC-IA";
    meta15.param_type = HV_DOUBLE;
    meta15.constraint_type = CONSTRAINT_RANGE;
    meta15.range_constraint = RangeConstraint(0.001, 100.0, 0.05);
    meta15.dependencies.push_back(ParamDependency(2, DEPENDS_ON_EQUALS, {"3"}));
    metadata_list.push_back(meta15);

    // 参数16: SAC-IA最大对应点距离 (依赖type=3)
    ParamMetadata meta16;
    meta16.param_name = "sacia max correspondence distance";
    meta16.param_description = "Maximum feature correspondence distance for SAC-IA";
    meta16.param_type = HV_DOUBLE;
    meta16.constraint_type = CONSTRAINT_RANGE;
    meta16.range_constraint = RangeConstraint(0.001, 100.0, 0.5);
    meta16.dependencies.push_back(ParamDependency(2, DEPENDS_ON_EQUALS, {"3"}));
    metadata_list.push_back(meta16);

    for (size_t i = 0; i < metadata_list.size(); ++i) {
        metadata_list[i].param_group = (i < 3) ? PARAM_GROUP_BASIC : PARAM_GROUP_ADVANCED;
    }

    return metadata_list;
}

int HVCloudRegister::get_algorithm_execute_status()
{
    return execute_status;
}

std::string HVCloudRegister::get_algorithm_error_message()
{
    return error_msg;
}

long HVCloudRegister::get_algorithm_use_time()
{
    return run_time;
}

bool HVCloudRegister::algorithm_params_setting_status()
{
    return true;
}

bool HVCloudRegister::algorithm_init_status()
{
    return true;
}

bool HVCloudRegister::save_params_to_json(const std::string& filePath)
{
    try {
        nlohmann::json params_json = nlohmann::json::array();

        add_param(params_json, "type",                              HV_INT,    this->type);
        add_param(params_json, "max_iterations",                    HV_INT,    this->max_iterations);
        add_param(params_json, "max_correspondence_distance",       HV_DOUBLE, this->max_correspondence_distance);
        add_param(params_json, "transformation_epsilon",            HV_DOUBLE, this->transformation_epsilon);
        add_param(params_json, "euclidean_fitness_epsilon",         HV_DOUBLE, this->euclidean_fitness_epsilon);
        add_param(params_json, "ndt_resolution",                    HV_DOUBLE, this->ndt_resolution);
        add_param(params_json, "ndt_step_size",                     HV_DOUBLE, this->ndt_step_size);
        add_param(params_json, "ndt_transformation_epsilon",        HV_DOUBLE, this->ndt_transformation_epsilon);
        add_param(params_json, "ndt_max_iterations",                HV_INT,    this->ndt_max_iterations);
        add_param(params_json, "gicp_rotation_epsilon",             HV_DOUBLE, this->gicp_rotation_epsilon);
        add_param(params_json, "normal_radius_search",              HV_DOUBLE, this->normal_radius_search);
        add_param(params_json, "fpfh_radius_search",                HV_DOUBLE, this->fpfh_radius_search);
        add_param(params_json, "sacia_max_iterations",              HV_INT,    this->sacia_max_iterations);
        add_param(params_json, "sacia_min_sample_distance",         HV_DOUBLE, this->sacia_min_sample_distance);
        add_param(params_json, "sacia_max_correspondence_distance", HV_DOUBLE, this->sacia_max_correspondence_distance);

        std::ofstream file(filePath);
        if (!file.is_open()) {
            return false;
        }
        file << params_json.dump(4);
        file.close();

        return true;
    }
    catch (...) {
        return false;
    }
}

bool HVCloudRegister::load_params_from_json(const std::string& filePath)
{
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            return false;
        }

        nlohmann::json params_json;
        file >> params_json;
        file.close();

        if (!params_json.is_array()) {
            return false;
        }

        for (const auto& param_json : params_json) {
            if (!param_json.contains("name") || !param_json.contains("type")) {
                continue;
            }

            std::string param_name = param_json["name"];

            if (param_name == "type")
                this->type = param_json["value"];
            else if (param_name == "max_iterations")
                this->max_iterations = param_json["value"];
            else if (param_name == "max_correspondence_distance")
                this->max_correspondence_distance = param_json["value"];
            else if (param_name == "transformation_epsilon")
                this->transformation_epsilon = param_json["value"];
            else if (param_name == "euclidean_fitness_epsilon")
                this->euclidean_fitness_epsilon = param_json["value"];
            else if (param_name == "ndt_resolution")
                this->ndt_resolution = param_json["value"];
            else if (param_name == "ndt_step_size")
                this->ndt_step_size = param_json["value"];
            else if (param_name == "ndt_transformation_epsilon")
                this->ndt_transformation_epsilon = param_json["value"];
            else if (param_name == "ndt_max_iterations")
                this->ndt_max_iterations = param_json["value"];
            else if (param_name == "gicp_rotation_epsilon")
                this->gicp_rotation_epsilon = param_json["value"];
            else if (param_name == "normal_radius_search")
                this->normal_radius_search = param_json["value"];
            else if (param_name == "fpfh_radius_search")
                this->fpfh_radius_search = param_json["value"];
            else if (param_name == "sacia_max_iterations")
                this->sacia_max_iterations = param_json["value"];
            else if (param_name == "sacia_min_sample_distance")
                this->sacia_min_sample_distance = param_json["value"];
            else if (param_name == "sacia_max_correspondence_distance")
                this->sacia_max_correspondence_distance = param_json["value"];
        }

        return true;
    }
    catch (...) {
        return false;
    }
}

AlgorithmType HVCloudRegister::get_algorithm_type()
{
    return AlgorithmType::PointCloudProcess;
}

NodeEngine* CreateInstance() {
    return new HVCloudRegister();
}

std::string GetInstanceName() {
    return "Point cloud register";
}
