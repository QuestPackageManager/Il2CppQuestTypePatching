#ifdef LOCAL_TEST_COROUTINE
#include "coroutine.hpp"
#include "register.hpp"
#include "beatsaber-hook/shared/hooking.hpp"

#include <chrono>

using namespace custom_types::Helpers;

modloader::ModInfo modInfo{MOD_ID "-test", VERSION, VERSION_LONG};

static constexpr auto logger = ::custom_types::logger;

Coroutine testNestedCoro(int n) {
    logger.debug("begin nested coro");
    co_yield nullptr;
    // logger.debug("one step nested coro");
    // This will not be deleted by GC, as it will be stored as a field in the coro when invoked.
    co_yield reinterpret_cast<enumeratorT>(i2c::new_ctor({"UnityEngine", "WaitForSecondsRealtime"}, 2.3f));
    logger.debug("yielding until: {}", n);
    for (int i = 0; i < n; i++) {
        logger.debug("nested coro yield: {}", i);
        co_yield nullptr;
    }
    co_return;
    logger.debug("nested coroutine complete");
}

Coroutine testRecursiveCall() {
    logger.debug("Recursive call start!");
    auto instance = CoroutineHelper::New<true>(testNestedCoro, 3);
    logger.debug("Recursive call yield 1 {}!", fmt::ptr(instance.convert()));
    co_yield instance;
    // Reset instance and try again
    logger.debug("Recursive yield reset!");
    instance->Reset();

    logger.debug("Recursive call yield 2! {}", fmt::ptr(instance.convert()));
    co_yield instance;

    logger.debug("all done");
    co_return;
}

Coroutine testWaitForSeconds() {
    logger.debug("About to wait!");
    auto start = std::chrono::system_clock::now();
    co_yield reinterpret_cast<enumeratorT>(i2c::new_ctor({"UnityEngine", "WaitForSecondsRealtime"}, 2.3f));
    auto end = std::chrono::system_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    logger.debug("I have performed a wait! {}ms", duration);
    co_return;
}

CUSTOM_TYPES_FUNC void setup(CModInfo* info) {
    *info = modInfo.to_c();
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
    MainMenuViewController_DidActivate(self, firstActivation, addedToHierarchy, screenSystemEnabling);

    logger.debug("Starting coroutine");

    i2c::run_method(self, "StartCoroutine", custom_types::Helpers::CoroutineHelper::New(testWaitForSeconds()));
    i2c::run_method(self, "StartCoroutine", custom_types::Helpers::new_coro(testRecursiveCall()));
}

MAKE_HOOK(abort_hook, (nullptr), void) {
    logger.info("abort was called!");
    logger.Backtrace(40);
    abort_hook();
}

CUSTOM_TYPES_FUNC void load() {
    INSTALL_HOOK(logger, MainMenuViewController_DidActivate);

    auto libc = dlopen("libc.so", RTLD_NOW);
    auto abort_addr = dlsym(libc, "abort");
    INSTALL_HOOK(logger, abort_hook, abort_addr);
}
#endif
