#include "HVCloudFit.h"
#include "HVUtils.h"
#include "HVI18n.h"

#include <chrono>
#include <sstream>
#include <iomanip>

#include <pcl/segmentation/sac_segmentation.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/features/normal_3d.h>
#include <pcl/search/kdtree.h>

namespace {

// Keep all user-visible strings centralized here so language switching stays consistent.
const hvi18n::Dictionary kTexts = {
    { "algorithm.display", { "点云拟合", "Point cloud fit" } },
    { "output.inlier_cloud", { "内点点云", "inlier cloud" } },
    { "output.model_coeff", { "模型系数", "model coefficients" } },
    { "option.plane", { "平面", "Plane" } },
    { "option.cylinder", { "圆柱", "Cylinder" } },
    { "msg.input_null", { "输入点云为空", "Input cloud is null" } },
    { "msg.plane_no_inliers", { "平面拟合未找到内点", "Plane fitting: no inliers found" } },
    { "msg.plane_success", { "平面拟合成功", "Plane fitting success" } },
    { "msg.cylinder_no_inliers", { "圆柱拟合未找到内点", "Cylinder fitting: no inliers found" } },
    { "msg.cylinder_success", { "圆柱拟合成功", "Cylinder fitting success" } },
    { "msg.unsupported", { "不支持的拟合类型", "Unsupported fit type" } },
    { "name.0", { "输入点云", "input cloud" } },
    { "name.1", { "拟合类型", "fit type" } },
    { "name.2", { "距离阈值", "distance threshold" } },
    { "name.3", { "最大迭代次数", "max iterations" } },
    { "name.4", { "法线搜索半径", "normal radius search" } },
    { "name.5", { "法线距离权重", "normal distance weight" } },
    { "name.6", { "最小半径", "radius min" } },
    { "name.7", { "最大半径", "radius max" } },
    { "desc.0", { "输入待拟合点云", "Input point cloud to be fitted" } },
    { "desc.1", { "待拟合的几何模型类型", "Geometric model type to fit" } },
    { "desc.2", { "判定为内点的点到模型最大距离", "Maximum distance from a point to the model to be considered an inlier" } },
    { "desc.3", { "RANSAC 最大迭代次数", "Maximum number of RANSAC iterations" } },
    { "desc.4", { "圆柱拟合使用的法线搜索半径", "Search radius for normal estimation (cylinder fitting only)" } },
    { "desc.5", { "距离度量中法线方向分量的权重 [0, 1]", "Weight for the normal direction component in the distance metric [0, 1]" } },
    { "desc.6", { "允许的最小圆柱半径", "Minimum allowed cylinder radius" } },
    { "desc.7", { "允许的最大圆柱半径", "Maximum allowed cylinder radius" } }
};

std::string Tr(int language, const std::string& key) {
    return hvi18n::Translate(kTexts, key, language);
}

}  // namespace

HVCloudFit::HVCloudFit()
{
}

HVCloudFit::~HVCloudFit()
{
}

int HVCloudFit::init()
{
    execute_status = NODE_STATUS_NOT_RUN;
    error_msg.clear();
    return SUCCESS;
}

