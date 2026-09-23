// HardeningAuditPass.cpp
//
// LLVM module pass that audits functions for compiler security hardening
// attributes: SSP, SafeStack, ShadowCallStack.
//
// Usage:
//   opt -load-pass-plugin ./HardeningAuditPass.so \
//       -passes="hardening-audit" -disable-output input.ll

#include "llvm/IR/Attributes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

struct FunctionAudit {
    std::string name;
    bool ssp        = false;
    bool safe_stack = false;
    bool shadow_cs  = false;
    bool no_return  = false;
    unsigned calls  = 0;  // call + invoke instructions
};

static FunctionAudit audit_function(const Function &F) {
    FunctionAudit a;
    a.name      = F.getName().str();
    a.ssp       = F.hasFnAttribute(Attribute::StackProtect)       ||
                  F.hasFnAttribute(Attribute::StackProtectStrong)  ||
                  F.hasFnAttribute(Attribute::StackProtectReq);
    a.safe_stack  = F.hasFnAttribute(Attribute::SafeStack);
    a.shadow_cs   = F.hasFnAttribute(Attribute::ShadowCallStack);
    a.no_return   = F.hasFnAttribute(Attribute::NoReturn);

    for (const BasicBlock &BB : F)
        for (const Instruction &I : BB)
            if (isa<CallInst>(I) || isa<InvokeInst>(I))
                ++a.calls;

    return a;
}

struct HardeningAuditPass : public PassInfoMixin<HardeningAuditPass> {

    PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
        std::vector<FunctionAudit> results;
        for (const Function &F : M)
            if (!F.isDeclaration())
                results.push_back(audit_function(F));

        print_report(M.getName(), results);
        return PreservedAnalyses::all();
    }

    static void print_report(StringRef module_name,
                              const std::vector<FunctionAudit> &results) {
        unsigned missing_ssp = 0;
        for (const auto &a : results)
            if (!a.ssp) ++missing_ssp;

        errs() << "\n";
        errs() << "=== hardening-audit: " << module_name << " ===\n";
        errs() << format("%-40s  %-5s  %-10s  %-9s  %s\n",
                         "function", "SSP", "SafeStack", "ShadowCS", "calls");
        errs() << std::string(72, '-') << "\n";

        for (const auto &a : results) {
            // The casts matter: when both branches of a ternary are string
            // literals of equal length the result keeps its array type
            // (char[4]) instead of decaying, and llvm::format cannot deduce
            // an array argument. Clang/libc++ tolerated it; GCC did not.
            const char *ssp_cell        = a.ssp        ? "YES" : "NO";
            const char *safe_stack_cell = a.safe_stack ? "yes" : "-";
            const char *shadow_cs_cell  = a.shadow_cs  ? "yes" : "-";

            errs() << format("%-40s  %-5s  %-10s  %-9s  %u\n",
                             a.name.c_str(),
                             ssp_cell,
                             safe_stack_cell,
                             shadow_cs_cell,
                             a.calls);
        }

        errs() << "\n"
               << "Functions audited : " << results.size() << "\n"
               << "Missing SSP       : " << missing_ssp << "\n";
        if (missing_ssp > 0)
            errs() << "WARNING: " << missing_ssp
                   << " function(s) compiled without stack-smashing protection.\n";
        errs() << "\n";
    }
};

} // namespace

llvm::PassPluginLibraryInfo getHardeningAuditPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION,
        "HardeningAudit",
        LLVM_VERSION_STRING,
        [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) -> bool {
                    if (name == "hardening-audit") {
                        MPM.addPass(HardeningAuditPass());
                        return true;
                    }
                    return false;
                });
        }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return getHardeningAuditPassPluginInfo();
}
