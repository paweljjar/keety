#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

echo "=== Installing KeetScript VS Code Extension Locally ==="

# Check node & npm
if ! command -v node >/dev/null 2>&1; then
    echo "Error: 'node' is not installed or not in PATH." >&2
    exit 1
fi

if ! command -v npm >/dev/null 2>&1; then
    echo "Error: 'npm' is not installed or not in PATH." >&2
    exit 1
fi

echo "[1/3] Installing dependencies..."
cd "${EXT_DIR}"
npm install

echo "[2/3] Compiling TypeScript extension..."
npm run compile

echo "[3/3] Linking into Antigravity IDE & VS Code extensions directories..."
for EXT_ROOT in "${HOME}/.antigravity-ide/extensions" "${HOME}/.vscode/extensions"; do
    mkdir -p "${EXT_ROOT}"
    TARGET_LINK="${EXT_ROOT}/keet-lang.keetscript-0.1.0"
    rm -rf "${TARGET_LINK}"
    ln -s "${EXT_DIR}" "${TARGET_LINK}"
    echo "  -> Linked: ${TARGET_LINK} -> ${EXT_DIR}"
done

echo ""
echo "=========================================================="
echo "✅ KeetScript extension installed successfully!"
echo ""
echo "To activate in Antigravity IDE / VS Code:"
echo "  1. Press Cmd+Shift+P (or Ctrl+Shift+P)"
echo "  2. Run: 'Developer: Reload Window'"
echo "  3. Open any '.keet' or '.kml' file and enjoy!"
echo "=========================================================="
