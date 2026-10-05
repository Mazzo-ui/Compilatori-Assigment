#include "llvm/IR/PassManager.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/MathExtras.h"
#include <cmath>

using namespace llvm;

namespace {

//X + 0 oppure 0 + x -> X
//X * 1 oppure 1 * X -> X
struct AlgebraicIdentityPass : public PassInfoMixin<AlgebraicIdentityPass> {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
        bool changed = false;

        for (auto &BB : F) {
            for (auto It = BB.begin(); It != BB.end();) {
                Instruction *I = &*It++;
                
                if (auto *BO = dyn_cast<BinaryOperator>(I)) {
                    Value *Op0 = BO->getOperand(0);
                    Value *Op1 = BO->getOperand(1);

                    if (BO->getOpcode() == Instruction::Add) {
                        if (auto *C = dyn_cast<ConstantInt>(Op1)) {
                            if (C->isZero()) {
                                BO->replaceAllUsesWith(Op0);
                                BO->eraseFromParent();
                                changed = true;
                            }
                        } else if (auto *C = dyn_cast<ConstantInt>(Op0)) {
                            if (C->isZero()) {
                                BO->replaceAllUsesWith(Op1);
                                BO->eraseFromParent();
                                changed = true;
                            }
                        }
                    } 
                    // Moltiplicazione: 
                    else if (BO->getOpcode() == Instruction::Mul) {
                        if (auto *C = dyn_cast<ConstantInt>(Op1)) {
                            if (C->isOne()) {
                                BO->replaceAllUsesWith(Op0);
                                BO->eraseFromParent();
                                changed = true;
                            }
                        } else if (auto *C = dyn_cast<ConstantInt>(Op0)) {
                            if (C->isOne()) {
                                BO->replaceAllUsesWith(Op1);
                                BO->eraseFromParent();
                                changed = true;
                            }
                        }
                    }
                }
            }
        }

        if (changed)
            return PreservedAnalyses::none();
        else
            return PreservedAnalyses::all();
    }
};

struct StrengthReductionPass : public PassInfoMixin<StrengthReductionPass> {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
        bool changed = false;

        for (auto &BB : F) {
            for (auto It = BB.begin(); It != BB.end();) {
                Instruction *I = &*It++;

                auto *BO = dyn_cast<BinaryOperator>(I);
                if (!BO) 
                    continue;

                //Moltiplicazione del tipo x * C oppure C * x
                if (BO->getOpcode() == Instruction::Mul) {
                    Value *X = nullptr;
                    ConstantInt *C = nullptr;

                    if ((C = dyn_cast<ConstantInt>(BO->getOperand(0))))
                        X = BO->getOperand(1);
                    else if ((C = dyn_cast<ConstantInt>(BO->getOperand(1))))
                        X = BO->getOperand(0);
                    else
                        continue;

                    //Valore esteso con segno: lo usiamo perchè se il valore è negativo o nullo viene scartato e non facciamo la shift
                    int val = C->getSExtValue();
                    if (val <= 1) 
                        continue;

                    // Determiniamo quale variante di potenza di 2 corrisponde, poi con il log troviamo l'esponente per la shift
                    int baseVal = 0;
                    if (isPowerOf2_32(val))
                        baseVal = val;
                    else if (isPowerOf2_32(val + 1))
                        baseVal = val + 1;
                    else if (isPowerOf2_32(val - 1))
                        baseVal = val - 1;
                    else
                        continue;

                    int N = std::log2(baseVal);
                    //creo la variabile per la shift siccome non posso passare un int direttamente
                    Constant *Shift = ConstantInt::get(X->getType(), N);
                    Instruction *Shl = BinaryOperator::Create(Instruction::Shl, X, Shift);
                    Shl->insertAfter(BO);

                    Instruction *Risultato = Shl;

                    if (baseVal == val + 1) { // Caso 2^n - 1 -> (x << n) - x
                        Risultato = BinaryOperator::Create(Instruction::Sub, Shl, X);
                        Risultato->insertAfter(Shl);
                    } else if (baseVal == val - 1) { // Caso 2^n + 1 -> (x << n) + x
                        Risultato = BinaryOperator::Create(Instruction::Add, Shl, X);
                        Risultato->insertAfter(Shl);
                    }

                    BO->replaceAllUsesWith(Risultato);
                    BO->eraseFromParent();
                    changed = true;
                    continue;
                }

                // Divisione del tipo x / 2^n -> x >> n
                if (BO->getOpcode() == Instruction::SDiv) {
                    Value *X = BO->getOperand(0);
                    auto *C = dyn_cast<ConstantInt>(BO->getOperand(1));
                    if (!C) 
                        continue;

                    int val = C->getSExtValue();
                    if (val <= 1 || !isPowerOf2_32(val)) 
                        continue;

                    int N = std::log2(val);
                    Constant *Shift = ConstantInt::get(X->getType(), N);
                    Instruction *Risultato = BinaryOperator::Create(Instruction::AShr, X, Shift);
                    Risultato->insertAfter(BO);

                    BO->replaceAllUsesWith(Risultato);
                    BO->eraseFromParent();
                    changed = true;
                    continue;
                }
            }
        }

