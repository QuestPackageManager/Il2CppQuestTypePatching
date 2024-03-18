#pragma once
#include <stddef.h>
#include <stdint.h>
#include <concepts>
#include <new>
#include <utility>
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "beatsaber-hook/shared/utils/utils.h"
#include "logging.hpp"
#include "register.hpp"
#include "types.hpp"
#include "util.hpp"

// name
#define CUSTOM_TYPE_NAME(m_namespaze, m_name)                                            \
    namespace m_namespaze {                                                              \
    struct m_name;                                                                       \
    }                                                                                    \
    template <>                                                                          \
    struct custom_types::CustomTypeInfoTrait<m_namespaze::m_name> final {                \
        static constexpr std::string_view namespaze() {                                  \
            return #m_namespaze;                                                         \
        }                                                                                \
        static constexpr std::string_view name() {                                       \
            return #m_name;                                                              \
        }                                                                                \
        static constexpr std::span<custom_types::FieldDesc const> fields() {             \
            return fieldsVec;                                                            \
        }                                                                                \
        static constexpr std::span<custom_types::StaticFieldDesc const> staticFields() { \
            return staticFieldsVec;                                                      \
        }                                                                                \
        static constexpr std::span<custom_types::MethodDesc const> methods() {           \
            return methodsVec;                                                           \
        }                                                                                \
                                                                                         \
       private:                                                                          \
        inline static std::vector<custom_types::FieldDesc> fieldsVec;                    \
        inline static std::vector<custom_types::StaticFieldDesc> staticFieldsVec;        \
        inline static std::vector<custom_types::MethodDesc> methodsVec;                  \
        friend struct m_namespaze::m_name;                                               \
        void addField(custom_types::FieldDesc&& f) {                                     \
            fieldsVec.emplace_back(std::move(f));                                        \
        }                                                                                \
        void addStaticField(custom_types::StaticFieldDesc&& f) {                         \
            staticFieldsVec.emplace_back(std::move(f));                                  \
        }                                                                                \
        void addMethod(custom_types::MethodDesc&& f) {                                   \
            methodsVec.emplace_back(std::move(f));                                       \
        }                                                                                \
    };

// inherit
#define CUSTOM_TYPE_INHERIT(m_typ, m_ParentNamespace, m_ParentName)     \
    template <>                                                         \
    struct custom_types::CustomTypeInheritTrait<m_typ> final {          \
        static constexpr CustomTypeTypeName parent() {                  \
            return CustomTypeTypeName(m_ParentNamespace, m_ParentName); \
        }                                                               \
    };

// codegen inherit
#define CUSTOM_TYPE_INHERIT_CODEGEN(m_typ, m_Parent)           \
    template <>                                                \
    struct custom_types::CustomTypeInheritTrait<m_typ> final { \
        static constexpr CustomTypeTypeName parent() {         \
            return typeName<m_Parent>();                       \
        }                                                      \
    };

// interfaces
#define CUSTOM_TYPE_INTERFACES(m_typ, ...)                          \
    template <>                                                     \
    struct custom_types::CustomTypeInterfacesTrait<m_typ> final {   \
        static constexpr auto interfaces() {                        \
            return std::array<CustomTypeTypeName>({ __VA_ARGS__ }); \
        }                                                           \
    };

// codegen interfaces
#define CUSTOM_TYPE_INTERFACES_CODEGEN(m_typ, ...)                \
    template <>                                                   \
    struct custom_types::CustomTypeInterfacesTrait<m_typ> final { \
        static constexpr auto interfaces() {                      \
            return typeNames<__VA_ARGS__>();                      \
        }                                                         \
    };

// fields
#define REGISTER_FIELD(typ, name)                                                                                                                                          \
   private:                                                                                                                                                                \
    /* I have no idea how this gets called properly. `*this` is probably undefined! */                                                                                     \
    __attribute__((constructor)) void __setup_##name() {                                                                                                                   \
        CustomTypeTypeNameTrait<std::decay_t<decltype(*this)>>::fields.emplace_back(#name, offsetof(std::decay_t<decltype(*this)>, name),                                  \
                                                                                    ::il2cpp_utils::il2cpp_type_check::il2cpp_no_arg_type<std::decay_t<decltype(*this)>>); \
    };

#define DECLARE_FIELD(acc, typ, name) \
    acc:                              \
    typ name;                         \
    REGISTER_FIELD(typ, name)         \
   public:
// static fields
#define REGISTER_STATIC_FIELD(typ, name)                                                                                                                                                      \
   private:                                                                                                                                                                                   \
    /* I have no idea how this gets called properly. `*this` is probably undefined! */                                                                                                        \
    __attribute__((constructor)) void __setup_##name() {                                                                                                                                      \
        CustomTypeTypeNameTrait<std::decay_t<decltype(*this)>>::staticFields.emplace_back(#name, name, ::il2cpp_utils::il2cpp_type_check::il2cpp_no_arg_type<std::decay_t<decltype(*this)>>); \
    };

#define DECLARE_STATIC_FIELD(acc, typ, name) \
    acc:                                     \
    typ name;                                \
    REGISTER_STATIC_FIELD(typ, name)         \
   public:
// methods
#define REGISTER_METHOD(typ, name)                                                                                                                                          \
   private:                                                                                                                                                                 \
    /* I have no idea how this gets called properly. `*this` is probably undefined! */                                                                                      \
    __attribute__((constructor)) void __setup_##name() {                                                                                                                    \
        CustomTypeTypeNameTrait<std::decay_t<decltype(*this)>>::methods.emplace_back(#name, offsetof(std::decay_t<decltype(*this)>, name),                                  \
                                                                                     ::il2cpp_utils::il2cpp_type_check::il2cpp_no_arg_type<std::decay_t<decltype(*this)>>, nullptr /* TODO:*/); \
    };

#define DECLARE_METHOD(acc, typ, name) \
    acc:                               \
    typ name;                          \
    REGISTER_METHOD(typ, name)         \
   public:

#define CUSTOM_TYPE_CODEGEN(namespaze, name, parent, ...)         \
    CUSTOM_TYPE_NAME(namespaze, name);                            \
    CUSTOM_TYPE_INHERIT_CODEGEN(namespaze::name, parent);         \
    CUSTOM_TYPE_INTERFACES_CODEGEN(namespaze::name, __VA_ARGS__); \
    MARK_REF_PTR_T(namespaze::name)                               \
    struct namespaze::name : public parent

namespace custom_types {
template <class T, class... TArgs>
void InvokeBaseCtor(Il2CppClass* klass, T* self, TArgs&&... args) {
    static auto m = ::il2cpp_utils::FindMethod(klass, ".ctor", std::array<Il2CppType const*, sizeof...(TArgs)>{ ::il2cpp_utils::ExtractIndependentType<TArgs>()... });
    ::il2cpp_utils::RunMethodRethrow(self, m, args...);
}
}  // namespace custom_types

#ifndef INVOKE_BASE_CTOR
#define INVOKE_BASE_CTOR(klass, ...) ::custom_types::InvokeBaseCtor(klass, this __VA_OPT__(, ) __VA_ARGS__)
#endif
