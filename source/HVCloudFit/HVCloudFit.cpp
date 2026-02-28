#include "HVCloudFit.h"
#include "HVUtils.h"

#include <chrono>
#include <sstream>
#include <iomanip>

#include <pcl/segmentation/sac_segmentation.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/features/normal_3d.h>
#include <pcl/search/kdtree.h>

HVCloudFit::HVCloudFit()
{
}

HVCloudFit::~HVCloudFit()
{
}

int HVCloudFit::init()
{
    return SUCCESS;
}

int HVCloudFit::run()
{
    auto start = std::chrono::steady_clock::now();

    if (!inputCloud) {
        error_msg = "Input cloud is null";
        execute_status = -1;
        return -1;
    }

    pcl::PointCloud<pcl::PointXYZ>::Ptr cloudIn = PointCloudConverter::ToPCL(*inputCloud);
    pcl::PointCloud<pcl::PointXYZ> cloudInlier;
    pcl::ModelCoefficients::Ptr coefficients(new pcl::ModelCoefficients);
    pcl::PointIndices::Ptr inliers(new pcl::PointIndices);

    switch (type)
    {
    case 0:
    {
        // 平面拟合：RANSAC + SACMODEL_PLANE
        pcl::SACSegmentation<pcl::PointXYZ> seg;
        seg.setOptimizeCoefficients(true);
        seg.setModelType(pcl::SACMODEL_PLANE);
        seg.setMethodType(pcl::SAC_RANSAC);
        seg.setDistanceThreshold(distance_threshold);
        seg.setMaxIterations(max_iterations);
        seg.setInputCloud(cloudIn);
        seg.segment(*inliers, *coefficients);

        if (inliers->indices.empty()) {
            error_msg = "Plane fitting: no inliers found";
            execute_status = -1;
            return -1;
        }

        // 提取内点
        pcl::ExtractIndices<pcl::PointXYZ> extract;
        extract.setInputCloud(cloudIn);
        extract.setIndices(inliers);
        extract.setNegative(false);
        extract.filter(cloudInlier);

        // 模型系数: [a, b, c, d]，平面方程 ax + by + cz + d = 0
        float a = coefficients->values[0];
        float b = coefficients->values[1];
        float c = coefficients->values[2];
        float d = coefficients->values[3];

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(8);
        oss << "{\"a\":" << a << ",\"b\":" << b << ",\"c\":" << c << ",\"d\":" << d << "}";
        modelCoefficients = oss.str();

        error_msg = "Plane fitting success, inliers: " + std::to_string(cloudInlier.size())
            + ", equation: " + std::to_string(a) + "x + " + std::to_string(b) + "y + "
            + std::to_string(c) + "z + " + std::to_string(d) + " = 0";
    }
    break;
    case 1:
    {
        // 圆柱拟合：需要法线，使用 SACSegmentationFromNormals + SACMODEL_CYLINDER
        // 1. 估计法线
        pcl::search::KdTree<pcl::PointXYZ>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZ>);
        pcl::PointCloud<pcl::Normal>::Ptr normals(new pcl::PointCloud<pcl::Normal>);

        pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> ne;
        ne.setSearchMethod(tree);
        ne.setRadiusSearch(normal_radius_search);
        ne.setInputCloud(cloudIn);
        ne.compute(*normals);

        // 2. 圆柱分割
        pcl::SACSegmentationFromNormals<pcl::PointXYZ, pcl::Normal> seg;
        seg.setOptimizeCoefficients(true);
        seg.setModelType(pcl::SACMODEL_CYLINDER);
        seg.setMethodType(pcl::SAC_RANSAC);
        seg.setNormalDistanceWeight(normal_distance_weight);
        seg.setDistanceThreshold(distance_threshold);
        seg.setMaxIterations(max_iterations);
        seg.setRadiusLimits(radius_min, radius_max);
        seg.setInputCloud(cloudIn);
        seg.setInputNormals(normals);
        seg.segment(*inliers, *coefficients);

        if (inliers->indices.empty()) {
            error_msg = "Cylinder fitting: no inliers found";
            execute_status = -1;
            return -1;
        }

        // 提取内点
        pcl::ExtractIndices<pcl::PointXYZ> extract;
        extract.setInputCloud(cloudIn);
        extract.setIndices(inliers);
        extract.setNegative(false);
        extract.filter(cloudInlier);

        // 模型系数: [point_x, point_y, point_z, axis_x, axis_y, axis_z, radius]
        // point 为轴线上一点，axis 为轴线方向向量，radius 为圆柱半径
        float px = coefficients->values[0];
        float py = coefficients->values[1];
        float pz = coefficients->values[2];
        float ax = coefficients->values[3];
        float ay = coefficients->values[4];
        float az = coefficients->values[5];
        float r  = coefficients->values[6];

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(8);
        oss << "{"
            << "\"point_x\":" << px << ","
            << "\"point_y\":" << py << ","
            << "\"point_z\":" << pz << ","
            << "\"axis_x\":"  << ax << ","
            << "\"axis_y\":"  << ay << ","
            << "\"axis_z\":"  << az << ","
            << "\"radius\":"  << r
            << "}";
        modelCoefficients = oss.str();

        error_msg = "Cylinder fitting success, inliers: " + std::to_string(cloudInlier.size())
            + ", radius: " + std::to_string(r);
    }
    break;
    default:
    {
        error_msg = "Unsupported fit type";
        execute_status = -1;
        return -1;
    }
    }

    inlierCloud = std::make_shared<HVPointCloud>(PointCloudConverter::FromPCL(cloudInlier));

    auto end = std::chrono::steady_clock::now();
    run_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    execute_status = 0;
    return 0;
}

