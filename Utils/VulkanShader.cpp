#include <VulkanShader.h>

#include <opt/build_module.h>

namespace OVS {

static void SPIRVErrorHandler(spv_message_level_t, const char*, const spv_position_t&, const char* m) {
    std::cout << "Print SPIRV: " << m << '\n';
};

void PrintSPIRV(const uint32_t* pCode, uint32_t codeSize, std::ostream& out) {
    auto context = spvtools::BuildModule(SPV_ENV_VULKAN_1_4, &SPIRVErrorHandler, pCode, codeSize / sizeof(uint32_t), true);
    if (!context) {
        return;
    }

    const auto& m = *context->module();
    PrintSPIRV(m, out);
}

void PrintSPIRV(const std::vector<uint32_t>& code, std::ostream& out) {
    PrintSPIRV(code.data(), code.size() * sizeof(uint32_t), out);
}

void PrintSPIRV(const spvtools::opt::Module& m, std::ostream& out) {
    out << "Module (version: " << m.version() << "):\n";
    for (const auto& inst : m.capabilities()) {
        out << "            " << inst.PrettyPrint() << '\n';
    }
    for (const auto& inst : m.ext_inst_imports()) {
        out << "            " << inst.PrettyPrint() << '\n';
    }
    out << "            " << m.GetMemoryModel()->PrettyPrint() << '\n';
    for (const auto& inst : m.entry_points()) {
        out << "            " << inst.PrettyPrint() << '\n';
    }
    for (const auto& inst : m.execution_modes()) {
        out << "            " << inst.PrettyPrint() << '\n';
    }
    for (const auto& inst : m.annotations()) {
        out << "            " << inst.PrettyPrint() << '\n';
    }
    for (const auto& inst : m.extensions()) {
        out << "            " << inst.PrettyPrint() << '\n';
    }
    for (const auto& inst : m.types_values()) {
        out << "            " << inst.PrettyPrint() << '\n';
    }
    out << '\n';

    for (const auto& f : m) {
        out << "    Function #" << f.result_id() << '\n';
        for (const auto& bb : f) {
            out << "        BB #" << bb.id() << '\n';
            for (const auto& inst : bb) {
                out << "            " << inst.PrettyPrint() << '\n';
            }
            out << '\n';
        }
        out << '\n';
    }
    out << '\n';
}

} // namespace OVS
