#pragma once
#include <dlfcn.h>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>
#include "_config.h"
#include "beatsaber-hook/shared/utils/il2cpp-functions.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "beatsaber-hook/shared/utils/size-concepts.hpp"
#include "beatsaber-hook/shared/utils/typedefs.h"
#include "beatsaber-hook/shared/utils/utils.h"
#include "logging.hpp"

struct Il2CppType;
struct ParameterInfo;
struct Il2CppClass;
struct FieldInfo;
struct MethodInfo;

namespace custom_types {
class Register;
class ClassWrapper;

struct FieldDesc {
    std::string_view name;
    std::size_t offset;
    Il2CppType const* type;

    constexpr FieldDesc(std::string_view name, std::size_t offset, Il2CppType const* type) : name(name), offset(offset), type(type) {}
};
struct StaticFieldDesc {
    std::string_view name;
    Il2CppType const* type;

    StaticFieldDesc(std::string_view name, Il2CppType const* type) : name(name), type(type) {}
};
struct MethodDesc {
    std::string_view name;
    void* address;
    Il2CppType const* retType;
    std::span<Il2CppType const* const> parameters;
    bool isStatic;
    bool isVirtual;
    // TODO: How to handle override?
    MethodInfo const* override;

    MethodDesc(std::string_view name, void* address, Il2CppType const* retType, std::span<Il2CppType const* const> parameters)
        : name(name), address(address), retType(retType), parameters(parameters) {}

    std::unique_ptr<MethodInfo> makeMethodInfo() const {
        auto info = std::make_unique<MethodInfo>();
        info->flags = 0;  // TODO:
        info->name = name.data();
        info->invoker_method = nullptr;  // TODO:
        info->methodPointer = reinterpret_cast<Il2CppMethodPointer>(address);
        info->return_type = retType;
        info->slot = kInvalidIl2CppMethodSlot;  // TODO:
        auto ps = parameters;
        info->parameters_count = ps.size();
        auto* paramList = reinterpret_cast<const Il2CppType**>(calloc(ps.size(), sizeof(Il2CppType*)));
        for (uint8_t pi = 0; pi < info->parameters_count; pi++) {
            paramList[pi] = ps[pi];
        }
        info->parameters = paramList;
        return info;
    }
};

struct CustomTypeInfo {
    constexpr CustomTypeInfo(std::string_view name, std::string_view namespaze, std::size_t size, std::span<FieldDesc const> fields, std::span<StaticFieldDesc const> staticFields,
                             std::span<MethodDesc const> methods)
        : name(name),
          namespaze(namespaze),
          size(size),
          fields(fields),
          staticFields(staticFields),
          methods(methods){

          };

    constexpr CustomTypeInfo(CustomTypeInfo&&) = default;
    constexpr CustomTypeInfo(CustomTypeInfo const&) = default;

    std::string_view name;
    std::string_view namespaze;
    std::size_t size;
    std::uint32_t typeFlags;
    std::optional<std::string_view> dllName;

    std::span<FieldDesc const> fields;
    std::span<StaticFieldDesc const> staticFields;
    std::span<MethodDesc const> methods;
};

template <typename T>
struct CustomTypeInfoTrait;

template <typename T>
struct CustomTypeInheritTrait;

template <typename T>
struct CustomTypeInterfacesTrait;

template <typename T, typename U>
concept convertible_to = std::is_convertible_v<T, U>;

// TODO: add require is marked as value type or not
template <typename T>
concept IsCustomType = ::il2cpp_utils::il2cpp_value_type_requirements<T> && requires {
    // immutable
    { CustomTypeInfoTrait<T>::name() } -> convertible_to<std::string>;
    { CustomTypeInfoTrait<T>::namespaze() } -> convertible_to<std::string>;

    // mutable since late registration
    { CustomTypeInfoTrait<T>::fields() } -> std::same_as<std::span<custom_types::FieldDesc const>>;
    { CustomTypeInfoTrait<T>::staticFields() } -> std::same_as<std::span<custom_types::StaticFieldDesc const>>;
    { CustomTypeInfoTrait<T>::methods() } -> std::same_as<std::span<custom_types::MethodDesc const>>;
};

template <typename T>
constexpr CustomTypeInfo typeInfo() {
    return CustomTypeInfo(CustomTypeInfoTrait<T>::namespaze(), CustomTypeInfoTrait<T>::name(), sizeof(T),
                          std::span(CustomTypeInfoTrait<T>::fields()),        //
                          std::span(CustomTypeInfoTrait<T>::staticFields()),  //
                          std::span(CustomTypeInfoTrait<T>::methods()));
}
template <typename... TArgs>
constexpr std::array<CustomTypeInfo, sizeof...(TArgs)> typeInfo() {
    return std::array<CustomTypeInfo, sizeof...(TArgs)>({ typeInfo<TArgs>()... });
}

// ret -> std::array<CustomTypeTypeName>
template <typename T>
constexpr auto interfaces() {
    return CustomTypeInterfacesTrait<T>::interfaces();
}

template <typename T>
constexpr CustomTypeInfo baseType() {
    return CustomTypeInheritTrait<T>::parent();
}
}

