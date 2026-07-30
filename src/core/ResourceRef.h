#pragma once

#include <cstdint>

namespace Hummingbird {

struct ImageBitmap;

// A handle to a resource owned by the engine's resource store (T-RESOURCE-REF-1).
//
// Layers below `engine/` hold one of these instead of a pointer to the payload.
// That is the whole point: the store frees and replaces decoded resources as the
// network answers, the cache evicts and the page navigates, and a raw payload
// pointer cached in the render tree or the retained display list outlives those
// decisions. Referring to a resource by identity means a free can never leave a
// dangling reader — the worst a stale handle can do is resolve to null.
//
// Trivially copyable and free of allocation on purpose: render objects are
// arena-allocated and effectively POD per the coding constitution, so a handle
// they store must not own memory. That is also why this is an interned index
// rather than a URL string.
struct ResourceRef {
    // 1-based; 0 is the null handle, so a default-constructed ref is "nothing".
    std::uint32_t index = 0;
    // Guards against a handle from a previous document resolving against a slot
    // that has since been reused. Without it, "index 3" means different things
    // before and after a navigation and the mistake is silent.
    std::uint32_t generation = 0;

    bool valid() const { return index != 0; }
    friend bool operator==(const ResourceRef& a, const ResourceRef& b) {
        return a.index == b.index && a.generation == b.generation;
    }
    friend bool operator!=(const ResourceRef& a, const ResourceRef& b) { return !(a == b); }
};

// Turns a handle back into pixels, at the point of use.
//
// A port, so the layers that draw can resolve without depending on the engine
// that owns the store (the dependency firewall forbids `layout/` and
// `renderer/` including `engine/`). The engine supplies the implementation.
//
// The returned pointer is valid for the duration of the call that resolved it
// and no longer — which is exactly the non-owning observer the constitution
// sanctions. Storing it is the bug this whole type exists to prevent.
class IResourceResolver {
public:
    virtual ~IResourceResolver() = default;

    // The pixels for `ref`, or nullptr when the handle is stale, the resource is
    // not decoded yet, or it failed. Callers draw nothing rather than treating
    // null as exceptional: a resource that has not arrived is the normal case.
    virtual const ImageBitmap* resolve_image(ResourceRef ref) const = 0;
};

}  // namespace Hummingbird
