#!/usr/bin/env python3
"""Sanity-check a fine-tuned QMark Assistant model.

Loads a merged HF model (the output of ``finetune_qwen05b.py``) and runs the
held-out prompts in ``assistant/dataset/eval.jsonl``, printing each response and
a simple keyword-overlap score against the reference answer. This is a smoke
check of behaviour, not a rigorous benchmark.

    python eval_model.py --model assistant/train/merged_model
    python eval_model.py --model /tmp/opencode/qmark_merged --limit 5
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
DEFAULT_EVAL = HERE.parent / "dataset" / "eval.jsonl"


def main(argv=None):
    p = argparse.ArgumentParser()
    p.add_argument("--model", required=True, help="merged HF model dir")
    p.add_argument("--eval", default=str(DEFAULT_EVAL))
    p.add_argument("--limit", type=int, default=0)
    p.add_argument("--max-new-tokens", type=int, default=192)
    args = p.parse_args(argv)

    import torch
    from transformers import AutoModelForCausalLM, AutoTokenizer

    tok = AutoTokenizer.from_pretrained(args.model)
    model = AutoModelForCausalLM.from_pretrained(
        args.model,
        torch_dtype=torch.bfloat16 if torch.cuda.is_available() else torch.float32,
        device_map="auto" if torch.cuda.is_available() else None,
    )
    model.eval()

    rows = [json.loads(l) for l in Path(args.eval).read_text(encoding="utf-8").splitlines() if l.strip()]
    if args.limit:
        rows = rows[:args.limit]

    for i, row in enumerate(rows, 1):
        msgs = row["messages"]
        prompt_msgs = [msgs[0], msgs[1]]
        ref = msgs[2]["content"]
        text = tok.apply_chat_template(prompt_msgs, tokenize=False, add_generation_prompt=True)
        enc = tok(text, return_tensors="pt").to(model.device)
        with torch.no_grad():
            out = model.generate(**enc, max_new_tokens=args.max_new_tokens,
                                 do_sample=False, pad_token_id=tok.eos_token_id)
        gen = tok.decode(out[0][enc["input_ids"].shape[1]:], skip_special_tokens=True).strip()
        ref_words = {w for w in ref.lower().split() if len(w) > 4}
        gen_words = set(gen.lower().split())
        overlap = len(ref_words & gen_words) / max(1, len(ref_words))
        print(f"\n===== [{i}] {msgs[1]['content']}")
        print(f"  ref : {ref[:160]}")
        print(f"  got : {gen[:400]}")
        print(f"  keyword-overlap: {overlap:.0%}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
