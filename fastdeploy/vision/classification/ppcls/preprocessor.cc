// Copyright (c) 2022 PaddlePaddle Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "fastdeploy/vision/classification/ppcls/preprocessor.h"
#include "fastdeploy/function/concat.h"
#include "yaml-cpp/yaml.h"
#ifdef WITH_GPU
#include <cuda_runtime_api.h>
#endif

namespace fastdeploy {
namespace vision {
namespace classification {

PaddleClasPreprocessor::PaddleClasPreprocessor(const std::string& config_file) {
  this->config_file_ = config_file;
  FDASSERT(BuildPreprocessPipelineFromConfig(),
           "Failed to create PaddleClasPreprocessor.");
  initialized_ = true;
}

bool PaddleClasPreprocessor::BuildPreprocessPipelineFromConfig() {
  processors_.clear();
  YAML::Node cfg;
  try {
    cfg = YAML::LoadFile(config_file_);
  } catch (YAML::BadFile& e) {
    FDERROR << "Failed to load yaml file " << config_file_
            << ", maybe you should check this file." << std::endl;
    return false;
  }
  auto preprocess_cfg = cfg["PreProcess"]["transform_ops"];
  processors_.push_back(std::make_shared<BGR2RGB>());
  for (const auto& op : preprocess_cfg) {
    FDASSERT(op.IsMap(),
             "Require the transform information in yaml be Map type.");
    auto op_name = op.begin()->first.as<std::string>();
    if (op_name == "ResizeImage") {
      int target_size = op.begin()->second["resize_short"].as<int>();
      bool use_scale = false;
      int interp = 1;
      processors_.push_back(
          std::make_shared<ResizeByShort>(target_size, 1, use_scale));
    } else if (op_name == "CropImage") {
      int width = op.begin()->second["size"].as<int>();
      int height = op.begin()->second["size"].as<int>();
      processors_.push_back(std::make_shared<CenterCrop>(width, height));
    } else if (op_name == "NormalizeImage") {
      if (!disable_normalize_) {
        auto mean = op.begin()->second["mean"].as<std::vector<float>>();
        auto std = op.begin()->second["std"].as<std::vector<float>>();
        auto scale = op.begin()->second["scale"].as<float>();
        FDASSERT((scale - 0.00392157) < 1e-06 && (scale - 0.00392157) > -1e-06,
                "Only support scale in Normalize be 0.00392157, means the pixel "
                "is in range of [0, 255].");
        processors_.push_back(std::make_shared<Normalize>(mean, std));
      }
    } else if (op_name == "ToCHWImage") {
      if (!disable_permute_) {
        processors_.push_back(std::make_shared<HWC2CHW>());
      }
    } else {
      FDERROR << "Unexcepted preprocess operator: " << op_name << "."
              << std::endl;
      return false;
    }
  }

  // Fusion will improve performance
  FuseTransforms(&processors_);
  return true;
}

void PaddleClasPreprocessor::DisableNormalize() {
  this->disable_normalize_ = true;
  // the DisableNormalize function will be invalid if the configuration file is loaded during preprocessing
  if (!BuildPreprocessPipelineFromConfig()) {
    FDERROR << "Failed to build preprocess pipeline from configuration file." << std::endl;
  }
}
void PaddleClasPreprocessor::DisablePermute() {
  this->disable_permute_ = true;
  // the DisablePermute function will be invalid if the configuration file is loaded during preprocessing
  if (!BuildPreprocessPipelineFromConfig()) {
    FDERROR << "Failed to build preprocess pipeline from configuration file." << std::endl;
  }
}

void PaddleClasPreprocessor::UseGpu(int gpu_id) {
#ifdef WITH_GPU
  use_cuda_ = true;
  if (gpu_id < 0) return;
  device_id_ = gpu_id;
  cudaSetDevice(device_id_);
#else
  FDWARNING << "FastDeploy didn't compile with WITH_GPU. "
            << "Will force to use CPU to run preprocessing." << std::endl;
  use_cuda_ = false;
#endif
}

bool PaddleClasPreprocessor::Run(std::vector<FDMat>* images, std::vector<FDTensor>* outputs) {
  if (!initialized_) {
    FDERROR << "The preprocessor is not initialized." << std::endl;
    return false;
  }
  if (images->size() == 0) {
    FDERROR << "The size of input images should be greater than 0." << std::endl;
    return false;
  }

  for (size_t i = 0; i < images->size(); ++i) {
    for (size_t j = 0; j < processors_.size(); ++j) {
      bool ret = false;
      if (processors_[j]->Name() == "NormalizeAndPermute" && use_cuda_) {
        ret = (*(processors_[j].get()))(&((*images)[i]), ProcLib::CUDA);
      } else {
        ret = (*(processors_[j].get()))(&((*images)[i]));
      }
      if (!ret) {
        FDERROR << "Failed to processs image:" << i << " in "
                << processors_[i]->Name() << "." << std::endl;
        return false;
      }
    }
  }

  outputs->resize(1);
  // Concat all the preprocessed data to a batch tensor
  std::vector<FDTensor> tensors(images->size()); 
  for (size_t i = 0; i < images->size(); ++i) {
    (*images)[i].ShareWithTensor(&(tensors[i]));
    tensors[i].ExpandDim(0);
  }
  if (tensors.size() == 1) {
    (*outputs)[0] = std::move(tensors[0]);
  } else {
    function::Concat(tensors, &((*outputs)[0]), 0);
  }
  (*outputs)[0].device_id = device_id_;
  return true;
}

}  // namespace classification
}  // namespace vision
}  // namespace fastdeploy
