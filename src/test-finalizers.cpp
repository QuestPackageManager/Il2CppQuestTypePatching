#ifdef LOCAL_TEST
#include "delegate.hpp"
#include "macros.hpp"

DECLARE_CLASS(FinalizerTests, Plain, "System", "Object", sizeof(Il2CppObject)){};
DEFINE_TYPE(FinalizerTests, Plain);

DECLARE_CLASS(FinalizerTests, Named, "System", "Object", sizeof(Il2CppObject)) {
    DECLARE_DTOR(Finalize);
};
DEFINE_TYPE(FinalizerTests, Named);
void FinalizerTests::Named::Finalize() {
    this->~Named();
}

DECLARE_CLASS(FinalizerTests, Renamed, "System", "Object", sizeof(Il2CppObject)) {
    DECLARE_DTOR(Cleanup);
};
DEFINE_TYPE(FinalizerTests, Renamed);
void FinalizerTests::Renamed::Cleanup() {
    this->~Renamed();
}

DECLARE_CLASS(FinalizerTests, Simple, "System", "Object", sizeof(Il2CppObject)) {
    DECLARE_SIMPLE_DTOR();
};
DEFINE_TYPE(FinalizerTests, Simple);

DECLARE_CLASS_CUSTOM(FinalizerTests, Inherited, FinalizerTests::Renamed){};
DEFINE_TYPE(FinalizerTests, Inherited);

DECLARE_CLASS_CUSTOM(FinalizerTests, OverrideInherited, FinalizerTests::Renamed) {
    DECLARE_OVERRIDE_METHOD(void, CleanupAgain, i2c::find_method(i2c::class_of<FinalizerTests::Renamed*>(), {"Cleanup", 0}));
};
DEFINE_TYPE(FinalizerTests, OverrideInherited);
void FinalizerTests::OverrideInherited::CleanupAgain() {
    this->~OverrideInherited();
}

// Merely having a method named Finalize must not opt a class into finalization.
DECLARE_CLASS(FinalizerTests, NonVirtual, "System", "Object", sizeof(Il2CppObject)) {
    DECLARE_INSTANCE_METHOD(void, Finalize);
};
DEFINE_TYPE(FinalizerTests, NonVirtual);
void FinalizerTests::NonVirtual::Finalize() {}

namespace {
    void CheckFinalizer(Il2CppClass* klass, bool expected) {
        if (!klass || bool(klass->has_finalize) != expected) {
            custom_types::logger.critical("Finalizer regression: {} expected has_finalize={}", klass ? klass->name : "<null>", expected);
            SAFE_ABORT();
        }
    }
}

// Called after AutoRegister by the LOCAL_TEST entry point. Referencing the
// delegate registration objects also ensures both template types are registered.
void testFinalizers() {
    CheckFinalizer(i2c::class_of<FinalizerTests::Plain*>(), false);
    CheckFinalizer(i2c::class_of<FinalizerTests::Named*>(), true);
    CheckFinalizer(i2c::class_of<FinalizerTests::Renamed*>(), true);
    CheckFinalizer(i2c::class_of<FinalizerTests::Simple*>(), true);
    CheckFinalizer(i2c::class_of<FinalizerTests::Inherited*>(), true);
    CheckFinalizer(i2c::class_of<FinalizerTests::OverrideInherited*>(), true);
    CheckFinalizer(i2c::class_of<FinalizerTests::NonVirtual*>(), false);
    CheckFinalizer(custom_types::DelegateWrapperStatic<void>::__registration_instance_DelegateWrapperStatic.klass(), true);
    CheckFinalizer(custom_types::DelegateWrapperInstance<void, Il2CppObject*>::__registration_instance_DelegateWrapperInstance.klass(), true);
    custom_types::logger.info("Finalizer registration: all 9 regression cases passed");
}
#endif
