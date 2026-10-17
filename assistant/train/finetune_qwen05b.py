#!/usr/bin/env python3
"""Fine-tune Qwen2.5-0.5B-Instruct into the QMark help assistant.

Reads the chat-format JSONL produced by ``assistant/dataset/build_dataset.py``
(each line: {"messages": [system, user, assistant]}) and trains a LoRA adapter,
then merges it into the base model and saves a full HF checkpoint that
``convert_to_gguf.sh`` can turn into a GGUF.

Backends (in order of preference):
  1. Unsloth  (FastLanguageModel) - if ``unsloth`` is importable.
  2. Hugging Face transformers + peft + trl - always available fallback.

Example
-------
    python finetune_qwen05b.py \
        --output-dir train/merged_model \
        --epochs 3 --lr 2e-4 --lora-r 16 --max-seq-len 1024

Smoke test (fast, few steps, no merged save):
    python finetune_qwen05b.py --limit 32 --max-steps 2 --no-merge \
        --output-dir /tmp/opencode/smoke_model
"""
from __future__ import annotations

import argparse
import json
import os
import sys
from pathlib import Path

BASE_MODEL_DEFAULT = "Qwen/Qwen2.5-0.5B-Instruct"
HERE = Path(__file__).resolve().parent
DATASET_DEFAULT = HERE.parent / "dataset" / "qmark_help.jsonl"


def parse_args(argv=None):
    p = argparse.ArgumentParser(description="QMark assistant LoRA fine-tuning")
    p.add_argument("--base-model", default=BASE_MODEL_DEFAULT)
    p.add_argument("--dataset", default=str(DATASET_DEFAULT))
    p.add_argument("--output-dir", default=str(HERE / "merged_model"),
                   help="where the merged HF model is written")
    p.add_argument("--adapter-dir", default=str(HERE / "lora_adapter"),
                   help="where the LoRA adapter is written")
    p.add_argument("--epochs", type=float, default=3.0)
    p.add_argument("--lr", type=float, default=2e-4)
    p.add_argument("--lora-r", type=int, default=16)
    p.add_argument("--lora-alpha", type=int, default=32)
    p.add_argument("--lora-dropout", type=float, default=0.05)
    p.add_argument("--max-seq-len", type=int, default=1024)
    p.add_argument("--batch-size", type=int, default=8)
    p.add_argument("--grad-accum", type=int, default=2)
    p.add_argument("--warmup-ratio", type=float, default=0.03)
    p.add_argument("--weight-decay", type=float, default=0.0)
    p.add_argument("--logging-steps", type=int, default=10)
    p.add_argument("--save-steps", type=int, default=0,
                   help="0 = only save at the end")
    p.add_argument("--seed", type=int, default=42)
    p.add_argument("--limit", type=int, default=0, help="use only N examples (smoke test)")
    p.add_argument("--max-steps", type=int, default=-1, help="-1 = full training")
    p.add_argument("--no-merge", action="store_true", help="skip merging; keep adapter only")
    p.add_argument("--load-in-4bit", action="store_true",
                   help="QLoRA (needs bitsandbytes); default is bf16 LoRA")
    p.add_argument("--no-unsloth", action="store_true", help="force the transformers+peft+trl path")
    return p.parse_args(argv)


