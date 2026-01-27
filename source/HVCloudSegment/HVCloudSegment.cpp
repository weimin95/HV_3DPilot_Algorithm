#include "HVCloudSegment.h"
#include "HVUtils.h"

#include <chrono>


HVCloudSegment::HVCloudSegment()
{

}

HVCloudSegment::~HVCloudSegment()
{

}

int HVCloudSegment::init()
{
	return SUCCESS;
}

int HVCloudSegment::run()
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

int HVCloudSegment::set_algorithm_params(const std::vector<void*>& params, const std::vector<int>& paramID)
{
    if (paramID.size() == 0)
    {
        inputCloud = cast_param_sharedPtr<HVPointCloud>(params, 0);
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

std::vector<void*> HVCloudSegment::get_current_params()
{
	return { &inputCloud, &type, &k, &nSigma, &radius, &pointsThrehold, &voxelSize };
}

std::vector<void*> HVCloudSegment::get_algorithm_result()
{
	return { &resultCloud };
}

std::vector<int> HVCloudSegment::get_algorithm_input_params_type()
{
    return { HV_POINTCLOUD, HV_INT, HV_INT, HV_FLOAT, HV_FLOAT, HV_INT, HV_FLOAT };
}

std::vector<int> HVCloudSegment::get_algorithm_output_params_type()
{
    return { HV_POINTCLOUD };
}

std::vector<std::string> HVCloudSegment::get_algorithm_input_params_name()
{
    return { "input cloud", "preprocess type", "k", "nSigma", "radius", "points threshold", "voxel size" };
}

std::vector<std::string> HVCloudSegment::get_algorithm_output_params_name()
{
    return { "output point cloud" };
}

std::vector<bool> HVCloudSegment::get_algorithm_input_params_bindable()
{
    return { true, false, false, false, false, false, false };
}

int HVCloudSegment::get_algorithm_execute_status()
{
	return execute_status;
}

std::string HVCloudSegment::get_algorithm_error_message()
{
    return error_msg;
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

AlgorithmType HVCloudSegment::get_algorithm_type()
{
    return AlgorithmType::PointCloudProcess;
}

NodeEngine* CreateInstance() {
    // 每一个 DLL 内部返回自己具体的实现类
    return new HVCloudSegment();
}

std::string GetInstanceName() {
    return "Point cloud segment"; // 告知主程序此 DLL 代表的类型
}