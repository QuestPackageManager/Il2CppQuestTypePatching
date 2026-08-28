#pragma once

// Defines the hooks used for liveness.
#ifndef CT_USE_GCDESCRIPTOR_DEBUG
#warning "You probably should only include this with CT_USE_GCDESCRIPTOR_DEBUG"
#endif
namespace liveness {
    void EnsureHooks();
}
