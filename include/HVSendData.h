#pragma once

#include "node_engine.h"
#include "3d_pliot_error.h"
#include "3d_pilot_public_def.h"
#include "param_meta_data.h"
#include "json.hpp"

#include <string>

class HVSendData : public NodeEngine
{
	// 通过 NodeEngine 继承
	int init() override;
	int run() override;
	int set_algorithm_params(const std::vector<void*>& params, const std::vector<int>& paramID) override;
	std::vector<void*> get_current_params() override;
	std::vector<void*> get_algorithm_result() override;
	std::vector<int> get_algorithm_input_params_type() override;
	std::vector<int> get_algorithm_output_params_type() override;
	std::vector<std::string> get_algorithm_input_params_name() override;
	std::vector<std::string> get_algorithm_output_params_name() override;
	std::vector<bool> get_algorithm_input_params_bindable() override;
	std::vector<ParamMetadata> get_algorithm_input_params_metadata() override;
	int get_algorithm_execute_status() override;
	std::string get_algorithm_error_message() override;
	long get_algorithm_use_time() override;
	bool algorithm_params_setting_status() override;
	bool algorithm_init_status() override;
	bool save_params_to_json(const std::string& filePath) override;
	bool load_params_from_json(const std::string& filePath) override;
	AlgorithmType get_algorithm_type() override;
	void set_language(int language) override;
	int get_language() const override;
	std::string get_algorithm_display_name() override;
	void set_host_services(NodeHostServices* host_services) override;

private:
	// 辅助函数：向 JSON 参数列表中添加参数
	template <typename T>
	void add_param(nlohmann::json& params_json, const std::string& name, int type, const T& value)
	{
		nlohmann::json param_json;
		param_json["name"] = name;
		param_json["type"] = type;
		param_json["value"] = value;
		params_json.push_back(param_json);
	}

	// 将 void* 转为对应参数类型指针
	template <typename T>
	T cast_param(const std::vector<void*>& params, int param_id)
	{
		if (param_id < 0 || param_id >= static_cast<int>(params.size()) || params[param_id] == nullptr)
		{
			return T();
		}
		return *static_cast<T*>(params[param_id]);
	}

	// 将 void* 转为对应参数类型智能指针
	template <typename T>
	std::shared_ptr<T> cast_param_sharedPtr(const std::vector<void*>& params, int param_id)
	{
		if (param_id < 0 || param_id >= static_cast<int>(params.size()) || params[param_id] == nullptr)
		{
			return nullptr;
		}
		return *static_cast<std::shared_ptr<T>*>(params[param_id]);
	}

private:
	int send_target = 0;
	int select_variable = 0;
	int select_comm_device = 0;

	NodeHostServices* host_services_ = nullptr;

	long run_time = -1;
	std::string error_msg;
	int language_ = static_cast<int>(UIPilotLanguage::ZH_CN);
	int execute_status = NODE_STATUS_NOT_RUN;
};

extern "C" __declspec(dllexport) NodeEngine* CreateInstance();
extern "C" __declspec(dllexport) std::string GetInstanceName();
