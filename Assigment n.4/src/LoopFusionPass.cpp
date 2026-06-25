#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

#include <set>

using namespace llvm;

namespace {

struct LoopFusionPass : public PassInfoMixin<LoopFusionPass> {

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
    outs() << "\n========== LOOP FUSION PASS su funzione: " << F.getName()
           << " ==========" << "\n";

    LoopInfo &LI = AM.getResult<LoopAnalysis>(F);
    DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);
    PostDominatorTree &PDT = AM.getResult<PostDominatorTreeAnalysis>(F);
    ScalarEvolution &SE = AM.getResult<ScalarEvolutionAnalysis>(F);
    DependenceInfo &DI = AM.getResult<DependenceAnalysis>(F);

    SmallVector<Loop *, 8> TopLevelLoops;
    for (Loop *L : LI) {
      if (!L->getParentLoop())
        TopLevelLoops.push_back(L);
    }

    if (TopLevelLoops.size() < 2) {
      outs() << "La funzione contiene meno di due loop top-level: nessuna fusion possibile.\n";
      return PreservedAnalyses::all();
    }

    bool Changed = false;

    for (unsigned I = 0; I + 1 < TopLevelLoops.size(); ++I) {
      Loop *L0 = TopLevelLoops[I];
      Loop *L1 = TopLevelLoops[I + 1];

      outs() << "\n--- Analisi coppia di loop ---\n";
      printLoopName("L0", L0);
      printLoopName("L1", L1);

      if (!isSimpleLoop(L0) || !isSimpleLoop(L1)) {
        outs() << "[NO] Almeno uno dei due loop non e' nella forma semplice gestita dal pass.\n";
        continue;
      }

      if (!areAdjacent(L0, L1)) {
        outs() << "[NO] I due loop non sono adiacenti.\n";
        continue;
      }
      outs() << "[OK] I loop sono adiacenti.\n";

      if (!sameTripCount(L0, L1, SE)) {
        outs() << "[NO] I due loop non hanno lo stesso trip count secondo ScalarEvolution.\n";
        continue;
      }
      outs() << "[OK] I loop hanno lo stesso trip count.\n";

      if (!controlFlowEquivalent(L0, L1, DT, PDT)) {
        outs() << "[NO] I due loop non sono control-flow equivalent.\n";
        continue;
      }
      outs() << "[OK] I loop sono control-flow equivalent.\n";

      if (hasNegativeDistanceDependence(L0, L1, DI)) {
        outs() << "[NO] Trovata possibile dipendenza a distanza negativa.\n";
        continue;
      }
      outs() << "[OK] Nessuna dipendenza negativa rilevata.\n";

      if (tryFuseSimpleLoops(L0, L1)) {
        outs() << "[FUSION] Trasformazione eseguita su questa coppia.\n";
        Changed = true;
        break; // dopo aver cambiato il CFG, evitiamo iteratori invalidati
      }

      outs() << "[NO] Le condizioni teoriche sono soddisfatte, ma la forma LLVM concreta non e' supportata.\n";
    }

    if (Changed) {
      outs() << "\nIl CFG e' stato modificato. Rieseguire eventualmente il pass per altre coppie.\n";
      return PreservedAnalyses::none();
    }

    outs() << "\nNessuna loop fusion applicata.\n";
    return PreservedAnalyses::all();
  }

