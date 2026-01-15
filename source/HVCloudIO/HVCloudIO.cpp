#include "HVCloudIO.h"
#include "HVUtils.h"

#include <chrono>


HVCloudIO::HVCloudIO()
{

}

HVCloudIO::~HVCloudIO()
{

}

int HVCloudIO::init()
{
	return SUCCESS;
}

int HVCloudIO::run()
{
    auto start = std::chrono::steady_clock::now();

    std::shared_ptr<open3d::geometry::PointCloud> cloud(new open3d::geometry::PointCloud);
    if (open3d::io::ReadPointCloud(path, *cloud))
    {
        execute_status = SUCCESS;
        error_msg = "Read point cloud success";
    }
    else
    {
        execute_status = ALGORITHM_RUN_ERROR;
        error_msg = "Read point cloud failed";
    }
    cloudout = std::make_shared<HVPointCloud>(PointCloudConverter::FromOpen3D(*cloud));
    auto end = std::chrono::steady_clock::now();
    run_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    return execute_status;
}

int HVCloudIO::set_algorithm_params(const std::vector<void*>& params, const std::vector<int>& paramID)
{
    if (params.size() < 1)
    {
        error_msg = "Param set failed";
        return INVALID_PARAMS_NUM;
    }
    path = *static_cast<std::string*>(params[0]);
	return SUCCESS;
}

std::vector<void*> HVCloudIO::get_algorithm_result()
{
	return { &cloudout };
}

std::vector<int> HVCloudIO::get_algorithm_input_params_type()
{
    return { HV_STRING };
}

std::vector<int> HVCloudIO::get_algorithm_output_params_type()
{
    return { HV_POINTCLOUD };
}

std::vector<std::string> HVCloudIO::get_algorithm_input_params_name()
{
	return { "file path" };
}

std::vector<std::string> HVCloudIO::get_algorithm_output_params_name()
{
	return { "output point cloud" };
}

std::vector<bool> HVCloudIO::get_algorithm_input_params_bindable()
{
	return { false };
}

int HVCloudIO::get_algorithm_execute_status()
{
	return execute_status;
}

std::string HVCloudIO::get_algorithm_error_message()
{
    return error_msg;
}

long HVCloudIO::get_algorithm_use_time()
{
	return run_time;
}

bool HVCloudIO::algorithm_params_setting_status()
{
	return true;
}

bool HVCloudIO::algorithm_init_status()
{
	return true;
}

NodeEngine* CreateInstance() {
    // 每一个 DLL 内部返回自己具体的实现类
    return new HVCloudIO();
}

std::string GetInstanceName() {
    return "Point cloud read"; // 告知主程序此 DLL 代表的类型
}