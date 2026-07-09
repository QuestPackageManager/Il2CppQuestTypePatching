#pragma once

#include "_config.h"
#include "logging.hpp"
#include "beatsaber-hook/shared/types.hpp"

struct Il2CppType;
struct ParameterInfo;
struct Il2CppClass;
struct FieldInfo;
struct MethodInfo;

namespace custom_types {
    class Register;
    class ClassWrapper;

    /// @brief An abstract type that holds the information required for an instance field.
    struct FieldRegistrator {
        virtual char const* name() const = 0;
        virtual Il2CppType const* type() const = 0;
        virtual size_t size() const = 0;
        virtual uint16_t fieldAttributes() const = 0;
        virtual int32_t offset() const = 0;
    };

    /// @brief An abstract type that holds the information required for a static field.
    struct StaticFieldRegistrator {
        virtual char const* name() const = 0;
        virtual Il2CppType const* type() const = 0;
        virtual size_t size() const = 0;
        virtual uint16_t fieldAttributes() const = 0;
        virtual int32_t offset() const = 0;
    };

    /// @brief An abstract type that holds the information required for a method.
    struct MethodRegistrator {
        virtual char const* name() const = 0;
        virtual char const* csharpName() const = 0;
        virtual int flags() const = 0;
        virtual MethodInfo const* virtualMethod() const = 0;
        virtual Il2CppType const* returnType() const = 0;
        virtual std::vector<Il2CppType const*> params() const = 0;
        virtual uint8_t params_size() const = 0;
        virtual Il2CppMethodPointer methodPointer() const = 0;
        virtual InvokerMethod invoker() const = 0;

        // This is an UNOWNED pointer, essentially, the pointer is owned entirely by TypeRegistrator and not by this instance.
        MethodInfo* info = nullptr;
        MethodInfo* get() {
            if (info) {
                return info;
            }
            info = new MethodInfo();
            info->flags = flags();
            info->name = csharpName();
            info->invoker_method = invoker();
            info->methodPointer = methodPointer();
            info->name = csharpName();
            info->return_type = returnType();
            info->slot = kInvalidIl2CppMethodSlot;
            auto ps = params();
            info->parameters_count = ps.size();
            auto* paramList = reinterpret_cast<Il2CppType const**>(calloc(ps.size(), sizeof(Il2CppType*)));
            for (uint8_t pi = 0; pi < info->parameters_count; pi++) {
                paramList[pi] = ps[pi];
            }
            info->parameters = paramList;
            return info;
        }
    };

    /// @brief An abstract type that holds all the information required to register a type.
    /// The general rationale here is that each of these types contains everything necessary for creating a custom type
    /// The custom type will get generated and then needs to be assigned properly
    struct CUSTOM_TYPES_EXPORT TypeRegistration {
        friend Register;

        virtual std::vector<FieldRegistrator*> const getFields() const = 0;
        virtual std::vector<StaticFieldRegistrator*> const getStaticFields() const = 0;
        virtual std::vector<MethodRegistrator*> const getMethods() const = 0;
        virtual char*& static_fields() = 0;
        virtual size_t static_fields_size() const = 0;

        virtual char const* name() const = 0;
        virtual char const* namespaze() const = 0;
        virtual char const* dllName() const = 0;
        virtual Il2CppClass* baseType() const = 0;
        virtual std::vector<Il2CppClass*> const interfaces() const = 0;
        virtual Il2CppTypeEnum typeEnum() const = 0;
        virtual uint32_t typeFlags() const = 0;
        virtual Il2CppClass*& klass() const = 0;
        virtual size_t size() const = 0;
        virtual TypeRegistration* customBase() const = 0;
        virtual bool initialized() const = 0;
        virtual void setInitialized() const = 0;

        uint16_t getVtableSize();
        Il2CppType* createType();
        void createClass();
        void populateFields();
        void populateMethods();
        bool
        checkVirtualsForMatch(MethodRegistrator* info, std::string_view namespaze, std::string_view name, std::string_view methodName, int paramCount);
        /// @brief Populates the vtable and offsets vectors with information from the base type's vtable.
        void getVtable(std::vector<VirtualInvokeData>& vtable, std::vector<Il2CppRuntimeInterfaceOffsetPair>& offsets);

