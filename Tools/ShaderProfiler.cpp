#include <ShaderProfiler.h>
#include <VulkanLayer.h>

#include <spirv-tools/libspirv.hpp>
#include <opt/build_module.h>
#include <spirv_constant.h>

#include <sstream>
#include <iostream>
#include <optional>
#include <ranges>

constexpr uint32_t OpLineOperandLineIndex = 1;

struct SPIRVDebugInfo {
    std::string filename;
    std::string source;
    std::unordered_map<uint32_t, size_t> instLines;
};

struct SPIRVProfileInfo {
    SPIRVDebugInfo debugInfo{};

    uint64_t totalFuncExecuted{0};
    uint64_t totalBBExecuted{0};
    uint64_t totalInstExecuted{0};

    std::unordered_map<uint32_t, uint64_t> funcExecuted;
    std::unordered_map<uint32_t, uint64_t> bbExecuted;
    std::unordered_map<uint32_t, uint64_t> instExecuted;

    std::unordered_map<size_t, uint64_t> linesExecuted;
};

static void SPIRVErrorHandler(spv_message_level_t, const char*, const spv_position_t&, const char* m) {
    std::cout << "SPIRV: " << m << '\n';
};

void ReplaceInString(std::string& str, const std::string& search, const std::string& replace) {
    size_t pos = 0;
    while ((pos = str.find(search, pos)) != std::string::npos) {
         str.replace(pos, search.length(), replace);
         pos += replace.length();
    }
}