def load_jsonl(path: Path, limit: int = 0):
    rows = []
    with path.open(encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            obj = json.loads(line)
            msgs = obj.get("messages")
            if not msgs or len(msgs) < 2:
                continue
            rows.append({"messages": msgs})
            if limit and len(rows) >= limit:
                break
    return rows


def main(argv=None):
    args = parse_args(argv)
    import torch
    from datasets import Dataset

    ds_path = Path(args.dataset)
    if not ds_path.exists():
        sys.exit(f"dataset not found: {ds_path}")
    raw = load_jsonl(ds_path, args.limit)
    if not raw:
        sys.exit(f"no usable examples in {ds_path}")
    print(f"[data] {len(raw)} examples from {ds_path}")

    use_unsloth = False
    if not args.no_unsloth:
        try:
            import unsloth  # noqa: F401
            use_unsloth = True
        except Exception:
            use_unsloth = False

    if use_unsloth:
        run_unsloth(args, raw)
    else:
        run_transformers(args, raw)


# --------------------------------------------------------------------------- #
# Backend 1: Unsloth
# --------------------------------------------------------------------------- #
def run_unsloth(args, raw):
    print("[backend] Unsloth")
    from unsloth import FastLanguageModel
    from trl import SFTTrainer
    import inspect

    model, tokenizer = FastLanguageModel.from_pretrained(
        model_name=args.base_model,
        max_seq_length=args.max_seq_len,
        load_in_4bit=args.load_in_4bit,
        dtype=None,
    )
    model = FastLanguageModel.get_peft_model(
        model,
        r=args.lora_r,
        lora_alpha=args.lora_alpha,
        lora_dropout=args.lora_dropout,
        target_modules=["q_proj", "k_proj", "v_proj", "o_proj",
                        "gate_proj", "up_proj", "down_proj"],
        use_gradient_checkpointing="unsloth",
        random_state=args.seed,
    )
    if tokenizer.pad_token is None:
        tokenizer.pad_token = tokenizer.eos_token

    def to_text(ex):
        return {"text": tokenizer.apply_chat_template(
            ex["messages"], tokenize=False, add_generation_prompt=False)}

    ds = Dataset.from_list(raw).map(to_text, remove_columns=["messages"])
    cfg = _make_sft_config(args, tokenizer, dataset_text_field="text")
    trainer = SFTTrainer(model=model, args=cfg, train_dataset=ds,
                         processing_class=tokenizer)
    trainer.train()
    _finish(args, model, tokenizer, merge=not args.no_merge)


# --------------------------------------------------------------------------- #
# Backend 2: transformers + peft + trl
# --------------------------------------------------------------------------- #
def run_transformers(args, raw):
    print("[backend] transformers + peft + trl")
    import inspect
    import torch
    from transformers import AutoModelForCausalLM, AutoTokenizer
    from peft import LoraConfig, get_peft_model
    from trl import SFTTrainer, SFTConfig
    from datasets import Dataset

    tokenizer = AutoTokenizer.from_pretrained(args.base_model)
    if tokenizer.pad_token is None:
        tokenizer.pad_token = tokenizer.eos_token
    tokenizer.padding_side = "right"

    compute_dtype = torch.bfloat16 if torch.cuda.is_bf16_supported() else torch.float16
    model_kwargs = {"torch_dtype": compute_dtype}
    if args.load_in_4bit:
        from transformers import BitsAndBytesConfig
        model_kwargs["quantization_config"] = BitsAndBytesConfig(
            load_in_4bit=True,
            bnb_4bit_quant_type="nf4",
            bnb_4bit_compute_dtype=compute_dtype,
            bnb_4bit_use_double_quant=True,
        )
        model_kwargs["device_map"] = "auto"
    model = AutoModelForCausalLM.from_pretrained(args.base_model, **model_kwargs)
    model.config.use_cache = False

    lora = LoraConfig(
        r=args.lora_r,
        lora_alpha=args.lora_alpha,
        lora_dropout=args.lora_dropout,
        bias="none",
        task_type="CAUSAL_LM",
        target_modules=["q_proj", "k_proj", "v_proj", "o_proj",
                        "gate_proj", "up_proj", "down_proj"],
    )
    model = get_peft_model(model, lora)
    model.print_trainable_parameters()

    # Prompt-completion format so TRL masks the loss to assistant turns only.
    eos = tokenizer.eos_token or ""

    def to_prompt_completion(ex):
        msgs = ex["messages"]
        prompt = tokenizer.apply_chat_template(
            msgs[:-1], tokenize=False, add_generation_prompt=True)
        completion = msgs[-1]["content"] + eos
        return {"prompt": prompt, "completion": completion}

    ds = Dataset.from_list(raw).map(to_prompt_completion, remove_columns=["messages"])

    cfg = _make_sft_config(args, tokenizer, dataset_text_field=None,
                           completion_only_loss=True)
    trainer = SFTTrainer(model=model, args=cfg, train_dataset=ds,
                         processing_class=tokenizer)
    trainer.train()
    trainer.save_model(args.adapter_dir)
    print(f"[save] adapter -> {args.adapter_dir}")
    _finish(args, model, tokenizer, merge=not args.no_merge)


def _make_sft_config(args, tokenizer, dataset_text_field, completion_only_loss=False):
    from trl import SFTConfig
    import inspect
    import torch

    params = inspect.signature(SFTConfig.__init__).parameters
    use_bf16 = torch.cuda.is_available() and torch.cuda.is_bf16_supported()
    use_fp16 = torch.cuda.is_available() and not use_bf16
    kwargs = dict(
        output_dir=args.adapter_dir,
        num_train_epochs=args.epochs,
        learning_rate=args.lr,
        per_device_train_batch_size=args.batch_size,
        gradient_accumulation_steps=args.grad_accum,
        weight_decay=args.weight_decay,
        lr_scheduler_type="cosine",
        logging_steps=args.logging_steps,
        save_strategy=("steps" if args.save_steps else "no"),
        save_steps=(args.save_steps or 500),
        max_steps=args.max_steps,
        gradient_checkpointing=True,
        bf16=use_bf16,
        fp16=use_fp16,
        optim="adamw_torch",
        report_to=[],
        seed=args.seed,
        packing=False,
    )
    # warmup: prefer warmup_ratio, fall back to a small warmup_steps
    if "warmup_ratio" in params:
        kwargs["warmup_ratio"] = args.warmup_ratio
    elif "warmup_steps" in params:
        kwargs["warmup_steps"] = max(1, int(20 * args.warmup_ratio / 0.03))
    # max_seq_len: trl renamed max_seq_length -> max_length
    if "max_length" in params:
        kwargs["max_length"] = args.max_seq_len
    elif "max_seq_length" in params:
        kwargs["max_seq_length"] = args.max_seq_len
    if dataset_text_field is not None and "dataset_text_field" in params:
        kwargs["dataset_text_field"] = dataset_text_field
    if completion_only_loss and "completion_only_loss" in params:
        kwargs["completion_only_loss"] = True
    # keep only params this trl/hf version actually accepts
    dropped = sorted(k for k in kwargs if k not in params)
    if dropped:
        print(f"[config] dropping unsupported SFTConfig keys: {dropped}")
        kwargs = {k: v for k, v in kwargs.items() if k in params}
    return SFTConfig(**kwargs)


def _finish(args, model, tokenizer, merge: bool):
    if not merge:
        print("[done] --no-merge set; skipping merged save")
        return
    try:
        import torch
        if getattr(model, "is_loaded_in_4bit", False):
            pass
    except Exception:
        pass
    print("[merge] merging LoRA adapter into base weights ...")
    merged = model.merge_and_unload() if hasattr(model, "merge_and_unload") else model
    out = Path(args.output_dir)
    out.mkdir(parents=True, exist_ok=True)
    merged.save_pretrained(str(out), safe_serialization=True)
    tokenizer.save_pretrained(str(out))
    print(f"[save] merged model -> {out}")


if __name__ == "__main__":
    main()
