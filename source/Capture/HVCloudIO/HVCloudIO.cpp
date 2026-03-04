#include "HVCloudIO.h"
#include "HVUtils.h"
#include "HVI18n.h"

namespace {

const hvi18n::Dictionary kTexts = {
    { "algorithm.display", { "点云读取", "Point cloud read" } },
    { "input.cloud_path.name", { "点云路径", "point cloud path" } },
    { "input.cloud_path.desc", { "点云文件路径", "Point cloud file path" } },
    { "output.cloud.name", { "输出点云", "output cloud" } },
    { "msg.read_success", { "点云读取成功", "Read point cloud success" } },
    { "msg.read_failed", { "点云读取失败", "Read point cloud failed" } },
    { "msg.param_set_failed", { "参数设置失败", "Param set failed" } }
};

std::string Tr(int language, const std::string& key) {
    return hvi18n::Translate(kTexts, key, language);
}

}  // namespace

HVCloudIO::HVCloudIO()
{

}

HVCloudIO::~HVCloudIO()
{

}

int HVCloudIO::init()
{
	execute_status = NODE_STATUS_NOT_RUN;
	error_msg.clear();
	return SUCCESS;
}

int HVCloudIO::run()
{
    auto start = std::chrono::steady_clock::now();
    execute_status = NODE_STATUS_RUNNING;
    error_msg.clear();

    std::shared_ptr<open3d::geometry::PointCloud> cloud(new open3d::geometry::PointCloud);
    if (open3d::io::ReadPointCloud(cloud_path, *cloud))
    {
        execute_status = SUCCESS;
        error_msg = "msg.read_success";
    }
    else
    {
        execute_status = ALGORITHM_RUN_ERROR;
        error_msg = "msg.read_failed";
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
        error_msg = "msg.param_set_failed";
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
	return { Tr(language_, "input.cloud_path.name") };
}

std::vector<std::string> HVCloudIO::get_algorithm_output_params_name()
{
	return { Tr(language_, "output.cloud.name") };
}

std::vector<bool> HVCloudIO::get_algorithm_input_params_bindable()
{
    return std::vector<bool>(get_algorithm_input_params_type().size(), true);
}

std::vector<ParamMetadata> HVCloudIO::get_algorithm_input_params_metadata()
{
    std::vector<ParamMetadata> metadata_list;

    // 参数0: 点云文件路径 (文件路径约束)
    ParamMetadata meta0;
    meta0.param_name = Tr(language_, "input.cloud_path.name");
    meta0.param_description = Tr(language_, "input.cloud_path.desc");
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
    if (error_msg.empty()) {
        return "";
    }
    return Tr(language_, error_msg);
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

void HVCloudIO::set_language(int language)
{
    if (hvi18n::IsSupportedLanguage(language)) {
        language_ = language;
    }
}

int HVCloudIO::get_language() const
{
    return language_;
}

std::string HVCloudIO::get_algorithm_display_name()
{
    return Tr(language_, "algorithm.display");
}

NodeEngine* CreateInstance() {
    // 每一个 DLL 内部返回自己具体的实现类
    return new HVCloudIO();
}

std::string GetInstanceName() {
    return "Point cloud read"; // 告知主程序此 DLL 代表的类型
}

extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion() {
    return NODE_ENGINE_ABI_VERSION;
}
