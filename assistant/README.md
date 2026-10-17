# QMark Assistant (fine-tuning + local inference)

This directory contains the dataset, training script, system prompt, and build instructions for a tiny, QMark-scoped help assistant.

## Quick Start

1. Set up Python environment (use Python 3.11 or 3.12; 3.14 lacks some wheels).
2. Install dependencies:
   ```bash
   pip install torch --index-url https://download.pytorch.org/whl/cu121
   pip install transformers peft trl datasets accelerate sentencepiece
   ```
   (Alternatively use uv/conda.)

3. Fine-tune and merge:
   ```bash
   python3 train/finetune_qwen05b.py
   ```
   Merged model goes to `train/merged_model/`.

4. Convert to GGUF Q4_K_M:
   ```bash
   ./train/convert_to_gguf.sh
   ```
   Outputs:
   - `release/QMark-Assistant-Qwen2.5-0.5B-Instruct.Q4_K_M.gguf`
   - `release/QMark-Assistant-Qwen2.5-0.5B-Instruct.Q4_K_M.gguf.sha256`

## Files

- `system_prompt.txt` — strict system prompt (QMark-only, refusal for off-topic)
- `dataset/qmark_help.jsonl` — training examples (~416 examples EN/PL)
- `dataset/eval.jsonl` — eval examples (~20)
- `train/finetune_qwen05b.py` — training script skeleton
- `train/convert_to_gguf.sh` — builds llama.cpp (if missing) and converts to GGUF Q4_K_M

## App integration

The QMark app looks for the GGUF in the app data directory under `assistant/` (or the configured path in settings). On first use with double confirmation, the app downloads the GGUF + sha256 from a GitHub release asset (or configured URL). The app can run inference via `llama-server` or `llama-cli`.

Asset naming is exact: `QMark-Assistant-Qwen2.5-0.5B-Instruct.Q4_K_M.gguf`
