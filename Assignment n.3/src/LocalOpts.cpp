#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

#include <set>
#include <vector>
#include <algorithm>

using namespace llvm;

namespace {

struct LoopInvariantMotionPass : PassInfoMixin<LoopInvariantMotionPass> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
    LoopInfo &LI = AM.getResult<LoopAnalysis>(F);
    DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);

    bool change = false;

    for (Loop *L : LI) {
      BasicBlock *Preheader = L->getLoopPreheader();
      if (!Preheader)
        continue;

      // ----------------------------------------------------
      // FASE 1: Individuazione istruzioni invarianti
      // ----------------------------------------------------
      std::set<Instruction *> istruzInvarianti;
      bool isOpInvariant = true;

      while (isOpInvariant) {
        isOpInvariant = false;
        for (BasicBlock *BB : L->blocks()) {
          for (Instruction &I : *BB) {
            if (istruzInvarianti.count(&I))
              continue;

            if (I.getOpcode() == Instruction::PHI || I.getOpcode() == Instruction::Br)
              continue;

            if (I.mayHaveSideEffects() || I.mayReadOrWriteMemory())
              continue;

            bool opInternaInv = true;
            for (Value *Op : I.operands()) {
              if (dyn_cast<Constant>(Op))
                continue;

              if (Instruction *OpInst = dyn_cast<Instruction>(Op)) {
                if (L->contains(OpInst) && !istruzInvarianti.count(OpInst)) {
                  opInternaInv = false;
                  break;
                }
              }
            }

            if (opInternaInv) {
              istruzInvarianti.insert(&I);
              isOpInvariant = true;
            }
          }
        }
      }

      // ----------------------------------------------------
      // FASE 2 & 3: Controllo condizioni di sicurezza e selezione
      // ----------------------------------------------------
      std::vector<BasicBlock *> ExitBlocks;
      L->getExitBlocks(ExitBlocks);

      std::vector<Instruction *> istruzDaSpostare;
      std::set<Instruction *> setDaSpostare;
      bool Progress = true;

      while (Progress) {
        Progress = false;
        for (BasicBlock *BB : L->blocks()) {
          for (Instruction &I : *BB) {
            if (!istruzInvarianti.count(&I) || setDaSpostare.count(&I))
              continue;

            // Condizione 1: Domina tutte le uscite oppure è Dead fuori
            bool DominatesExits = true;
            for (BasicBlock *ExitBB : ExitBlocks) {
              if (!DT.dominates(I.getParent(), ExitBB)) {
                DominatesExits = false;
                break;
              }
            }

            bool DeadOutside = true;
            for (User *U : I.users()) {
              if (Instruction *UI = dyn_cast<Instruction>(U)) {
                if (!L->contains(UI->getParent())) {
                  DeadOutside = false;
                  break;
                }
              }
            }

            if (!DominatesExits && !DeadOutside)
              continue;

            // Condizione 2: Domina tutti i suoi usi interni al loop
            bool DominatesUses = true;
            for (User *U : I.users()) {
              if (Instruction *UI = dyn_cast<Instruction>(U)) {
                if (L->contains(UI) && !DT.dominates(&I, UI)) {
                  DominatesUses = false;
                  break;
                }
              }
            }

            if (!DominatesUses)
              continue;

            // Condizione 3: Dipendenze SSA (gli operandi interni devono essere già pronti)
            bool DepsReady = true;
            for (Value *Op : I.operands()) {
              if (Instruction *OpInst = dyn_cast<Instruction>(Op)) {
                if (L->contains(OpInst) && !setDaSpostare.count(OpInst)) {
                  DepsReady = false;
                  break;
                }
              }
            }

            if (!DepsReady)
              continue;

            istruzDaSpostare.push_back(&I);
            setDaSpostare.insert(&I);
            Progress = true;
          }
        }
      }

      // Spostamento effettivo nel Preheader
      Instruction *InsertPt = Preheader->getTerminator();
      for (Instruction *I : istruzDaSpostare) {
        I->moveBefore(InsertPt);
        change = true;
      }
    } // Chiusura del for (Loop *L : LI)

    if (change)
      return PreservedAnalyses::none();

    return PreservedAnalyses::all();
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