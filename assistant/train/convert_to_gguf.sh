#!/usr/bin/env bash
# Convert fine-tuned model to GGUF (llama.cpp)
set -euo pipefail

BASE_DIR="$(cd "$(dirname "$0")/.." && pwd)"
SRC_DIR="$BASE_DIR/train/merged_model"
GGUF_DIR="$BASE_DIR/release"
mkdir -p "$GGUF_DIR"

GGUF="$GGUF_DIR/QMark-Assistant-Qwen2.5-0.5B-Instruct.Q4_K_M.gguf"
LLAMA_CPP_DIR="${LLAMA_CPP_DIR:-/tmp/opencode/llama.cpp}"

if [ ! -f "$SRC_DIR/model.safetensors" ] && [ ! -f "$SRC_DIR/config.json" ]; then
  echo "Merged model not found at $SRC_DIR" >&2
  exit 1
fi

if [ ! -d "$LLAMA_CPP_DIR" ]; then
  git clone --depth 1 https://github.com/ggml-org/llama.cpp "$LLAMA_CPP_DIR"
fi
make -C "$LLAMA_CPP_DIR" -j "$(nproc)" llama-gguf-convert llama-quantize 2>/dev/null || true

"$LLAMA_CPP_DIR/llama-gguf-convert" "$SRC_DIR" "$GGUF_DIR/model.f16.gguf" --vocab-type hf
"$LLAMA_CPP_DIR/llama-quantize" "$GGUF_DIR/model.f16.gguf" "$GGUF" Q4_K_M

sha256sum "$GGUF" | awk '{print $1}' > "${GGUF}.sha256"
echo "GGUF: $GGUF"
cat "${GGUF}.sha256"
