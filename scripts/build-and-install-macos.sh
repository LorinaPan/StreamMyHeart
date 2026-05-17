#!/bin/zsh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
OBS_PLUGIN_DIR="${HOME}/Library/Application Support/obs-studio/plugins"

PRESET="macos"
CONFIG="RelWithDebInfo"
BUILD_DIR="${REPO_ROOT}/build_macos"

usage() {
  cat <<'EOF'
Usage: scripts/build-and-install-macos.sh [--dev] [--debug] [--preset <name>] [--skip-configure]

Builds the macOS OBS plugin and installs it into:
  ~/Library/Application Support/obs-studio/plugins

Options:
  --dev             Use the `macos-dev` preset (RelWithDebInfo + debug features)
  --debug           Use the `macos-debug` preset and Debug config
  --preset <name>   Override the CMake preset explicitly
  --skip-configure  Skip the configure step and only build/install
  -h, --help        Show this help
EOF
}

SKIP_CONFIGURE=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --dev)
      PRESET="macos-dev"
      CONFIG="RelWithDebInfo"
      BUILD_DIR="${REPO_ROOT}/build_macos_dev"
      shift
      ;;
    --debug)
      PRESET="macos-debug"
      CONFIG="Debug"
      BUILD_DIR="${REPO_ROOT}/build_macos_debug"
      shift
      ;;
    --preset)
      PRESET="${2:-}"
      if [[ -z "${PRESET}" ]]; then
        echo "Missing value for --preset" >&2
        exit 1
      fi
      case "${PRESET}" in
        macos)
          CONFIG="RelWithDebInfo"
          BUILD_DIR="${REPO_ROOT}/build_macos"
          ;;
        macos-dev)
          CONFIG="RelWithDebInfo"
          BUILD_DIR="${REPO_ROOT}/build_macos_dev"
          ;;
        macos-debug)
          CONFIG="Debug"
          BUILD_DIR="${REPO_ROOT}/build_macos_debug"
          ;;
        *)
          echo "Unsupported preset '${PRESET}'. Expected 'macos', 'macos-dev', or 'macos-debug'." >&2
          exit 1
          ;;
      esac
      shift 2
      ;;
    --skip-configure)
      SKIP_CONFIGURE=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

if [[ ${SKIP_CONFIGURE} -eq 0 ]]; then
  echo "==> Configuring with preset '${PRESET}'"
  cmake --preset "${PRESET}"
fi

echo "==> Building with preset '${PRESET}'"
cmake --build --preset "${PRESET}"

PLUGIN_BUNDLE="${BUILD_DIR}/${CONFIG}/stream-my-heart.plugin"

if [[ ! -d "${PLUGIN_BUNDLE}" ]]; then
  echo "Built plugin bundle not found at:" >&2
  echo "  ${PLUGIN_BUNDLE}" >&2
  exit 1
fi

mkdir -p "${OBS_PLUGIN_DIR}"
rm -rf "${OBS_PLUGIN_DIR}/stream-my-heart.plugin"

echo "==> Installing plugin to OBS user plugins folder"
cp -R "${PLUGIN_BUNDLE}" "${OBS_PLUGIN_DIR}/"

echo "==> Done"
echo "Installed bundle:"
echo "  ${OBS_PLUGIN_DIR}/stream-my-heart.plugin"
