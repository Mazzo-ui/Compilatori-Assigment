# Assignment 4 - Loop Fusion

Questo progetto implementa un `FunctionPass` LLVM per l'**Assignment 4: Loop Fusion**.
Il pass parte dal lavoro precedente su `LoopInfo`, `DominatorTree`, `PostDominatorTree`,
`ScalarEvolution` e `DependenceAnalysis`.

Il nome del pass è:

```bash
loop-fusion-pass
```

## Struttura

```text
Assignment4_LoopFusion/
├── README.md
├── src/
│   ├── CMakeLists.txt
│   └── LoopFusionPass.cpp
└── test/
    ├── test_fusion.c
    └── manual_fusion.ll
```

## Cosa controlla il pass

Per ogni coppia di loop top-level consecutivi controlla le quattro condizioni viste a lezione:

1. **Adiacenza**: l'exit block del primo loop coincide con il preheader del secondo loop, oppure punta direttamente al preheader del secondo loop senza istruzioni intermedie utili.
2. **Stesso trip count**: confronto tramite `ScalarEvolution` usando `getBackedgeTakenCount`.
3. **Control-flow equivalence**: il primo loop domina il secondo e il secondo postdomina il primo.
4. **Assenza di dipendenze negative**: controllo conservativo tramite `DependenceAnalysis`.

Se tutte le condizioni sono vere, il pass prova a fondere i loop. La trasformazione automatica è volutamente prudente e gestisce il caso didattico più semplice: due loop canonicali single-block.

## Compilazione

Dalla cartella `src`:

```bash
mkdir -p build
cd build
cmake ..
make
```

Il plugin generato si chiamerà di solito:

```text
LoopFusionPass.so
```

oppure, su alcune installazioni:

```text
libLoopFusionPass.so
```

## Esecuzione sul test manuale

Dal build directory:

```bash
opt -S -load-pass-plugin ./LoopFusionPass.so \
  -passes="loop-fusion-pass" \
  ../../test/manual_fusion.ll \
  -o ../../test/manual_fused.ll
```

Se il tuo sistema genera `libLoopFusionPass.so`, usa:

```bash
opt -S -load-pass-plugin ./libLoopFusionPass.so \
  -passes="loop-fusion-pass" \
  ../../test/manual_fusion.ll \
  -o ../../test/manual_fused.ll
```

## Esecuzione partendo da C

Genera l'IR non ottimizzato:

```bash
clang -S -O0 -emit-llvm -Xclang -disable-O0-optnone \
  test_fusion.c -o test_fusion.ll
```

Applica alcune normalizzazioni utili:

```bash
opt -S -passes="mem2reg,loop-simplify,lcssa" \
  test_fusion.ll -o test_fusion.prepared.ll
```

Esegui il pass:

```bash
opt -S -load-pass-plugin ../src/build/LoopFusionPass.so \
  -passes="loop-fusion-pass" \
  test_fusion.prepared.ll -o test_fusion.out.ll
```

## Nota importante

La loop fusion completa in LLVM è una trasformazione complessa. Questo progetto implementa una versione didattica e conservativa, pensata per mostrare chiaramente:

- recupero di `LoopInfo`;
- uso di `DominatorTree` e `PostDominatorTree`;
- uso di `ScalarEvolution` per il trip count;
- uso di `DependenceAnalysis` per evitare casi pericolosi;
- modifica degli usi della induction variable;
- modifica del CFG in un caso semplice.

Per loop più complessi il pass stampa il motivo per cui non applica la trasformazione.
