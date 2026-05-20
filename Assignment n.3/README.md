# Assignment 3 - Loop-Invariant Code Motion

Questo progetto implementa un passo LLVM per il terzo assignment sul tema **Loop-Invariant Code Motion**.

Il passo si chiama:

```bash
loop-invariant-motion
```

Non è stato chiamato `licm`, perché LLVM possiede già un passo ufficiale con quell'acronimo.

## Struttura del progetto

```text
Assignment3_LICM/
├── README.md
├── src/
│   ├── CMakeLists.txt
│   └── LoopInvariantCodeMotion.cpp
└── test/
    ├── test_licm.c
    └── manual_licm.ll
```

## Cosa fa il passo

Il passo recupera le analisi già usate nell'esercitazione precedente:

```cpp
LoopInfo &LI = AM.getResult<LoopAnalysis>(F);
DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);
```

Poi, per ogni loop:

1. recupera l'header;
2. recupera il preheader;
3. trova le uscite del loop;
4. identifica istruzioni loop-invariant;
5. verifica se sono candidate alla code motion;
6. le sposta nel preheader usando `Instruction::moveBefore()`.

## Criterio usato per riconoscere istruzioni loop-invariant

Un'istruzione viene considerata loop-invariant se tutti i suoi operandi sono:

- costanti;
- definiti fuori dal loop;
- oppure prodotti da un'altra istruzione già riconosciuta come loop-invariant.

Per prudenza il passo considera solo istruzioni senza effetti collaterali, come:

- operazioni binarie (`add`, `mul`, `sub`, ecc.);
- confronti (`icmp`, `fcmp`);
- cast;
- `select`;
- `getelementptr`.

Non vengono spostate istruzioni come:

- `load`;
- `store`;
- `call`;
- `phi`;
- `br`;
- `ret`.

## Condizioni per spostare l'istruzione nel preheader

Un'istruzione loop-invariant viene spostata solo se:

1. il loop ha un preheader;
2. il blocco dell'istruzione domina tutte le uscite del loop;
3. l'istruzione domina tutti i suoi usi interni al loop.

Queste condizioni rendono il passo prudente: non tutte le istruzioni loop-invariant vengono spostate, ma solo quelle che rispettano le condizioni di sicurezza richieste dall'assignment.

## Compilazione

Dalla cartella principale del progetto:

```bash
mkdir -p build
cd build
cmake -DLT_LLVM_INSTALL_DIR=/home/marco/LLVM-19.1.7-Linux-X64 ../src
make
```

Se hai già esportato la variabile `LLVM_DIR`, puoi usare:

```bash
cmake -DLT_LLVM_INSTALL_DIR=$LLVM_DIR ../src
make
```

## Generare il file LLVM IR dal C

Dalla cartella `test`:

```bash
clang -S -O0 -emit-llvm -Xclang -disable-O0-optnone test_licm.c -o test_licm.ll
opt -passes="mem2reg,loop-simplify" test_licm.ll -S -o test_licm.m2r.ll
```

`loop-simplify` è utile perché crea una forma più regolare del loop, con preheader.

## Eseguire il passo

Dalla cartella `build`:

```bash
opt -load-pass-plugin ./libLoopInvariantMotion.so \
    -passes="loop-invariant-motion" \
    ../test/test_licm.m2r.ll \
    -S -o ../test/test_licm.optimized.ll
```

Per testare il file scritto a mano:

```bash
opt -load-pass-plugin ./libLoopInvariantMotion.so \
    -passes="loop-invariant-motion" \
    ../test/manual_licm.ll \
    -S -o ../test/manual_licm.optimized.ll
```

## Cosa osservare nell'output

Nel file ottimizzato dovresti vedere alcune istruzioni spostate nel blocco `preheader`.

Esempio concettuale:

Prima:

```llvm
body:
  %x = mul i32 %a, %b
  %y = add i32 %x, %c
```

Dopo:

```llvm
preheader:
  %x = mul i32 %a, %b
  %y = add i32 %x, %c
  br label %header
```

Il pass stampa anche a schermo le istruzioni riconosciute come loop-invariant e quelle effettivamente spostate.