int HVCloudFit::set_algorithm_params(const std::vector<void*>& params, const std::vector<int>& paramID)
{
    if (paramID.size() == 0)
    {
        inputCloud            = cast_param_sharedPtr<HVPointCloud>(params, 0);
        type                  = cast_param<int>(params, 1);
        distance_threshold    = cast_param<double>(params, 2);
        max_iterations        = cast_param<int>(params, 3);
        normal_radius_search  = cast_param<double>(params, 4);
        normal_distance_weight = cast_param<double>(params, 5);
        radius_min            = cast_param<double>(params, 6);
        radius_max            = cast_param<double>(params, 7);
    }
    else
    {
        for (size_t i = 0; i < paramID.size(); i++)
        {
            int id = paramID[i];
            switch (id)
            {
            case 0: inputCloud             = cast_param_sharedPtr<HVPointCloud>(params, i); break;
            case 1: type                   = cast_param<int>(params, i);    break;
            case 2: distance_threshold     = cast_param<double>(params, i); break;
            case 3: max_iterations         = cast_param<int>(params, i);    break;
            case 4: normal_radius_search   = cast_param<double>(params, i); break;
            case 5: normal_distance_weight = cast_param<double>(params, i); break;
            case 6: radius_min             = cast_param<double>(params, i); break;
            case 7: radius_max             = cast_param<double>(params, i); break;
            default: break;
            }
        }
    }

    return SUCCESS;
}

std::vector<void*> HVCloudFit::get_current_params()
{
    return {
        &inputCloud,
        &type,
        &distance_threshold,
        &max_iterations,
        &normal_radius_search,
        &normal_distance_weight,
        &radius_min,
        &radius_max
    };
}

std::vector<void*> HVCloudFit::get_algorithm_result()
{
    if (execute_status == SUCCESS)
        return { &inlierCloud, &modelCoefficients };
    return { nullptr, nullptr };
}

std::vector<int> HVCloudFit::get_algorithm_input_params_type()
{
    return { HV_POINTCLOUD, HV_INT, HV_DOUBLE, HV_INT, HV_DOUBLE, HV_DOUBLE, HV_DOUBLE, HV_DOUBLE };
}

std::vector<int> HVCloudFit::get_algorithm_output_params_type()
{
    return { HV_POINTCLOUD, HV_STRING };
}

std::vector<std::string> HVCloudFit::get_algorithm_input_params_name()
{
    return {
        "input cloud",
        "fit type",
        "distance threshold",
        "max iterations",
        "normal radius search",
        "normal distance weight",
        "radius min",
        "radius max"
    };
}

std::vector<std::string> HVCloudFit::get_algorithm_output_params_name()
{
    return { "inlier cloud", "model coefficients" };
}

std::vector<bool> HVCloudFit::get_algorithm_input_params_bindable()
{
    return { true, false, false, false, false, false, false, false };
}

