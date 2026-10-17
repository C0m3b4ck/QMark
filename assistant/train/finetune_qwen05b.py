#!/usr/bin/env python3
import os
from pathlib import Path

# Minimal skeleton; actual fine-tuning will use transformers/peft/trl if installed
DATASET = Path(__file__).resolve().parent.parent / "dataset" / "qmark_help.jsonl"
OUT = Path(__file__).resolve().parent / "finetuned"

print("DATASET:", DATASET)
print("OUT:", OUT)
print("Note: install torch/transformers/peft/trl/datasets/accelerate/sentencepiece/unsloth if training")