int HVCloudFit::run()
{
    auto start = std::chrono::steady_clock::now();
    execute_status = NODE_STATUS_RUNNING;
    error_msg.clear();

    if (!inputCloud) {
        error_msg = "msg.input_null";
        execute_status = ALGORITHM_RUN_ERROR;
        return ALGORITHM_RUN_ERROR;
    }

    pcl::PointCloud<pcl::PointXYZ>::Ptr cloudIn = PointCloudConverter::ToPCL(*inputCloud);
    pcl::PointCloud<pcl::PointXYZ> cloudInlier;
    pcl::ModelCoefficients::Ptr coefficients(new pcl::ModelCoefficients);
    pcl::PointIndices::Ptr inliers(new pcl::PointIndices);

    switch (type)
    {
    case 0:
    {
        // 骞抽潰鎷熷悎锛歊ANSAC + SACMODEL_PLANE
        pcl::SACSegmentation<pcl::PointXYZ> seg;
        seg.setOptimizeCoefficients(true);
        seg.setModelType(pcl::SACMODEL_PLANE);
        seg.setMethodType(pcl::SAC_RANSAC);
        seg.setDistanceThreshold(distance_threshold);
        seg.setMaxIterations(max_iterations);
        seg.setInputCloud(cloudIn);
        seg.segment(*inliers, *coefficients);

        if (inliers->indices.empty()) {
            error_msg = "msg.plane_no_inliers";
            execute_status = ALGORITHM_RUN_ERROR;
            return ALGORITHM_RUN_ERROR;
        }

        // 鎻愬彇鍐呯偣
        pcl::ExtractIndices<pcl::PointXYZ> extract;
        extract.setInputCloud(cloudIn);
        extract.setIndices(inliers);
        extract.setNegative(false);
        extract.filter(cloudInlier);

        // 妯″瀷绯绘暟: [a, b, c, d]锛屽钩闈㈡柟绋?ax + by + cz + d = 0
        float a = coefficients->values[0];
        float b = coefficients->values[1];
        float c = coefficients->values[2];
        float d = coefficients->values[3];

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(8);
        oss << "{\"a\":" << a << ",\"b\":" << b << ",\"c\":" << c << ",\"d\":" << d << "}";
        modelCoefficients = oss.str();

        error_msg = "msg.plane_success";
    }
    break;
    case 1:
    {
        // 鍦嗘煴鎷熷悎锛氶渶瑕佹硶绾匡紝浣跨敤 SACSegmentationFromNormals + SACMODEL_CYLINDER
        // 1. 浼拌娉曠嚎
        pcl::search::KdTree<pcl::PointXYZ>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZ>);
        pcl::PointCloud<pcl::Normal>::Ptr normals(new pcl::PointCloud<pcl::Normal>);

        pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> ne;
        ne.setSearchMethod(tree);
        ne.setRadiusSearch(normal_radius_search);
        ne.setInputCloud(cloudIn);
        ne.compute(*normals);

        // 2. 鍦嗘煴鍒嗗壊
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
            error_msg = "msg.cylinder_no_inliers";
            execute_status = ALGORITHM_RUN_ERROR;
            return ALGORITHM_RUN_ERROR;
        }

        // 鎻愬彇鍐呯偣
        pcl::ExtractIndices<pcl::PointXYZ> extract;
        extract.setInputCloud(cloudIn);
        extract.setIndices(inliers);
        extract.setNegative(false);
        extract.filter(cloudInlier);

        // 妯″瀷绯绘暟: [point_x, point_y, point_z, axis_x, axis_y, axis_z, radius]
        // point 涓鸿酱绾夸笂涓€鐐癸紝axis 涓鸿酱绾挎柟鍚戝悜閲忥紝radius 涓哄渾鏌卞崐寰?
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

        error_msg = "msg.cylinder_success";
    }
    break;
    default:
    {
        error_msg = "msg.unsupported";
        execute_status = ALGORITHM_RUN_ERROR;
        return ALGORITHM_RUN_ERROR;
    }
    }

    inlierCloud = std::make_shared<HVPointCloud>(PointCloudConverter::FromPCL(cloudInlier));

    auto end = std::chrono::steady_clock::now();
    run_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    execute_status = SUCCESS;
    return SUCCESS;
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
        return { &inlierCloud, &modelCoefficients, &execute_status };
    return { nullptr, nullptr, &execute_status };
}

std::vector<int> HVCloudFit::get_algorithm_input_params_type()
{
    return { HV_POINTCLOUD, HV_INT, HV_DOUBLE, HV_INT, HV_DOUBLE, HV_DOUBLE, HV_DOUBLE, HV_DOUBLE };
}

std::vector<int> HVCloudFit::get_algorithm_output_params_type()
{
    return { HV_POINTCLOUD, HV_STRING, HV_INT };
}

std::vector<std::string> HVCloudFit::get_algorithm_input_params_name()
{
    return {
        Tr(language_, "name.0"),
        Tr(language_, "name.1"),
        Tr(language_, "name.2"),
        Tr(language_, "name.3"),
        Tr(language_, "name.4"),
        Tr(language_, "name.5"),
        Tr(language_, "name.6"),
        Tr(language_, "name.7")
    };
}

std::vector<std::string> HVCloudFit::get_algorithm_output_params_name()
{
    return {
        Tr(language_, "output.inlier_cloud"),
        Tr(language_, "output.model_coeff"),
        language_ == static_cast<int>(UIPilotLanguage::EN_US) ? "Execute status" : "运行状态"
    };
}

std::vector<bool> HVCloudFit::get_algorithm_input_params_bindable()
{
    // Used as the default UI input mode hint, not as a hard binding restriction.
    return { true, false, false, false, false, false, false, false };
}

