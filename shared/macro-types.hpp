#pragma once

#include "beatsaber-hook/shared/api.hpp"

namespace custom_types {
    template <typename T, typename R, Il2CppTypeEnum typeEnum_, size_t baseSize>
    struct TypeWrapperParent {
        using ___TargetType = T;
        using ___TypeRegistration = R;
        constexpr static auto ___Base__Size = baseSize;
        uint8_t _baseFields[baseSize];
        TypeWrapperParent(TypeWrapperParent&&) = delete;
        TypeWrapperParent(TypeWrapperParent const&) = delete;

       protected:
        TypeWrapperParent() {};
    };

    template <typename T, typename R, Il2CppTypeEnum typeEnum_, typename baseT>
    struct TypeWrapperInheritanceParent : public baseT {
        using ___TargetType = T;
        using ___TypeRegistration = R;
        constexpr static auto ___Base__Size = sizeof(baseT);
        TypeWrapperInheritanceParent(TypeWrapperInheritanceParent&&) = delete;
        TypeWrapperInheritanceParent(TypeWrapperInheritanceParent const&) = delete;

       protected:
        TypeWrapperInheritanceParent() {};
    };
}
