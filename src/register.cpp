#include "register.hpp"

#include "extra-typedefs.hpp"
#include "logging.hpp"
#include "beatsaber-hook/shared/binary.hpp"
#include "beatsaber-hook/shared/capstone.hpp"
#include "beatsaber-hook/shared/debug.hpp"
#include "beatsaber-hook/shared/hooking.hpp"

#include <filesystem>

#ifdef CT_USE_GCDESCRIPTOR_DEBUG
#include "capstone-helpers.hpp"
#include "liveness-state-tracker.hpp"

#include <atomic>
#endif

bool disableLivenessChecks() {
    return std::getenv("CT_DISABLE_LIVENESS_CHECKS") != nullptr || std::getenv("ANDROID_EMULATOR") != nullptr;
}

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

#ifdef CT_USE_GCDESCRIPTOR_DEBUG

#ifdef LOCAL_TEST
#warning "Hey, you shouldn't use LOCAL_TEST while building with CT_USE_GCDESCRIPTOR_DEBUG!"
#endif

#include <cstdlib>

void* ct_malloc(std::size_t sz) {
    custom_types::logger.debug("Tracking an il2cpp malloc of: {}", sz);
    return malloc(sz);
}
void* ct_aligned_malloc(std::size_t sz, std::size_t alignment) {
    custom_types::logger.debug("Tracking an il2cpp aligned malloc: {}, {}", sz, alignment);
    return memalign(sz, alignment);
}
void ct_free(void* ptr) {
    custom_types::logger.debug("Tracking an il2cpp free: {}", fmt::ptr(ptr));
    free(ptr);
}
void ct_aligned_free(void* ptr) {
    custom_types::logger.debug("Tracking an il2cpp aligned free: {}", fmt::ptr(ptr));
    free(ptr);
}
void* ct_calloc(std::size_t nmeb, std::size_t sz) {
    custom_types::logger.debug("Tracking an il2cpp calloc: {}, {}", nmeb, sz);
    return calloc(nmeb, sz);
}
void* ct_realloc(void* ptr, std::size_t sz) {
    custom_types::logger.debug("Tracking an il2cpp realloc: {}, {}", fmt::ptr(ptr), sz);
    return realloc(ptr, sz);
}
void* ct_aligned_realloc(void* memory, std::size_t newSize, std::size_t alignment) {
    custom_types::logger.debug("Tracking an il2cpp aligned realloc: {}, {}, {}", fmt::ptr(memory), newSize, alignment);
    void* newMemory = realloc(memory, newSize);

    // Fast path: realloc returned aligned memory
    if ((reinterpret_cast<uintptr_t>(newMemory) & (alignment - 1)) == 0)
        return newMemory;

    // Slow path: realloc returned non-aligned memory
    void* alignedMemory = ct_aligned_malloc(newSize, alignment);
    memcpy(alignedMemory, newMemory, newSize);
    free(newMemory);
    return alignedMemory;
}

char const* namespaze(Il2CppClass* const klass) {
    return klass->namespaze ? klass->namespaze : "<NULL_NAMESPAZE>";
}
char const* namek(Il2CppClass* const klass) {
    return klass->name ? klass->name : "<NULL_NAME>";
}
std::string generics(Il2CppClass* const klass) {
    auto* genClass = klass->generic_class;
    if (genClass) {
        auto* genInst = genClass->context.class_inst;
        if (genInst) {
            std::string outp;
            for (size_t i = 0; i < genInst->type_argc; i++) {
                outp += std::string(" ") + i2c::type_simple_name(genInst->type_argv[i]);
            }
            return outp;
        }
    }
    return "NO GENERICS";
}

#define WORDSIZE ((int)sizeof(size_t) * 8)
#define GET_CLASS(obj) ((Il2CppClass*)(((size_t)(obj)->klass) & ~(size_t)1))
#define IS_MARKED(obj) (((size_t)(obj)->klass) & (size_t)1)

std::atomic_size_t generic_obj_traverse_count = 0;
std::atomic_size_t obj_traverse_count = 0;