#if __has_include(<concepts>)
#include <concepts>
#include <type_traits>
template <typename T>
constexpr bool has_get = requires(const T& t) { t.get(); };

#ifndef CUSTOM_TYPES_NO_CONCEPTS
#define CUSTOM_TYPES_USE_CONCEPTS
#endif

#elif __has_include(<experimental/type_traits>)
#include <experimental/type_traits>
template <typename T>
using get_type = decltype(T::get());

template <typename T>
constexpr bool has_get = std::experimental::is_detected_v<get_type, T>;

#else
#error No libraries for the implementation of "has_" anything available!
#endif
template <class...>
constexpr std::false_type false_t{};

/// @struct A helper structure for getting the name of the type.
template <typename T>
struct name_registry {};

/// @struct A helper structure for getting the invoker function of a method
template <typename>
struct invoker_creator {};

/// @struct Type mapping struct
template <typename T>
struct type_tag {};

/// @struct A helper structure for converting parameter types to il2cpp types.
template <typename Decl, typename... Ps>
struct parameter_converter;

// Create a vector of ParameterInfo objects (good ol tail recursion)
// 1 or more parameters
template <typename Decl, typename P, typename... Ps>
struct parameter_converter<Decl, P, Ps...> {
    static inline std::vector<const Il2CppType*> get() {
        std::vector<const Il2CppType*> params;
        auto& info = params.emplace_back();
        il2cpp_functions::Init();
        const Il2CppType* type = ::il2cpp_functions::class_get_type(::il2cpp_utils::il2cpp_type_check::il2cpp_no_arg_class<P>::get());
        // Ignore name, it will be set when we iterate over all of them (param_1, param_2, etc.)
        // Ignore position, it will also be set on iteration.
        // TODO: Maybe some day we can actually use the parameters names themselves!
        if (type == nullptr) {
            _logger().warning("Failed to get type of parameter!");
        }
        info = type;
        for (const auto& q : parameter_converter<Decl, Ps...>::get()) {
            params.push_back(q);
        }
        return params;
    }
};

// 0 parameters
template <typename Decl>
struct parameter_converter<Decl> {
    static inline std::vector<const Il2CppType*> get() {
        return std::vector<const Il2CppType*>();
    }
};

/// @struct Helper structure for unpacking and packing arguments/return types for invoker function creation
struct arg_helper {
    template <typename Q>
    static inline Q unpack_arg(void* arg, type_tag<Q>) {
        if constexpr (std::is_pointer_v<Q>) {
            return reinterpret_cast<Q>(arg);
        } else if constexpr (il2cpp_utils::il2cpp_reference_type_wrapper<Q>) {
            return Q(arg);
        } else {
            return *reinterpret_cast<Q*>(arg);
        }
    }
    template <typename Q>
    static inline void* pack_result(Q&& thing) {
        if constexpr (std::is_pointer_v<Q>) {
            return reinterpret_cast<void*>(std::forward<Q>(thing));
        } else if constexpr (il2cpp_utils::has_il2cpp_conversion<Q>) {
            return thing.convert();
        } else {
            // We SHOULD simply be able to grab the class and box our result
            // Once boxed, we should just be able to return without any issue
            // I DO wonder if our invoke functions miss registration with GC...
            auto* klass = il2cpp_utils::il2cpp_type_check::il2cpp_no_arg_class<Q>::get();
            if (!klass) {
                _logger().critical("Failed to get non-null Il2CppClass* during invoke of custom function!");
                return nullptr;
            }
            il2cpp_functions::Init();
            return static_cast<void*>(il2cpp_functions::value_box(klass, static_cast<void*>(&thing)));
        }
    }

