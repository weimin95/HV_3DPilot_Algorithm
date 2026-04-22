#include "HVSendData.h"
//#include "HVUtils.h"
//#include "json.hpp"
#include "HVI18n.h"

#include <chrono>
#include <fstream>
#include <sstream>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// ---- CommunicatePlugin dynamic loader ----
namespace cp_loader {

	static HMODULE hModule = nullptr;

	using fn_get_device_count = int(*)();
	using fn_get_device_list  = int(*)(char*, int);
	using fn_send_string      = int(*)(const char*, const char*);

	static fn_get_device_count  pGetDeviceCount  = nullptr;
	static fn_get_device_list   pGetDeviceList   = nullptr;
	static fn_send_string       pSendString      = nullptr;

	static bool EnsureLoaded() {
		if (hModule) return true;
		hModule = ::LoadLibraryA("CommunicatePlugin.dll");
		if (!hModule) return false;
		pGetDeviceCount = (fn_get_device_count)::GetProcAddress(hModule, "cp_get_device_count");
		pGetDeviceList  = (fn_get_device_list) ::GetProcAddress(hModule, "cp_get_device_list");
		pSendString     = (fn_send_string)     ::GetProcAddress(hModule, "cp_send_string");
		return (pGetDeviceCount && pGetDeviceList && pSendString);
	}

	static std::vector<std::string> GetDeviceList() {
		std::vector<std::string> devices;
		if (!EnsureLoaded() || !pGetDeviceCount || !pGetDeviceList) return devices;
		int count = pGetDeviceCount();
		if (count <= 0) return devices;
		std::vector<char> buf(count * 256, '\0');
		int written = pGetDeviceList(buf.data(), static_cast<int>(buf.size()));
		const char* p = buf.data();
		for (int i = 0; i < written; ++i) {
			std::string id(p);
			p += id.size() + 1;
			if (!id.empty()) devices.push_back(std::move(id));
		}
		return devices;
	}

	static int SendString(const std::string& deviceId, const std::string& str) {
		if (!EnsureLoaded() || !pSendString) return -1;
		return pSendString(deviceId.c_str(), str.c_str());
	}

} // namespace cp_loader

namespace {

	const hvi18n::Dictionary kTexts = {
		{ "algorithm.display", { "发送数据", "SendData" } },
		{ "input.send_target.name", { "发送目标", "send target" } },
		{ "input.select_variable.name", { "选择变量", "select variable" } },
		{ "input.select_comm_device.name", { "通信设备", "communication device" } },
		{ "input.data_source.name", { "数据来源", "data source" } },
		{ "msg.send_data_failed", { "发送数据失败", "Send data failed" } },
		{ "msg.send_data_success", { "发送数据成功", "Send data success" } }
	};

	std::string Tr(int language, const std::string& key) {
		return hvi18n::Translate(kTexts, key, language);
	}

}  // namespace

int HVSendData::init()
{
	execute_status = NODE_STATUS_NOT_RUN;
	error_msg.clear();
	return SUCCESS;
}

