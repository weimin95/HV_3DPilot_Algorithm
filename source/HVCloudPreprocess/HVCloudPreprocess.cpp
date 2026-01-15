#include "HVCloudPreprocess.h"
#include "HVUtils.h"

#include <chrono>


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
        inputCloud = *static_cast<std::shared_ptr<HVPointCloud>*>(params[0]);
        type = *static_cast<int*>(params[1]);
        k = *static_cast<int*>(params[2]);
        nSigma = *static_cast<float*>(params[2]);
        radius = *static_cast<float*>(params[3]);
        pointsThrehold = *static_cast<int*>(params[4]);
        voxelSize = *static_cast<float*>(params[5]);
    }
    else
    {
        for (size_t i = 0; i < paramID.size(); i++)
        {
            int id = paramID[i];
            switch (id)
            {
            case 0:
                inputCloud = *static_cast<std::shared_ptr<HVPointCloud>*>(params[i]);
                break;
            case 1:
                type = *static_cast<int*>(params[i]);
                break;
            case 2:
                k = *static_cast<int*>(params[i]);
            case 3:
                nSigma = *static_cast<float*>(params[i]);
                break;
            case 4:
                radius = *static_cast<float*>(params[i]);
                break;
            case 5:
                pointsThrehold = *static_cast<int*>(params[i]);
                break;
            case 6:
                voxelSize = *static_cast<float*>(params[i]);
                break;
            default:
                break;
            }
		}
    }

	return SUCCESS;
}

std::vector<void*> HVCloudPreprocess::get_algorithm_result()
{
	return { &resultCloud };
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

NodeEngine* CreateInstance() {
    // 每一个 DLL 内部返回自己具体的实现类
    return new HVCloudPreprocess();
}

std::string GetInstanceName() {
    return "Point cloud preprocess"; // 告知主程序此 DLL 代表的类型
}