std::optional<size_t> GetInstructionLine(const spvtools::opt::Instruction& inst) {
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

bool HandleShaderProfilerLayer(std::FILE* inputFile, std::FILE* outputFile) {
    OVS::ShaderProfilerFileHeader shaderProfHeader{};
    std::fread(&shaderProfHeader, sizeof(OVS::ShaderProfilerFileHeader), 1, inputFile);
    if (std::feof(inputFile)) {
        return false;
    }

    std::vector<uint8_t> data(shaderProfHeader.byteSize);
    std::fread(data.data(), sizeof(uint8_t), data.size(), inputFile);
    if (std::feof(inputFile)) {
        return false;
    }

    OVS::ReadStream stream(data);

    uint64_t collectedProfileInfoSize = 0;
    stream.Read(collectedProfileInfoSize);

    std::vector<OVS::CollectedPipelineProfileInfo> collectedProfileInfos(collectedProfileInfoSize);
    for (auto& collectedProfileInfo : collectedProfileInfos) {
        OVS::DeserializeFromStream(collectedProfileInfo, stream);
    }

    std::stringstream ss;
    for (const auto& profileInfo : collectedProfileInfos) {
        ss << "Pipeline " << profileInfo.pipeline << " (" << OVS::GetVulkanPipelineBindPointName(profileInfo.bindPoint) << "):\n";
        for (const auto& shaderInfo : profileInfo.shaderInfos) {
            const auto& profileData = shaderInfo.profileData;

            ss << "    Shader " << shaderInfo.shader << " (" << OVS::GetVulkanShaderStageName(shaderInfo.stage) << "): [";
            for (size_t i = 0; i < profileData.size(); ++i) {
                if (i > 0) {
                    ss << ", ";
                }
                ss << profileData[i];
            }
            ss << "]\n";

            auto context = spvtools::BuildModule(SPV_ENV_VULKAN_1_4, &SPIRVErrorHandler, shaderInfo.code.data(), shaderInfo.code.size(), true);
            if (!context) {
                std::cout << "SPIRV: Could not build module\n";
                continue;
            }

            auto& m = *context->module();

            SPIRVProfileInfo spvProfileInfo;
            SetupSPIRVProfileInfo(m, profileData, spvProfileInfo);

            if (ParseSPIRVDebugInfo(m, spvProfileInfo.debugInfo)) {
                ComputeLinesExecuted(m, spvProfileInfo);

                const auto& source = spvProfileInfo.debugInfo.source;
                auto lines = source | std::views::split('\n');

                size_t maxLineLen = 0;
                for (const auto& line : lines) {
                    maxLineLen = std::max(maxLineLen, line.size());
                }

                size_t lineNum = 1;
                std::string separator = std::string(maxLineLen, '=');

                ss << "        " << separator << '\n';
                for (const auto& line : lines) {
                    ss << "        " << std::string_view(line.data(), line.size());

                    auto lineExecutedIt = spvProfileInfo.linesExecuted.find(lineNum);
                    if (lineExecutedIt != spvProfileInfo.linesExecuted.end()) {
                        uint64_t lineExecuted = lineExecutedIt->second;
                        double lineExecutedPercent = 100.0 * double(lineExecuted) / double(spvProfileInfo.totalInstExecuted);

                        size_t spacingLength = maxLineLen - line.size() + 1;
                        std::string spacing(spacingLength, ' ');
                        ss << spacing << lineExecuted << " (" << lineExecutedPercent << "%)";
                    }

                    ss << '\n';
                    ++lineNum;
                }
                ss << "        " << separator << '\n';
            }

            constexpr uint32_t printFlags = SPV_BINARY_TO_TEXT_OPTION_INDENT | SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES | SPV_BINARY_TO_TEXT_OPTION_NESTED_INDENT;
            std::vector<std::string> instsStr;
            size_t maxInstStrLen = 0;

            for (const auto& f : m) {
                for (const auto& bb : f) {
                    for (const auto& inst : bb) {
                        auto str = inst.PrettyPrint(printFlags);
                        maxInstStrLen = std::max(maxInstStrLen, str.length());
                        instsStr.emplace_back(std::move(str));
                    }
                }
            }

            size_t instIdx = 0;
            size_t bbIdx = 0;
            size_t funcIdx = 0;
            std::string separator = std::string(maxInstStrLen, '-');
            for (const auto& f : m) {
                uint64_t currFuncExecuted = spvProfileInfo.funcExecuted[f.result_id()];
                double currFuncExecutedPercent = 100.0 * double(currFuncExecuted) / double(spvProfileInfo.totalFuncExecuted);

                ss << "        Function #" << (funcIdx++) << ", id = " << f.result_id() << ": " << currFuncExecuted << " (" << currFuncExecutedPercent << "%)\n";
                for (const auto& bb : f) {
                    uint64_t currBBExecuted = spvProfileInfo.bbExecuted[bb.id()];
                    double currBBExecutedPercent = 100.0 * double(currBBExecuted) / double (spvProfileInfo.totalBBExecuted);

                    ss << "            Basic Block #" << (bbIdx++) << ", id = " << bb.id() << ": " << currBBExecuted << " (" << currBBExecutedPercent << "%)\n";
                    ss << "            " << separator << '\n';
                    for (const auto& inst : bb) {
                        const auto& instStr = instsStr[instIdx++];
                        uint64_t currInstExecuted = spvProfileInfo.instExecuted[inst.unique_id()];
                        double currInstExecutedPercent = 100.0 * double(currInstExecuted) / double(spvProfileInfo.totalInstExecuted);

                        size_t spacingLength = maxInstStrLen - instStr.length() + 1;
                        std::string spacing(spacingLength, ' ');
                        ss << "            " << instStr << spacing;

                        auto instLineIt = spvProfileInfo.debugInfo.instLines.find(inst.unique_id());
                        if (instLineIt != spvProfileInfo.debugInfo.instLines.end()) {
                            auto instLine = instLineIt->second;
                            ss << "Line " << instLine << ", ";
                        }

                        ss << currInstExecuted << " (" << currInstExecutedPercent << "%)\n";
                    }
                    ss << "            " << separator << "\n\n";
                }
            }
        }
        ss << "\n";
    }

    const auto& str = ss.str();
    std::fwrite(str.c_str(), sizeof(char), str.size(), outputFile);

    return true;
}