int HVSendData::run()
{
	auto start = std::chrono::high_resolution_clock::now();

	execute_status = NODE_STATUS_RUNNING;
	error_msg.clear();

	// 获取数据来源 (参数3) 的绑定值
	NodeHostDataView data_view;
	int data_ret = INSTANCE_NOT_EXIST;
	if (host_services_ != nullptr) {
		data_ret = host_services_->get_current_input_data(3, data_view);
	}
	// 注意：data_ret 表示宿主的返回码，若为 INSTANCE_NOT_EXIST 表示未绑定或不存在

	// 将数据来源转换为发送字符串
	std::string send_str;
	if (data_view.has_value && data_view.data != nullptr) {
		switch (data_view.type) {
		case HV_STRING:
			send_str = *static_cast<const std::string*>(data_view.data);
			break;
		case HV_INT:
			send_str = std::to_string(*static_cast<const int*>(data_view.data));
			break;
		case HV_FLOAT:
			send_str = std::to_string(*static_cast<const float*>(data_view.data));
			break;
		case HV_DOUBLE:
			send_str = std::to_string(*static_cast<const double*>(data_view.data));
			break;
		case HV_LONG:
			send_str = std::to_string(*static_cast<const long*>(data_view.data));
			break;
		case HV_BOOLEAN:
			send_str = (*static_cast<const bool*>(data_view.data)) ? "true" : "false";
			break;
		default:
			send_str = "";
			break;
		}
	}

	// 校验数据来源是否有效
	if (data_ret != SUCCESS) {
		// 宿主未返回有效数据，常见原因：参数未在工作流中绑定到上游节点输出，或宿主未实现该接口
		error_msg = "msg.send_data_failed";
		execute_status = ALGORITHM_RUN_ERROR;
		auto end = std::chrono::high_resolution_clock::now();
		run_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
		return ALGORITHM_RUN_ERROR;
	}
	if (!data_view.has_value || data_view.data == nullptr || send_str.empty()) {
		error_msg = "msg.send_data_failed";
		execute_status = ALGORITHM_RUN_ERROR;
		auto end = std::chrono::high_resolution_clock::now();
		run_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
		return ALGORITHM_RUN_ERROR;
	}

	if (send_target == 0) {
		// 发送到全局变量
		if (host_services_ != nullptr) {
			NodeHostValue val;
			// 宿主侧会根据 type/has_value 做类型转换，字符串写回时必须补齐这两个字段。
			val.type = HV_STRING;
			val.has_value = true;
			val.string_value = send_str;
			int ret = host_services_->set_global_value(select_variable, val);
			if (ret != SUCCESS) {
				error_msg = "msg.send_data_failed";
				execute_status = ALGORITHM_RUN_ERROR;
				auto end = std::chrono::high_resolution_clock::now();
				run_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
				return ALGORITHM_RUN_ERROR;
			}
		}
	}
	else if (send_target == 1) {
		// 发送到通信设备
		auto devices = cp_loader::GetDeviceList();
		if (select_comm_device < 0 || select_comm_device >= static_cast<int>(devices.size())) {
			error_msg = "msg.send_data_failed";
			execute_status = ALGORITHM_RUN_ERROR;
			auto end = std::chrono::high_resolution_clock::now();
			run_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
			return ALGORITHM_RUN_ERROR;
		}
		int ret = cp_loader::SendString(devices[select_comm_device], send_str);
		if (ret != 0) {
			error_msg = "msg.send_data_failed";
			execute_status = ALGORITHM_RUN_ERROR;
			auto end = std::chrono::high_resolution_clock::now();
			run_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
			return ALGORITHM_RUN_ERROR;
		}
	}

	execute_status = SUCCESS;

	auto end = std::chrono::high_resolution_clock::now();
	run_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	error_msg = "msg.send_data_success";

	return SUCCESS;
}

int HVSendData::set_algorithm_params(const std::vector<void*>& params, const std::vector<int>& paramID)
{
	if (params.size() != paramID.size()) {
		return INVALID_PARAMS_NUM;
	}

	for (size_t i = 0; i < paramID.size(); ++i) {
		switch (paramID[i]) {
		case 0:
			send_target = (params[i] == nullptr) ? 0 : *static_cast<int*>(params[i]);
			break;
		case 1:
			select_variable = (params[i] == nullptr) ? 0 : *static_cast<int*>(params[i]);
			break;
		case 2:
			select_comm_device = (params[i] == nullptr) ? 0 : *static_cast<int*>(params[i]);
			break;
		case 3:
			// data source 是运行时绑定输入，不落到插件静态参数里。
			break;
		default:
			return INVALID_PARAMS_NUM;
		}
	}

	return SUCCESS;
}

std::vector<void*> HVSendData::get_current_params()
{
	return { &send_target, &select_variable, &select_comm_device, nullptr };
}

std::vector<void*> HVSendData::get_algorithm_result()
{
	if (execute_status == SUCCESS)
		return { /*&resultImg, */&execute_status };
	return {/* nullptr,*/ &execute_status };
}

std::vector<int> HVSendData::get_algorithm_input_params_type()
{
	return { HV_INT, HV_INT, HV_INT, HV_ANYINPUT };
}

std::vector<int> HVSendData::get_algorithm_output_params_type()
{
	return { /*HV_IMAGEDATAINFO2D,*/ HV_INT };
}

std::vector<std::string> HVSendData::get_algorithm_input_params_name()
{
	//{ "input.send_target.name", { "发送目标", "send target" } },
	//{ "input.select_variable.name", { "选择变量", "select variable" } },
	//{ "input.data_source.name", { "数据来源", "data source" } },
	return { Tr(language_, "input.send_target.name"), Tr(language_, "input.select_variable.name"), Tr(language_, "input.select_comm_device.name"), Tr(language_, "input.data_source.name") };
}

std::vector<std::string> HVSendData::get_algorithm_output_params_name()
{
	return {
	//Tr(language_, "output.image.name"),
	language_ == static_cast<int>(UIPilotLanguage::EN_US) ? "Execute status" : "运行状态"
	};
}

std::vector<bool> HVSendData::get_algorithm_input_params_bindable()
{
	// Used as the default UI input mode hint, not as a hard binding restriction.
	// 只有数据来源 (data_source) 支持绑定，其余参数仅支持本地编辑。
	return { false, false, false, true };
}

