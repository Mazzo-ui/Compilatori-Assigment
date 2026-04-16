#include "llvm/IR/PassManager.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Constants.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {


// - ALGEBRAIC IDENTITY PASS

struct AlgebraicIdentityPass : public PassInfoMixin<AlgebraicIdentityPass> {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
        bool Changed = false;

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
                                Changed = true;
                                
                            }
                        }
                        else if (auto *C = dyn_cast<ConstantInt>(Op0)) {
                            if (C->isZero()) {
                                BO->replaceAllUsesWith(Op1);
                                BO->eraseFromParent();
                                Changed = true;
                                
                            }
                        }
                    } else if (BO->getOpcode() == Instruction::Mul) {
                        if (auto *C = dyn_cast<ConstantInt>(Op1)) {
                            if (C->isOne()) {
                                BO->replaceAllUsesWith(Op0);
                                BO->eraseFromParent();
                                Changed = true;
                                
                            }
                        } else if (auto *C = dyn_cast<ConstantInt>(Op0)) {
                            if (C->isOne()) {
                                BO->replaceAllUsesWith(Op1);
                                BO->eraseFromParent();
                                Changed = true;
                            }
                        }
                        
                    }
                }
            }
        }

        return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
    }
};


// - STRENGTH REDUCTION PASS

struct StrengthReductionPass : public PassInfoMixin<StrengthReductionPass> {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
        bool modif = false;

        for (auto &BB : F) {
            for (auto It = BB.begin(); It != BB.end();) {
                Instruction *I = &*It++;

                auto *BO = dyn_cast<BinaryOperator>(I);
                if (!BO) continue;

                IRBuilder<> Builder(BO);

                // MUL: x * C
                if (BO->getOpcode() == Instruction::Mul) {
                    Value *X = nullptr;
                    ConstantInt *C = nullptr;

                    // Riconosce sia x * C che C * x
                    if ((C = dyn_cast<ConstantInt>(BO->getOperand(0))))
                        X = BO->getOperand(1);
                    else if ((C = dyn_cast<ConstantInt>(BO->getOperand(1))))
                        X = BO->getOperand(0);
                    else
                        continue;

                    // Consideriamo solo costanti positive non nulle
                    const APInt &Val = C->getValue();
                    if (Val.isZero() || Val.isNegative()) continue;

                    Value *NewVal = nullptr;

                    // Caso 1: C = 2^n
                    if (Val.isPowerOf2()) {
                        unsigned ShiftAmt = Val.logBase2();
                        NewVal = Builder.CreateShl(X,ConstantInt::get(X->getType(), ShiftAmt),"mul ottimizzata 0");
                    }

                    // Caso 2: C = 2^n - 1  
                    else {
                        APInt PlusOne = Val + 1;
                        if (PlusOne.isPowerOf2()) {
                            unsigned ShiftAmt = PlusOne.logBase2();
                            Value *Shl = Builder.CreateShl(X,ConstantInt::get(X->getType(), ShiftAmt),"mul ottimizzata 1");
                            NewVal = Builder.CreateSub(Shl, X, "mul_m1");
                        } else {
                            // Caso 3: C = 2^n + 1  
                            APInt MinusOne = Val - 1;
                            if (!MinusOne.isZero() && MinusOne.isPowerOf2()) {
                                unsigned ShiftAmt = MinusOne.logBase2();
                                Value *Shl = Builder.CreateShl(X,ConstantInt::get(X->getType(), ShiftAmt),"mul ottimizzata 2");
                                NewVal = Builder.CreateAdd(Shl, X, "mul_p1");
                            }
                        }
                    }

                    if (NewVal) {
                        BO->replaceAllUsesWith(NewVal);
                        BO->eraseFromParent();
                        modif = true;
                    }

                    continue;
                }

                // x / 2^n  -> x>>n

                if (BO->getOpcode() == Instruction::SDiv) {

                    Value *X = BO->getOperand(0);
                    auto *C = dyn_cast<ConstantInt>(BO->getOperand(1));
                    if (!C)
                        continue;

                    const APInt &Val = C->getValue();

                    // solo divisori positivi e potenze di 2
                    if (Val.isZero() || Val.isNegative() || !Val.isPowerOf2())
                        continue;

                    unsigned ShiftAmt = Val.logBase2();

                    Value *NewVal = Builder.CreateAShr(X,ConstantInt::get(X->getType(), ShiftAmt),"Divisione ot");

                    BO->replaceAllUsesWith(NewVal);
                    BO->eraseFromParent();
                    modif = true;
                    continue;
                }
            }
        }

