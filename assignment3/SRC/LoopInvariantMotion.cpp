#include "llvm/IR/PassManager.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/Dominators.h"

#include "llvm/Analysis/LoopInfo.h"

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/SetVector.h"

using namespace llvm;

namespace {

struct LoopInvariantMotion : PassInfoMixin<LoopInvariantMotion> {

    /**
     * @brief Verifica che l'istruzione sia potenzialmente una nostra candidata.
     * In particolare il pass esclude tutte le istruzioni di tipo Load/Store, Terminatori e Chiamate a Funzioni
     * @param I istruzione corrente
     * @return true se è valida
     * @return false altrimenti
     */
    bool isCandidateInstruction(Instruction &I) {
        if (I.isTerminator())
            return false;

        if (isa<PHINode>(&I))
            return false;

        if (I.mayHaveSideEffects())
            return false;

        if (I.mayReadFromMemory())
            return false;

        if (I.getType()->isVoidTy())
            return false;

        return true;
    }

    /**
     * @brief Raccoglie esplicitamente le reaching definitions di un valore.
     * @param V 
     * @param Defs 
     */
    void collectReachingDefinitions(Value *V, SmallVectorImpl<Instruction *> &Defs) {
        if (auto *DefInst = dyn_cast<Instruction>(V))
            Defs.push_back(DefInst);
    }

    /**
     * @brief Analizza un operando per verificare che sia effettivamente compatibile con la
     * definizione di Istruzione Invariante.
     * Sono invarianti:
     * -Costanti, argomenti e var. globali
     * -Definizioni fuori dal loop
     * -Istruzioni dentro al loop se la sua reaching è invariante
     * @param V valore
     * @param L loop di riferimento
     * @param InvariantInsts vettore delle istruzioni invarianti
     * @return true se il valore è invariante
     * @return false altrimenti
     */
    bool isLoopInvariantValue(Value *V, Loop *L, const SmallSetVector<Instruction *, 16> &InvariantInsts) {
        if (!V)
            return false;

        if (isa<Constant>(V))
            return true;

        if (isa<Argument>(V))
            return true;

        if (isa<GlobalValue>(V))
            return true;

        SmallVector<Instruction *, 4> ReachingDefs;
        collectReachingDefinitions(V, ReachingDefs);

        if (ReachingDefs.empty())
            return false;

        for (Instruction *DefInst : ReachingDefs) {
            //Se il valore è esterno al Loop, allora va bene
            if (!L->contains(DefInst->getParent()))
                continue;
            //Controlliamo che la reaching definitions sia all'interno del corpo del Loop
            if (InvariantInsts.count(DefInst))
                continue;

            return false;
        }

        return true;
    }

    /**
     * @brief Controlla se una PHI è invariante.
     * Succede nel caso in cui in entrambi i rami abbiamo la presenza della stessa istruzione. Se i casi fossero diversi allora ritorniamo false
     * @param PN il PHI Node
     * @param L Loop di riferimento
     * @param InvariantInsts Il vettore da riempire
     * @return true se la phi è invariante
     * @return false altrimenti
     */
    bool isLoopInvariantPhi(PHINode &PN, Loop *L, const SmallSetVector<Instruction *, 16> &InvariantInsts) {
        if (PN.getNumIncomingValues() == 0)
            return false;

        Value *FirstValue = PN.getIncomingValue(0);

        for (unsigned i = 1; i < PN.getNumIncomingValues(); ++i) {
            if (PN.getIncomingValue(i) != FirstValue)
                return false;
        }

        return isLoopInvariantValue(FirstValue, L, InvariantInsts);
    }

    /**
     * @brief Restituisce true se l'istruzione è loop-invariant rispetto al loop.
     * 
     * @param I 
     * @param L 
     * @param InvariantInsts 
     * @return true 
     * @return false 
     */
    bool isLoopInvariantInstruction(Instruction &I, Loop *L, const SmallSetVector<Instruction *, 16> &InvariantInsts) {
        //istruzione di terminazione non vanno bene
        if (I.isTerminator())
            return false;

        //se il phi node ha rami diversi idem
        if (auto *PN = dyn_cast<PHINode>(&I))
            return isLoopInvariantPhi(*PN, L, InvariantInsts);

        if (!isCandidateInstruction(I))
            return false;

        //controlliamo gli Usee, cioè la catena UD Use-Definitions
        for (Use &U : I.operands()) {
            Value *Op = U.get();

            if (!isLoopInvariantValue(Op, L, InvariantInsts))
                return false;
        }

        return true;
    }