private:
  static void printLoopName(StringRef Name, Loop *L) {
    outs() << Name << " header: ";
    if (BasicBlock *H = L->getHeader())
      H->printAsOperand(outs(), false);
    else
      outs() << "<null>";
    outs() << "\n";
  }

  // Versione volutamente conservativa: gestiamo loop canonicali semplici.
  static bool isSimpleLoop(Loop *L) {
    if (!L)
      return false;
    if (!L->isLoopSimplifyForm())
      return false;
    if (!L->getLoopPreheader())
      return false;
    if (!L->getHeader())
      return false;
    if (!L->getLoopLatch())
      return false;
    if (!L->getExitingBlock())
      return false;
    if (!L->getExitBlock())
      return false;
    return true;
  }

  static bool areAdjacent(Loop *L0, Loop *L1) {
    BasicBlock *Exit0 = L0->getExitBlock();
    BasicBlock *Preheader1 = L1->getLoopPreheader();
    if (!Exit0 || !Preheader1)
      return false;

    if (Exit0 == Preheader1)
      return true;

    if (Exit0->getTerminator()->getNumSuccessors() == 1 &&
        Exit0->getTerminator()->getSuccessor(0) == Preheader1 &&
        onlyTrivialInstructions(Exit0))
      return true;

    return false;
  }

  static bool onlyTrivialInstructions(BasicBlock *BB) {
    for (Instruction &I : *BB) {
      if (isa<PHINode>(&I) || I.isTerminator() || isa<DbgInfoIntrinsic>(&I))
        continue;
      return false;
    }
    return true;
  }

  static bool sameTripCount(Loop *L0, Loop *L1, ScalarEvolution &SE) {
    const SCEV *TC0 = SE.getBackedgeTakenCount(L0);
    const SCEV *TC1 = SE.getBackedgeTakenCount(L1);

    outs() << "Trip count L0: ";
    TC0->print(outs());
    outs() << "\nTrip count L1: ";
    TC1->print(outs());
    outs() << "\n";

    if (isa<SCEVCouldNotCompute>(TC0) || isa<SCEVCouldNotCompute>(TC1))
      return false;

    return TC0 == TC1 || SE.isKnownPredicate(CmpInst::ICMP_EQ, TC0, TC1);
  }

  static bool controlFlowEquivalent(Loop *L0, Loop *L1, DominatorTree &DT,
                                    PostDominatorTree &PDT) {
    BasicBlock *H0 = L0->getHeader();
    BasicBlock *H1 = L1->getHeader();
    if (!H0 || !H1)
      return false;

    return DT.dominates(H0, H1) && PDT.dominates(H1, H0);
  }

  static bool hasNegativeDistanceDependence(Loop *L0, Loop *L1,
                                            DependenceInfo &DI) {
    SmallVector<Instruction *, 16> Mem0;
    SmallVector<Instruction *, 16> Mem1;

    collectMemoryInstructions(L0, Mem0);
    collectMemoryInstructions(L1, Mem1);

    for (Instruction *I0 : Mem0) {
      for (Instruction *I1 : Mem1) {
        auto Dep = DI.depends(I0, I1, true);
        if (!Dep)
          continue;

        outs() << "Possibile dipendenza tra: " << *I0 << "  E  " << *I1 << "\n";

        bool DirectionKnown = false;
        for (unsigned Level = 1; Level <= Dep->getLevels(); ++Level) {
          unsigned Dir = Dep->getDirection(Level, true);
          DirectionKnown = true;
          if (Dir & Dependence::DVEntry::GT)
            return true;
        }
        if (!DirectionKnown)
          return true;
      }
    }
    return false;
  }

  static void collectMemoryInstructions(Loop *L, SmallVectorImpl<Instruction *> &Out) {
    for (BasicBlock *BB : L->blocks()) {
      for (Instruction &I : *BB) {
        if (isa<LoadInst>(&I) || isa<StoreInst>(&I))
          Out.push_back(&I);
      }
    }
  }

  static bool tryFuseSimpleLoops(Loop *L0, Loop *L1) {
    BasicBlock *H0 = L0->getHeader();
    BasicBlock *H1 = L1->getHeader();
    BasicBlock *Exit1 = L1->getExitBlock();

    if (!H0 || !H1 || !Exit1)
      return false;

    PHINode *IV0 = L0->getCanonicalInductionVariable();
    PHINode *IV1 = L1->getCanonicalInductionVariable();
    if (!IV0 || !IV1) {
      outs() << "Manca una canonical induction variable in uno dei due loop.\n";
      return false;
    }

    if (L0->getNumBlocks() != 1 || L1->getNumBlocks() != 1) {
      outs() << "La trasformazione automatica e' limitata a loop single-block.\n";
      return false;
    }

    SmallVector<Use *, 16> UsesToReplace;
    for (Use &U : IV1->uses()) {
      if (Instruction *UserI = dyn_cast<Instruction>(U.getUser())) {
        if (L1->contains(UserI->getParent()))
          UsesToReplace.push_back(&U);
      }
    }
    for (Use *U : UsesToReplace)
      U->set(IV0);

    SmallVector<Instruction *, 16> ToMove;
    std::set<Instruction *> Skip;
    Skip.insert(IV1);

    for (User *U : IV1->users()) {
      if (Instruction *I = dyn_cast<Instruction>(U))
        if (L1->contains(I->getParent()))
          Skip.insert(I);
    }

    for (Instruction &I : *H1) {
      if (isa<PHINode>(&I) || I.isTerminator() || Skip.count(&I))
        continue;
      ToMove.push_back(&I);
    }

    if (ToMove.empty()) {
      outs() << "Nessuna istruzione utile da spostare dal loop 2.\n";
      return false;
    }

    Instruction *InsertBefore = H0->getTerminator();
    for (Instruction *I : ToMove) {
      outs() << "Sposto nel loop 1: " << *I << "\n";
      I->moveBefore(InsertBefore);
    }

    Instruction *T0 = H0->getTerminator();
    bool Rewired = false;
    BasicBlock *OldExit0 = L0->getExitBlock();
    for (unsigned S = 0; S < T0->getNumSuccessors(); ++S) {
      if (T0->getSuccessor(S) == OldExit0) {
        T0->setSuccessor(S, Exit1);
        Rewired = true;
      }
    }

    if (!Rewired)
      return false;

    for (Instruction &I : *Exit1) {
      PHINode *PN = dyn_cast<PHINode>(&I);
      if (!PN)
        break;
      for (unsigned Idx = 0; Idx < PN->getNumIncomingValues(); ++Idx) {
        if (PN->getIncomingBlock(Idx) == H1)
          PN->setIncomingBlock(Idx, H0);
      }
    }

    outs() << "CFG aggiornato: l'uscita del loop 1 punta all'uscita del loop 2.\n";
    return true;
  }
};

} 

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "LoopFusionPass", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "loop-fusion-pass") {
                    FPM.addPass(LoopFusionPass());
                    return true;
                  }
                  return false;
                });
          }};
}
