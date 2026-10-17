#!/usr/bin/env bash
# Convert the fine-tuned QMark Assistant HF model into a quantized GGUF file.
#
# Produces (in assistant/release/):
#   QMark-Assistant-Qwen2.5-0.5B-Instruct.Q4_K_M.gguf
#   QMark-Assistant-Qwen2.5-0.5B-Instruct.Q4_K_M.gguf.sha256
#
# Environment overrides:
#   SRC_DIR        HF model dir (default: assistant/train/merged_model)
#   GGUF_DIR       output dir   (default: assistant/release)
#   LLAMA_CPP_DIR  llama.cpp checkout (default: /tmp/opencode/llama.cpp)
#   QUANT          quantization type (default: Q4_K_M)
#   PYTHON         python interpreter with gguf/torch/transformers (default: python3)
set -euo pipefail

BASE_DIR="$(cd "$(dirname "$0")/.." && pwd)"
SRC_DIR="${SRC_DIR:-$BASE_DIR/train/merged_model}"
GGUF_DIR="${GGUF_DIR:-$BASE_DIR/release}"
LLAMA_CPP_DIR="${LLAMA_CPP_DIR:-/tmp/opencode/llama.cpp}"
QUANT="${QUANT:-Q4_K_M}"
PYTHON="${PYTHON:-python3}"

NAME="QMark-Assistant-Qwen2.5-0.5B-Instruct.${QUANT}"
GGUF="$GGUF_DIR/$NAME.gguf"
F16="$GGUF_DIR/QMark-Assistant.f16.gguf"
mkdir -p "$GGUF_DIR"

# --- sanity checks ---------------------------------------------------------
if [ ! -f "$SRC_DIR/config.json" ]; then
  echo "ERROR: merged HF model not found at: $SRC_DIR" >&2
  echo "Run train/finetune_qwen05b.py first (or set SRC_DIR=...)." >&2
  exit 1
fi

# --- get llama.cpp ---------------------------------------------------------
if [ ! -d "$LLAMA_CPP_DIR/.git" ]; then
  echo "[1/5] cloning llama.cpp -> $LLAMA_CPP_DIR"
  git clone --depth 1 https://github.com/ggml-org/llama.cpp "$LLAMA_CPP_DIR"
else
  echo "[1/5] reusing llama.cpp at $LLAMA_CPP_DIR"
fi

# --- python conversion deps ------------------------------------------------
echo "[2/5] ensuring python conversion deps (gguf)"
"$PYTHON" -c "import gguf" 2>/dev/null || "$PYTHON" -m pip install --quiet gguf

# --- convert HF -> GGUF (f16) ---------------------------------------------
echo "[3/5] converting HF model to GGUF (f16)"
"$PYTHON" "$LLAMA_CPP_DIR/convert_hf_to_gguf.py" "$SRC_DIR" \
    --outfile "$F16" --outtype f16

# --- build llama-quantize --------------------------------------------------
echo "[4/5] building llama-quantize (CPU)"
cmake -S "$LLAMA_CPP_DIR" -B "$LLAMA_CPP_DIR/build" \
    -DGGML_CUDA=OFF -DLLAMA_CURL=OFF -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$LLAMA_CPP_DIR/build" --target llama-quantize -j "$(nproc)"

QUANTIZE_BIN="$LLAMA_CPP_DIR/build/bin/llama-quantize"
[ -x "$QUANTIZE_BIN" ] || QUANTIZE_BIN="$LLAMA_CPP_DIR/build/llama-quantize"

# --- quantize --------------------------------------------------------------
echo "[5/5] quantizing to $QUANT"
"$QUANTIZE_BIN" "$F16" "$GGUF" "$QUANT"

sha256sum "$GGUF" | awk '{print $1}' > "${GGUF}.sha256"
rm -f "$F16"

echo
echo "GGUF : $GGUF"
echo "SHA  : $(cat "${GGUF}.sha256")"
echo "SIZE : $(du -h "$GGUF" | cut -f1)"