    /**
     * @brief Verifica che l'istruzione corrente domini tutte le uscite, come indicato dal PDF.
     * @param I L'istruzione che ci interessa
     * @param L loop di riferimento
     * @param DT dominator tree dei basic block
     * @return true se domina tutte le uscite
     * @return false altrimenti
     */
    bool dominatesAllLoopExits(Instruction &I, Loop *L, DominatorTree &DT) {
        BasicBlock *InstBB = I.getParent();

        SmallVector<BasicBlock *, 8> ExitBlocks;
        L->getExitBlocks(ExitBlocks);

        if (ExitBlocks.empty())
            return false;

        for (BasicBlock *ExitBB : ExitBlocks) {
            if (!DT.dominates(InstBB, ExitBB))
                return false;
        }

        return true;
    }

    /**
     * @brief Controlliamo che la nostra istruzione domini effettivamente gli utilizzatori all'interno del Loop;
     * ci rifacciamo come di consueto ai basic block; controllo altri usi della consegna
     * @param I Istruzione
     * @param L Loop
     * @param DT Dominator Tree dei Basic Block
     * @return true Se l'istruzione domina
     * @return false altrimenti
     */
    bool dominatesAllLoopUses(Instruction &I, Loop *L, DominatorTree &DT) {
        BasicBlock *InstBB = I.getParent();

        for (User *U : I.users()) {
            Instruction *UserInst = dyn_cast<Instruction>(U);

            if (!UserInst)
                return false;

            BasicBlock *UserBB = UserInst->getParent();

            if (!L->contains(UserBB))
                continue;

            if (!DT.dominates(InstBB, UserBB))
                return false;
        }

        return true;
    }

    /**
     * @brief Controlliamo come da consegna che l'istruzione sia effettivamente morta oltre il Loop
     * Controlliamo per negazione le varie condizioni degli users
     * @param I Istruzione
     * @param L Loop di riferimento
     * @return true se è morta
     * @return false altrimenti
     */
    bool isDeadAtLoopExit(Instruction &I, Loop *L) {
        //qui indichiamo gli users, cioè la catena DU
        for (User *U : I.users()) {
            Instruction *UserInst = dyn_cast<Instruction>(U);

            if (!UserInst)
                return false;

            //guardiamo se gli utilizzatori sono interni o esterni
            if (!L->contains(UserInst->getParent()))
                return false;
        }

        return true;
    }

    /**
     * @brief Verifica se un'istruzione invariante può essere spostata nel preheader.
     * Condizioni:
     * -Il blocco domina tutte le uscite;
     * -Il blocco domina tutti i blocchi che usano il valore.
     * Eventualmente il valore è morto all'uscita.
     * @param I 
     * @param L 
     * @param DT 
     * @return true 
     * @return false 
     */
    bool isSafeToMove(Instruction &I, Loop *L, DominatorTree &DT) {
        if (isa<PHINode>(&I))
            return false;

        if (dominatesAllLoopExits(I, L, DT) && dominatesAllLoopUses(I, L, DT))
            return true;

        if (isDeadAtLoopExit(I, L))
            return true;

        return false;
    }

    /**
     * @brief Esegue la ricerca DFS dei blocchi.
     * Ripetiamo l'analisi finché troviamo nuove istruzioni invarianti, così
     * una definizione può sbloccarne altre nelle iterazioni successive.
     * @param L 
     * @param LI 
     * @param InvariantInsts 
     */
    void collectLoopInvariantInstructions(Loop *L, LoopInfo &LI, SmallSetVector<Instruction *, 16> &InvariantInsts) {
        SmallVector<BasicBlock *, 16> DFSBlocks;

        for (BasicBlock *BB : depth_first(L->getHeader())) {
            if (!L->contains(BB))
                continue;

            // I loop annidati vengono analizzati separatamente in ricorsione, quindi li saltiamo
            if (LI.getLoopFor(BB) != L)
                continue;

            DFSBlocks.push_back(BB);
        }

        bool FoundNewInvariant = true;

        while (FoundNewInvariant) {
            FoundNewInvariant = false;

            for (BasicBlock *BB : DFSBlocks) {
                for (Instruction &I : *BB) {
                    if (!isLoopInvariantInstruction(I, L, InvariantInsts))
                        continue;

                    if (!InvariantInsts.insert(&I))
                        continue;

                    FoundNewInvariant = true;

                    errs() << "  Loop-invariant found: ";
                    I.print(errs());
                    errs() << "\n";
                }
            }
        }
    }

