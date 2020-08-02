#include "types.hpp"
#include "logging.hpp"
#include "beatsaber-hook/shared/utils/typedefs.h"
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"

type_info::type_info(Il2CppTypeEnum typeE, std::string_view ns, std::string_view n, Il2CppClass* b) {
    typeEnum = typeE;
    base = b;
    namespaze = std::string(ns);
    name = std::string(n);
}

// type_info::~type_info() {
//     logger().debug("Deleting type_info!");
//     logger().debug("This would invalidate type namespace and name, this shouldn't happen!");
// }

field_info::field_info(std::string_view name, const Il2CppType* type, int32_t offset, uint16_t fieldAttrs) {
    // Create FieldInfo*
    info = FieldInfo{};
    info.name = name.data();
    // We want to make sure we set the correct attributes on this type
    // For that purpose, we actually want to COPY the type
    auto* tmp = new Il2CppType();
    tmp->attrs = type->attrs | fieldAttrs;
    tmp->byref = type->byref;
    tmp->data.dummy = type->data.dummy;
    tmp->num_mods = type->num_mods;
    tmp->pinned = type->pinned;
    tmp->type = type->type;
    info.type = tmp;
    info.offset = offset;
    info.token = -1;
}

// field_info::~field_info() {
//     logger().debug("Deleting field_info!");
//     logger().debug("This shouldn't happen!");
//     // logger().debug("Deleting field_info.Il2CppType! Ptr: %p", info.type);
//     // delete info.type;
// }

method_info::method_info(std::string_view name, void* func, InvokerMethod invoker, const Il2CppType* returnType, std::vector<ParameterInfo>& parameters, uint16_t flags) {
    // Create MethodInfo*
    info = new MethodInfo();
    params = parameters;
    info->name = name.data();
    info->methodPointer = (Il2CppMethodPointer)func;
    // TODO: Need to figure out how to reliably set invoker functions
    // This is probably going to be really challenging...
    // For now, we will try to make our own and see if we crash
    info->invoker_method = invoker;
    info->return_type = returnType;
    info->parameters = params.data();
    info->parameters_count = params.size();
    info->flags = flags;
    info->slot = kInvalidIl2CppMethodSlot;
    // TODO: set more data on method, perhaps pass in less?
}

// method_info::~method_info() {
//     logger().debug("Deleting method_info!");
//     logger().debug("This would invalidate our params vector, this call shouldn't happen!");
//     // logger().debug("Deleting method_info! Ptr: %p", info);
//     // delete info;
// }

void* ::custom_types::allocate(std::size_t size) {
    // Allocate using il2cpp
    // This is a bit tricky since we want to ensure il2cpp cleans up after we are done
    // This is specifically for value types and primitives
    // since reference types should be created via il2cpp_utils::New and returned.
    return nullptr;
}