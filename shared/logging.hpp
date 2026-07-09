#pragma once

#include "_config.h"
#include "beatsaber-hook/shared/config.hpp"

#include <string_view>

struct Il2CppAssemblyName;
struct Il2CppAssembly;
struct Il2CppCodeGenModule;
struct Il2CppImage;
struct Il2CppType;
struct ParameterInfo;
struct MethodInfo;
struct Il2CppClass;
struct VirtualInvokeData;
struct Il2CppRuntimeInterfaceOffsetPair;

namespace custom_types {
    /// @brief Returns the logger used within custom types. Should not be called publicly.
    /// @return The logger used internally
    static constexpr auto logger = Paper::ConstLoggerContext("CustomTypes");

    /// @brief Logs the provided Il2CppAssemblyName* with a provided label.
    /// @param name The Il2CppAssemblyName* to log all fields on.
    /// @param anameLabel The label for the logging.
    CUSTOM_TYPES_EXPORT void logAname(Il2CppAssemblyName const* name, std::string_view anameLabel);

    /// @brief Logs the provided Il2CppAssembly*
    /// @param assem The Il2CppAssembly* to log all fields on.
    CUSTOM_TYPES_EXPORT void logAsm(Il2CppAssembly const* assem);

    /// @brief Logs the provided Il2CppCodeGenModule* with a provided label.
    /// @param module The Il2CppCodeGenModule* to log all fields on.
    /// @param s The label for the logging.
    CUSTOM_TYPES_EXPORT void logCodegen(Il2CppCodeGenModule const* m, std::string_view s);

    /// @brief Logs the provided Il2CppImage*
    /// @param img The Il2CppImage* to log all fields on.
    CUSTOM_TYPES_EXPORT void logImage(Il2CppImage const* img);

    /// @brief Logs the provided Il2CppType* with a provided label.
    /// @param t The Il2CppType* to log all fields on.
    /// @param s The label for the logging.
    CUSTOM_TYPES_EXPORT void logType(Il2CppType const* t, std::string_view s);

    /// @brief Logs the provided Il2CppClass*'s vtable
    CUSTOM_TYPES_EXPORT void logVtable(VirtualInvokeData const* invokeData);

    /// @brief Logs various information about the fields of the provided Il2CppClass*
    CUSTOM_TYPES_EXPORT void logFields(Il2CppClass const* klass);

    /// @brief Logs the provided Il2CppRuntimeInterfaceOffsetPair
    CUSTOM_TYPES_EXPORT void logInterfaceOffset(Il2CppRuntimeInterfaceOffsetPair const* pair);

    /// @brief Logs the provided ParameterInfo*
    /// @param info The ParameterInfo* to log all fields on.
    CUSTOM_TYPES_EXPORT void logParam(Il2CppType const* info, int index);

    /// @brief Logs the provided MethodInfo*
    /// @param info The MethodInfo* to log all fields on.
    CUSTOM_TYPES_EXPORT void logMethod(MethodInfo const* info);

    /// @brief Logs the provided Il2CppClass*
    /// @param klass The Il2CppClass* to log all fields on.
    CUSTOM_TYPES_EXPORT void logAll(Il2CppClass const* klass);
}
