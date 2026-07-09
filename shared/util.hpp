#pragma once

#include "_config.h"
#include "beatsaber-hook/shared/api.hpp"
#include "beatsaber-hook/shared/exceptions.hpp"
#include "beatsaber-hook/shared/types.hpp"

namespace custom_types {
    template <class F>
    struct Decomposer;

    template <class R, class T, class... TArgs>
    struct Decomposer<R (T::*)(TArgs...)> {
        template <class... TArgs2>
        static constexpr bool equal() {
            if constexpr (sizeof...(TArgs) != sizeof...(TArgs2)) {
                return false;
            } else {
                return (std::is_same_v<TArgs, TArgs2> && ...);
            }
        }
        template <class... TArgs2>
        static constexpr bool convertible() {
            if constexpr (sizeof...(TArgs) != sizeof...(TArgs2)) {
                return false;
            } else {
                // Convertible from the args specified TO the function pointer provided
                return (std::is_convertible_v<TArgs2, TArgs> && ...);
            }
        }
    };

    struct CUSTOM_TYPES_EXPORT NullAccessException : ::i2c::trace_exception {
        NullAccessException() : ::i2c::trace_exception("Null instance access on a custom type field!") {}
    };

    template <typename T>
    Il2CppClass* inited_class_of() {
        ::i2c::functions::initialize();
        auto klass = ::i2c::class_of<T>();
        if (!klass->initialized)
            ::i2c::functions::Class_Init(klass);
        return klass;
    }

    template <typename... Ts>
    std::vector<Il2CppClass*> ExtractClasses() {
        return {::i2c::class_of<Ts>()...};
    }

    template <typename T>
    struct field_accessor {
        constexpr T const* field_addr(void const* instance, std::size_t offset) const noexcept {
            return static_cast<T const*>(static_cast<void const*>(static_cast<uint8_t const*>(instance) + offset));
        }

        constexpr T* field_addr(void* instance, std::size_t offset) const noexcept {
            return static_cast<T*>(static_cast<void*>(static_cast<uint8_t*>(instance) + offset));
        }

        constexpr T& read(void* instance, std::size_t offset) const noexcept { return *field_addr(instance, offset); }

        constexpr T const& read(void const* instance, std::size_t offset) const noexcept { return *field_addr(instance, offset); }

        constexpr void write(void* instance, std::size_t offset, T&& v) const noexcept { *field_addr(instance, offset) = v; }
    };

    template <::i2c::type_check::ref_type T>
    struct field_accessor<T> {
        constexpr void* field_addr(void* instance, std::size_t offset) const noexcept {
            return static_cast<void*>(static_cast<uint8_t*>(instance) + offset);
        }

        constexpr void const* field_addr(void const* instance, std::size_t offset) const noexcept {
            return static_cast<void const*>(static_cast<uint8_t const*>(instance) + offset);
        }

        constexpr T& read(void* instance, std::size_t offset) const noexcept { return *static_cast<T*>(field_addr(instance, offset)); }

        constexpr T const& read(void const* instance, std::size_t offset) const noexcept {
            return *static_cast<T const*>(field_addr(instance, offset));
        }

        void write(void* instance, std::size_t offset, T&& v) const noexcept {
            ::i2c::functions::initialize();
            ::i2c::functions::gc_wbarrier_set_field(
                static_cast<Il2CppObject*>(instance), static_cast<void**>(field_addr(instance, offset)), ::i2c::to_object<false>(v)
            );
        }
    };
}
