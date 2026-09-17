#!/usr/bin/env bash
# Convenience wrapper — calls the real build script
# Dependencies are auto-detected from common paths.
# Override with: QT_DIR=... SQLITE3_DIR=... SODIUM_DIR=... ./build-win-x64.sh
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
exec "$SCRIPT_DIR/build-win-x64.sh" "$@"
