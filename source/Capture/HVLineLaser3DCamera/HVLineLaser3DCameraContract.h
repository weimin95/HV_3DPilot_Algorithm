#pragma once

#include "3d_pilot_public_def.h"
#include "param_meta_data.h"

#include <string>
#include <vector>

namespace line_laser_3d_camera {

struct Contract {
    std::string algorithm_name;
    std::string zh_display_name;
    std::string en_display_name;
    AlgorithmType algorithm_type = AlgorithmType::Capture;
    std::vector<int> input_types;
    std::vector<std::string> input_names;
    std::vector<bool> input_bindable_hints;
    std::vector<ParamMetadata> input_metadata;
    std::vector<int> output_types;
    std::vector<std::string> output_names;
};

Contract BuildContract();

}  // namespace line_laser_3d_camera
