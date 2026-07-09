// If LOCAL_TEST is defined, create a mod that uses custom types.
// Otherwise, it will be built as a library.
#ifdef LOCAL_TEST
#include "coroutine.hpp"
#include "logging.hpp"
#include "macros.hpp"
#include "register.hpp"
#include "types.hpp"
#include "beatsaber-hook/shared/api.hpp"
#include "beatsaber-hook/shared/debug.hpp"
#include "beatsaber-hook/shared/find.hpp"
#include "beatsaber-hook/shared/hooking.hpp"
#include "beatsaber-hook/shared/stringw.hpp"
#include "beatsaber-hook/shared/types.hpp"
#include "scotland2/shared/loader.hpp"

#include <unordered_map>

modloader::ModInfo modInfo{"test", "0.0.0", 0};

static constexpr auto logger = ::custom_types::logger;

DECLARE_CLASS(Il2CppNamespace, MyType, "UnityEngine", "MonoBehaviour", sizeof(Il2CppObject) + sizeof(void*)) {
    // TODO: Fields need to be copied from base type, size needs to be adjusted to match, offsets of all declared fields need to be correct
    // DECLARE_INSTANCE_FIELD(int, x);
    // DECLARE_INSTANCE_FIELD(Vector3, y);

    DECLARE_CTOR(ctor);

    DECLARE_INSTANCE_METHOD(void, Start);

    DECLARE_STATIC_METHOD(void, Test, int x, float y);

    DECLARE_INSTANCE_METHOD(int, asdf, int q);

    // DECLARE_STATIC_FIELD(int, x);

    DECLARE_OVERRIDE_METHOD(StringW, ToString, i2c::find_method({"UnityEngine", "Object"}, "ToString"));
};

DEFINE_TYPE(Il2CppNamespace, MyType);

void Il2CppNamespace::MyType::ctor() {
    logger.debug("Called Il2CppNamespace::MyType::ctor");
    // y = {1, 2, 3};
    // x = 5;
}

void Il2CppNamespace::MyType::Start() {
    logger.debug("Called Il2CppNamespace::MyType::Start!");
    logger.debug("Return of asdf(1): {}", asdf(1));
    // Runtime invoke it.
    // We ARE NOT able to call GetClassFromName.
    // This is because our class name is NOT in the nameToClassHashTable
    // However, we ARE able to get our Il2CppClass* via the klass private static field of this type.
    auto* il2cppKlass = i2c::get_class_from_name("Il2CppNamespace", "MyType");
    logger.debug("il2cpp obtained klass: {}", fmt::ptr(il2cppKlass));
    logger.debug("klass: {}", fmt::ptr(___TypeRegistration::klass_ptr));
    auto* asdfMethod = i2c::find_method(___TypeRegistration::klass_ptr, {"asdf", 1});
    logger.debug("asdf method info: {}", fmt::ptr(asdfMethod));
    // logger.debug("x: {}", x);
    // logger.debug("y: ({}, {}, {})", y.x, y.y, y.z);
    logger.debug("runtime invoke of asdf(1): {}", i2c::run_method<int>(this, asdfMethod, 1));
}

int Il2CppNamespace::MyType::asdf(int q) {
    return q + 1;
}

StringW Il2CppNamespace::MyType::ToString() {
    // We want to create a C# string that is different than from what might otherwise be expected!
    logger.info("Calling custom ToString!");
    return "My Custom ToString!";
}

void Il2CppNamespace::MyType::Test([[maybe_unused]] int x, [[maybe_unused]] float y) {
    logger.debug("Called Il2CppNamespace::MyType::Test!");
}

DECLARE_CLASS_DLL(Il2CppNamespace, MyTypeDllTest, "UnityEngine", "MonoBehaviour", sizeof(Il2CppObject) + sizeof(void*), "MyCoolDll.dll") {
    DECLARE_CTOR(ctor);
};

DEFINE_TYPE(Il2CppNamespace, MyTypeDllTest);

void Il2CppNamespace::MyTypeDllTest::ctor() {
    logger.debug("MyTypeDllTest debug call!");
    custom_types::logImage(i2c::class_of<MyTypeDllTest*>()->image);
}

DECLARE_CLASS_INTERFACES(Il2CppNamespace, MyCustomRandom, "System", "Object", sizeof(Il2CppObject), INTERFACE_NAME("", "IRandom")) {
    DECLARE_INSTANCE_FIELD(double, fixedValue);

    DECLARE_OVERRIDE_METHOD(double, Sample, i2c::find_method({"", "IRandom"}, "Sample"));
    DECLARE_CTOR(ctor, double value);
};

DEFINE_TYPE(Il2CppNamespace, MyCustomRandom);

void Il2CppNamespace::MyCustomRandom::ctor(double value) {
    // We want to basically wrap the original instance.
    // Also log.
    fixedValue = value;
    logger.debug("Created random with value: {}", fixedValue);
}

double Il2CppNamespace::MyCustomRandom::Sample() {
    logger.debug("My cool sample fixedValue: {}!", fixedValue);
    return fixedValue;
}

DECLARE_CLASS_CUSTOM(Il2CppNamespace, MyCustomRandom2, Il2CppNamespace::MyCustomRandom) {
    DECLARE_CTOR(ctor, double original);
};

DEFINE_TYPE(Il2CppNamespace, MyCustomRandom2);

void Il2CppNamespace::MyCustomRandom2::ctor(double original) {
    fixedValue = original;
    logger.debug("Custom inherited type original: {}", original);
}

