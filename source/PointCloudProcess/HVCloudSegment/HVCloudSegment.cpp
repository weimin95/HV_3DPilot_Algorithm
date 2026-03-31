#include "HVCloudSegment.h"
#include "HVUtils.h"
#include "HVI18n.h"

#include <chrono>

#include <pcl/segmentation/sac_segmentation.h>
#include <pcl/segmentation/extract_clusters.h>
#include <pcl/segmentation/region_growing.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/features/normal_3d.h>
#include <pcl/search/kdtree.h>

namespace {

const hvi18n::Dictionary kTexts = {
    { "algorithm.display", { "点云分割", "Point cloud segment" } },
    { "input.cloud.name", { "输入点云", "input cloud" } },
    { "input.type.name", { "分割类型", "segment type" } },
    { "input.distance_threshold.name", { "距离阈值", "distance threshold" } },
    { "input.max_iterations.name", { "最大迭代次数", "max iterations" } },
    { "input.cluster_tolerance.name", { "聚类距离容差", "cluster tolerance" } },
    { "input.min_cluster_size.name", { "最小聚类点数", "min cluster size" } },
    { "input.max_cluster_size.name", { "最大聚类点数", "max cluster size" } },
    { "input.neighbours.name", { "邻域点数", "number of neighbours" } },
    { "input.smoothness.name", { "平滑度阈值", "smoothness threshold" } },
    { "input.curvature.name", { "曲率阈值", "curvature threshold" } },
    { "output.cloud.name", { "输出点云", "output cloud" } },
    { "input.cloud.desc", { "输入点云数据", "Input cloud data" } },
    { "input.type.desc", { "点云分割算法类型", "Point cloud segmentation type" } },
    { "input.distance_threshold.desc", { "RANSAC点到平面距离阈值", "RANSAC point-to-plane threshold" } },
    { "input.max_iterations.desc", { "RANSAC最大迭代次数", "RANSAC max iterations" } },
    { "input.cluster_tolerance.desc", { "欧式聚类点间距离容差", "Euclidean cluster tolerance" } },
    { "input.min_cluster_size.desc", { "最小聚类点数", "Minimum cluster size" } },
    { "input.max_cluster_size.desc", { "最大聚类点数", "Maximum cluster size" } },
    { "input.neighbours.desc", { "区域生长KNN邻域点数", "Region growing KNN neighbours" } },
    { "input.smoothness.desc", { "区域生长平滑度角度阈值（度）", "Region growing smoothness threshold (deg)" } },
    { "input.curvature.desc", { "区域生长曲率阈值", "Region growing curvature threshold" } },
    { "option.ransac", { "RANSAC平面分割", "RANSAC plane segmentation" } },
    { "option.euclidean", { "欧式聚类分割", "Euclidean cluster extraction" } },
    { "option.region_growing", { "区域生长分割", "Region growing segmentation" } },
    { "msg.input_null", { "输入点云为空", "Input cloud is null" } },
    { "msg.ransac_no_inliers", { "RANSAC平面分割未找到内点", "RANSAC plane segmentation: no inliers found" } },
    { "msg.ransac_success", { "RANSAC平面分割成功", "RANSAC plane segmentation success" } },
    { "msg.euclidean_no_clusters", { "欧式聚类未找到聚类", "Euclidean cluster extraction: no clusters found" } },
    { "msg.euclidean_success", { "欧式聚类成功", "Euclidean cluster extraction success" } },
    { "msg.region_no_clusters", { "区域生长未找到聚类", "Region growing segmentation: no clusters found" } },
    { "msg.region_success", { "区域生长分割成功", "Region growing segmentation success" } },
    { "msg.unsupported", { "不支持的分割类型", "Unsupported segmentation type" } }
};

std::string Tr(int language, const std::string& key) {
    return hvi18n::Translate(kTexts, key, language);
}

}  // namespace

HVCloudSegment::HVCloudSegment()
{

}

HVCloudSegment::~HVCloudSegment()
{

}

