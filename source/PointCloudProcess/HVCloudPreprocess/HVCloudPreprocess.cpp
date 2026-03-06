#include "HVCloudPreprocess.h"
#include "HVUtils.h"
#include "HVI18n.h"

namespace {

const hvi18n::Dictionary kTexts = {
    { "algorithm.display", { "点云预处理", "Point cloud preprocess" } },
    { "input.cloud.name", { "输入点云", "input cloud" } },
    { "input.type.name", { "预处理类型", "preprocess type" } },
    { "input.k.name", { "k", "k" } },
    { "input.nsigma.name", { "nSigma", "nSigma" } },
    { "input.radius.name", { "radius", "radius" } },
    { "input.points_threshold.name", { "点数阈值", "points threshold" } },
    { "input.voxel_size.name", { "体素大小", "voxel size" } },
    { "output.cloud.name", { "输出点云", "output point cloud" } },
    { "input.cloud.desc", { "输入点云数据", "Input point cloud" } },
    { "input.type.desc", { "预处理算法类型", "Preprocess algorithm type" } },
    { "input.k.desc", { "SOR滤波邻域点个数", "SOR neighbor count" } },
    { "input.nsigma.desc", { "SOR滤波标准差倍数", "SOR sigma multiplier" } },
    { "input.radius.desc", { "半径滤波搜索半径", "Radius search radius" } },
    { "input.points_threshold.desc", { "半径滤波邻域点阈值", "Radius neighbor threshold" } },
    { "input.voxel_size.desc", { "体素大小", "Voxel size" } },
    { "option.sor", { "统计滤波", "SOR" } },
    { "option.radius", { "半径滤波", "Radius" } },
    { "option.voxel", { "体素下采样", "Voxel" } },
    { "msg.input_null", { "输入点云为空", "Input cloud is null" } },
    { "msg.statistic_success", { "统计滤波成功", "Statistic filter success" } },
    { "msg.radius_success", { "半径滤波成功", "Radius filter success" } },
    { "msg.voxel_success", { "体素下采样成功", "Voxel downsample success" } },
    { "msg.unsupported", { "不支持的滤波类型", "Unsupport filter" } }
};

std::string Tr(int language, const std::string& key) {
    return hvi18n::Translate(kTexts, key, language);
}

}  // namespace

HVCloudPreprocess::HVCloudPreprocess()
{

}

HVCloudPreprocess::~HVCloudPreprocess()
{

}

int HVCloudPreprocess::init()
{
	execute_status = NODE_STATUS_NOT_RUN;
	error_msg.clear();
	return SUCCESS;
}

int HVCloudPreprocess::run()
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

    std::shared_ptr<open3d::geometry::PointCloud> cloudIn = PointCloudConverter::ToOpen3D(*inputCloud);
    std::shared_ptr<open3d::geometry::PointCloud> cloudOut(new open3d::geometry::PointCloud);
    switch (type)
    {
    case 0:
    {
        // sor filter
        auto res = cloudIn->RemoveStatisticalOutliers(k, nSigma);
        cloudOut = std::get<0>(res);
        error_msg = "msg.statistic_success";
    }
    break;
    case 1:
    {
        // radius filter 
        auto res = cloudIn->RemoveRadiusOutliers(pointsThrehold, radius);
        cloudOut = std::get<0>(res);
        error_msg = "msg.radius_success";
    }
    break;
    case 2:
    {
        // voxel filter
        cloudOut = cloudIn->VoxelDownSample(voxelSize);
        error_msg = "msg.voxel_success";
    }
    break;
    default:
    {
        error_msg = "msg.unsupported";
        execute_status = ALGORITHM_RUN_ERROR;
        return ALGORITHM_RUN_ERROR;
    } 
    }

    resultCloud = std::make_shared<HVPointCloud>(PointCloudConverter::FromOpen3D(*cloudOut));

    auto end = std::chrono::steady_clock::now();
    run_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    execute_status = SUCCESS;
    return SUCCESS;
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
    return {
        Tr(language_, "input.cloud.name"),
        Tr(language_, "input.type.name"),
        Tr(language_, "input.k.name"),
        Tr(language_, "input.nsigma.name"),
        Tr(language_, "input.radius.name"),
        Tr(language_, "input.points_threshold.name"),
        Tr(language_, "input.voxel_size.name")
    };
}

std::vector<std::string> HVCloudPreprocess::get_algorithm_output_params_name()
{
    return { Tr(language_, "output.cloud.name") };
}

std::vector<bool> HVCloudPreprocess::get_algorithm_input_params_bindable()
{
    // Used as the default UI input mode hint, not as a hard binding restriction.
    return { true, false, false, false, false, false, false };
}