        return modif ? PreservedAnalyses::none(): PreservedAnalyses::all();
    }
};

// - MULTI-INSTRUCTION OPT PASS
struct MultiInstructionOptPass : public PassInfoMixin<MultiInstructionOptPass> {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
        bool Changed = false;

        for (auto &BB : F) {
            for (auto It = BB.begin(); It != BB.end();) {
                Instruction *I = &*It++;

                auto *BO = dyn_cast<BinaryOperator>(I);
                if (!BO)
                    continue;

                IRBuilder<> Builder(BO);

                // =====================================================
                // Caso 1: (B + C) - C -> B
                // =====================================================
                if (BO->getOpcode() == Instruction::Sub) {
                    auto *OuterC = dyn_cast<ConstantInt>(BO->getOperand(1));
                    auto *Inner = dyn_cast<BinaryOperator>(BO->getOperand(0));

                    if (OuterC && Inner && Inner->getOpcode() == Instruction::Add) {
                        ConstantInt *InnerC = nullptr;
                        Value *B = nullptr;

                        if ((InnerC = dyn_cast<ConstantInt>(Inner->getOperand(0))))
                            B = Inner->getOperand(1);
                        else if ((InnerC = dyn_cast<ConstantInt>(Inner->getOperand(1))))
                            B = Inner->getOperand(0);

                        if (InnerC && InnerC->getValue() == OuterC->getValue()) {
                            BO->replaceAllUsesWith(B);
                            BO->eraseFromParent();
                            Changed = true;
                            continue;
                        }
                    }
                }

                // =====================================================
                // Caso 2: (B - C) + C -> B
                // =====================================================
                if (BO->getOpcode() == Instruction::Add) {
                    ConstantInt *OuterC = nullptr;
                    BinaryOperator *Inner = nullptr;

                    // sia (B - C) + C che C + (B - C)
                    if ((OuterC = dyn_cast<ConstantInt>(BO->getOperand(1))))
                        Inner = dyn_cast<BinaryOperator>(BO->getOperand(0));
                    else if ((OuterC = dyn_cast<ConstantInt>(BO->getOperand(0))))
                        Inner = dyn_cast<BinaryOperator>(BO->getOperand(1));

                    if (OuterC && Inner && Inner->getOpcode() == Instruction::Sub) {
                        Value *B = Inner->getOperand(0);
                        auto *InnerC = dyn_cast<ConstantInt>(Inner->getOperand(1));

                        if (InnerC && InnerC->getValue() == OuterC->getValue()) {
                            BO->replaceAllUsesWith(B);
                            BO->eraseFromParent();
                            Changed = true;
                            continue;
                        }
                    }
                }

                // Caso 3: C - (B + C) -> -B
                if (BO->getOpcode() == Instruction::Sub) {
                    auto *OuterC = dyn_cast<ConstantInt>(BO->getOperand(0));
                    auto *Inner = dyn_cast<BinaryOperator>(BO->getOperand(1));

                    if (OuterC && Inner && Inner->getOpcode() == Instruction::Add) {
                        ConstantInt *InnerC = nullptr;
                        Value *B = nullptr;

                        if ((InnerC = dyn_cast<ConstantInt>(Inner->getOperand(0))))
                            B = Inner->getOperand(1);
                        else if ((InnerC = dyn_cast<ConstantInt>(Inner->getOperand(1))))
                            B = Inner->getOperand(0);

                        if (InnerC && InnerC->getValue() == OuterC->getValue()) {
                            Value *Zero = ConstantInt::get(B->getType(), 0);
                            Value *NegB = Builder.CreateSub(Zero, B, "negb");
                            BO->replaceAllUsesWith(NegB);
                            BO->eraseFromParent();
                            Changed = true;
                            continue;
                        }
                    }
                }
            }
        }

        return Changed ? PreservedAnalyses::none(): PreservedAnalyses::all();
    }
};

} // namespace

// REGISTRAZIONE PLUGIN
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