#pragma once

/// Implementation details of the ECS: not meant to be used directly.
namespace ecs::detail {

/// Type-erased owner of one resource, so `Resources` can keep resources of
/// different types in a single container (see `ResourceHolder`).
class IResourceHolder {
public:
    virtual ~IResourceHolder() = default;

    IResourceHolder(const IResourceHolder&) = delete;
    IResourceHolder& operator=(const IResourceHolder&) = delete;
    IResourceHolder(IResourceHolder&&) = delete;
    IResourceHolder& operator=(IResourceHolder&&) = delete;

protected:
    IResourceHolder() = default;
};

} // namespace ecs::detail