    template <typename Q>
    static inline void pack_result_into(Q&& thing, void* retval) {
        if constexpr (std::is_pointer_v<Q>) {
            *static_cast<void**>(retval) = std::forward<Q>(thing);
        } else if constexpr (il2cpp_utils::il2cpp_reference_type_wrapper<Q>) {
            *static_cast<void**>(retval) = thing.convert();
        } else {
            auto* klass = il2cpp_utils::il2cpp_type_check::il2cpp_no_arg_class<Q>::get();
            if (!klass) {
                _logger().critical("Failed to get non-null Il2CppClass* during invoke of custom function!");
                return;
            }
            // the void* retval is a buffer created as being klass->instance_size - sizeof(Il2CppObject), see Runtime::InvokeWithThrow
            auto sz = sizeof(std::decay_t<Q>);
            std::memcpy(retval, &thing, sz);
        }
    }
};

template <typename TRet, typename T, typename... TArgs>
struct invoker_creator<TRet (T::*)(TArgs...)> {
    template <std::size_t... Ns>
    static void instance_invoke(TRet (*func)(T*, TArgs...), T* self, void** args, std::index_sequence<Ns...>, void* retval) {
        IL2CPP_CATCH_HANDLER(
            if constexpr (std::is_same_v<TRet, void>) { func(self, arg_helper::unpack_arg(args[Ns], type_tag<TArgs>{})...); } else {
                arg_helper::pack_result_into(func(self, arg_helper::unpack_arg(args[Ns], type_tag<TArgs>{})...), retval);
            })
    }
    [[gnu::noinline]] static void invoke(Il2CppMethodPointer ptr, [[maybe_unused]] const MethodInfo* m, void* obj, void** args, void* retval) {
        // We also don't need to use anything from m so it is ignored.
        // Perhaps far in the future we will check attributes on it
        auto func = reinterpret_cast<TRet (*)(T*, TArgs...)>(ptr);
        T* self = static_cast<T*>(obj);

        auto seq = std::make_index_sequence<sizeof...(TArgs)>();
        instance_invoke(func, self, args, seq, retval);
    }
    template <TRet (T::*member)(TArgs...)>
    [[gnu::noinline]] static inline TRet wrap(T* self, TArgs... args) {
        return (self->*member)(args...);
    }
};

template <typename TRet, typename... TArgs>
struct invoker_creator<TRet (*)(TArgs...)> {
    template <std::size_t... Ns>
    static void static_invoke(TRet (*func)(TArgs...), void** args, std::index_sequence<Ns...>, void* retval) {
        IL2CPP_CATCH_HANDLER(
            if constexpr (std::is_same_v<TRet, void>) { func(arg_helper::unpack_arg(args[Ns], type_tag<TArgs>{})...); } else {
                arg_helper::pack_result_into(func(arg_helper::unpack_arg(args[Ns], type_tag<TArgs>{})...), retval);
            })
    }
    template <std::size_t... Ns>
    static void static_invoke_method(TRet (*func)(TArgs..., const MethodInfo*), void** args, const MethodInfo* m, std::index_sequence<Ns...>, void* retval) {
        IL2CPP_CATCH_HANDLER(
            if constexpr (std::is_same_v<TRet, void>) { func(arg_helper::unpack_arg(args[Ns], type_tag<TArgs>{})..., m); } else {
                arg_helper::pack_result_into(func(arg_helper::unpack_arg(args[Ns], type_tag<TArgs>{})..., m), retval);
            })
    }
    [[gnu::noinline]] static void invoke(Il2CppMethodPointer ptr, [[maybe_unused]] const MethodInfo* m, [[maybe_unused]] void* obj, void** args, void* retval) {
        // We also don't need to use anything from m so it is ignored.
        // Perhaps far in the future we will check attributes on it

        auto seq = std::make_index_sequence<sizeof...(TArgs)>();

        // post unity update delegates changed which use this invoke method
        // they get passed a nullptr ptr arg, so if they do we just take the method pointer from the method info instead!
        auto func = ptr ? reinterpret_cast<TRet (*)(TArgs...)>(ptr) : reinterpret_cast<TRet (*)(TArgs...)>(m->methodPointer);

        static_invoke(func, args, seq, retval);
    }
    [[gnu::noinline]] static void* invoke_method(Il2CppMethodPointer ptr, const MethodInfo* m, [[maybe_unused]] void* obj, void** args, void* retval) {
        auto func = reinterpret_cast<TRet (*)(TArgs..., const MethodInfo*)>(ptr);
        auto seq = std::make_index_sequence<sizeof...(TArgs)>();
        static_invoke_method(func, args, m, seq, retval);
    }
};
}  // namespace custom_types