std::vector<ParamMetadata> HVCloudPreprocess::get_algorithm_input_params_metadata()
{
    std::vector<ParamMetadata> metadata_list;

    // 参数0: 输入点云 (可绑定，无约束)
    ParamMetadata meta0;
    meta0.param_name = Tr(language_, "input.cloud.name");
    meta0.param_description = Tr(language_, "input.cloud.desc");
    meta0.param_type = HV_POINTCLOUD;
    meta0.constraint_type = CONSTRAINT_NONE;
    metadata_list.push_back(meta0);

    // 参数1: 预处理类型 (选项约束)
    ParamMetadata meta1;
    meta1.param_name = Tr(language_, "input.type.name");
    meta1.param_description = Tr(language_, "input.type.desc");
    meta1.param_type = HV_INT;
    meta1.constraint_type = CONSTRAINT_OPTIONS;
    meta1.options_constraint.AddOption(0, Tr(language_, "option.sor"));
    meta1.options_constraint.AddOption(1, Tr(language_, "option.radius"));
    meta1.options_constraint.AddOption(2, Tr(language_, "option.voxel"));
    meta1.options_constraint.default_index = 0;
    metadata_list.push_back(meta1);

    // 参数2: k值 (范围约束，SOR滤波参数，依赖type=0)
    ParamMetadata meta2;
    meta2.param_name = Tr(language_, "input.k.name");
    meta2.param_description = Tr(language_, "input.k.desc");
    meta2.param_type = HV_INT;
    meta2.constraint_type = CONSTRAINT_RANGE;
    meta2.range_constraint = RangeConstraint(1, 100, 30);
    meta2.dependencies.push_back(ParamDependency(1, DEPENDS_ON_EQUALS, {"0"}));
    metadata_list.push_back(meta2);

    // 参数3: nSigma (范围约束，SOR滤波参数，依赖type=0)
    ParamMetadata meta3;
    meta3.param_name = Tr(language_, "input.nsigma.name");
    meta3.param_description = Tr(language_, "input.nsigma.desc");
    meta3.param_type = HV_FLOAT;
    meta3.constraint_type = CONSTRAINT_RANGE;
    meta3.range_constraint = RangeConstraint(0.1, 10.0, 1.5);
    meta3.dependencies.push_back(ParamDependency(1, DEPENDS_ON_EQUALS, {"0"}));
    metadata_list.push_back(meta3);

    // 参数4: radius (范围约束，半径滤波参数，依赖type=1)
    ParamMetadata meta4;
    meta4.param_name = Tr(language_, "input.radius.name");
    meta4.param_description = Tr(language_, "input.radius.desc");
    meta4.param_type = HV_FLOAT;
    meta4.constraint_type = CONSTRAINT_RANGE;
    meta4.range_constraint = RangeConstraint(0.01, 100.0, 1.0);
    meta4.dependencies.push_back(ParamDependency(1, DEPENDS_ON_EQUALS, {"1"}));
    metadata_list.push_back(meta4);

    // 参数5: pointsThreshold (范围约束，半径滤波参数，依赖type=1)
    ParamMetadata meta5;
    meta5.param_name = Tr(language_, "input.points_threshold.name");
    meta5.param_description = Tr(language_, "input.points_threshold.desc");
    meta5.param_type = HV_INT;
    meta5.constraint_type = CONSTRAINT_RANGE;
    meta5.range_constraint = RangeConstraint(1, 1000, 100);
    meta5.dependencies.push_back(ParamDependency(1, DEPENDS_ON_EQUALS, {"1"}));
    metadata_list.push_back(meta5);

    // 参数6: voxelSize (范围约束，体素下采样参数，依赖type=2)
    ParamMetadata meta6;
    meta6.param_name = Tr(language_, "input.voxel_size.name");
    meta6.param_description = Tr(language_, "input.voxel_size.desc");
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
    if (error_msg.empty()) {
        return "";
    }
    return Tr(language_, error_msg);
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

void HVCloudPreprocess::set_language(int language)
{
    if (hvi18n::IsSupportedLanguage(language)) {
        language_ = language;
    }
}

int HVCloudPreprocess::get_language() const
{
    return language_;
}

std::string HVCloudPreprocess::get_algorithm_display_name()
{
    return Tr(language_, "algorithm.display");
}

NodeEngine* CreateInstance() {
    // 每一个 DLL 内部返回自己具体的实现类
    return new HVCloudPreprocess();
}

std::string GetInstanceName() {
    return "Point cloud preprocess"; // 告知主程序此 DLL 代表的类型
}

extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion() {
    return NODE_ENGINE_ABI_VERSION;
}
