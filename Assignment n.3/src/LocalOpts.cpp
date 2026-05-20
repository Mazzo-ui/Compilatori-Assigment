#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Value.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/DenseMap.h"

using namespace llvm;

namespace {

class LoopInvariantMotionPass : public PassInfoMixin<LoopInvariantMotionPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
    LoopInfo &LI = AM.getResult<LoopAnalysis>(F);
    DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);

    if (LI.empty()) {
      outs() << "[LoopInvariantMotion] Funzione " << F.getName()
             << ": nessun loop trovato.\n";
      return PreservedAnalyses::all();
    }

    bool Changed = false;

    outs() << "\n==============================\n";
    outs() << "LoopInvariantMotion su funzione: " << F.getName() << "\n";
    outs() << "==============================\n";

    for (Loop *L : LI) {
      Changed |= processLoopAndSubLoops(L, DT);
    }

    if (!Changed) {
      return PreservedAnalyses::all();
    }

    PreservedAnalyses PA;
    PA.preserve<DominatorTreeAnalysis>();
    PA.preserve<LoopAnalysis>();
    return PA;
  }

private:
  bool processLoopAndSubLoops(Loop *L, DominatorTree &DT) {
    bool Changed = false;

    for (Loop *SubLoop : L->getSubLoops()) {
      Changed |= processLoopAndSubLoops(SubLoop, DT);
    }

    Changed |= processSingleLoop(L, DT);
    return Changed;
  }

  bool processSingleLoop(Loop *L, DominatorTree &DT) {
    BasicBlock *Header = L->getHeader();
    BasicBlock *Preheader = L->getLoopPreheader();

    outs() << "\n[Loop] Header: ";
    Header->printAsOperand(outs(), false);
    outs() << "\n";

    if (!Preheader) {
      outs() << "  Nessun preheader: salto il loop.\n";
      return false;
    }

    outs() << "  Preheader: ";
    Preheader->printAsOperand(outs(), false);
    outs() << "\n";

    SmallPtrSet<Instruction *, 32> InvariantInstructions;
    SmallVector<Instruction *, 32> ToMove;

    bool FoundNewInvariant = true;
    while (FoundNewInvariant) {
      FoundNewInvariant = false;

      for (BasicBlock *BB : L->blocks()) {
        for (Instruction &I : *BB) {
          if (InvariantInstructions.contains(&I)) {
            continue;
          }

          if (!isInstructionSafeForThisAssignment(I)) {
            continue;
          }

          if (!areOperandsLoopInvariant(I, L, InvariantInstructions)) {
            continue;
          }

          InvariantInstructions.insert(&I);
          FoundNewInvariant = true;

          outs() << "  Loop-invariant trovata: ";
          I.print(outs());
          outs() << "\n";
        }
      }
    }

    for (Instruction *I : InvariantInstructions) {
      if (canMoveInstruction(*I, L, DT)) {
        ToMove.push_back(I);
      } else {
        outs() << "  Non spostabile (violazione dominanza usi):\n";
        I->print(outs());
        outs() << "\n";
      }
    }

    if (ToMove.empty()) {
      outs() << "  Nessuna istruzione candidata alla code motion.\n";
      return false;
    }

    sortInstructionsInProgramOrder(ToMove, L);

    Instruction *InsertPoint = Preheader->getTerminator();
    for (Instruction *I : ToMove) {
      outs() << "  Sposto nel preheader: ";
      I->print(outs());
      outs() << "\n";
      I->moveBefore(InsertPoint);
    }

    return true;
  }

  bool isInstructionSafeForThisAssignment(Instruction &I) const {
    if (I.isTerminator()) {
      return false;
    }

    if (isa<PHINode>(&I)) {
      return false;
    }

    if (I.mayReadOrWriteMemory()) {
      return false;
    }

    if (I.mayHaveSideEffects()) {
      return false;
    }

    return isa<BinaryOperator>(&I) || isa<CastInst>(&I) || isa<CmpInst>(&I) ||
           isa<SelectInst>(&I) || isa<GetElementPtrInst>(&I);
  }

  bool areOperandsLoopInvariant(
      Instruction &I, Loop *L,
      const SmallPtrSetImpl<Instruction *> &InvariantInstructions) const {
    for (Value *Op : I.operands()) {
      if (isa<Constant>(Op)) {
        continue;
      }

      Instruction *OpInst = dyn_cast<Instruction>(Op);

      if (!OpInst) {
        continue;
      }

      if (!L->contains(OpInst)) {
        continue;
      }

      if (InvariantInstructions.contains(OpInst)) {
        continue;
      }

      return false;
    }

    return true;
  }

  bool canMoveInstruction(Instruction &I, Loop *L, DominatorTree &DT) const {
    // Controllo fondamentale, L'istruzione deve dominare tutti i suoi usi interni al loop.
    // Assumiamo che le operazioni matematiche sicure (senza side-effects) rilevate 
    // siano speculabili senza vincoli di dominanza sui blocchi condizionali di uscita.
    for (User *U : I.users()) {
      Instruction *UserI = dyn_cast<Instruction>(U);
      if (!UserI) {
        continue;
      }

      if (!L->contains(UserI)) {
        continue;
      }

      // Gestione dei nodi PHI interni/di uscita
      if (PHINode *PN = dyn_cast<PHINode>(UserI)) {
        for (unsigned Idx = 0; Idx < PN->getNumIncomingValues(); ++Idx) {
          if (PN->getIncomingValue(Idx) == &I) {
            if (!DT.dominates(I.getParent(), PN->getIncomingBlock(Idx))) {
              return false;
            }
          }
        }
      } else {
        if (!DT.dominates(&I, UserI)) {
          return false;
        }
      }
    }

    return true;
  }

  void sortInstructionsInProgramOrder(SmallVectorImpl<Instruction *> &Insts,
                                      Loop *L) const {
    DenseMap<Instruction *, unsigned> Order;
    unsigned Index = 0;

    for (BasicBlock *BB : L->blocks()) {
      for (Instruction &I : *BB) {
        Order[&I] = Index++;
      }
    }

    llvm::sort(Insts, [&](Instruction *A, Instruction *B) {
      return Order.lookup(A) < Order.lookup(B);
    });
  }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "LoopInvariantMotionPass", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "loop-invariant-motion") {
                    FPM.addPass(LoopInvariantMotionPass());
                    return true;
                  }
                  return false;
                });
          }};
}