std::vector<ParamMetadata> HVCloudFit::get_algorithm_input_params_metadata()
{
    std::vector<ParamMetadata> metadata_list;

    // 参数0: 输入点云 (可绑定，无约束)
    ParamMetadata meta0;
    meta0.param_name = "input cloud";
    meta0.param_description = "Input point cloud to be fitted";
    meta0.param_type = HV_POINTCLOUD;
    meta0.constraint_type = CONSTRAINT_NONE;
    metadata_list.push_back(meta0);

    // 参数1: 拟合类型 (选项约束)
    ParamMetadata meta1;
    meta1.param_name = "fit type";
    meta1.param_description = "Geometric model type to fit";
    meta1.param_type = HV_INT;
    meta1.constraint_type = CONSTRAINT_OPTIONS;
    meta1.options_constraint.AddOption(0, "Plane");
    meta1.options_constraint.AddOption(1, "Cylinder");
    meta1.options_constraint.default_index = 0;
    metadata_list.push_back(meta1);

    // 参数2: RANSAC距离阈值 (共用, 依赖type IN [0,1])
    ParamMetadata meta2;
    meta2.param_name = "distance threshold";
    meta2.param_description = "Maximum distance from a point to the model to be considered an inlier";
    meta2.param_type = HV_DOUBLE;
    meta2.constraint_type = CONSTRAINT_RANGE;
    meta2.range_constraint = RangeConstraint(0.0001, 100.0, 0.01);
    meta2.dependencies.push_back(ParamDependency(1, DEPENDS_ON_IN_LIST, {"0", "1"}));
    metadata_list.push_back(meta2);

    // 参数3: 最大迭代次数 (共用, 依赖type IN [0,1])
    ParamMetadata meta3;
    meta3.param_name = "max iterations";
    meta3.param_description = "Maximum number of RANSAC iterations";
    meta3.param_type = HV_INT;
    meta3.constraint_type = CONSTRAINT_RANGE;
    meta3.range_constraint = RangeConstraint(1, 100000, 1000);
    meta3.dependencies.push_back(ParamDependency(1, DEPENDS_ON_IN_LIST, {"0", "1"}));
    metadata_list.push_back(meta3);

    // 参数4: 法线估计搜索半径 (圆柱, 依赖type=1)
    ParamMetadata meta4;
    meta4.param_name = "normal radius search";
    meta4.param_description = "Search radius for normal estimation (cylinder fitting only)";
    meta4.param_type = HV_DOUBLE;
    meta4.constraint_type = CONSTRAINT_RANGE;
    meta4.range_constraint = RangeConstraint(0.001, 100.0, 0.1);
    meta4.dependencies.push_back(ParamDependency(1, DEPENDS_ON_EQUALS, {"1"}));
    metadata_list.push_back(meta4);

    // 参数5: 法线距离权重 (圆柱, 依赖type=1)
    ParamMetadata meta5;
    meta5.param_name = "normal distance weight";
    meta5.param_description = "Weight for the normal direction component in the distance metric [0, 1]";
    meta5.param_type = HV_DOUBLE;
    meta5.constraint_type = CONSTRAINT_RANGE;
    meta5.range_constraint = RangeConstraint(0.0, 1.0, 0.1);
    meta5.dependencies.push_back(ParamDependency(1, DEPENDS_ON_EQUALS, {"1"}));
    metadata_list.push_back(meta5);

    // 参数6: 最小半径 (圆柱, 依赖type=1)
    ParamMetadata meta6;
    meta6.param_name = "radius min";
    meta6.param_description = "Minimum allowed cylinder radius";
    meta6.param_type = HV_DOUBLE;
    meta6.constraint_type = CONSTRAINT_RANGE;
    meta6.range_constraint = RangeConstraint(0.0, 10000.0, 0.0);
    meta6.dependencies.push_back(ParamDependency(1, DEPENDS_ON_EQUALS, {"1"}));
    metadata_list.push_back(meta6);

    // 参数7: 最大半径 (圆柱, 依赖type=1)
    ParamMetadata meta7;
    meta7.param_name = "radius max";
    meta7.param_description = "Maximum allowed cylinder radius";
    meta7.param_type = HV_DOUBLE;
    meta7.constraint_type = CONSTRAINT_RANGE;
    meta7.range_constraint = RangeConstraint(0.0, 10000.0, 1.0);
    meta7.dependencies.push_back(ParamDependency(1, DEPENDS_ON_EQUALS, {"1"}));
    metadata_list.push_back(meta7);

    return metadata_list;
}

int HVCloudFit::get_algorithm_execute_status()
{
    return execute_status;
}

std::string HVCloudFit::get_algorithm_error_message()
{
    return error_msg;
}

long HVCloudFit::get_algorithm_use_time()
{
    return run_time;
}

bool HVCloudFit::algorithm_params_setting_status()
{
    return true;
}

bool HVCloudFit::algorithm_init_status()
{
    return true;
}

bool HVCloudFit::save_params_to_json(const std::string& filePath)
{
    try {
        nlohmann::json params_json = nlohmann::json::array();

        add_param(params_json, "type",                   HV_INT,    this->type);
        add_param(params_json, "distance_threshold",     HV_DOUBLE, this->distance_threshold);
        add_param(params_json, "max_iterations",         HV_INT,    this->max_iterations);
        add_param(params_json, "normal_radius_search",   HV_DOUBLE, this->normal_radius_search);
        add_param(params_json, "normal_distance_weight", HV_DOUBLE, this->normal_distance_weight);
        add_param(params_json, "radius_min",             HV_DOUBLE, this->radius_min);
        add_param(params_json, "radius_max",             HV_DOUBLE, this->radius_max);

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

bool HVCloudFit::load_params_from_json(const std::string& filePath)
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
            else if (param_name == "distance_threshold")
                this->distance_threshold = param_json["value"];
            else if (param_name == "max_iterations")
                this->max_iterations = param_json["value"];
            else if (param_name == "normal_radius_search")
                this->normal_radius_search = param_json["value"];
            else if (param_name == "normal_distance_weight")
                this->normal_distance_weight = param_json["value"];
            else if (param_name == "radius_min")
                this->radius_min = param_json["value"];
            else if (param_name == "radius_max")
                this->radius_max = param_json["value"];
        }

        return true;
    }
    catch (...) {
        return false;
    }
}

AlgorithmType HVCloudFit::get_algorithm_type()
{
    return AlgorithmType::PointCloudProcess;
}

NodeEngine* CreateInstance() {
    return new HVCloudFit();
}

std::string GetInstanceName() {
    return "Point cloud fit";
}
