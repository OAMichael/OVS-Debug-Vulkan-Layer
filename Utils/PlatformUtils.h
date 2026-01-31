#ifndef UTILS__PLATFORM_UTILS_H
#define UTILS__PLATFORM_UTILS_H

#include <string>
#include <cstdlib>
#include <cstdint>

namespace OVS {

std::string GetEnvVar(const std::string& name);

uint16_t GetThreadID();

} // namespace OVS

#endif // UTILS__PLATFORM_UTILS_H