        void clear();
    };

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
        static inline std::vector<Il2CppType const*> get() {
            std::vector<Il2CppType const*> params;
            auto& info = params.emplace_back();
            ::i2c::functions::initialize();
            Il2CppType const* type = ::i2c::type_of<P>();
            // Ignore name, it will be set when we iterate over all of them (param_1, param_2, etc.)
            // Ignore position, it will also be set on iteration.
            // TODO: Maybe some day we can actually use the parameters names themselves!
            if (type == nullptr) {
                custom_types::logger.warn("Failed to get type of parameter!");
            }
            info = type;
            for (auto const& q : parameter_converter<Decl, Ps...>::get()) {
                params.push_back(q);
            }
            return params;
        }
    };

    // 0 parameters
    template <typename Decl>
    struct parameter_converter<Decl> {
        static inline std::vector<Il2CppType const*> get() { return std::vector<Il2CppType const*>(); }
    };

    /// @struct Helper structure for unpacking and packing arguments/return types for invoker function creation
    struct arg_helper {
        template <typename Q>
        static inline Q unpack_arg(void* arg, type_tag<Q>) {
            if constexpr (std::is_pointer_v<Q>) {
                return reinterpret_cast<Q>(arg);
            } else if constexpr (::i2c::type_check::wrapper_ref_type<Q>) {
                return Q(arg);
            } else {
                return *reinterpret_cast<Q*>(arg);
            }
        }
        template <typename Q>
        static inline void* pack_result(Q&& thing) {
            if constexpr (std::is_pointer_v<Q>) {
                return reinterpret_cast<void*>(std::forward<Q>(thing));
            } else if constexpr (::i2c::type_check::wrapper_type<Q>) {
                return thing.convert();
            } else {
                // We SHOULD simply be able to grab the class and box our result
                // Once boxed, we should just be able to return without any issue
                // I DO wonder if our invoke functions miss registration with GC...
                auto* klass = ::i2c::class_of<Q>();
                if (!klass) {
                    custom_types::logger.critical("Failed to get non-null Il2CppClass* during invoke of custom function!");
                    return nullptr;
                }
                ::i2c::functions::initialize();
                return static_cast<void*>(::i2c::functions::value_box(klass, static_cast<void*>(&thing)));
            }
        }

        template <typename Q>
        static inline void pack_result_into(Q&& thing, void* retval) {
            if constexpr (std::is_pointer_v<Q>) {
                *static_cast<void**>(retval) = std::forward<Q>(thing);
            } else if constexpr (::i2c::type_check::wrapper_ref_type<Q>) {
                *static_cast<void**>(retval) = thing.convert();
            } else {
                auto* klass = ::i2c::class_of<Q>();
                if (!klass) {
                    custom_types::logger.critical("Failed to get non-null Il2CppClass* during invoke of custom function!");
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
        static void instance_invoke(function_ptr_t<TRet, T*, TArgs...> func, T* self, void** args, std::index_sequence<Ns...>, void* retval) {
            if constexpr (std::is_same_v<TRet, void>) {
                func(self, arg_helper::unpack_arg(args[Ns], type_tag<TArgs>{})...);
            } else {
                arg_helper::pack_result_into(func(self, arg_helper::unpack_arg(args[Ns], type_tag<TArgs>{})...), retval);
            }
        }
        [[gnu::noinline]] static void invoke(Il2CppMethodPointer ptr, [[maybe_unused]] MethodInfo const* m, void* obj, void** args, void* retval) {
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
        static void static_invoke(function_ptr_t<TRet, TArgs...> func, void** args, std::index_sequence<Ns...>, void* retval) {
            if constexpr (std::is_same_v<TRet, void>) {
                func(arg_helper::unpack_arg(args[Ns], type_tag<TArgs>{})...);
            } else {
                arg_helper::pack_result_into(func(arg_helper::unpack_arg(args[Ns], type_tag<TArgs>{})...), retval);
            }
        }
        template <std::size_t... Ns>
        static void static_invoke_method(
            function_ptr_t<TRet, TArgs..., MethodInfo const*> func, void** args, MethodInfo const* m, std::index_sequence<Ns...>, void* retval
        ) {
            if constexpr (std::is_same_v<TRet, void>) {
                func(arg_helper::unpack_arg(args[Ns], type_tag<TArgs>{})..., m);
            } else {
                arg_helper::pack_result_into(func(arg_helper::unpack_arg(args[Ns], type_tag<TArgs>{})..., m), retval);
            }
        }
        [[gnu::noinline]] static void
        invoke(Il2CppMethodPointer ptr, [[maybe_unused]] MethodInfo const* m, [[maybe_unused]] void* obj, void** args, void* retval) {
            // We also don't need to use anything from m so it is ignored.
            // Perhaps far in the future we will check attributes on it

            auto seq = std::make_index_sequence<sizeof...(TArgs)>();

            // post unity update delegates changed which use this invoke method
            // they get passed a nullptr ptr arg, so if they do we just take the method pointer from the method info instead!
            auto func = reinterpret_cast<function_ptr_t<TRet, TArgs...>>(ptr ? ptr : m->methodPointer);

            static_invoke(func, args, seq, retval);
        }
        [[gnu::noinline]] static void*
        invoke_method(Il2CppMethodPointer ptr, MethodInfo const* m, [[maybe_unused]] void* obj, void** args, void* retval) {
            auto func = reinterpret_cast<function_ptr_t<TRet, TArgs..., MethodInfo const*>>(ptr);
            auto seq = std::make_index_sequence<sizeof...(TArgs)>();
            static_invoke_method(func, args, m, seq, retval);
        }
    };
}
