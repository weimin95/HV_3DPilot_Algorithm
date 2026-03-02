#include "HVCloudPreprocess.h"
#include "HVUtils.h"

HVCloudPreprocess::HVCloudPreprocess()
{

}

HVCloudPreprocess::~HVCloudPreprocess()
{

}

int HVCloudPreprocess::init()
{
	return SUCCESS;
}

int HVCloudPreprocess::run()
{
    auto start = std::chrono::steady_clock::now();

    std::shared_ptr<open3d::geometry::PointCloud> cloudIn = PointCloudConverter::ToOpen3D(*inputCloud);
    std::shared_ptr<open3d::geometry::PointCloud> cloudOut(new open3d::geometry::PointCloud);
    switch (type)
    {
    case 0:
    {
        // sor filter
        auto res = cloudIn->RemoveStatisticalOutliers(k, nSigma);
        cloudOut = std::get<0>(res);
        error_msg = "Statistic filter success";
    }
    break;
    case 1:
    {
        // radius filter 
        auto res = cloudIn->RemoveRadiusOutliers(pointsThrehold, radius);
        cloudOut = std::get<0>(res);
        error_msg = "Radius filter success";
    }
    break;
    case 2:
    {
        // voxel filter
        cloudOut = cloudIn->VoxelDownSample(voxelSize);
        error_msg = "Voxel downsample success";
    }
    break;
    default:
    {
        error_msg = "Unsupport filter";
        break;
    } 
    }

    resultCloud = std::make_shared<HVPointCloud>(PointCloudConverter::FromOpen3D(*cloudOut));

    auto end = std::chrono::steady_clock::now();
    run_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    execute_status = 0;
    return 0;
}

int HVCloudPreprocess::set_algorithm_params(const std::vector<void*>& params, const std::vector<int>& paramID)
{
    if (paramID.size() == 0)
    {
        inputCloud = cast_param_sharedPtr<HVPointCloud>(params, paramID[0]);
        type = cast_param<int>(params, 1);
        k = cast_param<int>(params, 2);
        nSigma = cast_param<float>(params, 3);
        radius = cast_param<float>(params, 4);
        pointsThrehold = cast_param<int>(params, 5);
        voxelSize = cast_param<float>(params, 6);
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
                k = cast_param<int>(params, i);
                break;
            case 3:
                nSigma = cast_param<float>(params, i);
                break;
            case 4:
                radius = cast_param<float>(params, i);
                break;
            case 5:
                pointsThrehold = cast_param<int>(params, i);
                break;
            case 6:
                voxelSize = cast_param<float>(params, i);
                break;
            default:
                break;
            }
		}
    }

	return SUCCESS;
}

std::vector<void*> HVCloudPreprocess::get_current_params()
{
    return {
        &inputCloud,
        &type,
        &k,
        &nSigma,
        &radius,
        &pointsThrehold,
        &voxelSize
	};
}

std::vector<void*> HVCloudPreprocess::get_algorithm_result()
{
	if (execute_status == SUCCESS)
	    return { &resultCloud };
    return { nullptr };
}

std::vector<int> HVCloudPreprocess::get_algorithm_input_params_type()
{
    return { HV_POINTCLOUD, HV_INT, HV_INT, HV_FLOAT, HV_FLOAT, HV_INT, HV_FLOAT };
}

std::vector<int> HVCloudPreprocess::get_algorithm_output_params_type()
{
    return { HV_POINTCLOUD };
}

std::vector<std::string> HVCloudPreprocess::get_algorithm_input_params_name()
{
    return { "input cloud", "preprocess type", "k", "nSigma", "radius", "points threshold", "voxel size" };
}

std::vector<std::string> HVCloudPreprocess::get_algorithm_output_params_name()
{
    return { "output point cloud" };
}

std::vector<bool> HVCloudPreprocess::get_algorithm_input_params_bindable()
{
    return { true, false, false, false, false, false, false };
}

