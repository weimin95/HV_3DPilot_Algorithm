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
        { "algorithm.display", { "SendData", "SendData" } },
        { "input.send_target.name", { "send target", "send target" } },
        { "input.select_variable.name", { "select variable", "select variable" } },
        { "input.select_comm_device.name", { "communication device", "communication device" } },
        { "input.data_source.name", { "data source", "data source" } },
        { "msg.send_data_failed", { "Send data failed", "Send data failed" } },
        { "msg.send_data_success", { "Send data success", "Send data success" } }
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

	// 锟斤拷取锟斤拷锟斤拷锟斤拷源 (锟斤拷锟斤拷3) 锟侥帮拷值
	NodeHostDataView data_view;
	int data_ret = INSTANCE_NOT_EXIST;
	if (host_services_ != nullptr) {
		data_ret = host_services_->get_current_input_data(3, data_view);
	}
	// 注锟解：data_ret 锟斤拷示锟斤拷锟斤拷锟侥凤拷锟斤拷锟诫，锟斤拷为 INSTANCE_NOT_EXIST 锟斤拷示未锟襟定或不达拷锟斤拷

	// 锟斤拷锟斤拷锟斤拷锟斤拷源转锟斤拷为锟斤拷锟斤拷锟街凤拷锟斤拷
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

	// 校锟斤拷锟斤拷锟斤拷锟斤拷源锟角凤拷锟斤拷效
	if (data_ret != SUCCESS) {
		// 锟斤拷锟斤拷未锟斤拷锟斤拷锟斤拷效锟斤拷锟捷ｏ拷锟斤拷锟斤拷原锟津：诧拷锟斤拷未锟节癸拷锟斤拷锟斤拷锟叫绑定碉拷锟斤拷锟轿节碉拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟轿词碉拷指媒涌锟?
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
		// 锟斤拷锟酵碉拷全锟街憋拷锟斤拷
		if (host_services_ != nullptr) {
			NodeHostValue val;
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
		// 锟斤拷锟酵碉拷通锟斤拷锟借备
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
    auto apply_param = [this](int target_param_id, void* value_ptr) -> int {
        switch (target_param_id) {
        case 0:
            if (value_ptr == nullptr) {
                return INVALID_PARAMS_NUM;
            }
            send_target = *static_cast<int*>(value_ptr);
            return SUCCESS;
        case 1:
            if (value_ptr == nullptr) {
                return INVALID_PARAMS_NUM;
            }
            select_variable = *static_cast<int*>(value_ptr);
            return SUCCESS;
        case 2:
            if (value_ptr == nullptr) {
                return INVALID_PARAMS_NUM;
            }
            select_comm_device = *static_cast<int*>(value_ptr);
            return SUCCESS;
        case 3:
            // data source comes from host-side binding; clearing before bind should be accepted.
            return SUCCESS;
        default:
            return INVALID_PARAMS_NUM;
        }
    };

    if (paramID.empty()) {
        for (int i = 0; i < static_cast<int>(params.size()); ++i) {
            const int ret = apply_param(i, params[i]);
            if (ret != SUCCESS) {
                return ret;
            }
        }
        return SUCCESS;
    }

    if (params.size() != paramID.size()) {
        return INVALID_PARAMS_NUM;
    }

    for (size_t i = 0; i < paramID.size(); ++i) {
        const int ret = apply_param(paramID[i], params[i]);
        if (ret != SUCCESS) {
            return ret;
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
	return { HV_INT, HV_INT, HV_INT, HV_INT };
}

std::vector<int> HVSendData::get_algorithm_output_params_type()
{
	return { /*HV_IMAGEDATAINFO2D,*/ HV_INT };
}

std::vector<std::string> HVSendData::get_algorithm_input_params_name()
{
	//{ "input.send_target.name", { "锟斤拷锟斤拷目锟斤拷", "send target" } },
	//{ "input.select_variable.name", { "选锟斤拷锟斤拷锟?, "select variable" } },
	//{ "input.data_source.name", { "锟斤拷锟斤拷锟斤拷源", "data source" } },
	return { Tr(language_, "input.send_target.name"), Tr(language_, "input.select_variable.name"), Tr(language_, "input.select_comm_device.name"), Tr(language_, "input.data_source.name") };
}

std::vector<std::string> HVSendData::get_algorithm_output_params_name()
{
	return {
	//Tr(language_, "output.image.name"),
	language_ == static_cast<int>(UIPilotLanguage::EN_US) ? "Execute status" : "锟斤拷锟斤拷状态"
	};
}

std::vector<bool> HVSendData::get_algorithm_input_params_bindable()
{
	// Used as the default UI input mode hint, not as a hard binding restriction.
	// 只锟斤拷锟斤拷锟斤拷锟斤拷源 (data_source) 支锟街绑定ｏ拷锟斤拷锟斤拷锟斤拷锟斤拷锟街э拷直锟斤拷乇嗉拷锟?
	return { false, false, false, true };
}

std::vector<ParamMetadata> HVSendData::get_algorithm_input_params_metadata()
{
	std::vector<ParamMetadata> metadata_list;

	// 锟斤拷锟斤拷0: 图锟斤拷锟侥硷拷路锟斤拷 (锟侥硷拷路锟斤拷约锟斤拷)
	//ParamMetadata meta0;
	//meta0.param_name = Tr(language_, "input.image_path.name");
	//meta0.param_description = Tr(language_, "input.image_path.desc");
	//meta0.param_type = HV_STRING;
	//meta0.constraint_type = CONSTRAINT_FILE_PATH;
	//meta0.file_filter = "*.jpg;*.jpeg;*.png;*.bmp;*.tiff;*.tif";
	//metadata_list.push_back(meta0);

	// 锟斤拷锟斤拷0: 锟斤拷锟斤拷目锟斤拷 (锟斤拷锟斤拷锟斤拷枚锟斤拷约锟斤拷)	
	ParamMetadata meta0;
	meta0.param_name = Tr(language_, "input.send_target.name");
	meta0.param_description = ""; // 锟斤拷选锟斤拷锟斤拷硬锟斤拷锟斤拷锟斤拷锟?
	meta0.param_type = HV_INT;
	meta0.constraint_type = CONSTRAINT_OPTIONS;
	meta0.options_constraint.AddOption(0, language_ == static_cast<int>(UIPilotLanguage::ZH_CN) ? "全锟街憋拷锟斤拷" : "Global Variable");
	meta0.options_constraint.AddOption(1, language_ == static_cast<int>(UIPilotLanguage::ZH_CN) ? "通锟斤拷锟借备" : "Communication Device");
	metadata_list.push_back(meta0);


	// 锟斤拷锟斤拷1: 选锟斤拷锟斤拷锟?(枚锟劫ｏ拷锟斤拷示全锟街憋拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷目锟斤拷为全锟街憋拷锟斤拷时锟斤拷示)
	ParamMetadata meta1;
	meta1.param_name = Tr(language_, "input.select_variable.name");
	meta1.param_description = "";
	meta1.param_type = HV_INT;
	meta1.constraint_type = CONSTRAINT_OPTIONS;
	if (host_services_ != nullptr) {
		
	}
	meta1.dependencies.push_back(ParamDependency(0, DEPENDS_ON_EQUALS, { "0" }));
	metadata_list.push_back(meta1);

	// 锟斤拷锟斤拷2: 选锟斤拷通锟斤拷锟借备 (枚锟劫ｏ拷锟斤拷示锟借备锟斤拷锟斤拷锟斤拷锟斤拷CommunicatePlugin锟斤拷取锟借备锟叫憋拷锟斤拷锟斤拷锟斤拷锟斤拷锟侥匡拷锟轿拷锟斤拷璞甘憋拷锟绞?
	ParamMetadata meta2;
	meta2.param_name = Tr(language_, "input.select_comm_device.name");
	meta2.param_description = "";
	meta2.param_type = HV_INT;
	meta2.constraint_type = CONSTRAINT_OPTIONS;
	
	// 锟斤拷锟斤拷通锟斤拷锟借备选锟斤拷
	{
		auto devices = cp_loader::GetDeviceList();
		for (int i = 0; i < static_cast<int>(devices.size()); ++i) {
			meta2.options_constraint.AddOption(i, devices[i]);
		}
	}

	meta2.dependencies.push_back(ParamDependency(0, DEPENDS_ON_EQUALS, { "1" }));
	metadata_list.push_back(meta2);

	// 锟斤拷锟斤拷3: 锟斤拷锟斤拷锟斤拷源 (锟斤拷锟斤拷锟斤拷锟酵ｏ拷锟斤拷锟斤拷时通锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟饺∈碉拷锟街?
	ParamMetadata meta3;
	meta3.param_name = Tr(language_, "input.data_source.name");
	meta3.param_description = "";
	meta3.param_type = HV_INT;
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

		// 写锟斤拷锟侥硷拷
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
		// 锟斤拷取锟侥硷拷
		std::ifstream file(filePath);
		if (!file.is_open()) {
			return false;
		}

		nlohmann::json params_json;
		file >> params_json;
		file.close();

		// 锟斤拷锟絁SON锟角凤拷为锟斤拷锟斤拷
		if (!params_json.is_array()) {
			return false;
		}

		// 锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷
		for (const auto& param_json : params_json) {
			// 锟斤拷锟斤拷要锟街讹拷锟角凤拷锟斤拷锟?
			if (!param_json.contains("name") || !param_json.contains("type")) {
				continue;
			}

			std::string param_name = param_json["name"];
			int param_type = param_json["type"];

			// 锟斤拷锟捷诧拷锟斤拷锟斤拷锟狡斤拷锟叫达拷锟斤拷
			//if (param_name == "image_path") {
			//	// 锟斤拷锟矫碉拷锟斤拷锟皆憋拷锟斤拷锟?
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
	// 每一锟斤拷 DLL 锟节诧拷锟斤拷锟斤拷锟皆硷拷锟斤拷锟斤拷锟绞碉拷锟斤拷锟?
	return new HVSendData();
}

std::string GetInstanceName() {
	return "SendData"; // 锟斤拷知锟斤拷锟斤拷锟斤拷锟?DLL 锟斤拷锟斤拷锟斤拷锟斤拷锟?
}

extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion() {
	return NODE_ENGINE_ABI_VERSION;
}
