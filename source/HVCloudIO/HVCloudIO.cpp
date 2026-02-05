#include "HVCloudIO.h"
#include "HVUtils.h"

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
    if (open3d::io::ReadPointCloud(cloud_path, *cloud))
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
    if (params.size() < 1 || params[0] == nullptr)
    {
        error_msg = "Param set failed";
        return INVALID_PARAMS_NUM;
    }
    cloud_path = cast_param<std::string>(params, 0);
	return SUCCESS;
}

std::vector<void*> HVCloudIO::get_algorithm_result()
{
    if (execute_status == SUCCESS)
	    return { &cloudout };
    return { nullptr };
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

std::vector<ParamMetadata> HVCloudIO::get_algorithm_input_params_metadata()
{
    std::vector<ParamMetadata> metadata_list;

    // 参数0: 点云文件路径 (文件路径约束)
    ParamMetadata meta0;
    meta0.param_name = "file path";
    meta0.param_description = "点云文件路径";
    meta0.param_type = HV_STRING;
    meta0.constraint_type = CONSTRAINT_FILE_PATH;
    meta0.file_filter = "*.pcd;*.ply;*.xyz;*.pts";
    metadata_list.push_back(meta0);

    return metadata_list;
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

bool HVCloudIO::save_params_to_json(const std::string& filePath)
{
    try {
        nlohmann::json params_json = nlohmann::json::array();

		add_param(params_json, "cloud_path", HV_STRING, this->cloud_path);

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

bool HVCloudIO::load_params_from_json(const std::string& filePath)
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
            if (param_name == "cloud_path") {
                // 设置到类成员变量
                this->cloud_path = param_json["value"];
            }
        }

        return true;
    }
    catch (...) {
        return false;
    }
}

std::vector<void*> HVCloudIO::get_current_params()
{
	return { &cloud_path };
}

AlgorithmType HVCloudIO::get_algorithm_type()
{
    return AlgorithmType::Capture;
}

NodeEngine* CreateInstance() {
    // 每一个 DLL 内部返回自己具体的实现类
    return new HVCloudIO();
}

std::string GetInstanceName() {
    return "Point cloud read"; // 告知主程序此 DLL 代表的类型
}