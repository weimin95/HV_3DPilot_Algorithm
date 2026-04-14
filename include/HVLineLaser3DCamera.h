#pragma once

#include "HVLineLaser3DCameraContract.h"
#include "SR7Link.h"
#include "node_engine.h"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class HVLineLaser3DCamera : public NodeEngine {
public:
    HVLineLaser3DCamera();
    ~HVLineLaser3DCamera() override;

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
    static void BatchOneTimeCallback(const void* info, const SR7IF_Data* data);

    int ApplyParam(int param_id, void* param);
    int EnsureInitialized();
    int ParseIpConfig(SR7IF_ETHERNET_CONFIG& config);
    int BuildOutputsFromLastFrame(double x_pitch_mm);
    void OnBatchOneTimeData(const void* info, const SR7IF_Data* data);
    void CloseCamera();
    void ClearOutputs();
    void ResetCallbackState();
    void SetFailure(int status, const std::string& message);

private:
    line_laser_3d_camera::Contract contract_;
    std::string ip_;
    int device_id_ = 0;
    int batch_points_ = 1000;
    int head_index_ = 0;
    int connect_timeout_ms_ = 2000;
    int wait_timeout_ms_ = 50000;
    double y_pitch_mm_ = 0.0;

    bool params_set_ = true;
    bool initialized_ = false;
    bool connected_ = false;
    int language_ = static_cast<int>(UIPilotLanguage::ZH_CN);
    int execute_status_ = NODE_STATUS_NOT_RUN;
    long use_time_ms_ = 0L;
    std::string error_message_;

    std::vector<int> height_buffer_;
    std::vector<unsigned char> gray_buffer_;
    std::vector<unsigned int> encoder_buffer_;
    int frame_width_ = 0;
    int frame_height_ = 0;
    int callback_status_ = NODE_STATUS_NOT_RUN;
    bool callback_received_ = false;

    std::shared_ptr<HVPointCloud> point_cloud_;
    std::shared_ptr<ImageDataInfoDepth> depth_image_;
    std::shared_ptr<ImageDataInfo2D> gray_image_;

    std::mutex mutex_;
    std::condition_variable callback_cv_;
};

extern "C" __declspec(dllexport) NodeEngine* CreateInstance();
extern "C" __declspec(dllexport) std::string GetInstanceName();
extern "C" __declspec(dllexport) int GetNodeEngineAbiVersion();