MAKE_HOOK(LivenessState_TraverseGenericObject, (nullptr), void, Il2CppObject* obj, void* state) {
    if (disableLivenessChecks()) {
        // If we are disabling liveness checks, do not call the original function.
        return LivenessState_TraverseGenericObject(obj, state);
    }
    generic_obj_traverse_count.fetch_add(1, std::memory_order_relaxed);
    LivenessState_TraverseGenericObject(obj, state);
}

MAKE_HOOK(LivenessState_TraverseObjectInternal, (nullptr), bool, Il2CppObject* obj, bool isStruct, Il2CppClass* klass, void* state) {
    if (disableLivenessChecks()) {
        // If we are disabling liveness checks, do not call the original function.
        return LivenessState_TraverseObjectInternal(obj, isStruct, klass, state);
    }
    obj_traverse_count.fetch_add(1, std::memory_order_relaxed);
    return LivenessState_TraverseObjectInternal(obj, isStruct, klass, state);
}

namespace {

    constexpr std::size_t livenessStateCapacity = 32;
    custom_types::liveness_debug::LivenessStateTracker<livenessStateCapacity> livenessStates;
    std::atomic_flag loggedMissingLivenessState = ATOMIC_FLAG_INIT;

    bool isRegisteredCustomClass(Il2CppClass const* klass) {
        for (auto const* registeredClass : custom_types::Register::classes) {
            if (registeredClass == klass)
                return true;
        }
        return false;
    }

}  // namespace

MAKE_HOOK(
    UnityLivenessAllocateStruct,
    nullptr,
    void*,
    Il2CppClass* filter,
    int maxObjectCount,
    il2cpp_register_object_callback callback,
    void* userdata,
    il2cpp_liveness_reallocate_callback reallocate
) {
    auto* state = UnityLivenessAllocateStruct(filter, maxObjectCount, callback, userdata, reallocate);
    auto const result = livenessStates.track(state, filter);
    if (result == custom_types::liveness_debug::TrackResult::tracked) {
        custom_types::logger.debug(
            "Captured liveness state {}, filter {} ({}::{}), max object count {}",
            fmt::ptr(state),
            fmt::ptr(filter),
            filter ? namespaze(filter) : "<none>",
            filter ? namek(filter) : "<none>",
            maxObjectCount
        );
    } else if (result == custom_types::liveness_debug::TrackResult::full) {
        custom_types::logger.warn(
            "Liveness diagnostic state registry is full (capacity: {}); state {} will be forwarded without inspection",
            livenessStateCapacity,
            fmt::ptr(state)
        );
    } else if (result == custom_types::liveness_debug::TrackResult::alreadyTracked) {
        custom_types::logger.warn("Liveness diagnostic received duplicate allocation for state {}", fmt::ptr(state));
    }
    return state;
}

MAKE_HOOK(UnityLivenessFreeStruct, nullptr, void, void* state) {
    livenessStates.untrack(state);
    UnityLivenessFreeStruct(state);
}
static inline bool HasParentUnsafe(Il2CppClass const* klass, Il2CppClass const* parent) {
    return klass->typeHierarchyDepth >= parent->typeHierarchyDepth && klass->typeHierarchy[parent->typeHierarchyDepth - 1] == parent;
}