int HVCloudSegment::init()
{
	execute_status = NODE_STATUS_NOT_RUN;
	error_msg.clear();
	return SUCCESS;
}

int HVCloudSegment::run()
{
    auto start = std::chrono::steady_clock::now();
    execute_status = NODE_STATUS_RUNNING;
    error_msg.clear();

    if (!inputCloud)
    {
        execute_status = ALGORITHM_RUN_ERROR;
        error_msg = "msg.input_null";
        return ALGORITHM_RUN_ERROR;
    }

    pcl::PointCloud<pcl::PointXYZ>::Ptr cloudIn = PointCloudConverter::ToPCL(*inputCloud);
    pcl::PointCloud<pcl::PointXYZ>::Ptr cloudOut(new pcl::PointCloud<pcl::PointXYZ>);

    switch (type)
    {
    case 0:
    {
        // RANSAC 平面分割
        pcl::ModelCoefficients::Ptr coefficients(new pcl::ModelCoefficients);
        pcl::PointIndices::Ptr inliers(new pcl::PointIndices);

        pcl::SACSegmentation<pcl::PointXYZ> seg;
        seg.setOptimizeCoefficients(true);
        seg.setModelType(pcl::SACMODEL_PLANE);
        seg.setMethodType(pcl::SAC_RANSAC);
        seg.setDistanceThreshold(distance_threshold);
        seg.setMaxIterations(max_iterations);
        seg.setInputCloud(cloudIn);
        seg.segment(*inliers, *coefficients);

        if (inliers->indices.empty()) {
            error_msg = "msg.ransac_no_inliers";
            execute_status = ALGORITHM_RUN_ERROR;
            return ALGORITHM_RUN_ERROR;
        }

        // 提取平面内点
        pcl::ExtractIndices<pcl::PointXYZ> extract;
        extract.setInputCloud(cloudIn);
        extract.setIndices(inliers);
        extract.setNegative(false);
        extract.filter(*cloudOut);

        error_msg = "msg.ransac_success";
    }
    break;
    case 1:
    {
        // 欧式聚类分割
        pcl::search::KdTree<pcl::PointXYZ>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZ>);
        tree->setInputCloud(cloudIn);

        std::vector<pcl::PointIndices> cluster_indices;
        pcl::EuclideanClusterExtraction<pcl::PointXYZ> ec;
        ec.setClusterTolerance(cluster_tolerance);
        ec.setMinClusterSize(min_cluster_size);
        ec.setMaxClusterSize(max_cluster_size);
        ec.setSearchMethod(tree);
        ec.setInputCloud(cloudIn);
        ec.extract(cluster_indices);

        if (cluster_indices.empty()) {
            error_msg = "msg.euclidean_no_clusters";
            execute_status = ALGORITHM_RUN_ERROR;
            return ALGORITHM_RUN_ERROR;
        }

        // 取最大聚类
        size_t largest_idx = 0;
        size_t largest_size = 0;
        for (size_t i = 0; i < cluster_indices.size(); ++i) {
            if (cluster_indices[i].indices.size() > largest_size) {
                largest_size = cluster_indices[i].indices.size();
                largest_idx = i;
            }
        }

        pcl::ExtractIndices<pcl::PointXYZ> extract;
        pcl::PointIndices::Ptr largest_cluster(new pcl::PointIndices(cluster_indices[largest_idx]));
        extract.setInputCloud(cloudIn);
        extract.setIndices(largest_cluster);
        extract.setNegative(false);
        extract.filter(*cloudOut);

        error_msg = "msg.euclidean_success";
    }
    break;
    case 2:
    {
        // 区域生长分割
        // 1. 法线估计
        pcl::search::KdTree<pcl::PointXYZ>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZ>);
        pcl::PointCloud<pcl::Normal>::Ptr normals(new pcl::PointCloud<pcl::Normal>);

        pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> normal_estimator;
        normal_estimator.setSearchMethod(tree);
        normal_estimator.setInputCloud(cloudIn);
        normal_estimator.setKSearch(number_of_neighbours);
        normal_estimator.compute(*normals);

        // 2. 区域生长
        pcl::RegionGrowing<pcl::PointXYZ, pcl::Normal> reg;
        reg.setMinClusterSize(min_cluster_size);
        reg.setMaxClusterSize(max_cluster_size);
        reg.setSearchMethod(tree);
        reg.setNumberOfNeighbours(number_of_neighbours);
        reg.setInputCloud(cloudIn);
        reg.setInputNormals(normals);
        reg.setSmoothnessThreshold(smoothness_threshold / 180.0f * static_cast<float>(M_PI));
        reg.setCurvatureThreshold(curvature_threshold);

        std::vector<pcl::PointIndices> clusters;
        reg.extract(clusters);

        if (clusters.empty()) {
            error_msg = "msg.region_no_clusters";
            execute_status = ALGORITHM_RUN_ERROR;
            return ALGORITHM_RUN_ERROR;
        }

        // 取最大聚类
        size_t largest_idx = 0;
        size_t largest_size = 0;
        for (size_t i = 0; i < clusters.size(); ++i) {
            if (clusters[i].indices.size() > largest_size) {
                largest_size = clusters[i].indices.size();
                largest_idx = i;
            }
        }

        pcl::ExtractIndices<pcl::PointXYZ> extract;
        pcl::PointIndices::Ptr largest_cluster(new pcl::PointIndices(clusters[largest_idx]));
        extract.setInputCloud(cloudIn);
        extract.setIndices(largest_cluster);
        extract.setNegative(false);
        extract.filter(*cloudOut);

        error_msg = "msg.region_success";
    }
    break;
    default:
    {
        error_msg = "msg.unsupported";
        execute_status = ALGORITHM_RUN_ERROR;
        return ALGORITHM_RUN_ERROR;
    }
    }

    resultCloud = std::make_shared<HVPointCloud>(PointCloudConverter::FromPCL(*cloudOut));

    auto end = std::chrono::steady_clock::now();
    run_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    execute_status = SUCCESS;
    return SUCCESS;
}

