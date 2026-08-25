// This file serves to provide debug information for liveness related issues.
// It is tied with register.cpp, who will install the exported hooks from this.

// This file is an il2cpp-private includer, so we should NEVER include bs-hook files here (sans logging).

#ifdef CT_USE_GCDESCRIPTOR_DEBUG

#ifdef LOCAL_TEST
#warning "Hey, you shouldn't use LOCAL_TEST while building with CT_USE_GCDESCRIPTOR_DEBUG!"
#endif

#include "liveness.hpp"

#include "logging.hpp"

#include <cstdlib>
#include <filesystem>

// The filter class offset is of the LivenessState structure, defined in: il2cpp/libil2cpp/vm/Liveness.cpp
// The issue, however, is that this is defined cleanly within a .cpp file. We CAN include this here, however.
// TODO: In the future, we should decide whether the overhead of emitting is actually worth it over copying the struct...
// WE NEED THE FOLLOWING DEFINES, AND THEY MUST BE BEFORE ANY BS-HOOK INCLUDE!
#define RUNTIME_IL2CPP 1
#define BASELIB_INLINE_NAMESPACE il2cpp_baselib
#pragma GCC visibility push(hidden)
#include "vm/Liveness.cpp"
#pragma GCC visibility pop

namespace {

    bool disableLivenessChecks() {
        return std::getenv("CT_DISABLE_LIVENESS_CHECKS") != nullptr || std::getenv("ANDROID_EMULATOR") != nullptr;
    }

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

}  // end anonymous namespace

#include "capstone-helpers.hpp"
#include "logging.hpp"
#include "beatsaber-hook/shared/api.hpp"
#include "beatsaber-hook/shared/capstone.hpp"
#include "beatsaber-hook/shared/hooking.hpp"

namespace {

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

    std::atomic_size_t generic_obj_traverse_count = 0;
    std::atomic_size_t obj_traverse_count = 0;

    MAKE_HOOK(LivenessState_TraverseGenericObject, (nullptr), void, Il2CppObject* obj, il2cpp::vm::LivenessState* state) {
        if (disableLivenessChecks()) {
            // If we are disabling liveness checks, just call the original function.
            return LivenessState_TraverseGenericObject(obj, state);
        }
        // We are calling this with an object and a state.
        // The state is the LivenessState instance
        // There is a process_array that we want to look at
        // Likewise we also want to look at our traverse_depth
        // Not to mention the actual object and state ptrs

        // auto klass = GET_CLASS(obj);
        // custom_types::logger.debug("{}:
        // LivenessState::TraverseGenericObject({}, {}), with klass: {} ({}::{})",
        // generic_obj_traverse_count++, fmt::ptr(obj), fmt::ptr(state), fmt::ptr(obj->klass), namespaze(klass),
        // namek(klass)); process_array is at 0x18 (custom_growable_array*)
        // traverse_depth is at 0x48 (int)
        // auto arrPtr =
        // *reinterpret_cast<il2cpp::utils::dynamic_array<Il2CppObject*>**>(reinterpret_cast<uint8_t*>(state)
        // + 0x18); custom_types::logger.debug("traverse_depth: {}",
        // *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(state) + 0x48));
        // custom_types::logger.debug("arrPtr: {}, inner: {}, size: {}", fmt::ptr(arrPtr),
        // fmt::ptr(arrPtr->data()), arrPtr->size()); custom_types::logger.flush(); for
        // (size_t i = 0; i < arrPtr->size(); i++) { 	auto inst = arrPtr->data()[i];
        // 	custom_types::logger.debug("arr val: {} ptr: {}, class: {}
        // ({}::{})", i, fmt::ptr(inst), fmt::ptr(inst->klass), namespaze(GET_CLASS(inst)),
        // namek(GET_CLASS(inst)));
        // }
        // if (arrPtr->size() > 0) {
        // 	auto inst = arrPtr->data()[arrPtr->size() - 1];
        // 	custom_types::logger.debug("arr val: {}, ptr: {}, class: {}
        // ({}::{})", arrPtr->size() - 1, fmt::ptr(inst), fmt::ptr(inst->klass),
        // namespaze(GET_CLASS(inst)), namek(GET_CLASS(inst)));
        // 	custom_types::logger.flush();
        // }
        generic_obj_traverse_count.fetch_add(1, std::memory_order_relaxed);
        LivenessState_TraverseGenericObject(obj, state);
        // custom_types::logger.debug("Complete
        // LivenessState::TraverseGenericObject");
    }

