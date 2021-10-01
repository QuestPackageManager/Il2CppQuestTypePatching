# Copyright (C) 2009 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
LOCAL_PATH := $(call my-dir)
TARGET_ARCH_ABI := $(APP_ABI)
rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))

# Creating prebuilt for dependency: modloader - version: 1.2.3
include $(CLEAR_VARS)
LOCAL_MODULE := modloader
LOCAL_EXPORT_C_INCLUDES := extern/modloader
LOCAL_SRC_FILES := extern/libmodloader.so
include $(PREBUILT_SHARED_LIBRARY)
# Creating prebuilt for dependency: beatsaber-hook - version: 3.0.6
include $(CLEAR_VARS)
LOCAL_MODULE := beatsaber-hook_3_0_6
LOCAL_EXPORT_C_INCLUDES := extern/beatsaber-hook
LOCAL_SRC_FILES := extern/libbeatsaber-hook_3_0_6.so
include $(PREBUILT_SHARED_LIBRARY)
# Creating prebuilt for dependency: capstone - version: 0.1.0
include $(CLEAR_VARS)
LOCAL_MODULE := capstone
LOCAL_EXPORT_C_INCLUDES := extern/capstone
LOCAL_SRC_FILES := extern/libcapstone.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := custom-types
LOCAL_SRC_FILES += $(call rwildcard,src/**,*.cpp)
LOCAL_SRC_FILES += $(call rwildcard,extern/beatsaber-hook/src/inline-hook/,*.cpp)
LOCAL_SHARED_LIBRARIES += beatsaber-hook_3_0_6
LOCAL_SHARED_LIBRARIES += modloader
LOCAL_STATIC_LIBRARIES += capstone
LOCAL_LDLIBS += -llog
LOCAL_CFLAGS += -std=c++20 -O3 -Wno-invalid-offsetof -isystem"./extern/libil2cpp/il2cpp/libil2cpp" -isystem"./extern" -I"./shared" -D"ID=\"custom-types\"" -I'./shared' -I'./extern' -DNEED_UNSAFE_CSHARP -DVERSION='"0.13.0"' -DNO_VERBOSE_LOGS
LOCAL_C_INCLUDES += ./include ./src
LOCAL_CPP_FEATURES += exceptions
# LOCAL_CFLAGS += -DLOCAL_TEST
# LOCAL_CFLAGS += -DLOCAL_TEST_COROUTINE
LOCAL_CPP_FLAGS += -Wall -Werror -fcoroutines
include $(BUILD_SHARED_LIBRARY)