int HVCloudSegment::set_algorithm_params(const std::vector<void*>& params, const std::vector<int>& paramID)
{
    if (paramID.size() == 0)
    {
        inputCloud = cast_param_sharedPtr<HVPointCloud>(params, 0);
        type = cast_param<int>(params, 1);
        distance_threshold = cast_param<double>(params, 2);
        max_iterations = cast_param<int>(params, 3);
        cluster_tolerance = cast_param<double>(params, 4);
        min_cluster_size = cast_param<int>(params, 5);
        max_cluster_size = cast_param<int>(params, 6);
        number_of_neighbours = cast_param<int>(params, 7);
        smoothness_threshold = cast_param<float>(params, 8);
        curvature_threshold = cast_param<float>(params, 9);
    }
    else
    {
        for (size_t i = 0; i < paramID.size(); i++)
        {
            int id = paramID[i];
            switch (id)
            {
            case 0:
                inputCloud = cast_param_sharedPtr<HVPointCloud>(params, i);
                break;
            case 1:
                type = cast_param<int>(params, i);
                break;
            case 2:
                distance_threshold = cast_param<double>(params, i);
                break;
            case 3:
                max_iterations = cast_param<int>(params, i);
                break;
            case 4:
                cluster_tolerance = cast_param<double>(params, i);
                break;
            case 5:
                min_cluster_size = cast_param<int>(params, i);
                break;
            case 6:
                max_cluster_size = cast_param<int>(params, i);
                break;
            case 7:
                number_of_neighbours = cast_param<int>(params, i);
                break;
            case 8:
                smoothness_threshold = cast_param<float>(params, i);
                break;
            case 9:
                curvature_threshold = cast_param<float>(params, i);
                break;
            default:
                break;
            }
        }
    }

    return SUCCESS;
}

std::vector<void*> HVCloudSegment::get_current_params()
{
    return {
        &inputCloud,
        &type,
        &distance_threshold,
        &max_iterations,
        &cluster_tolerance,
        &min_cluster_size,
        &max_cluster_size,
        &number_of_neighbours,
        &smoothness_threshold,
        &curvature_threshold
    };
}

