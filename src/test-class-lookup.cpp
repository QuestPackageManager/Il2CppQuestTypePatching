#ifdef LOCAL_TEST
#include "extra-typedefs.hpp"
#include "register.hpp"

#include <atomic>
#include <memory>
#include <thread>

namespace {
    // Runtime names let the test grow both the registry and an existing image's table.
    struct LookupRegistration final : custom_types::TypeRegistration {
        std::string typeName;
        char const* imageName;
        mutable Il2CppClass* type = nullptr;
        mutable bool ready = false;
        char* fields = nullptr;

        LookupRegistration(std::string name, char const* image) : typeName(std::move(name)), imageName(image) {}
        std::vector<custom_types::FieldRegistrator*> const getFields() const override { return {}; }
        std::vector<custom_types::StaticFieldRegistrator*> const getStaticFields() const override { return {}; }
        std::vector<custom_types::MethodRegistrator*> const getMethods() const override { return {}; }
        char*& static_fields() override { return fields; }
        size_t static_fields_size() const override { return 0; }
        char const* name() const override { return typeName.c_str(); }
        char const* namespaze() const override { return "LookupTests"; }
        char const* dllName() const override { return imageName; }
        Il2CppClass* baseType() const override { return i2c::class_of<Il2CppObject*>(); }
        std::vector<Il2CppClass*> const interfaces() const override { return {}; }
        Il2CppTypeEnum typeEnum() const override { return IL2CPP_TYPE_CLASS; }
        uint32_t typeFlags() const override { return 0; }
        Il2CppClass*& klass() const override { return type; }
        size_t size() const override { return sizeof(Il2CppObject); }
        custom_types::TypeRegistration* customBase() const override { return nullptr; }
        bool initialized() const override { return ready; }
        void setInitialized() const override { ready = true; }
    };

    Il2CppClass* addType(std::string name, char const* image) {
        // TypeRegistration and its names must outlive the registered class.
        static std::vector<std::unique_ptr<LookupRegistration>> registrations;
        auto& registration = registrations.emplace_back(std::make_unique<LookupRegistration>(std::move(name), image));
        custom_types::Register::ExplicitRegister({registration.get()});
        return registration->klass();
    }

    void checkLookup(Il2CppClass* expected) {
        if (i2c::functions::class_from_name(expected->image, expected->namespaze, expected->name) != expected) {
            SAFE_ABORT("Class lookup returned the wrong class: {}::{} in {}", expected->namespaze, expected->name, expected->image->name);
        }
    }
}

void testClassLookupPublication() {
    auto* first = addType("SameName", "LookupTests.First.dll");
    auto* second = addType("SameName", "LookupTests.Second.dll");
    checkLookup(first);
    checkLookup(second);

    // Only modify this isolated test image before starting concurrent readers.
    auto& table = *first->image->nameToClassHashTable;
    auto key = std::make_pair("LookupTests", "InvalidHandle");
    for (auto handle :
         {uintptr_t{0},
          uintptr_t{0x100000000},
          uintptr_t{0x80000000},
          reinterpret_cast<uintptr_t>(first->byval_arg.data.typeHandle),
          reinterpret_cast<uintptr_t>(second->byval_arg.data.typeHandle)}) {
        table.insert({key, reinterpret_cast<Il2CppMetadataTypeHandle>(handle)});
        if (i2c::functions::class_from_name(first->image, key.first, key.second) != nullptr) {
            SAFE_ABORT("Class lookup accepted an invalid or mismatched handle");
        }
        table.erase(key);
    }
    // A foreign dynamic image with our name must still delegate to Unity.
    auto foreign = *first->image;
    if (custom_types::Register::FindClass(&foreign, first->namespaze, first->name).has_value()) {
        SAFE_ABORT("Class lookup claimed a foreign image");
    }

    auto* object = i2c::class_of<Il2CppObject*>();
    std::atomic<bool> stop = false;
    std::atomic<size_t> reads = 0;
    auto read = [&] {
        auto* thread = i2c::functions::thread_attach(i2c::functions::domain_get());
        do {
            checkLookup(first);
            checkLookup(second);
            checkLookup(object);
            if (i2c::functions::class_from_name(first->image, "LookupTests", "MissingType") != nullptr) {
                SAFE_ABORT("Missing class lookup returned a class during registration");
            }
            reads.fetch_add(1, std::memory_order_relaxed);
        } while (!stop.load(std::memory_order_relaxed));
        i2c::functions::thread_detach(thread);
    };
    std::thread reader1(read);
    std::thread reader2(read);
    while (reads.load(std::memory_order_relaxed) < 2) {
        std::this_thread::yield();
    }
    auto before = reads.load(std::memory_order_relaxed);
    for (size_t i = 0; i < 128; ++i) {
        checkLookup(addType("Growing" + std::to_string(i), first->image->name));
    }
    auto during = reads.load(std::memory_order_relaxed) - before;
    stop.store(true, std::memory_order_relaxed);
    reader1.join();
    reader2.join();
    if (!during) {
        SAFE_ABORT("Concurrent lookup test did not overlap registration");
    }
    custom_types::logger.info(
        "Class lookup: duplicate names across images, invalid handles, foreign image, and {} read batches during 128 registrations passed", during
    );
}
#endif
