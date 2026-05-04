# Guida alla build e ai test

## Struttura del progetto

```text
assign/
├─ build/
├─ source/
│  ├─ CMakeLists.txt
│  └─ AllInOne.cpp
└─ test/
   ├─ test.cpp
   └─ test.ll
```

## Comandi

Imposta il percorso di LLVM e aggiungilo al `PATH`:

```bash
export LLVM_DIR=/user/posizione/bin/di/llvm/LLVM-19.1.7-Linux-X64
export PATH=$LLVM_DIR/bin:$PATH
```

Spostati nella cartella `build`, configura CMake e compila il plugin:

```bash
cd build
cmake -DLT_LLVM_INSTALL_DIR=$LLVM_DIR ../source
make
```

Genera il file LLVM IR di test a partire da `test.cpp`:

```bash
clang++ -O0 -Xclang -disable-O0-optnone -S -emit-llvm ../test/test.cpp -o ../test/test.ll
```

Esegui i pass del plugin sul file di test:

```bash
opt -load-pass-plugin ./libAllInOne.so -passes="mem2reg,algebraic-identity,strength-reduction,multi-inst-opt" ../test/test.ll -S -o ../test/outputOptimized.ll
```

## Note

- `mem2reg` è utile perché elimina molte istruzioni `alloca`, `load` e `store`, aiutando `multi-inst-opt` a riconoscere i pattern.
-  per generare il file llvm da test utilizziamo O0 per tradurre 1 a 1 la funzione senza alcun tipo di ottimizzazione.
