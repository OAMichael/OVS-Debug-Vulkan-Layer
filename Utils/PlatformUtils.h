#ifndef UTILS__PLATFORM_UTILS_H
#define UTILS__PLATFORM_UTILS_H

#include <string>
#include <cstdlib>

namespace OVS {

static std::string GetEnvVar(const std::string& name) {
    const char* var = std::getenv(name.c_str());
    if (!var) {
        var = "";
    }
    return std::string(var);
}

} // namespace OVS

#endif // UTILS__PLATFORM_UTILS_H