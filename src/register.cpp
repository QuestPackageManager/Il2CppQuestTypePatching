#include "register.hpp"

#include "extra-typedefs.hpp"
#include "logging.hpp"
#include "beatsaber-hook/shared/binary.hpp"
#include "beatsaber-hook/shared/capstone.hpp"
#include "beatsaber-hook/shared/debug.hpp"
#include "beatsaber-hook/shared/hooking.hpp"

#include <filesystem>

#ifdef CT_USE_GCDESCRIPTOR_DEBUG
#include "liveness.hpp"
#endif

namespace {

// checks whether the ty->data could be a pointer. technically could be UB if the address is low enough
bool MetadataHandleSet(Il2CppType const* ty) {
    return ((uint64_t) ty->data.typeHandle >> 32);
}

template <class... TArgs>
struct Hook_FromIl2CppTypeMain {
    using func_t = Il2CppClass* (*) (TArgs...);
    __INTERNAL_HOOK_STRUCT(FromIl2CppType, static_cast<void*>(nullptr), Il2CppClass*, TArgs...)
};
template <class... TArgs>
Il2CppClass* Hook_FromIl2CppTypeMain<TArgs...>::hook_m_FromIl2CppType(TArgs... args) {
    Il2CppType const* typ = std::get<0>(std::make_tuple(args...));
    // custom_types::logger.debug("FromIl2CppType: {}", fmt::ptr(typ));
    if (typ == nullptr) {
        // Extra error checking to avoid unknown null derefs.
        custom_types::logger.warn("FromIl2CppType was given a null Il2CppType*! Returning a null!");
        return nullptr;
    }
    // preliminary check, if the metadata handle is not set this could be ours
    bool shouldBeOurs = !MetadataHandleSet(typ);
    // klassIndex is only meaningful for these types
    if (shouldBeOurs && (typ->type == IL2CPP_TYPE_CLASS || typ->type == IL2CPP_TYPE_VALUETYPE) && typ->data.__klassIndex < 0) {
        shouldBeOurs = true;
        // If the type matches our type
        size_t idx = kTypeDefinitionIndexInvalid - typ->data.__klassIndex;
#ifndef NO_VERBOSE_LOGS
        custom_types::logger.debug("Custom idx: {} for type: {}", idx, fmt::ptr(typ));
#endif
        if (idx < custom_types::Register::classes.size() && idx >= 0) {
#ifndef NO_VERBOSE_LOGS
            custom_types::logger.debug("Returning custom class with idx {}!", idx);
#endif
            auto* k = custom_types::Register::classes[idx];
            if (k == nullptr) {
                custom_types::logger.warn("Class at idx: {} is null!", idx);
            }
            return k;
        }
    }
    // Otherwise, return orig
    auto klass = FromIl2CppType(args...);
    if (shouldBeOurs) {
        custom_types::logger.debug("Called with klassIndex {} which is not our custom type?!", typ->data.__klassIndex);
        i2c::log_class(custom_types::logger, klass, false);
    }
    return klass;
}

MAKE_HOOK(Type_GetClassOrElementClass, (nullptr), Il2CppClass*, Il2CppType* type) {
    if (type->type == IL2CPP_TYPE_ARRAY) {
        return i2c::functions::Class_FromIl2CppType(const_cast<Il2CppType*>(type->data.array->etype));
    }
    if (type->type == IL2CPP_TYPE_SZARRAY) {
        return i2c::functions::Class_FromIl2CppType(const_cast<Il2CppType*>(type->data.type));
    }
    // this is what il2cpp does here normally, but we want to redirect through our checked call of FromIl2CppType instead...
    // return MetadataCache::GetTypeInfoFromType(type);
    return i2c::functions::Class_FromIl2CppType(type);
}

MAKE_HOOK(Class_Init, (nullptr), bool, Il2CppClass* klass) {
    // If we are attempting to call Class::Init() on an Il2CppClass* that is a
    // custom Il2CppClass*, we need to ignore.
    if (!klass) {
        // We will provide some useful debug info here
        custom_types::logger.warn("Called with a null Il2CppClass*! (Specifically: {})", fmt::ptr(klass));
        SAFE_ABORT();
    }

    auto typ = klass->this_arg;
    if (!MetadataHandleSet(&typ) && (typ.type == IL2CPP_TYPE_CLASS || typ.type == IL2CPP_TYPE_VALUETYPE) && typ.data.__klassIndex < 0) {
        auto idx = kTypeDefinitionIndexInvalid - typ.data.__klassIndex;
        if (idx < (int) custom_types::Register::classes.size() && idx >= 0) {
            // This is a custom class. Skip it.
#ifndef NO_VERBOSE_LOGS
            logger.debug("custom idx: {}", idx);
#endif
            return true;
        }
    }

    return Class_Init(klass);
}

// TODO: bitwise maybe
union CastHelper {
    TypeDefinitionIndex index;
    Il2CppMetadataTypeHandle handle;
};

MAKE_HOOK(GlobalMetadata_GetTypeInfoFromHandle, (nullptr), Il2CppClass*, Il2CppMetadataTypeHandle handle) {
    if ((bool) ((uint64_t) handle >> 32)) {
        return GlobalMetadata_GetTypeInfoFromHandle(handle);
    }
    CastHelper caster;
    caster.handle = handle;
    auto index = caster.index;
    if (index < 0) {
        // index is either invalid or one of ours
        size_t idx = kTypeDefinitionIndexInvalid - index;
        custom_types::logger.debug("custom idx: {}", idx);
        if (idx < custom_types::Register::classes.size() && idx >= 0) {
            custom_types::logger.debug("Returning custom class with idx {}!", idx);
            auto* k = custom_types::Register::classes[idx];
            return k;
        }
    }
    custom_types::logger.warn("Calling orig with likely broken type handle!");
    return GlobalMetadata_GetTypeInfoFromHandle(handle);
}

MAKE_HOOK(GlobalMetadata_GetTypeInfoFromTypeDefinitionIndex, (nullptr), Il2CppClass*, TypeDefinitionIndex index) {
    if (index < 0) {
        // index is either invalid or one of ours
        size_t idx = kTypeDefinitionIndexInvalid - index;
        custom_types::logger.debug("custom idx: {}", idx);
        if (idx < custom_types::Register::classes.size() && idx >= 0) {
            custom_types::logger.debug("Returning custom class with idx {}!", idx);
            auto* k = custom_types::Register::classes[idx];
            return k;
        }
    }
    // Otherwise, return orig
    return GlobalMetadata_GetTypeInfoFromTypeDefinitionIndex(index);
}

MAKE_HOOK(GetScriptingClass, (nullptr), Il2CppClass*, void* thisptr, char* assembly, char* namespaze, char* name) {
    auto ret = GetScriptingClass(thisptr, assembly, namespaze, name);
    if (!ret) {
        for (auto clazz : custom_types::Register::classes) {
            if (strcmp(clazz->namespaze, namespaze) == 0 && strcmp(clazz->name, name) == 0) {
                custom_types::logger.debug("Found class: {}, {}", namespaze, name);
                return clazz;
            }
        }
    }
    return ret;
}

} // end anonymous namespace