std::vector<ParamMetadata> HVCloudPreprocess::get_algorithm_input_params_metadata()
{
    std::vector<ParamMetadata> metadata_list;

    // 参数0: 输入点云 (可绑定，无约束)
    ParamMetadata meta0;
    meta0.param_name = "input cloud";
    meta0.param_description = "输入点云数据";
    meta0.param_type = HV_POINTCLOUD;
    meta0.constraint_type = CONSTRAINT_NONE;
    metadata_list.push_back(meta0);

    // 参数1: 预处理类型 (选项约束)
    ParamMetadata meta1;
    meta1.param_name = "preprocess type";
    meta1.param_description = "预处理算法类型";
    meta1.param_type = HV_INT;
    meta1.constraint_type = CONSTRAINT_OPTIONS;
    meta1.options_constraint.AddOption(0, "SOR (统计滤波)");
    meta1.options_constraint.AddOption(1, "Radius (半径滤波)");
    meta1.options_constraint.AddOption(2, "Voxel (体素下采样)");
    meta1.options_constraint.default_index = 0;
    metadata_list.push_back(meta1);

    // 参数2: k值 (范围约束，SOR滤波参数，依赖type=0)
    ParamMetadata meta2;
    meta2.param_name = "k";
    meta2.param_description = "SOR滤波邻域点个数";
    meta2.param_type = HV_INT;
    meta2.constraint_type = CONSTRAINT_RANGE;
    meta2.range_constraint = RangeConstraint(1, 100, 30);
    meta2.dependencies.push_back(ParamDependency(1, DEPENDS_ON_EQUALS, {"0"}));
    metadata_list.push_back(meta2);

    // 参数3: nSigma (范围约束，SOR滤波参数，依赖type=0)
    ParamMetadata meta3;
    meta3.param_name = "nSigma";
    meta3.param_description = "SOR滤波标准差倍数";
    meta3.param_type = HV_FLOAT;
    meta3.constraint_type = CONSTRAINT_RANGE;
    meta3.range_constraint = RangeConstraint(0.1, 10.0, 1.5);
    meta3.dependencies.push_back(ParamDependency(1, DEPENDS_ON_EQUALS, {"0"}));
    metadata_list.push_back(meta3);

    // 参数4: radius (范围约束，半径滤波参数，依赖type=1)
    ParamMetadata meta4;
    meta4.param_name = "radius";
    meta4.param_description = "半径滤波搜索半径";
    meta4.param_type = HV_FLOAT;
    meta4.constraint_type = CONSTRAINT_RANGE;
    meta4.range_constraint = RangeConstraint(0.01, 100.0, 1.0);
    meta4.dependencies.push_back(ParamDependency(1, DEPENDS_ON_EQUALS, {"1"}));
    metadata_list.push_back(meta4);

    // 参数5: pointsThreshold (范围约束，半径滤波参数，依赖type=1)
    ParamMetadata meta5;
    meta5.param_name = "points threshold";
    meta5.param_description = "半径滤波邻域点阈值";
    meta5.param_type = HV_INT;
    meta5.constraint_type = CONSTRAINT_RANGE;
    meta5.range_constraint = RangeConstraint(1, 1000, 100);
    meta5.dependencies.push_back(ParamDependency(1, DEPENDS_ON_EQUALS, {"1"}));
    metadata_list.push_back(meta5);

    // 参数6: voxelSize (范围约束，体素下采样参数，依赖type=2)
    ParamMetadata meta6;
    meta6.param_name = "voxel size";
    meta6.param_description = "体素大小";
    meta6.param_type = HV_FLOAT;
    meta6.constraint_type = CONSTRAINT_RANGE;
    meta6.range_constraint = RangeConstraint(0.001, 100.0, 1.0);
    meta6.dependencies.push_back(ParamDependency(1, DEPENDS_ON_EQUALS, {"2"}));
    metadata_list.push_back(meta6);

    for (size_t i = 0; i < metadata_list.size(); ++i) {
        metadata_list[i].param_group = (i < 2) ? PARAM_GROUP_BASIC : PARAM_GROUP_ADVANCED;
    }

    return metadata_list;
}

int HVCloudPreprocess::get_algorithm_execute_status()
{
	return execute_status;
}

std::string HVCloudPreprocess::get_algorithm_error_message()
{
    return error_msg;
}

long HVCloudPreprocess::get_algorithm_use_time()
{
	return run_time;
}

bool HVCloudPreprocess::algorithm_params_setting_status()
{
	return true;
}

bool HVCloudPreprocess::algorithm_init_status()
{
	return true;
}

bool HVCloudPreprocess::save_params_to_json(const std::string& filePath)
{
    try {
        nlohmann::json params_json = nlohmann::json::array();

		add_param(params_json, "type", HV_INT, this->type);
		add_param(params_json, "k", HV_INT, this->k);
		add_param(params_json, "nSigma", HV_FLOAT, this->nSigma);
		add_param(params_json, "radius", HV_FLOAT, this->radius);
		add_param(params_json, "pointsThrehold", HV_INT, this->pointsThrehold);
		add_param(params_json, "voxelSize", HV_FLOAT, this->voxelSize);

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

bool HVCloudPreprocess::load_params_from_json(const std::string& filePath)
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
            // 检查必要字段是否存在
            if (!param_json.contains("name") || !param_json.contains("type")) {
                continue;
            }

            std::string param_name = param_json["name"];
            int param_type = param_json["type"];

            // 根据参数名称进行处理
            if (param_name == "type") {
                // 设置到类成员变量
                this->type = param_json["value"];
            }

            if (param_name == "k") {
                // 设置到类成员变量
                this->k = param_json["value"];
            }

            if (param_name == "nSigma") {
                // 设置到类成员变量
                this->nSigma = param_json["value"];
            }

            if (param_name == "radius") {
                // 设置到类成员变量
                this->radius = param_json["value"];
            }

            if (param_name == "pointsThrehold") {
                // 设置到类成员变量
                this->pointsThrehold = param_json["value"];
            }

            if (param_name == "voxelSize") {
                // 设置到类成员变量
                this->voxelSize = param_json["value"];
            }
        }

        return true;
    }
    catch (...) {
        return false;
    }
}

AlgorithmType HVCloudPreprocess::get_algorithm_type()
{
    return AlgorithmType::PointCloudProcess;
}

NodeEngine* CreateInstance() {
    // 每一个 DLL 内部返回自己具体的实现类
    return new HVCloudPreprocess();
}

std::string GetInstanceName() {
    return "Point cloud preprocess"; // 告知主程序此 DLL 代表的类型
}