std::vector<ParamMetadata> HVCloudFit::get_algorithm_input_params_metadata()
{
    std::vector<ParamMetadata> metadata_list;

    // 鍙傛暟0: 杈撳叆鐐逛簯 (鍙粦瀹氾紝鏃犵害鏉?
    ParamMetadata meta0;
    meta0.param_name = "input cloud";
    meta0.param_description = "Input point cloud to be fitted";
    meta0.param_type = HV_POINTCLOUD;
    meta0.constraint_type = CONSTRAINT_NONE;
    metadata_list.push_back(meta0);

    // 鍙傛暟1: 鎷熷悎绫诲瀷 (閫夐」绾︽潫)
    ParamMetadata meta1;
    meta1.param_name = "fit type";
    meta1.param_description = "Geometric model type to fit";
    meta1.param_type = HV_INT;
    meta1.constraint_type = CONSTRAINT_OPTIONS;
    meta1.options_constraint.AddOption(0, "Plane");
    meta1.options_constraint.AddOption(1, "Cylinder");
    meta1.options_constraint.default_index = 0;
    metadata_list.push_back(meta1);

    // 鍙傛暟2: RANSAC璺濈闃堝€?(鍏辩敤, 渚濊禆type IN [0,1])
    ParamMetadata meta2;
    meta2.param_name = "distance threshold";
    meta2.param_description = "Maximum distance from a point to the model to be considered an inlier";
    meta2.param_type = HV_DOUBLE;
    meta2.constraint_type = CONSTRAINT_RANGE;
    meta2.range_constraint = RangeConstraint(0.0001, 100.0, 0.01);
    meta2.dependencies.push_back(ParamDependency(1, DEPENDS_ON_IN_LIST, {"0", "1"}));
    metadata_list.push_back(meta2);

    // 鍙傛暟3: 鏈€澶ц凯浠ｆ鏁?(鍏辩敤, 渚濊禆type IN [0,1])
    ParamMetadata meta3;
    meta3.param_name = "max iterations";
    meta3.param_description = "Maximum number of RANSAC iterations";
    meta3.param_type = HV_INT;
    meta3.constraint_type = CONSTRAINT_RANGE;
    meta3.range_constraint = RangeConstraint(1, 100000, 1000);
    meta3.dependencies.push_back(ParamDependency(1, DEPENDS_ON_IN_LIST, {"0", "1"}));
    metadata_list.push_back(meta3);

    // 鍙傛暟4: 娉曠嚎浼拌鎼滅储鍗婂緞 (鍦嗘煴, 渚濊禆type=1)
    ParamMetadata meta4;
    meta4.param_name = "normal radius search";
    meta4.param_description = "Search radius for normal estimation (cylinder fitting only)";
    meta4.param_type = HV_DOUBLE;
    meta4.constraint_type = CONSTRAINT_RANGE;
    meta4.range_constraint = RangeConstraint(0.001, 100.0, 0.1);
    meta4.dependencies.push_back(ParamDependency(1, DEPENDS_ON_EQUALS, {"1"}));
    metadata_list.push_back(meta4);

    // 鍙傛暟5: 娉曠嚎璺濈鏉冮噸 (鍦嗘煴, 渚濊禆type=1)
    ParamMetadata meta5;
    meta5.param_name = "normal distance weight";
    meta5.param_description = "Weight for the normal direction component in the distance metric [0, 1]";
    meta5.param_type = HV_DOUBLE;
    meta5.constraint_type = CONSTRAINT_RANGE;
    meta5.range_constraint = RangeConstraint(0.0, 1.0, 0.1);
    meta5.dependencies.push_back(ParamDependency(1, DEPENDS_ON_EQUALS, {"1"}));
    metadata_list.push_back(meta5);

    // 鍙傛暟6: 鏈€灏忓崐寰?(鍦嗘煴, 渚濊禆type=1)
    ParamMetadata meta6;
    meta6.param_name = "radius min";
    meta6.param_description = "Minimum allowed cylinder radius";
    meta6.param_type = HV_DOUBLE;
    meta6.constraint_type = CONSTRAINT_RANGE;
    meta6.range_constraint = RangeConstraint(0.0, 10000.0, 0.0);
    meta6.dependencies.push_back(ParamDependency(1, DEPENDS_ON_EQUALS, {"1"}));
    metadata_list.push_back(meta6);

    // 鍙傛暟7: 鏈€澶у崐寰?(鍦嗘煴, 渚濊禆type=1)
    ParamMetadata meta7;
    meta7.param_name = "radius max";
    meta7.param_description = "Maximum allowed cylinder radius";
    meta7.param_type = HV_DOUBLE;
    meta7.constraint_type = CONSTRAINT_RANGE;
    meta7.range_constraint = RangeConstraint(0.0, 10000.0, 1.0);
    meta7.dependencies.push_back(ParamDependency(1, DEPENDS_ON_EQUALS, {"1"}));
    metadata_list.push_back(meta7);

    for (size_t i = 0; i < metadata_list.size(); ++i) {
        const std::string idx = std::to_string(i);
        metadata_list[i].param_name = Tr(language_, "name." + idx);
        metadata_list[i].param_description = Tr(language_, "desc." + idx);
    }
    if (metadata_list.size() > 1 && metadata_list[1].options_constraint.option_labels.size() >= 2) {
        metadata_list[1].options_constraint.option_labels[0] = Tr(language_, "option.plane");
        metadata_list[1].options_constraint.option_labels[1] = Tr(language_, "option.cylinder");
    }

    for (size_t i = 0; i < metadata_list.size(); ++i) {
        metadata_list[i].param_group = (i < 2) ? PARAM_GROUP_BASIC : PARAM_GROUP_ADVANCED;
    }

    return metadata_list;
}

int HVCloudFit::get_algorithm_execute_status()
{
    return execute_status;
}

std::string HVCloudFit::get_algorithm_error_message()
{
    if (error_msg.empty()) {
        return "";
    }
    return Tr(language_, error_msg);
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

void HVCloudFit::set_language(int language)
{
    if (hvi18n::IsSupportedLanguage(language)) {
        language_ = language;
    }
}

int HVCloudFit::get_language() const
{
    return language_;
}

std::string HVCloudFit::get_algorithm_display_name()
{
    return Tr(language_, "algorithm.display");
}

NodeEngine* CreateInstance() {
    return new HVCloudFit();
}

std::string GetInstanceName() {
    return "Point cloud fit";
}

extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion() {
    return NODE_ENGINE_ABI_VERSION;
}