std::vector<void*> HVCloudSegment::get_algorithm_result()
{
	if (execute_status == SUCCESS)
	    return { &resultCloud, &execute_status };
    return { nullptr, &execute_status };
}

std::vector<int> HVCloudSegment::get_algorithm_input_params_type()
{
    return { HV_POINTCLOUD, HV_INT, HV_DOUBLE, HV_INT, HV_DOUBLE, HV_INT, HV_INT, HV_INT, HV_FLOAT, HV_FLOAT };
}

std::vector<int> HVCloudSegment::get_algorithm_output_params_type()
{
    return { HV_POINTCLOUD, HV_INT };
}

std::vector<std::string> HVCloudSegment::get_algorithm_input_params_name()
{
    return {
        Tr(language_, "input.cloud.name"),
        Tr(language_, "input.type.name"),
        Tr(language_, "input.distance_threshold.name"),
        Tr(language_, "input.max_iterations.name"),
        Tr(language_, "input.cluster_tolerance.name"),
        Tr(language_, "input.min_cluster_size.name"),
        Tr(language_, "input.max_cluster_size.name"),
        Tr(language_, "input.neighbours.name"),
        Tr(language_, "input.smoothness.name"),
        Tr(language_, "input.curvature.name")
    };
}

std::vector<std::string> HVCloudSegment::get_algorithm_output_params_name()
{
    return {
        Tr(language_, "output.cloud.name"),
        language_ == static_cast<int>(UIPilotLanguage::EN_US) ? "Execute status" : "运行状态"
    };
}

std::vector<bool> HVCloudSegment::get_algorithm_input_params_bindable()
{
    // Used as the default UI input mode hint, not as a hard binding restriction.
    return { true, false, false, false, false, false, false, false, false, false };
}