    MAKE_HOOK(
        LivenessState_TraverseObjectInternal, (nullptr), bool, Il2CppObject* obj, bool isStruct, Il2CppClass* klass, il2cpp::vm::LivenessState* state
    ) {
        if (disableLivenessChecks()) {
            // If we are disabling liveness checks, do not call the original function.
            return LivenessState_TraverseObjectInternal(obj, isStruct, klass, state);
        }
        // Here we are going to log... AGAIN
        // but this time only a few things
        // custom_types::logger.debug("LivenessState::TraverseObjectInternal({},
        // {}, {}, {})", fmt::ptr(obj), isStruct, fmt::ptr(klass), fmt::ptr(state));
        // custom_types::logger.flush();
        // custom_types::logger.debug("class: ({}::{})", namespaze(klass),
        // namek(klass)); custom_types::logger.flush();
        obj_traverse_count.fetch_add(1, std::memory_order_relaxed);
        auto ret = LivenessState_TraverseObjectInternal(obj, isStruct, klass, state);
        // custom_types::logger.debug("Complete
        // LivenessState::TraverseObjectInternal");
        return ret;
    }

    // MAKE_HOOK(Liveness_FromStatics, nullptr, void, void* state) {
    // 	auto filter = state->filter);
    // 	custom_types::logger.debug("Liveness::FromStatics({})", fmt::ptr(state));
    // 	custom_types::logger.debug("filter class: {}", fmt::ptr(filter));
    // 	custom_types::logger.flush();
    // 	custom_types::logger.debug("filter class: {}::{}", namespaze(filter),
    // namek(filter)); 	custom_types::logger.flush();
    // 	// TODO: Log class statics info
    // 	Liveness_FromStatics(state);
    // 	custom_types::logger.debug("Complete Liveness::FromStatics");
    // }

    static inline bool HasParentUnsafe(Il2CppClass const* klass, Il2CppClass const* parent) {
        return klass->typeHierarchyDepth >= parent->typeHierarchyDepth && klass->typeHierarchy[parent->typeHierarchyDepth - 1] == parent;
    }

