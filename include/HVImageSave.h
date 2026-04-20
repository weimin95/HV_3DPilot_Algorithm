#pragma once

#include "3d_pilot_public_def.h"
#include "node_engine.h"
#include "param_meta_data.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class HVImageSave : public NodeEngine {
public:
    HVImageSave();
    ~HVImageSave() override = default;

    int init() override;
    int run() override;
    int set_algorithm_params(const std::vector<void*>& params, const std::vector<int>& paramID = std::vector<int>()) override;
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

private:
    int ApplyParam(int param_id, void* param);
    int Save2DImage();
    int SaveDepthImage();
    int SavePointCloud();
    int PrepareOutputDirectory(std::filesystem::path& output_dir);
    int ApplyRetentionPolicy(const std::filesystem::path& root_dir);
    int EnsureStorageCapacity(const std::filesystem::path& root_dir, std::string& skip_message_key);
    void ResetOutputs();
    int Fail(int status, const std::string& message_key);
    int Succeed(const std::string& saved_path, bool saved, const std::string& message_key);

private:
    int input_type_ = 0;
    std::shared_ptr<ImageDataInfo2D> input_2d_image_;
    std::shared_ptr<ImageDataInfoDepth> input_depth_image_;
    std::shared_ptr<HVPointCloud> input_point_cloud_;
    std::string output_root_dir_;
    std::string file_name_prefix_ = "image";
    bool create_date_directories_ = false;
    int storage_mode_ = 0;
    int max_storage_count_ = 0;
    double min_free_space_mb_ = 0.0;
    int max_save_days_ = 0;
    int save_format_ = 0;
    int jpeg_quality_ = 95;

    std::string saved_path_;
    bool saved_ = false;
    int execute_status_ = NODE_STATUS_NOT_RUN;
    long run_time_ms_ = 0;
    std::string error_message_key_;
    int language_ = static_cast<int>(UIPilotLanguage::ZH_CN);
};

extern "C" __declspec(dllexport) NodeEngine* CreateInstance();
extern "C" __declspec(dllexport) std::string GetInstanceName();
extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion();