std::vector<ParamMetadata> HVCloudSegment::get_algorithm_input_params_metadata()
{
    std::vector<ParamMetadata> metadata_list;

    // 参数0: 输入点云 (可绑定，无约束)
    ParamMetadata meta0;
    meta0.param_name = Tr(language_, "input.cloud.name");
    meta0.param_description = Tr(language_, "input.cloud.desc");
    meta0.param_type = HV_POINTCLOUD;
    meta0.constraint_type = CONSTRAINT_NONE;
    metadata_list.push_back(meta0);

    // 参数1: 分割类型 (选项约束)
    ParamMetadata meta1;
    meta1.param_name = Tr(language_, "input.type.name");
    meta1.param_description = Tr(language_, "input.type.desc");
    meta1.param_type = HV_INT;
    meta1.constraint_type = CONSTRAINT_OPTIONS;
    meta1.options_constraint.AddOption(0, Tr(language_, "option.ransac"));
    meta1.options_constraint.AddOption(1, Tr(language_, "option.euclidean"));
    meta1.options_constraint.AddOption(2, Tr(language_, "option.region_growing"));
    meta1.options_constraint.default_index = 0;
    metadata_list.push_back(meta1);

    // 参数2: 距离阈值 (RANSAC, 依赖type=0)
    ParamMetadata meta2;
    meta2.param_name = Tr(language_, "input.distance_threshold.name");
    meta2.param_description = Tr(language_, "input.distance_threshold.desc");
    meta2.param_type = HV_DOUBLE;
    meta2.constraint_type = CONSTRAINT_RANGE;
    meta2.range_constraint = RangeConstraint(0.001, 100.0, 0.01);
    meta2.dependencies.push_back(ParamDependency(1, DEPENDS_ON_EQUALS, {"0"}));
    metadata_list.push_back(meta2);

    // 参数3: 最大迭代次数 (RANSAC, 依赖type=0)
    ParamMetadata meta3;
    meta3.param_name = Tr(language_, "input.max_iterations.name");
    meta3.param_description = Tr(language_, "input.max_iterations.desc");
    meta3.param_type = HV_INT;
    meta3.constraint_type = CONSTRAINT_RANGE;
    meta3.range_constraint = RangeConstraint(1, 10000, 1000);
    meta3.dependencies.push_back(ParamDependency(1, DEPENDS_ON_EQUALS, {"0"}));
    metadata_list.push_back(meta3);

    // 参数4: 聚类距离容差 (欧式聚类, 依赖type=1)
    ParamMetadata meta4;
    meta4.param_name = Tr(language_, "input.cluster_tolerance.name");
    meta4.param_description = Tr(language_, "input.cluster_tolerance.desc");
    meta4.param_type = HV_DOUBLE;
    meta4.constraint_type = CONSTRAINT_RANGE;
    meta4.range_constraint = RangeConstraint(0.001, 100.0, 0.02);
    meta4.dependencies.push_back(ParamDependency(1, DEPENDS_ON_EQUALS, {"1"}));
    metadata_list.push_back(meta4);

    // 参数5: 最小聚类点数 (欧式聚类/区域生长, 依赖type IN [1,2])
    ParamMetadata meta5;
    meta5.param_name = Tr(language_, "input.min_cluster_size.name");
    meta5.param_description = Tr(language_, "input.min_cluster_size.desc");
    meta5.param_type = HV_INT;
    meta5.constraint_type = CONSTRAINT_RANGE;
    meta5.range_constraint = RangeConstraint(1, 1000000, 100);
    meta5.dependencies.push_back(ParamDependency(1, DEPENDS_ON_IN_LIST, {"1", "2"}));
    metadata_list.push_back(meta5);

    // 参数6: 最大聚类点数 (欧式聚类/区域生长, 依赖type IN [1,2])
    ParamMetadata meta6;
    meta6.param_name = Tr(language_, "input.max_cluster_size.name");
    meta6.param_description = Tr(language_, "input.max_cluster_size.desc");
    meta6.param_type = HV_INT;
    meta6.constraint_type = CONSTRAINT_RANGE;
    meta6.range_constraint = RangeConstraint(1, 10000000, 100000);
    meta6.dependencies.push_back(ParamDependency(1, DEPENDS_ON_IN_LIST, {"1", "2"}));
    metadata_list.push_back(meta6);

    // 参数7: 邻域点数 (区域生长, 依赖type=2)
    ParamMetadata meta7;
    meta7.param_name = Tr(language_, "input.neighbours.name");
    meta7.param_description = Tr(language_, "input.neighbours.desc");
    meta7.param_type = HV_INT;
    meta7.constraint_type = CONSTRAINT_RANGE;
    meta7.range_constraint = RangeConstraint(1, 200, 30);
    meta7.dependencies.push_back(ParamDependency(1, DEPENDS_ON_EQUALS, {"2"}));
    metadata_list.push_back(meta7);

    // 参数8: 平滑度阈值 (区域生长, 依赖type=2)
    ParamMetadata meta8;
    meta8.param_name = Tr(language_, "input.smoothness.name");
    meta8.param_description = Tr(language_, "input.smoothness.desc");
    meta8.param_type = HV_FLOAT;
    meta8.constraint_type = CONSTRAINT_RANGE;
    meta8.range_constraint = RangeConstraint(0.1, 180.0, 3.0);
    meta8.dependencies.push_back(ParamDependency(1, DEPENDS_ON_EQUALS, {"2"}));
    metadata_list.push_back(meta8);

    // 参数9: 曲率阈值 (区域生长, 依赖type=2)
    ParamMetadata meta9;
    meta9.param_name = Tr(language_, "input.curvature.name");
    meta9.param_description = Tr(language_, "input.curvature.desc");
    meta9.param_type = HV_FLOAT;
    meta9.constraint_type = CONSTRAINT_RANGE;
    meta9.range_constraint = RangeConstraint(0.001, 100.0, 1.0);
    meta9.dependencies.push_back(ParamDependency(1, DEPENDS_ON_EQUALS, {"2"}));
    metadata_list.push_back(meta9);

    for (size_t i = 0; i < metadata_list.size(); ++i) {
        metadata_list[i].param_group = (i < 2) ? PARAM_GROUP_BASIC : PARAM_GROUP_ADVANCED;
    }

    return metadata_list;
}

