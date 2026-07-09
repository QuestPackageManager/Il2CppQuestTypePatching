#pragma once

#include "beatsaber-hook/shared/api.hpp"

#include <cstddef>
#include <cstdint>

// After above
#include "utils/HashUtils.h"
#include "utils/Il2CppHashMap.h"
#include "utils/StringUtils.h"

struct Il2CppImageGlobalMetadata {
    TypeDefinitionIndex typeStart;
    TypeDefinitionIndex exportedTypeStart;
    CustomAttributeIndex customAttributeStart;
    MethodIndex entryPointIndex;
    Il2CppImage const* image;
};

struct NamespaceAndNamePairHash {
    size_t operator()(std::pair<char const*, char const*> const& pair) const {
        return il2cpp::utils::HashUtils::Combine(il2cpp::utils::StringUtils::Hash(pair.first), il2cpp::utils::StringUtils::Hash(pair.second));
    }
};

struct NamespaceAndNamePairEquals {
    bool operator()(std::pair<char const*, char const*> const& p1, std::pair<char const*, char const*> const& p2) const {
        return !strcmp(p1.first, p2.first) && !strcmp(p1.second, p2.second);
    }
};

struct Il2CppNameToTypeHandleHashTable :
    public Il2CppHashMap<std::pair<char const*, char const*>, Il2CppMetadataTypeHandle, NamespaceAndNamePairHash, NamespaceAndNamePairEquals> {
    Il2CppNameToTypeHandleHashTable() :
        Il2CppHashMap<std::pair<char const*, char const*>, Il2CppMetadataTypeHandle, NamespaceAndNamePairHash, NamespaceAndNamePairEquals>() {}
};