MAKE_HOOK(LivenessState_TraverseGCDescriptor, (nullptr), void, Il2CppObject* obj, void* state) {
    if (disableLivenessChecks()) {
        return LivenessState_TraverseGCDescriptor(obj, state);
    }

    auto const trackedState = livenessStates.find(state);
    if (!trackedState.found) {
        if (!loggedMissingLivenessState.test_and_set(std::memory_order_relaxed)) {
            custom_types::logger.warn(
                "No public-API filter capture exists for liveness state {}; forwarding without inspecting private LivenessState memory",
                fmt::ptr(state)
            );
        }
        return LivenessState_TraverseGCDescriptor(obj, state);
    }
    auto* filterClass = static_cast<Il2CppClass*>(trackedState.filter);

    // Note: gc_desc is a bitfield that holds which fields of the type has
    // references, or, if it is too large, a pointer to a table which holds a
    // larger bitfield.
    int i = 0;
    size_t mask = (size_t) (GET_CLASS(obj)->gc_desc);

    IL2CPP_ASSERT(mask & (size_t) 1);

    for (i = 0; i < WORDSIZE - 2; i++) {
        size_t offset = ((size_t) 1 << (WORDSIZE - 1 - i));
        if (mask & offset) {
            Il2CppObject** valptr = (Il2CppObject**) (((char*) obj) + i * sizeof(void*));
            Il2CppObject* val = *valptr;
            if (!val || IS_MARKED(val)) {
                // Null instances are permitted
                continue;
            }

            auto* const rawValueClass = val->klass;
            auto* const valueClass = GET_CLASS(val);
            char const* suspiciousReason = nullptr;
            if (!rawValueClass) {
                suspiciousReason = "referenced object has a null class pointer";
            } else if (valueClass->has_references == 0 && valueClass->klass != valueClass && valueClass->name == nullptr) {
                suspiciousReason = "referenced class metadata has an inconsistent shape";
            } else if (
                // If our filter class is not null, and
                // our filter class' type hierarchy depth is <= ours and
                // our type hierarchy pointer is garbage
                (filterClass && filterClass->typeHierarchyDepth <= GET_CLASS(val)->typeHierarchyDepth &&
                 (reinterpret_cast<uintptr_t>(GET_CLASS(val)->typeHierarchy) <= 0x1000 ||
                  (reinterpret_cast<uintptr_t>(GET_CLASS(val)->typeHierarchy) & 0x00FFFFFFFFFFFFFFULL) > 0xe000000000))
            ) {
                suspiciousReason = "referenced class has a suspicious type-hierarchy pointer";
            }

            if (suspiciousReason) {
                custom_types::logger.critical("CustomTypes detected suspicious metadata during IL2CPP liveness traversal");
                custom_types::logger.critical("reason: {}", suspiciousReason);
                custom_types::logger.critical(
                    "state: {}, captured filter: {} ({}::{}), object: {}, object class: {}, object type: {}::{}, gc_desc: {}",
                    fmt::ptr(state),
                    fmt::ptr(filterClass),
                    filterClass ? namespaze(filterClass) : "<none>",
                    filterClass ? namek(filterClass) : "<none>",
                    fmt::ptr(obj),
                    fmt::ptr(GET_CLASS(obj)),
                    namespaze(GET_CLASS(obj)),
                    namek(GET_CLASS(obj)),
                    fmt::ptr(GET_CLASS(obj)->gc_desc)
                );
                custom_types::logger.critical(
                    "field index: {}, field offset: {}, field address: {}, raw value: {}, raw value class: {}, registered CustomTypes class: {}",
                    i,
                    i * sizeof(void*),
                    fmt::ptr(valptr),
                    fmt::ptr(val),
                    fmt::ptr(rawValueClass),
                    isRegisteredCustomClass(valueClass)
                );
                custom_types::logger.critical(
                    "object traversals: {}, generic object traversals: {}",
                    obj_traverse_count.load(std::memory_order_relaxed),
                    generic_obj_traverse_count.load(std::memory_order_relaxed)
                );

                custom_types::logger.critical("KLASS PTR: {}", fmt::ptr(val->klass));

                if (filterClass) {
                    custom_types::logger.critical("Attempting HasParentUnsafe({}, {})...", fmt::ptr(GET_CLASS(val)), fmt::ptr(filterClass));
                    auto ret = HasParentUnsafe(GET_CLASS(val), filterClass);
                    custom_types::logger.critical("HasParentUnsafe return: {}", ret);
                }

                custom_types::logger.critical(
                    "No further class/type-hierarchy dereference will be attempted by CustomTypes; forwarding to Unity's original traversal"
                );
                // Give Paper a bounded opportunity to persist the report without
                // holding Unity's asset-GC thread indefinitely.
                Paper::ffi::paper2_wait_flush_timeout(250);
                return LivenessState_TraverseGCDescriptor(obj, state);
            }
        }
    }
    // Call orig
    LivenessState_TraverseGCDescriptor(obj, state);
#undef WORDSIZE
#undef GET_CLASS
#undef IS_MARKED
}

