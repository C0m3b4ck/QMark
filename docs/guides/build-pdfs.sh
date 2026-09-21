#!/usr/bin/env bash
#
# build-pdfs.sh — Build PDF versions of the QMark guides.
#
# Pipeline: Markdown -> HTML (pandoc) -> PDF (headless Chromium).
# Chromium is used instead of pdflatex because the installed TeX is broken
# and/or lacks a Unicode engine with Polish glyph support; Chromium renders
# UTF-8 (Polish diacritics) correctly out of the box.
#
# Requirements: pandoc, chromium (or chromium-browser, or CHROME env var).
#
# Usage:   ./build-pdfs.sh            # build all four PDFs
#          ./build-pdfs.sh QMark_User_Guide_EN   # build just one (base name)

set -euo pipefail
cd "$(dirname "$0")"

if command -v chromium >/dev/null 2>&1; then
    CHROME="${CHROME:-chromium}"
elif command -v chromium-browser >/dev/null 2>&1; then
    CHROME="${CHROME:-chromium-browser}"
else
    echo "ERROR: chromium not found. Set CHROME=/path/to/chromium." >&2
    exit 1
fi
if ! command -v pandoc >/dev/null 2>&1; then
    echo "ERROR: pandoc not found." >&2
    exit 1
fi

BUILD=".build"
mkdir -p "$BUILD"

GUIDES=(
    QMark_User_Guide_EN
    QMark_User_Guide_PL
    QMark_SuperAdmin_Guide_EN
    QMark_SuperAdmin_Guide_PL
)

targets=("$@")
[ ${#targets[@]} -eq 0 ] && targets=("${GUIDES[@]}")

for base in "${targets[@]}"; do
    src="${base}.md"
    if [ ! -f "$src" ]; then
        echo "ERROR: $src not found." >&2
        exit 1
    fi

    echo "→ $base"
    # --section-divs lets the CSS start each chapter (level-2 section) on
    # a new page; --toc generates the table of contents.
    pandoc "$src" \
        -o "$BUILD/${base}.html" \
        --standalone \
        --embed-resources \
        --toc --toc-depth=2 \
        --section-divs \
        --css=pdf-style.css \
        --metadata pagetitle="$base"

    url="file://$(pwd)/$BUILD/${base}.html"
    profile="$BUILD/cr-${base}"
    "$CHROME" --headless --disable-gpu --no-sandbox \
        --user-data-dir="$profile" \
        --no-pdf-header-footer \
        --print-to-pdf="${base}.pdf" \
        "$url" >/dev/null 2>&1

    if [ -f "${base}.pdf" ]; then
        echo "  ✓ ${base}.pdf ($(du -h "${base}.pdf" | cut -f1))"
    else
        echo "ERROR: failed to build ${base}.pdf" >&2
        exit 1
    fi
done

echo
echo "Done. PDFs are next to the .md files in docs/guides/."