    MAKE_HOOK(LivenessState_TraverseGCDescriptor, (nullptr), void, Il2CppObject* obj, il2cpp::vm::LivenessState* state) {
        if (disableLivenessChecks()) {
            return LivenessState_TraverseGCDescriptor(obj, state);
        }

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
                // We aren't marked at this point, val is a new pointer in our instance
                // PreviewDifficultyBeatmapSet
                // mask: 0b0011000...1
                // f1: BeatmapCharacteristicSO (skipped explicitly in if above)
                // f2: BeatmapDifficulty[] (crashes below)
                // val is: pointer to BeatmapDifficulty[] (at least, it SHOULD be)
                // specifically, val is a pointer to: 39D0 before obj
                // Offset of filter class
                auto filterClass = state->filter;
                if (!val->klass ||
                    (GET_CLASS(val)->has_references == 0 && GET_CLASS(val)->klass != GET_CLASS(val) && GET_CLASS(val)->name == nullptr) ||
                    // If our filter class is not null, and
                    // our filter class' type hierarchy depth is <= ours and
                    // our type hierarchy pointer is garbage
                    (filterClass && filterClass->typeHierarchyDepth <= GET_CLASS(val)->typeHierarchyDepth &&
                     (reinterpret_cast<uintptr_t>(GET_CLASS(val)->typeHierarchy) <= 0x1000 ||
                      (reinterpret_cast<uintptr_t>(GET_CLASS(val)->typeHierarchy) & 0x00FFFFFFFFFFFFFFULL) > 0xe000000000))) {
                    // We have a VERY BIG PROBLEM!
                    // This will cause a (hard to diagnose) crash!
                    // So, we will dump as much info as we can.
                    custom_types::logger.critical("WARNING! THIS WILL CRASH, DUMPING SEMANTIC INFORMATION...");
                    custom_types::logger.critical(
                        "LivenessState::TraverseGCDescriptor({}, {}) class: {}, gc_desc: "
                        "{}, {}::{} {}",
                        fmt::ptr(obj),
                        fmt::ptr(state),
                        fmt::ptr(GET_CLASS(obj)),
                        fmt::ptr(GET_CLASS(obj)->gc_desc),
                        namespaze(GET_CLASS(obj)),
                        namek(GET_CLASS(obj)),
                        generics(GET_CLASS(obj)).c_str()
                    );
                    // malloc_info()
                    // TODO: Yeah
                    std::filesystem::path path = "/sdcard/ModData/com.beatgames.beatsaber/Mods/CustomTypes/CustomTypesMallocInfoOnExit.xml";
                    if (!std::filesystem::exists(path.parent_path()))
                        std::filesystem::create_directories(path.parent_path());
                    auto f = fopen(path.c_str(), "w");
                    if (!malloc_info(0, f)) {
                        custom_types::logger.critical("Failed to write to: {}!", path.c_str());
                    } else {
                        custom_types::logger.debug("Wrote malloc info to: {}", path.c_str());
                    }
                    fclose(f);
                    custom_types::logger.critical(
                        "LivenessState::TraverseGCDescriptor({}, {}), with val: {} (klass: "
                        "{}), idx: {}",
                        fmt::ptr(obj),
                        fmt::ptr(state),
                        fmt::ptr(val),
                        fmt::ptr(val->klass),
                        i
                    );
                    if (GET_CLASS(val)) {
                        custom_types::logger.critical("has_references: {}", (bool) GET_CLASS(val)->has_references);
                    }
                    // custom_types::logger.critical("Logging filterClass: {}",
                    // fmt::ptr(filterClass)); custom_types::logAll(filterClass);
                    // custom_types::logger.critical("Logging all registered custom
                    // types..."); for (auto k : custom_types::Register::classes) {
                    // 	custom_types::logger.critical("KLASS PTR: {}", fmt::ptr(k));
                    // 	custom_types::logAll(k);
                    // }

                    custom_types::logger.debug(
                        "gc descriptor test field: obj: {}, "
                        "field: {}, value: {}, class: {}",
                        fmt::ptr(obj),
                        fmt::ptr(valptr),
                        fmt::ptr(val),
                        fmt::ptr(val ? val->klass : nullptr)
                    );
                    custom_types::logger.critical("obj_traverse_count: {}", obj_traverse_count.load(std::memory_order_relaxed));
                    custom_types::logger.critical("generic_obj_traverse_count: {}", generic_obj_traverse_count.load(std::memory_order_relaxed));

                    custom_types::logger.critical(
                        "Talk to Sc2ad to try and understand what the hell is going on "
                        "here and why."
                    );
                    custom_types::logger.critical(
                        "Also, please be very kind and send him this whole log file! It "
                        "would be much appreciated."
                    );
                    custom_types::logger.critical(
                        "With that said, the log in this file may have been truncated, so "
                        "consider grabbing the file log for custom types instead."
                    );
                    custom_types::logger.critical(
                        "custom types will now try to log as much information it can about "
                        "the offending instance's class before crashing..."
                    );
                    custom_types::logger.debug("Capturing memory snapshot...");
                    // auto snapshot = il2cpp_functions::capture_memory_snapshot();
                    // auto snapshot_path = string_format(LOG_PATH,
                    // "com.beatgames.beatsaber") + "MemoryDump.bin"; std::ofstream
                    // memory_snapshot(snapshot_path, std::ios::binary);
                    // memory_snapshot.write(reinterpret_cast<char*>(snapshot),
                    // sizeof(*snapshot)); memory_snapshot.close();
                    // il2cpp_functions::free_captured_memory_snapshot(snapshot);
                    // custom_types::logger.debug("Logging memory dump to {}",
                    // snapshot_path.c_str());
                    custom_types::logger.critical("KLASS PTR: {}", fmt::ptr(obj->klass));
                    if (GET_CLASS(obj)) {
                        custom_types::logAll(GET_CLASS(obj));
                    }
                    custom_types::logger.critical("KLASS PTR: {}", fmt::ptr(val->klass));
                    if (filterClass) {
                        custom_types::logger.critical("Attempting HasParentUnsafe({}, {})...", fmt::ptr(GET_CLASS(val)), fmt::ptr(filterClass));
                        auto ret = HasParentUnsafe(GET_CLASS(val), filterClass);
                        custom_types::logger.critical("HasParentUnsafe return: {}", ret);
                    }
                    if (GET_CLASS(val)) {
                        custom_types::logAll(GET_CLASS(val));
                    }
                    // Things I have learned, just dumping here:
                    // static fields and classes that have a nonzero quantity of static
                    // fields need to be added to: Class::GetStaticFieldData() as for
                    // Liveness::FromRoot, it MIGHT NOT be the base, because the recursion
                    // layer is not bt-able due to b's instead of bl's So, what COULD happen
                    // is that the root liveness calc DOES NOT have a gc descriptor, and it
                    // is only a type later on that does. Perhaps we should hook the
                    // TraverseGenericObject function instead and see what we can learn as
                    // we walk the root? Seems to be the first field of a dictionary that
                    // has this issue Also the 0x60 field of a Regex::Match type, which is
                    // an array: [FieldOffset(Offset = "0x60")] [Token(Token =
                    // "0x040002A5")] internal int[] _matchcount;

                    // Have we damaged an array creation method on accident?
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
        auto count =
            cs_disasm(cs::get_handle(), reinterpret_cast<uint8_t const*>(addr), sizeof(uint32_t), reinterpret_cast<uint64_t>(addr), 1, &insns);
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

}  // end anonymous namespace

void liveness::EnsureHooks() {
    using namespace custom_types;
    // Install memory callbacks
    Il2CppMemoryCallbacks callbacks{
        .malloc_func = ct_malloc,
        .aligned_malloc_func = ct_aligned_malloc,
        .free_func = ct_free,
        .aligned_free_func = ct_aligned_free,
        .calloc_func = ct_calloc,
        .realloc_func = ct_realloc,
        .aligned_realloc_func = ct_aligned_realloc,
    };
    // il2cpp_functions::set_memory_callbacks(&callbacks);
#define BREAK(var, ...)               \
    do {                              \
        if (!var) {                   \
            logger.warn(__VA_ARGS__); \
            goto exit;                \
        }                             \
    } while (0)
    {
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
        // We found all of the chain, lets install our debug hook!
        INSTALL_HOOK(logger, LivenessState_TraverseGenericObject, traverseGeneric);
        INSTALL_HOOK(logger, LivenessState_TraverseGCDescriptor, *opt);
        INSTALL_HOOK(logger, LivenessState_TraverseObjectInternal, *traverseInternal);
        // opt =
        // readsafeb((uint32_t*)i2c::il2cpp_unity_liveness_calculation_from_statics);
        // BREAK(opt, "Failed to find b in
        // il2cpp_unity_liveness_calculation_from_statics!");
        // INSTALL_HOOK(logger, Liveness_FromStatics, *opt);
    }
#undef BREAK
exit:
    return;
}

#endif