    /**
     * @brief Sposta l'istuzione nel Preheader del Blocco
     * Con GetTerminator prendo l'ultima istruzione del BasicBlock
     * @param I Istruzione
     * @param Preheader 
     */
    void moveToPreheader(Instruction &I, BasicBlock *Preheader) {
        I.moveBefore(Preheader->getTerminator());
    }

    /**
     * @brief Effettua la code motion effettiva dopo che le condizioni sono state verificate
     * @param L Loop di Riferimento
     * @param DT Dominator Tree dei Basic Blocks
     * @param InvariantInsts vettore di tutte le istruzioni invarianti
     * @return true se la code motion è stata verificata
     * @return false altrimenti
     */
    bool hoistLoopInvariantInstructions(Loop *L, DominatorTree &DT, SmallSetVector<Instruction *, 16> &InvariantInsts) {
        BasicBlock *Preheader = L->getLoopPreheader();

        if (!Preheader) {
            errs() << "Loop without preheader, skipped.\n";
            return false;
        }

        bool Changed = false;

        for (Instruction *I : InvariantInsts) {
            // Le PHI possono essere invarianti ma non vanno hoistate fuori dal
            // loro blocco: il loro ruolo è solo quello di aiutare l'analisi.
            if (isa<PHINode>(I))
                continue;

            if (I->getParent() == Preheader)
                continue;

            if (!isSafeToMove(*I, L, DT)) {
                errs() << "  Not safe to move: ";
                I->print(errs());
                errs() << "\n";
                continue;
            }

            errs() << "  Moving to preheader: ";
            I->print(errs());
            errs() << "\n";

            moveToPreheader(*I, Preheader);
            Changed = true;
        }

        return Changed;
    }

    /**
     * @brief Processa un singolo loop.
     * @param L 
     * @param LI 
     * @param DT 
     * @return true 
     * @return false 
     */
    bool processLoop(Loop *L, LoopInfo &LI, DominatorTree &DT) {
        errs() << "Processing loop with header: ";
        L->getHeader()->printAsOperand(errs(), false);
        errs() << "\n";

        SmallSetVector<Instruction *, 16> InvariantInsts;
        collectLoopInvariantInstructions(L, LI, InvariantInsts);

        return hoistLoopInvariantInstructions(L, DT, InvariantInsts);
    }

    /**
     * @brief Processa i Loop visitando l'albero in Post-Order(come da spiegazione delle slide).
     * Prima scendo nei figli, poi passo al padre, quindi dal più interno al più esterno
     * @param L il singolo Loop che stiamo analizzando in quel momento
     * @param LI struttura LoopInfo che contiene de facto tutti i loop che ci interessano
     * @param DT dominator Tree
     * @return true se avviene poi lo spostamente nel preheader
     * @return false altrimenti
     */
    bool processLoopRecursive(Loop *L, LoopInfo &LI, DominatorTree &DT) {
        bool Changed = false;

        for (Loop *SubLoop : L->getSubLoops()) {
            if (processLoopRecursive(SubLoop, LI, DT))
                Changed = true;
        }

        if (processLoop(L, LI, DT))
            Changed = true;

        return Changed;
    }

    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
        bool Changed = false;

        LoopInfo &LI = AM.getResult<LoopAnalysis>(F);
        DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);

        errs() << "========================================\n";
        errs() << "Function: " << F.getName() << "\n";
        errs() << "========================================\n";

        if (LI.empty()) {
            errs() << "No loops found.\n\n";
            return PreservedAnalyses::all();
        }

        for (Loop *L : LI) {
            if (processLoopRecursive(L, LI, DT))
                Changed = true;
        }

        errs() << "\n";

        if (Changed)
            return PreservedAnalyses::none();

        return PreservedAnalyses::all();
    }

    static bool isRequired() {
        return true;
    }
};

}
llvm::PassPluginLibraryInfo getLoopInvariantMotionPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION,
        "LoopInvariantMotion",
        LLVM_VERSION_STRING,
        [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                     [](StringRef Name, FunctionPassManager &FPM, ArrayRef<PassBuilder::PipelineElement>) {
                    if (Name == "loop-invariant-motion") {
                        FPM.addPass(LoopInvariantMotion());
                        return true;
                    }

                    return false;
                });
        }
    };
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return getLoopInvariantMotionPluginInfo();
}