        if (changed)
            return PreservedAnalyses::none();
        else
            return PreservedAnalyses::all();
    }
};
struct MultiInstructionOptPass : public PassInfoMixin<MultiInstructionOptPass> {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
        bool changed = false;

        for (auto &BB : F) {
            for (auto It = BB.begin(); It != BB.end();) {
                Instruction *I = &*It++;

                //con il downcast analizziamo se abbiamo davanti un'operazione binaria(add, sub, mul ecc...), altrimenti la scartiamo
                auto *BO = dyn_cast<BinaryOperator>(I);
                if (!BO) 
                    continue;

                // Caso 1: (B + C) - C -> B
                if (BO->getOpcode() == Instruction::Sub) {
                    auto *costEsterna = dyn_cast<ConstantInt>(BO->getOperand(1));
                    auto *opInterna = dyn_cast<BinaryOperator>(BO->getOperand(0));

                    if (costEsterna && opInterna && opInterna->getOpcode() == Instruction::Add) {
                        ConstantInt *opInternaC = nullptr;
                        Value *B = nullptr;

                        if ((opInternaC = dyn_cast<ConstantInt>(opInterna->getOperand(0))))
                            B = opInterna->getOperand(1);
                        else if ((opInternaC = dyn_cast<ConstantInt>(opInterna->getOperand(1))))
                            B = opInterna->getOperand(0);

                        if (opInternaC && opInternaC->getSExtValue() == costEsterna->getSExtValue()) {
                            BO->replaceAllUsesWith(B);
                            BO->eraseFromParent();
                            changed = true;
                            continue;
                        }
                    }
                }

                // Caso 2: (B - C) + C oppure C + (B - C) -> B
                if (BO->getOpcode() == Instruction::Add) {
                    ConstantInt *costEsterna = nullptr;
                    BinaryOperator *opInterna = nullptr;

                    if ((costEsterna = dyn_cast<ConstantInt>(BO->getOperand(1))))
                        opInterna = dyn_cast<BinaryOperator>(BO->getOperand(0));
                    else if ((costEsterna = dyn_cast<ConstantInt>(BO->getOperand(0))))
                        opInterna = dyn_cast<BinaryOperator>(BO->getOperand(1));

                    if (costEsterna && opInterna && opInterna->getOpcode() == Instruction::Sub) {
                        Value *B = opInterna->getOperand(0);
                        auto *opInternaC = dyn_cast<ConstantInt>(opInterna->getOperand(1));

                        if (opInternaC && opInternaC->getSExtValue() == costEsterna->getSExtValue()) {
                            BO->replaceAllUsesWith(B);
                            BO->eraseFromParent();
                            changed = true;
                            continue;
                        }
                    }
                }
            }
        }

        if (changed)
            return PreservedAnalyses::none();
        else
            return PreservedAnalyses::all();
    }
};

}
llvm::PassPluginLibraryInfo getMyPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION,
        "MyOptimizationPasses",
        LLVM_VERSION_STRING,
        [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {

                    if (Name == "algebraic-identity") {
                        FPM.addPass(AlgebraicIdentityPass());
                        return true;
                    }

                    if (Name == "strength-reduction") {
                        FPM.addPass(StrengthReductionPass());
                        return true;
                    }

                    if (Name == "multi-inst-opt") {
                        FPM.addPass(MultiInstructionOptPass());
                        return true;
                    }

                    return false;
                });
        }
    };
}

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return getMyPassPluginInfo();
}