int HVCloudSegment::get_algorithm_execute_status()
{
	return execute_status;
}

std::string HVCloudSegment::get_algorithm_error_message()
{
    if (error_msg.empty()) {
        return "";
    }
    return Tr(language_, error_msg);
}

long HVCloudSegment::get_algorithm_use_time()
{
	return run_time;
}

bool HVCloudSegment::algorithm_params_setting_status()
{
	return true;
}

bool HVCloudSegment::algorithm_init_status()
{
	return true;
}

bool HVCloudSegment::save_params_to_json(const std::string& filePath)
{
    try {
        nlohmann::json params_json = nlohmann::json::array();

        add_param(params_json, "type", HV_INT, this->type);
        add_param(params_json, "distance_threshold", HV_DOUBLE, this->distance_threshold);
        add_param(params_json, "max_iterations", HV_INT, this->max_iterations);
        add_param(params_json, "cluster_tolerance", HV_DOUBLE, this->cluster_tolerance);
        add_param(params_json, "min_cluster_size", HV_INT, this->min_cluster_size);
        add_param(params_json, "max_cluster_size", HV_INT, this->max_cluster_size);
        add_param(params_json, "number_of_neighbours", HV_INT, this->number_of_neighbours);
        add_param(params_json, "smoothness_threshold", HV_FLOAT, this->smoothness_threshold);
        add_param(params_json, "curvature_threshold", HV_FLOAT, this->curvature_threshold);

        // 写入文件
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

bool HVCloudSegment::load_params_from_json(const std::string& filePath)
{
    try {
        // 读取文件
        std::ifstream file(filePath);
        if (!file.is_open()) {
            return false;
        }

        nlohmann::json params_json;
        file >> params_json;
        file.close();

        // 检查JSON是否为数组
        if (!params_json.is_array()) {
            return false;
        }

        // 遍历参数数组
        for (const auto& param_json : params_json) {
            if (!param_json.contains("name") || !param_json.contains("type")) {
                continue;
            }

            std::string param_name = param_json["name"];

            if (param_name == "type") {
                this->type = param_json["value"];
            }
            else if (param_name == "distance_threshold") {
                this->distance_threshold = param_json["value"];
            }
            else if (param_name == "max_iterations") {
                this->max_iterations = param_json["value"];
            }
            else if (param_name == "cluster_tolerance") {
                this->cluster_tolerance = param_json["value"];
            }
            else if (param_name == "min_cluster_size") {
                this->min_cluster_size = param_json["value"];
            }
            else if (param_name == "max_cluster_size") {
                this->max_cluster_size = param_json["value"];
            }
            else if (param_name == "number_of_neighbours") {
                this->number_of_neighbours = param_json["value"];
            }
            else if (param_name == "smoothness_threshold") {
                this->smoothness_threshold = param_json["value"];
            }
            else if (param_name == "curvature_threshold") {
                this->curvature_threshold = param_json["value"];
            }
        }

        return true;
    }
    catch (...) {
        return false;
    }
}

AlgorithmType HVCloudSegment::get_algorithm_type()
{
    return AlgorithmType::PointCloudProcess;
}

void HVCloudSegment::set_language(int language)
{
    if (hvi18n::IsSupportedLanguage(language)) {
        language_ = language;
    }
}

int HVCloudSegment::get_language() const
{
    return language_;
}

std::string HVCloudSegment::get_algorithm_display_name()
{
    return Tr(language_, "algorithm.display");
}

NodeEngine* CreateInstance() {
    // 每一个 DLL 内部返回自己具体的实现类
    return new HVCloudSegment();
}

std::string GetInstanceName() {
    return "Point cloud segment"; // 告知主程序此 DLL 代表的类型
}

extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion() {
    return NODE_ENGINE_ABI_VERSION;
}