// NOTE THAT THIS HOOK DOES NOT PERMIT TYPES OF IDENTICAL NAMESPACE AND NAME BUT
// IN DIFFERENT IMAGES! This could be worked around if we also have image be a
// part of the key, but as it stands, that is not necessary.
// MAKE_HOOK(Class_FromName, nullptr, Il2CppClass*, Il2CppImage* image, const
// char* namespaze, const char* name) {
//     pair = std::make_pair(std::string(namespaze), std::string(name)); auto
//     itr = custom_types::Register::classMapping.find(pair); if (itr !=
//     custom_types::Register::classMapping.end()) {
//         #ifndef NO_VERBOSE_LOGS
//         custom_types::logger.debug("Returning custom class from: {}::{} lookup: {}",
//         namespaze, name, fmt::ptr(itr->second)); #endif return itr->second;
//     }
//     return Class_FromName(image, namespaze, name);
// }

namespace custom_types {
    std::unordered_map<std::string, Il2CppAssembly*> Register::assembs;
    std::unordered_map<std::string, Il2CppImage*> Register::images;
    std::unordered_map<std::pair<std::string, std::string>, Il2CppClass*> Register::classMapping;
    std::shared_mutex Register::assemblyMtx;
    std::shared_mutex Register::imageMtx;
    std::mutex Register::classMappingMtx;
    std::mutex Register::registrationMtx;
    std::mutex installationMtx;
    bool Register::installed = false;
    std::vector<Il2CppClass*> Register::classes;
    std::vector<TypeRegistration*> Register::toRegister;
    std::vector<TypeRegistration*> Register::registeredTypes;

    Il2CppAssembly* Register::createAssembly(std::string_view name, Il2CppImage* img) {
        // Name is NOT copied, so should be a constant string
        // Check to see if an assembly with the given name already exists.
        // If it does, use that instead.
        std::string strName(name);
        {
            std::shared_lock lock(assemblyMtx);
            auto itr = assembs.find(strName);
            if (itr != assembs.end()) {
                return itr->second;
            }
        }
        auto assemb = new Il2CppAssembly();
        assemb->image = img;
        img->assembly = assemb;
        assemb->aname.name = name.data();
        {
            std::unique_lock lock(assemblyMtx);
            // Add our new assembly to the collection of all known assemblies
            i2c::functions::Assembly_GetAllAssemblies()->push_back(assemb);
            custom_types::logger.debug("Added new assembly image: {}", fmt::ptr(assemb->image));
            assembs.insert({strName, assemb});
        }
        custom_types::logger.debug("Created new assembly: {}, {}", name, fmt::ptr(assemb));
        return assemb;
    }

