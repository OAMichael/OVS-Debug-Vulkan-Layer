#ifdef _WIN32
#include <windows.h>
#endif

#include <PlatformUtils.h>

namespace OVS {

std::string GetEnvVar(const std::string& name) {
    const char* var = std::getenv(name.c_str());
    if (!var) {
        var = "";
    }
    return std::string(var);
}

uint16_t GetThreadID() {
    static thread_local uint16_t id = 0;
    if (id == 0) {
#ifdef _WIN32
        id = GetCurrentThreadId();
#endif
    }
    return id;
}

} // namespace OVS