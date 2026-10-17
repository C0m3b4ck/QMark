# QMark Assistant — fine-tuning & local inference

A tiny, **QMark-scoped** help assistant. It knows *only* how to use QMark (a Qt6
point-of-sale app for school shops) and politely refuses anything off-topic. This
directory contains everything needed to reproduce it from scratch: the system
prompt, the dataset generator, the training script, and the GGUF conversion
script.

Base model: **`Qwen/Qwen2.5-0.5B-Instruct`** (Apache-2.0), fine-tuned with LoRA
and quantized to **Q4_K_M** GGUF for CPU-only inference.

---

## What you get

| Artifact | Path | Notes |
| --- | --- | --- |
| System prompt | `assistant/system_prompt.txt` | strict QMark-only scope + refusals |
| Training data | `assistant/dataset/qmark_help.jsonl` | 934 examples, ChatML messages |
| Eval data | `assistant/dataset/eval.jsonl` | 20 held-out prompts |
| Dataset generator | `assistant/dataset/build_dataset.py` | reproduces both JSONL files |
| Training script | `assistant/train/finetune_qwen05b.py` | LoRA / QLoRA → merged HF model |
| GGUF converter | `assistant/train/convert_to_gguf.sh` | llama.cpp → Q4_K_M + sha256 |
| **Final model** | `assistant/release/QMark-Assistant-Qwen2.5-0.5B-Instruct.Q4_K_M.gguf` | ~400 MB |
| **Checksum** | `assistant/release/QMark-Assistant-Qwen2.5-0.5B-Instruct.Q4_K_M.gguf.sha256` | hex sha256 |

Dataset language mix: **~60 % English / ~40 % Polish** (QMark ships in both
languages). Each training line is:

```json
{"messages": [
  {"role": "system",    "content": "<contents of system_prompt.txt>"},
  {"role": "user",      "content": "How do I sell an item?"},
  {"role": "assistant", "content": "Open Sell -> Sell Item (POS) ..."}
]}
```

---

## Prerequisites

- **Python 3.11 or 3.12** (3.13/3.14 lack some wheels — notably `torch`).
- **GPU is optional.** Training this 0.5B model is comfortable on a single
  8 GB GPU (e.g. RTX 4060), but it also fits on CPU (slower).
- Build tools for llama.cpp: `git`, `cmake`, `make`, a C++ compiler.
- ~5 GB free disk for training + conversion (the merged model and an
  intermediate f16 GGUF are ~1 GB each before they are deleted).

---

## Reproduce from scratch

All commands assume you start in the repository root.

### 0) Create a Python environment

```bash
# Option A: uv (fast, handles Python 3.12 for you)
uv venv --python 3.12 .venv-assistant
source .venv-assistant/bin/activate

# Option B: plain venv
python3.12 -m venv .venv-assistant
source .venv-assistant/bin/activate
```

### 1) Install dependencies

```bash
# CUDA build of torch (change cu128 to match your driver / use cpu wheels if no GPU):
pip install torch --index-url https://download.pytorch.org/whl/cu128
pip install transformers peft trl datasets accelerate sentencepiece
# Optional, for QLoRA (4-bit) only:
# pip install bitsandbytes
```

### 2) (Re)build the dataset

```bash
python assistant/dataset/build_dataset.py
# -> writes assistant/dataset/qmark_help.jsonl (934) and assistant/dataset/eval.jsonl (20)
```

### 3) Fine-tune

```bash
python assistant/train/finetune_qwen05b.py \
  --epochs 3 \
  --lr 2e-4 \
  --lora-r 16 \
  --max-seq-len 1024 \
  --batch-size 8 \
  --grad-accum 2 \
  --output-dir assistant/train/merged_model
```

Defaults are already `epochs=3, lr=2e-4, lora-r=16, max-seq-len=1024`, so plain
`python assistant/train/finetune_qwen05b.py` works too. The script:

1. loads `Qwen/Qwen2.5-0.5B-Instruct`,
2. trains a LoRA adapter (masked to assistant turns only),
3. merges the adapter and saves a full HF model to `--output-dir`.

Useful flags: `--load-in-4bit` (QLoRA, needs `bitsandbytes`), `--no-unsloth`
(force the transformers+peft+trl backend), `--limit N --max-steps 2` (fast smoke
test), `--no-merge` (keep the adapter only).

### 4) Convert to GGUF (Q4_K_M)

```bash
bash assistant/train/convert_to_gguf.sh
# -> assistant/release/QMark-Assistant-Qwen2.5-0.5B-Instruct.Q4_K_M.gguf
# -> assistant/release/QMark-Assistant-Qwen2.5-0.5B-Instruct.Q4_K_M.gguf.sha256
```

The script clones and builds llama.cpp (default `/tmp/opencode/llama.cpp`),
converts the merged model to f16 GGUF with `convert_hf_to_gguf.py`, builds
`llama-quantize`, quantizes to `Q4_K_M`, and writes the sha256. Override paths
with env vars, e.g.:

```bash
SRC_DIR=assistant/train/merged_model \
GGUF_DIR=assistant/release \
LLAMA_CPP_DIR=/opt/llama.cpp \
QUANT=Q4_K_M \
bash assistant/train/convert_to_gguf.sh
```

---

## Running the model (target machine)

The target is a **Windows 10 ThinkPad, 8 GB RAM, CPU only**. The Q4_K_M GGUF is
~400 MB and runs comfortably in a few hundred MB of RAM with llama.cpp — no GPU
needed.

Grab a prebuilt `llama.cpp` release for Windows (`llama-server.exe` /
`llama-cli.exe`) from the llama.cpp releases page, then:

```powershell
# Interactive chat, QMark system prompt applied:
llama-cli.exe -m QMark-Assistant-Qwen2.5-0.5B-Instruct.Q4_K_M.gguf ^
  -sys "$(Get-Content assistant\system_prompt.txt -Raw)" ^
  -p "How do I turn on barcode scanner mode?" -n 256

# Or as a small local HTTP server the app can call:
llama-server.exe -m QMark-Assistant-Qwen2.5-0.5B-Instruct.Q4_K_M.gguf --port 8081 -c 2048
```

Always pass `system_prompt.txt` as the system message; it is what keeps the
assistant on-topic.

---

## Hosting as a GitHub release asset

The app can download the model on first use (with double confirmation). Upload
both files as release assets with **exact** names:

- `QMark-Assistant-Qwen2.5-0.5B-Instruct.Q4_K_M.gguf`
- `QMark-Assistant-Qwen2.5-0.5B-Instruct.Q4_K_M.gguf.sha256`

The `.sha256` file contains only the hex digest; the app verifies the download
against it before use. Any file host that serves the same two names works too,
as long as the app's download URL is pointed at it.

---

## License / attribution

- Base model: Qwen2.5-0.5B-Instruct (Apache-2.0).
- QMark itself: Apache-2.0.
- The dataset and scripts in this directory are provided for use with QMark.
