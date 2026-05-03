#include <VulkanShader.h>

#include <opt/build_module.h>

#include <ranges>
#include <optional>

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


static constexpr uint32_t OpLineOperandLineIndex = 1;

static void ReplaceInString(std::string& str, const std::string& search, const std::string& replace) {
    size_t pos = 0;
    while ((pos = str.find(search, pos)) != std::string::npos) {
         str.replace(pos, search.length(), replace);
         pos += replace.length();
    }
}

static std::optional<size_t> GetInstructionLine(const spvtools::opt::Instruction& inst) {
    const auto& dbgLineInsts = inst.dbg_line_insts();
    for (const auto& dbgLineInst : dbgLineInsts | std::views::reverse) {
        switch (dbgLineInst.opcode()) {
            case spv::Op::OpLine: {
                size_t instLine = dbgLineInst.GetSingleWordOperand(OpLineOperandLineIndex);
                return instLine;
            }
            case spv::Op::OpNoLine: {
                return std::nullopt;
            }
            default: {
                break;
            }
        }
    }
    return std::nullopt;
}

bool ParseSPIRVDebugInfo(const spvtools::opt::Module& m, SPIRVDebugInfo& spvDebugInfoOut) {
    SPIRVDebugInfo debugInfo;

    std::unordered_map<uint32_t, std::string> stringMap;
    for (const auto& inst : m.debugs1()) {
        switch (inst.opcode()) {
            case spv::Op::OpString: {
                if (inst.NumOperands() == 2) {
                    const auto& operand = inst.GetOperand(1);
                    uint32_t resultId = inst.result_id();
                    std::string str = operand.AsString();
                    stringMap[resultId] = std::move(str);
                }
                break;
            }
            case spv::Op::OpSource: {
                if (inst.NumOperands() == 4) {
                    const auto& operandFileId = inst.GetOperand(2);
                    const auto& operandSource = inst.GetOperand(3);

                    uint32_t fileId = operandFileId.AsId();
                    auto stringIt = stringMap.find(fileId);
                    if (stringIt != stringMap.end()) {
                        const auto& filename = stringIt->second;
                        debugInfo.filename = filename;
                    }

                    std::string source = operandSource.AsString();

                    const char lineString[] = "#line 1";
                    auto pos = source.find(lineString);
                    if (pos != std::string::npos) {
                        source = source.substr(pos + sizeof(lineString));
                    }

                    debugInfo.source = std::move(source);
                }
                break;
            }
            case spv::Op::OpSourceContinued: {
                if (inst.NumOperands() == 1) {
                    const auto& operandSource = inst.GetOperand(0);

                    std::string sourceContinued = operandSource.AsString();
                    debugInfo.source.append(sourceContinued);
                }
                break;
            }
            default: {
                break;
            }
        }
    }

    ReplaceInString(debugInfo.source, "\t", "    ");

    const spvtools::opt::Instruction* pPrevInstWithLine = nullptr;
    std::vector<const spvtools::opt::Instruction*> phiInsts;
    m.ForEachInst([&debugInfo, &pPrevInstWithLine, &phiInsts](const spvtools::opt::Instruction* pInst) {
        const auto& inst = *pInst;
        auto instLine = GetInstructionLine(inst);
        if (instLine) {
            debugInfo.instLines[inst.unique_id()] = instLine.value();
            pPrevInstWithLine = pInst;
        }
        else if (inst.IsBlockTerminator() && pPrevInstWithLine) {
            auto instLineIt = debugInfo.instLines.find(pPrevInstWithLine->unique_id());
            if (instLineIt != debugInfo.instLines.end()) {
                debugInfo.instLines[inst.unique_id()] = instLineIt->second;
                pPrevInstWithLine = pInst;
            }
        }
        else if (inst.opcode() == spv::Op::OpPhi) {
            phiInsts.emplace_back(pInst);
        }
    });

    for (const auto* phiInst : phiInsts) {
        if (debugInfo.instLines.contains(phiInst->unique_id())) {
            continue;
        }

        const auto* nextInst = phiInst->NextNode();
        auto instLineIt = debugInfo.instLines.find(nextInst->unique_id());
        if (instLineIt != debugInfo.instLines.end()) {
            debugInfo.instLines[phiInst->unique_id()] = instLineIt->second;
        }
    }

    spvDebugInfoOut = std::move(debugInfo);
    return !spvDebugInfoOut.source.empty();
}

bool SetupSPIRVProfileInfo(const spvtools::opt::Module& m, const std::vector<uint64_t>& profileData, SPIRVProfileInfo& spvProfileInfoOut) {
    SPIRVProfileInfo profileInfo;

    auto& funcExecuted = profileInfo.funcExecuted;
    auto& bbExecuted = profileInfo.bbExecuted;
    auto& instExecuted = profileInfo.instExecuted;

    size_t bbIdx = 0;
    for (const auto& f : m) {
        for (const auto& bb : f) {
            bbExecuted[bb.id()] = profileData[bbIdx++];
        }
    }

    for (const auto& f : m) {
        const auto& entry = f.entry();
        uint64_t currFuncExecuted = bbExecuted[entry->id()];
        funcExecuted[f.result_id()] = currFuncExecuted;
        profileInfo.totalFuncExecuted += currFuncExecuted;

        for (const auto& bb : f) {
            uint64_t currBBExecuted = bbExecuted[bb.id()];
            profileInfo.totalBBExecuted += currBBExecuted;

            for (const auto& inst : bb) {
                instExecuted[inst.unique_id()] = currBBExecuted;
                profileInfo.totalInstExecuted += currBBExecuted;
            }
        }
    }

    spvProfileInfoOut = std::move(profileInfo);
    return true;
}

void ComputeLinesExecuted(const spvtools::opt::Module& m, SPIRVProfileInfo& spvProfileInfo) {
    auto& instLines = spvProfileInfo.debugInfo.instLines;
    auto& instExecuted = spvProfileInfo.instExecuted;
    auto& linesExecuted = spvProfileInfo.linesExecuted;
    for (const auto& f : m) {
        for (const auto& bb : f) {
            for (const auto& inst : bb) {
                uint32_t id = inst.unique_id();
                auto instLineIt = instLines.find(id);
                if (instLineIt == instLines.end()) {
                    continue;
                }

                size_t instLine = instLineIt->second;
                linesExecuted[instLine] += instExecuted[id];
            }
        }
    }
}

} // namespace OVS
