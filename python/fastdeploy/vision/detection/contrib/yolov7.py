# Copyright (c) 2022 PaddlePaddle Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from __future__ import absolute_import
import logging
from .... import FastDeployModel, ModelFormat
from .... import c_lib_wrap as C


class YOLOv7(FastDeployModel):
    def __init__(self,
                 model_file,
                 params_file="",
                 runtime_option=None,
                 model_format=ModelFormat.ONNX):
        """Load a YOLOv7 model exported by YOLOv7.

        :param model_file: (str)Path of model file, e.g ./yolov7.onnx
        :param params_file: (str)Path of parameters file, e.g yolox/model.pdiparams, if the model_fomat is ModelFormat.ONNX, this param will be ignored, can be set as empty string
        :param runtime_option: (fastdeploy.RuntimeOption)RuntimeOption for inference this model, if it's None, will use the default backend on CPU
        :param model_format: (fastdeploy.ModelForamt)Model format of the loaded model
        """
        # 调用基函数进行backend_option的初始化
        # 初始化后的option保存在self._runtime_option
        super(YOLOv7, self).__init__(runtime_option)

        self._model = C.vision.detection.YOLOv7(
            model_file, params_file, self._runtime_option, model_format)
        # 通过self.initialized判断整个模型的初始化是否成功
        assert self.initialized, "YOLOv7 initialize failed."

    def predict(self, input_image, conf_threshold=0.25, nms_iou_threshold=0.5):
        """Detect an input image

        :param input_image: (numpy.ndarray)The input image data, 3-D array with layout HWC, BGR format
        :param conf_threshold: confidence threashold for postprocessing, default is 0.25
        :param nms_iou_threshold: iou threashold for NMS, default is 0.5
        :return: DetectionResult
        """
        return self._model.predict(input_image, conf_threshold,
                                   nms_iou_threshold)

    # 一些跟YOLOv7模型有关的属性封装
    # 多数是预处理相关，可通过修改如model.size = [1280, 1280]改变预处理时resize的大小（前提是模型支持）
    @property
    def size(self):
        """
        Argument for image preprocessing step, the preprocess image size, tuple of (width, height)
        """
        return self._model.size

    @property
    def padding_value(self):
        #  padding value, size should be the same as channels
        return self._model.padding_value

    @property
    def is_no_pad(self):
        # while is_mini_pad = false and is_no_pad = true, will resize the image to the set size
        return self._model.is_no_pad

    @property
    def is_mini_pad(self):
        # only pad to the minimum rectange which height and width is times of stride
        return self._model.is_mini_pad

    @property
    def is_scale_up(self):
        # if is_scale_up is false, the input image only can be zoom out, the maximum resize scale cannot exceed 1.0
        return self._model.is_scale_up

    @property
    def stride(self):
        # padding stride, for is_mini_pad
        return self._model.stride

    @property
    def max_wh(self):
        # for offseting the boxes by classes when using NMS
        return self._model.max_wh

    @size.setter
    def size(self, wh):
        assert isinstance(wh, (list, tuple)),\
            "The value to set `size` must be type of tuple or list."
        assert len(wh) == 2,\
            "The value to set `size` must contatins 2 elements means [width, height], but now it contains {} elements.".format(
            len(wh))
        self._model.size = wh

    @padding_value.setter
    def padding_value(self, value):
        assert isinstance(
            value,
            list), "The value to set `padding_value` must be type of list."
        self._model.padding_value = value

    @is_no_pad.setter
    def is_no_pad(self, value):
        assert isinstance(
            value, bool), "The value to set `is_no_pad` must be type of bool."
        self._model.is_no_pad = value

    @is_mini_pad.setter
    def is_mini_pad(self, value):
        assert isinstance(
            value,
            bool), "The value to set `is_mini_pad` must be type of bool."
        self._model.is_mini_pad = value

    @is_scale_up.setter
    def is_scale_up(self, value):
        assert isinstance(
            value,
            bool), "The value to set `is_scale_up` must be type of bool."
        self._model.is_scale_up = value

    @stride.setter
    def stride(self, value):
        assert isinstance(
            value, int), "The value to set `stride` must be type of int."
        self._model.stride = value

    @max_wh.setter
    def max_wh(self, value):
        assert isinstance(
            value, float), "The value to set `max_wh` must be type of float."
        self._model.max_wh = value