std::vector<ParamMetadata> HVSendData::get_algorithm_input_params_metadata()
{
	std::vector<ParamMetadata> metadata_list;

	// 参数0: 图像文件路径 (文件路径约束)
	//ParamMetadata meta0;
	//meta0.param_name = Tr(language_, "input.image_path.name");
	//meta0.param_description = Tr(language_, "input.image_path.desc");
	//meta0.param_type = HV_STRING;
	//meta0.constraint_type = CONSTRAINT_FILE_PATH;
	//meta0.file_filter = "*.jpg;*.jpeg;*.png;*.bmp;*.tiff;*.tif";
	//metadata_list.push_back(meta0);

	// 参数0: 发送目标 (整数，枚举约束)	
	ParamMetadata meta0;
	meta0.param_name = Tr(language_, "input.send_target.name");
	meta0.param_description = ""; // 可选：添加参数描述
	meta0.param_type = HV_INT;
	meta0.constraint_type = CONSTRAINT_OPTIONS;
	meta0.options_constraint.AddOption(0, language_ == static_cast<int>(UIPilotLanguage::ZH_CN) ? "全局变量" : "Global Variable");
	meta0.options_constraint.AddOption(1, language_ == static_cast<int>(UIPilotLanguage::ZH_CN) ? "通信设备" : "Communication Device");
	metadata_list.push_back(meta0);


	// 参数1: 选择变量 (枚举，显示全局变量名，仅当发送目标为全局变量时显示)
	ParamMetadata meta1;
	meta1.param_name = Tr(language_, "input.select_variable.name");
	meta1.param_description = "";
	meta1.param_type = HV_INT;
	meta1.constraint_type = CONSTRAINT_OPTIONS;
	if (host_services_ != nullptr) {
		
	}
	meta1.dependencies.push_back(ParamDependency(0, DEPENDS_ON_EQUALS, { "0" }));
	metadata_list.push_back(meta1);

	// 参数2: 选择通信设备 (枚举，显示设备名，调用CommunicatePlugin获取设备列表，仅当发送目标为通信设备时显示)
	ParamMetadata meta2;
	meta2.param_name = Tr(language_, "input.select_comm_device.name");
	meta2.param_description = "";
	meta2.param_type = HV_INT;
	meta2.constraint_type = CONSTRAINT_OPTIONS;
	
	// 增加通信设备选项
	{
		auto devices = cp_loader::GetDeviceList();
		for (int i = 0; i < static_cast<int>(devices.size()); ++i) {
			meta2.options_constraint.AddOption(i, devices[i]);
		}
	}

	meta2.dependencies.push_back(ParamDependency(0, DEPENDS_ON_EQUALS, { "1" }));
	metadata_list.push_back(meta2);

	// 参数3: 数据来源 (任意类型，运行时通过宿主服务获取实际值)
	ParamMetadata meta3;
	meta3.param_name = Tr(language_, "input.data_source.name");
	meta3.param_description = "";
	meta3.param_type = HV_ANYINPUT;
	meta3.constraint_type = CONSTRAINT_NONE;
	metadata_list.push_back(meta3);

	return metadata_list;
}

int HVSendData::get_algorithm_execute_status()
{
	return execute_status;
}

std::string HVSendData::get_algorithm_error_message()
{
	if (error_msg.empty()) {
		return "";
	}
	return Tr(language_, error_msg);
}

long HVSendData::get_algorithm_use_time()
{
	return run_time;
}

bool HVSendData::algorithm_params_setting_status()
{
	return true;
}

bool HVSendData::algorithm_init_status()
{
	return true;
}

bool HVSendData::save_params_to_json(const std::string& filePath)
{
	try {
		nlohmann::json params_json = nlohmann::json::array();

		add_param(params_json, "send_target", HV_INT, this->send_target);
		add_param(params_json, "select_variable", HV_INT, this->select_variable);
		add_param(params_json, "select_comm_device", HV_INT, this->select_comm_device);

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



bool HVSendData::load_params_from_json(const std::string& filePath)
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
			//if (param_name == "image_path") {
			//	// 设置到类成员变量
			//	this->image_path = param_json["value"];
			//}

			if (param_name == "send_target")
			{
				this->send_target = param_json["value"];
			}

			if (param_name == "select_variable")
			{
				this->select_variable = param_json["value"];
			}

			if (param_name == "select_comm_device")
			{
				this->select_comm_device = param_json["value"];
			}
		}

		return true;
	}
	catch (...) {
		return false;
	}
}

void HVSendData::set_host_services(NodeHostServices* host_services)
{
	host_services_ = host_services;
}

AlgorithmType HVSendData::get_algorithm_type()
{
	return AlgorithmType::Capture;
}

void HVSendData::set_language(int language)
{
	if (hvi18n::IsSupportedLanguage(language)) {
		language_ = language;
	}
}

int HVSendData::get_language() const
{
	return language_;
}

std::string HVSendData::get_algorithm_display_name()
{
	return Tr(language_, "algorithm.display");
}

NodeEngine* CreateInstance() {
	// 每一个 DLL 内部返回自己具体的实现类
	return new HVSendData();
}

std::string GetInstanceName() {
	return "SendData"; // 告知主程序此 DLL 代表的类型
}

extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion() {
	return NODE_ENGINE_ABI_VERSION;
}
