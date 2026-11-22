#pragma once
#include <string>
#include <utility>

namespace akai::cd_project {
// Validated, non-RT CD media intent. A native MAME state carries actual sample
// memory; an absent optical image must not silently discard that state.
struct Plan {
    bool valid = false;
    bool present = false;
    bool mount = false;
    bool missing = false;
    std::string path;
};

inline Plan resolve(bool hasPath, bool hasLoadedFlag, bool wantsDisc,
                    const std::string& savedPath, bool existsOnDisk)
{
    // Legacy projects have no optical media attributes.
    if (!hasPath && !hasLoadedFlag) return {true,false,false,false,{}};
    // One missing half is not an authoritative project media identity.
    if (hasPath != hasLoadedFlag || savedPath.size() > 4096
        || savedPath.find('\0') != std::string::npos)
        return {false,false,false,false,{}};
    if (!wantsDisc) return {true,true,false,false,{}};
    if (savedPath.empty() || !existsOnDisk)
        return {true,true,false,true,{}};
    return {true,true,true,false,savedPath};
}
} // namespace akai::cd_project
