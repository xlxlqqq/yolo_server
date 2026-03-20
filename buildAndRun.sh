#!/usr/bin/env bash
set -e

# ===== 获取脚本真实路径（解决 sudo / 软链接 / 相对路径问题）=====
SCRIPT_PATH="$(readlink -f "${BASH_SOURCE[0]}")"
SCRIPT_DIR="$(dirname "${SCRIPT_PATH}")"

# 假设 scripts/ 在项目根目录下
PROJECT_ROOT="$(realpath "${SCRIPT_DIR}/.")"

BUILD_DIR="${PROJECT_ROOT}/build"
BIN="${BUILD_DIR}/storage_server"
CONFIG1="${PROJECT_ROOT}/config/server_8080.conf"

echo "[INFO] script path   : ${SCRIPT_PATH}"
echo "[INFO] project root  : ${PROJECT_ROOT}"
echo "[INFO] build dir     : ${BUILD_DIR}"
echo "[INFO] config file   : ${CONFIG1}"

# ===== 构建 =====
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

cmake ..
# 降低并发编译数量，防止内存不足(OOM)导致编译被 Kill
# 如果依然 OOM，说明内存非常有限，改为单线程编译
make -j2

# ===== 运行 =====
exec "${BIN}" --config "${CONFIG1}"
