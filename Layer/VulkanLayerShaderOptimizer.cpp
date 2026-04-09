#include <VulkanLayerShaderOptimizer.h>
#include <VulkanShader.h>

#include <spirv-tools/optimizer.hpp>

#include <iostream>
#include <string>

namespace OVS {

static void SPIRVErrorHandler(spv_message_level_t, const char*, const spv_position_t&, const char* m) {
    std::cout << "Shader Optimizer SPIRV: " << m << '\n';
};

VulkanLayerShaderOptimizerSettings VulkanLayerShaderOptimizer::ParseSettingsFromJSON(const nlohmann::json& layerInfo)
{
    VulkanLayerShaderOptimizerSettings settings{};
    if (layerInfo.contains("Settings")) {
        const auto& settingsJSON = layerInfo["Settings"];
        if (settingsJSON.contains("Mode")) {
            std::string modeStr = settingsJSON["Mode"];
            if (modeStr == "Performance") {
                settings.mode = ShaderOptimizerMode::Performance;
            }
            else if (modeStr == "Size") {
                settings.mode = ShaderOptimizerMode::Size;
            }
            else if (modeStr == "Custom") {
                settings.mode = ShaderOptimizerMode::Custom;
            }
            else if (modeStr != "None") {
                std::cout << "[DEBUG] Unknown Shader Optimizer Mode \"" << modeStr << "\". Skipping...\n";
            }
        }
        if (settingsJSON.contains("Passes")) {
            if (settings.mode != ShaderOptimizerMode::Custom) {
                std::cout << "[DEBUG] Shader Optimizer Passes only take place if \"Custom\" Mode is selected. Using specified Mode \"" << GetShaderOptimizerModeName(settings.mode) << "\"...\n";
            }
            else {
                const auto& passes = settingsJSON["Passes"];
                if (passes.is_array()) {
                    for (const auto& pass : passes) {
                        std::string passStr = pass;
                        std::string_view passSV = std::string_view(passStr.c_str(), passStr.size());
                        ShaderOptimizerPass passVal = GetShaderOptimizerPassByName(passSV);
                        if (passVal == ShaderOptimizerPass::None) {
                            std::cout << "[DEBUG] Unknown Shader Optimizer Pass \"" << passStr << "\". Skipping...\n";
                            continue;
                        }

                        settings.customPasses.push_back(passVal);
                    }
                }
            }
        }
    }
    return settings;
}

void VulkanLayerShaderOptimizer::SetupOptimizer(spvtools::Optimizer& opt)
{
    opt.SetTargetEnv(SPV_ENV_VULKAN_1_4);
    opt.SetMessageConsumer(&SPIRVErrorHandler);

    if (settings_.mode == ShaderOptimizerMode::Performance) {
        opt.RegisterPerformancePasses(true);
    }
    else if (settings_.mode == ShaderOptimizerMode::Size) {
        opt.RegisterSizePasses(true);
    }
    else if (settings_.mode == ShaderOptimizerMode::Custom) {
        for (auto pass : settings_.customPasses) {
            switch (pass) {
                case ShaderOptimizerPass::StripDebugInfo:                       opt.RegisterPass(spvtools::CreateStripDebugInfoPass());                      break;
                case ShaderOptimizerPass::StripNonSemanticInfo:                 opt.RegisterPass(spvtools::CreateStripNonSemanticInfoPass());                break;
                case ShaderOptimizerPass::EliminateDeadFunctions:               opt.RegisterPass(spvtools::CreateEliminateDeadFunctionsPass());              break;
                case ShaderOptimizerPass::EliminateDeadMembers:                 opt.RegisterPass(spvtools::CreateEliminateDeadMembersPass());                break;
                case ShaderOptimizerPass::FoldSpecConstantOpAndComposite:       opt.RegisterPass(spvtools::CreateFoldSpecConstantOpAndCompositePass());      break;
                case ShaderOptimizerPass::UnifyConstant:                        opt.RegisterPass(spvtools::CreateUnifyConstantPass());                       break;
                case ShaderOptimizerPass::EliminateDeadConstant:                opt.RegisterPass(spvtools::CreateEliminateDeadConstantPass());               break;
                case ShaderOptimizerPass::StrengthReduction:                    opt.RegisterPass(spvtools::CreateStrengthReductionPass());                   break;
                case ShaderOptimizerPass::BlockMerge:                           opt.RegisterPass(spvtools::CreateBlockMergePass());                          break;
                case ShaderOptimizerPass::InlineExhaustive:                     opt.RegisterPass(spvtools::CreateInlineExhaustivePass());                    break;
                case ShaderOptimizerPass::InlineOpaque:                         opt.RegisterPass(spvtools::CreateInlineOpaquePass());                        break;
                case ShaderOptimizerPass::DeadBranchElim:                       opt.RegisterPass(spvtools::CreateDeadBranchElimPass());                      break;
                case ShaderOptimizerPass::AggressiveDCE:                        opt.RegisterPass(spvtools::CreateAggressiveDCEPass());                       break;
                case ShaderOptimizerPass::RemoveUnusedInterfaceVariables:       opt.RegisterPass(spvtools::CreateRemoveUnusedInterfaceVariablesPass());      break;
                case ShaderOptimizerPass::RemoveDuplicates:                     opt.RegisterPass(spvtools::CreateRemoveDuplicatesPass());                    break;
                case ShaderOptimizerPass::CFGCleanup:                           opt.RegisterPass(spvtools::CreateCFGCleanupPass());                          break;
                case ShaderOptimizerPass::DeadVariableElimination:              opt.RegisterPass(spvtools::CreateDeadVariableEliminationPass());             break;
                case ShaderOptimizerPass::MergeReturn:                          opt.RegisterPass(spvtools::CreateMergeReturnPass());                         break;
                case ShaderOptimizerPass::LocalRedundancyElimination:           opt.RegisterPass(spvtools::CreateLocalRedundancyEliminationPass());          break;
                case ShaderOptimizerPass::LoopInvariantCodeMotion:              opt.RegisterPass(spvtools::CreateLoopInvariantCodeMotionPass());             break;
                case ShaderOptimizerPass::LoopFission:                          opt.RegisterPass(spvtools::CreateLoopFissionPass(100));                      break;
                case ShaderOptimizerPass::LoopFusion:                           opt.RegisterPass(spvtools::CreateLoopFusionPass(100));                       break;
                case ShaderOptimizerPass::LoopPeeling:                          opt.RegisterPass(spvtools::CreateLoopPeelingPass());                         break;
                case ShaderOptimizerPass::LoopUnswitch:                         opt.RegisterPass(spvtools::CreateLoopUnswitchPass());                        break;
                case ShaderOptimizerPass::RedundancyElimination:                opt.RegisterPass(spvtools::CreateRedundancyEliminationPass());               break;
                case ShaderOptimizerPass::CoditionalConstantPropagation:        opt.RegisterPass(spvtools::CreateCCPPass());                                 break;
                case ShaderOptimizerPass::IfConversion:                         opt.RegisterPass(spvtools::CreateIfConversionPass());                        break;
                case ShaderOptimizerPass::Simplification:                       opt.RegisterPass(spvtools::CreateSimplificationPass());                      break;
                case ShaderOptimizerPass::LoopUnroll:                           opt.RegisterPass(spvtools::CreateLoopUnrollPass(true));                      break;
                case ShaderOptimizerPass::ConvertRelaxedToHalf:                 opt.RegisterPass(spvtools::CreateConvertRelaxedToHalfPass());                break;
                case ShaderOptimizerPass::RelaxFloatOps:                        opt.RegisterPass(spvtools::CreateRelaxFloatOpsPass());                       break;
                case ShaderOptimizerPass::CopyPropagateArrays:                  opt.RegisterPass(spvtools::CreateCopyPropagateArraysPass());                 break;
                case ShaderOptimizerPass::VectorDCE:                            opt.RegisterPass(spvtools::CreateVectorDCEPass());                           break;
                case ShaderOptimizerPass::ReduceLoadSize:                       opt.RegisterPass(spvtools::CreateReduceLoadSizePass());                      break;
                case ShaderOptimizerPass::CombineAccessChains:                  opt.RegisterPass(spvtools::CreateCombineAccessChainsPass());                 break;
                case ShaderOptimizerPass::GraphicsRobustAccess:                 opt.RegisterPass(spvtools::CreateGraphicsRobustAccessPass());                break;
                case ShaderOptimizerPass::InterfaceVariableScalarReplacement:   opt.RegisterPass(spvtools::CreateInterfaceVariableScalarReplacementPass());  break;
                case ShaderOptimizerPass::TrimCapabilities:                     opt.RegisterPass(spvtools::CreateTrimCapabilitiesPass());                    break;
                case ShaderOptimizerPass::SplitCombinedImageSampler:            opt.RegisterPass(spvtools::CreateSplitCombinedImageSamplerPass());           break;
                default: break;
            }
        }
    }
}

VkResult VulkanLayerShaderOptimizer::vkCreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkShaderModule* pShaderModule)
{
    if (!pCreateInfo || !pCreateInfo->pCode) {
        return next_->vkCreateShaderModule(device, pCreateInfo, pAllocator, pShaderModule);
    }

    uint32_t codeSize = pCreateInfo->codeSize;
    const uint32_t* pCode = pCreateInfo->pCode;

    spvtools::Optimizer opt(SPV_ENV_VULKAN_1_4);
    SetupOptimizer(opt);

    std::vector<uint32_t> modifiedCode;
    if (!opt.Run(pCode, codeSize / sizeof(uint32_t), &modifiedCode)) {
        std::cout << "SPIRV: Could not optimize shader code. Fallback to original shader code\n";
        return next_->vkCreateShaderModule(device, pCreateInfo, pAllocator, pShaderModule);
    }

    VkShaderModuleCreateInfo modifiedCreateInfo = *pCreateInfo;
    modifiedCreateInfo.pCode = modifiedCode.data();
    modifiedCreateInfo.codeSize = modifiedCode.size() * sizeof(uint32_t);

    VkResult res = next_->vkCreateShaderModule(device, &modifiedCreateInfo, pAllocator, pShaderModule);
    if (res != VK_SUCCESS) {
        std::cout << "SPIRV: Could not create optimized shader. Fallback to original shader\n";
        return next_->vkCreateShaderModule(device, pCreateInfo, pAllocator, pShaderModule);
    }

    std::cout << "SPIRV: Optimized shader: " << pCreateInfo->codeSize << " -> " << modifiedCreateInfo.codeSize << " bytes\n";

    return res;
}

} // namespace OVS