DECLARE_CLASS(SmallTest, Test, "System", "Object", sizeof(Il2CppObject)) {
    DECLARE_STATIC_METHOD(SmallTest::Test*, SelfRef, int);
    DECLARE_STATIC_FIELD(SmallTest::Test*, selfRefField);
    DECLARE_STATIC_FIELD(Il2CppNamespace::MyType*, AnotherRef);
};

DEFINE_TYPE(SmallTest, Test);

SmallTest::Test* SmallTest::Test::SelfRef(int) {
    return i2c::new_ctor<SmallTest::Test*>();
}

DECLARE_VALUE(ValueTest, Test, "System", "ValueType", 0) {
    DECLARE_INSTANCE_FIELD(int, x);
    DECLARE_INSTANCE_FIELD(int, y);
    DECLARE_INSTANCE_FIELD(int, z);
};

DEFINE_TYPE(ValueTest, Test);

DECLARE_CLASS(SmallTest, TestIt2, "System", "Object", sizeof(Il2CppObject)) {
    std::vector<void*> allocField;
    int x = 3;
    DECLARE_CTOR(ctor);
    DECLARE_SIMPLE_DTOR();
};

DEFINE_TYPE(SmallTest, TestIt2);

void SmallTest::TestIt2::ctor() {
    // This invokes the C++ constructor for this type
    INVOKE_CTOR();
    logger.debug("X is: {}", x);
}

DECLARE_CLASS(SmallTest, TestIt3, "System", "Object", sizeof(Il2CppObject)) {
    // These fields are initialized in the DEFAULT_CTOR
   public:
    std::vector<void*> allocField;
    int x = 3;
    DECLARE_DEFAULT_CTOR();
    DECLARE_SIMPLE_DTOR();
};

DEFINE_TYPE(SmallTest, TestIt3);

static custom_types::ClassWrapper* klassWrapper;

CUSTOM_TYPES_FUNC void setup(CModInfo* info) {
    info->id = MOD_ID;
    info->version = VERSION;
    info->version_long = VERSION_LONG;
    modInfo.assign(*info);
    logger.debug("Completed setup!");
}

MAKE_HOOK(
    MainMenuViewController_DidActivate,
    ({"", "MainMenuViewController"}, "DidActivate"),
    void,
    Il2CppObject* self,
    bool firstActivation,
    bool addedToHierarchy,
    bool screenSystemEnabling
) {
    logger.debug("MainMenuViewController.DidActivate!");
    MainMenuViewController_DidActivate(self, firstActivation, addedToHierarchy, screenSystemEnabling);

    logger.debug("Creating random...");
    auto* rand = Il2CppNamespace::MyCustomRandom::New_ctor(1.23456789);
    logger.debug("Created random: {}", fmt::ptr(rand));
    custom_types::logAll(i2c::class_of<Il2CppNamespace::MyCustomRandom*>());
    auto* method = RET_V_UNLESS(logger, i2c::find_method(i2c::class_of<Il2CppNamespace::MyCustomRandom*>(), {{"", "IRandom"}, 0}));
    logger.debug("Found IRandom.Sample MethodInfo at {}", fmt::ptr(method));
    auto num = RET_V_UNLESS(logger, i2c::run_method<i2c::result<double>>(rand, method));
    logger.debug("Sample method returned {}", num);

    logger.debug("Getting GO...");
    auto* go = RET_V_UNLESS(logger, i2c::get_property<i2c::result<Il2CppObject*>>(self, "gameObject"));
    logger.debug("Got GO: {}", fmt::ptr(go));
    custom_types::logAll(i2c::class_of<Il2CppNamespace::MyType*>());
    custom_types::logAll(i2c::class_of<Il2CppNamespace::MyType*>()->parent);
    auto* customType = i2c::get_system_type(custom_types::Register::classes[0]);
    logger.debug("Custom System.Type: {}", fmt::ptr(customType));
    auto strType = RET_V_UNLESS(logger, i2c::run_method<i2c::result<StringW>>(customType, "ToString"));
    logger.debug("ToString: {}", strType);
    auto name = RET_V_UNLESS(logger, i2c::get_property<i2c::result<StringW>>(customType, "Name"));
    logger.debug("Name: {}", name);
    logger.debug("Actual type: {}", fmt::ptr(&custom_types::Register::classes[0]->byval_arg));
    logger.debug("Type: {}", fmt::ptr(customType->type));
    // crashNow = true;
    auto* comp = RET_V_UNLESS(logger, i2c::run_method<i2c::result<Il2CppObject*>>(go, "AddComponent", customType));
    logger.debug("Custom Type added as a component: {}", fmt::ptr(comp));
}

CUSTOM_TYPES_FUNC void load() {
    static constexpr auto& logger = custom_types::logger;
    logger.debug("Registering types! (current size: {})", custom_types::Register::classes.size());
    custom_types::Register::AutoRegister();
    logger.debug("Registered: {} types!", custom_types::Register::classes.size());
    INSTALL_HOOK(logger, MainMenuViewController_DidActivate);
    logger.debug("Custom types size: {}", custom_types::Register::classes.size());
    logger.debug("Logging vtables for custom type! There are: {} vtables", custom_types::Register::classes[0]->vtable_count);
    i2c::log_class(logger, custom_types::Register::classes[0]);
    i2c::log_class(logger, custom_types::Register::classes[1]);
    i2c::new_ctor<SmallTest::TestIt2*>();
    auto* testIt3 = i2c::new_ctor<SmallTest::TestIt3*>();
    logger.debug("testIt3 default value {}", testIt3->x);
    for (auto itr : custom_types::Register::classes) {
        logger.debug("Image for custom type: {}::{} {}", itr->namespaze, itr->name, fmt::ptr(itr->image));
    }
}
#endif