std::optional<uint32_t*> readsafeb(uint32_t const* const addr) {
    cs_insn* insns;
    // Read from addr, 1 instruction, with pc at addr, into insns.
    // TODO: consider using cs_disasm_iter
    auto count = cs_disasm(cs::get_handle(), reinterpret_cast<uint8_t const*>(addr), sizeof(uint32_t), reinterpret_cast<uint64_t>(addr), 1, &insns);
    RET_DEF_UNLESS(custom_types::logger, count == 1);
    auto inst = insns[0];
    // Thunks have a single b
    RET_DEF_UNLESS(custom_types::logger, inst.id == ARM64_INS_B);
    auto platinsn = inst.detail->arm64;
    RET_DEF_UNLESS(custom_types::logger, platinsn.op_count == 1);
    auto op = platinsn.operands[0];
    RET_DEF_UNLESS(custom_types::logger, op.type == ARM64_OP_IMM);
    // Our b dest is addr + (imm << 2), except capstone does this for us.
    auto dst = reinterpret_cast<uint32_t*>(op.imm);
    cs_free(insns, 1);
    return dst;
}
#endif

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
#define BREAK(var, ...)                  \
    do {                                 \
        if (!var) {                      \
            logger.warn(__VA_ARGS__); \
            goto exit;                   \
        }                                \
    } while (0)

            {
                // IL2CPP's exported liveness APIs are adjacent 16-byte branch
                // veneers. Inline-hooking those veneers directly would overwrite
                // neighboring exports. Resolve every veneer before installing any
                // hook, then hook the non-adjacent implementation functions.
                auto allocateLiveness = readsafeb(reinterpret_cast<uint32_t*>(il2cpp_functions::il2cpp_unity_liveness_allocate_struct));
                BREAK(allocateLiveness, "Failed to resolve il2cpp_unity_liveness_allocate_struct!");
                auto freeLiveness = readsafeb(reinterpret_cast<uint32_t*>(il2cpp_functions::il2cpp_unity_liveness_free_struct));
                BREAK(freeLiveness, "Failed to resolve il2cpp_unity_liveness_free_struct!");

                // We need to xref trace to get to LivenessState stuff
                // il2cpp_unity_liveness_calculation_from_root
                // only a b --> Liveness::FromRoot
                // 2nd bl --> LivenessState::TraverseObjects
                // 1st bl --> LivenessState::TraverseGenericObject
                // 2nd b --> LivenessState::TraverseGCDescriptor

                // If we fail anywhere in this chain, simply log it and move on.
                // We shouldn't care, this is just a debug hook after all.
                auto opt = readsafeb((uint32_t*) i2c::functions::unity_liveness_calculation_from_root);
                BREAK(opt, "Failed to find b in il2cpp_unity_liveness_calculation_from_root!");
                opt = cs::findNthBlSafe<2>(*opt);
                BREAK(opt, "Failed to find 2nd bl in Liveness::FromRoot!");
                opt = cs::findNthBlSafe<1>(*opt);
                BREAK(opt, "Failed to find 1st bl in Liveness::TraverseObject!");
                auto traverseGeneric = *opt;
                auto traverseInternal = cs::findNthBSafe<3>(*opt);
                opt = cs::findNthBSafe<2>(*opt);
                BREAK(opt, "Failed to find 2nd b in LivenessState::TraverseGenericObject!");
                BREAK(traverseInternal, "Failed to find 3rd b in LivenessState::TraverseGenericObject!");
                // Capture the filter at the stable public API boundary and install
                // the internal traversal diagnostics only after every address has
                // been resolved successfully.
                INSTALL_HOOK(logger, UnityLivenessAllocateStruct, *allocateLiveness);
                INSTALL_HOOK(logger, UnityLivenessFreeStruct, *freeLiveness);
                INSTALL_HOOK(logger, LivenessState_TraverseGenericObject, traverseGeneric);
                INSTALL_HOOK(logger, LivenessState_TraverseGCDescriptor, *opt);
                INSTALL_HOOK(logger, LivenessState_TraverseObjectInternal, *traverseInternal);
                // opt =
                // readsafeb((uint32_t*)i2c::functions::unity_liveness_calculation_from_statics);
                // BREAK(opt, "Failed to find b in
                // il2cpp_unity_liveness_calculation_from_statics!");
                // INSTALL_HOOK(logger, Liveness_FromStatics, *opt);
            }
#undef BREAK
        exit:
#endif
            installed = true;
        }
    }
}  // namespace custom_types