    Il2CppImage const* Register::createImage(std::string_view name) {
        // Name is NOT copied, so should be a constant string
        // Check to see if an image with the given name already exists.
        // If it does, use that instead.
        std::string strName(name);
        {
            std::shared_lock lock(imageMtx);
            auto itr = images.find(strName);
            if (itr != images.end()) {
                return itr->second;
            }
        }
        auto img = new Il2CppImage();
        std::unique_lock lock(imageMtx);
        auto res = images.insert({strName, img});
        lock.unlock();
        img->name = res.first->first.c_str();
        auto strToCopy = strName.substr(0, strName.find_last_of('.'));
        auto* allocNameNoExt = new char[strToCopy.size() + 1];
        memcpy(allocNameNoExt, strToCopy.c_str(), strToCopy.size());
        allocNameNoExt[strToCopy.size()] = '\0';
        img->nameNoExt = allocNameNoExt;
        img->dynamic = true;
        img->assembly = createAssembly(allocNameNoExt, img);
        img->nameToClassHashTable = new Il2CppNameToTypeHandleHashTable();
        auto metadata = new Il2CppImageGlobalMetadata();
        metadata->image = img;
        img->metadataHandle = reinterpret_cast<Il2CppMetadataImageHandle>(metadata);
        // Types are pushed here on class creation
        // TODO: Avoid copying eventually
        metadata->typeStart = 0;
        metadata->exportedTypeStart = 0;
        img->exportedTypeCount = 0;
        // Custom attribute start and count is used somewhere within unity
        // (which makes a call to:
        // il2cpp_custom_attrs_from_class/il2cpp_custom_attrs_from_method) These are
        // required to not be undefined (though perhaps a -1 and a 0 would work just
        // as well here?) RGCTXes are also from codeGenModule, so that must also be
        // defined.
        metadata->customAttributeStart = 0;
        img->customAttributeCount = 0;
        metadata->entryPointIndex = 0;
        // TODO: Populate this in a more reasonable way
        // auto* codegen = new Il2CppCodeGenModule{Il2CppCodeGenModule{
        //     .moduleName = name.data(),
        //     .methodPointerCount = 0,
        //     .reversePInvokeWrapperCount = 0,
        //     .rgctxRangesCount = 0,
        //     .rgctxsCount = 0
        // }};
        // img->codeGenModule = codegen;
        // TOOD: We shall leave the others undefined for now.
        custom_types::logger.debug("Created new image: {}, {}", name, fmt::ptr(img));
        return img;
    }

    void Register::EnsureHooks() {
        std::lock_guard lock(installationMtx);
        if (!installed) {
            i2c::functions::initialize();
            Paper::Logger::RegisterFileContextId(custom_types::logger.tag);

            logger.debug("Installing FromIl2CppType hook...");
            if constexpr (sizeof(Il2CppCodeGenModule) < 104) {
                i2c::install_hook<Hook_FromIl2CppTypeMain<Il2CppType*>>(logger, (void*) i2c::functions::Class_FromIl2CppType);
            } else {
                i2c::install_hook<Hook_FromIl2CppTypeMain<Il2CppType*, bool>>(logger, (void*) i2c::functions::Class_FromIl2CppType);
            }
            INSTALL_HOOK(logger, GlobalMetadata_GetTypeInfoFromHandle, (void*) i2c::functions::GlobalMetadata_GetTypeInfoFromHandle);
            INSTALL_HOOK(
                logger, GlobalMetadata_GetTypeInfoFromTypeDefinitionIndex, (void*) i2c::functions::GlobalMetadata_GetTypeInfoFromTypeDefinitionIndex
            );
            INSTALL_HOOK(logger, Class_Init, (void*) i2c::functions::Class_Init);

            bool multiple;
            uintptr_t GetScriptingClassAddr = i2c::binary::libunity_unique_pattern(
                multiple,
                "ff 43 02 d1 fe 23 00 f9 fa 67 05 a9 f8 5f 06 a9 f6 57 07 a9 f4 4f 08 a9 57 d0 3b d5 ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 21 ?? ?? 91 e0 03 16 aa"
            );
            INSTALL_HOOK(logger, GetScriptingClass, reinterpret_cast<void*>(GetScriptingClassAddr));

            // get the location of Type::GetClassOrElementClass
            uint32_t* get_class_or_element_class_addr = (uint32_t*) i2c::functions::type_get_class_or_element_class;
            auto opt = cs::find_nth_b<1>(get_class_or_element_class_addr);
            if (opt) {
                INSTALL_HOOK(logger, Type_GetClassOrElementClass, reinterpret_cast<void*>(opt.value()));
            } else {
                logger.warn("Failed to find 1st bl in il2cpp_type_get_class_or_element_class!");
            }
            // {
            //     // We need to do a tiny bit of xref tracing to find the bottom level
            //     Class::FromName call
            //     // Trace is: il2cpp_class_from_name --> b --> b --> result
            //     INSTALL_HOOK(logger, Class_FromName,
            //     (void*)cs::findNthB<1>(reinterpret_cast<const
            //     uint32_t*>(i2c::functions::class_from_name)));
            // }

#ifdef CT_USE_GCDESCRIPTOR_DEBUG
            liveness::EnsureHooks();
#endif
            installed = true;
        }
    }
}  // namespace custom_types
