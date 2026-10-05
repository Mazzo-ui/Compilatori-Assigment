#!/bin/bash

set -euo pipefail

if [ $# -eq 0 ]; then
  echo "Uso: $0 <nome_file.cpp>"
  echo "Esempio: $0 test_loop_invariant_no_motion.cpp"
  exit 1
fi

SRC="$1"

if [ ! -f "$SRC" ]; then
  echo "Errore: file '$SRC' non trovato!"
  exit 1
fi

BASE="${SRC%.cpp}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

export PATH="$HOME/tools/llvm-19.1.7/bin:$PATH"

echo "=========================================================="
echo "  Esecuzione singolo test: $SRC"
echo "=========================================================="

echo "[1] Generating LLVM IR from $SRC"
clang -O0 -Xclang -disable-O0-optnone -emit-llvm -S -c "$SRC" -o "${BASE}.O0.ll"

echo "[2] Running mem2reg and loop-simplify"
opt -passes='mem2reg,loop-simplify' "${BASE}.O0.ll" -S -o "${BASE}.m2r.ll"

echo "[3] Running LoopInvariantMotion"
opt -S \
  -load-pass-plugin ../BUILD/libLoopInvariantMotion.so \
  -p loop-invariant-motion \
  "${BASE}.m2r.ll" \
  -o "${BASE}.optimized.ll"

echo "=========================================================="
echo "--- DIFF: mem2reg (ingresso) vs optimized (dopo LICM) ---"
echo "=========================================================="
diff -u "${BASE}.m2r.ll" "${BASE}.optimized.ll" || true

echo ""
echo "Completato. File intermedi generati per $SRC:"
echo " - ${BASE}.m2r.ll"
echo " - ${BASE}.optimized.ll"