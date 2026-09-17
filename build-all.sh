#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════
#  QMark — Build All Targets
#  Runs each platform build script in sequence.
#  Stops on first failure.
# ═══════════════════════════════════════════════════════════════════
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "═══════════════════════════════════════════════════════════════"
echo "  QMark — Building All Targets"
echo "═══════════════════════════════════════════════════════════════"
echo ""

TARGETS=(
    "build-linux-x64.sh:Linux x64"
    "build-linux-x86.sh:Linux x86"
    "build-win-x64.sh:Windows x64"
    "build-win-x86.sh:Windows x86"
)

PASSED=0
FAILED=0
SKIPPED=0

for entry in "${TARGETS[@]}"; do
    script="${entry%%:*}"
    name="${entry##*:}"
    script_path="$SCRIPT_DIR/$script"

    echo "───────────────────────────────────────────────────────────────"
    echo "  Building: $name ($script)"
    echo "───────────────────────────────────────────────────────────────"

    if [ ! -x "$script_path" ]; then
        echo "  SKIP: $script not found"
        SKIPPED=$((SKIPPED + 1))
        continue
    fi

    if "$script_path"; then
        PASSED=$((PASSED + 1))
    else
        echo "  FAILED: $name build failed"
        FAILED=$((FAILED + 1))
        exit 1
    fi
    echo ""
done

echo "═══════════════════════════════════════════════════════════════"
echo "  Results: $PASSED passed, $FAILED failed, $SKIPPED skipped"
echo "═══════════════════════════════════════════════════════════════"

# Show built binaries
echo ""
echo "Built binaries:"
for f in "$SCRIPT_DIR"/QMark*; do
    if [ -f "$f" ]; then
        echo "  $(ls -lh "$f" | awk '{print $5, $NF}')"
    fi
done

exit $FAILED
