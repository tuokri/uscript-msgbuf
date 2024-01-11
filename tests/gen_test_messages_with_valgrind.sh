#!/usr/bin/env bash

# Run UMB message generator with Valgrind on the test message set.
# NOTE: on WSL Debian:
# - fails with Valgrind 3.19.0
# - works with Valgrind 3.21.0

set +x
set -e
shopt -s nullglob

# TODO: modularize this.
# TODO: Valgrind runs for generator failure branches. (Argument handling, invalid files, etc.)

# The directory of this script.
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

UMB_GEN_EXECUTABLE_DEFAULT="${SCRIPT_DIR}/../cmake-build-debug-wsl/uscript_msgbuf_generator"
UMB_GENERATED_DIR_DEFAULT="${SCRIPT_DIR}/../cmake-build-debug-wsl/generated/"

[[ -z "${UMB_VALGRIND_BIN}" ]] && VALGRIND='valgrind' || VALGRIND="${UMB_VALGRIND_BIN}"
[[ -z "${UMB_GEN_BIN}" ]] && UMB_GEN="${UMB_GEN_EXECUTABLE_DEFAULT}" || UMB_GEN="${UMB_GEN_BIN}"
[[ -z "${UMB_VALGRIND_GEN_DIR}" ]] && GEN_DIR="${UMB_GENERATED_DIR_DEFAULT}" || GEN_DIR="${UMB_VALGRIND_GEN_DIR}"

DATA_FILES=("${SCRIPT_DIR}"/data/*.json)
OUT_DIR="${GEN_DIR}/valgrind"

mkdir -p "${OUT_DIR}"

echo "using valgrind: '${VALGRIND}'"
echo "using UMB generator: '${UMB_GEN}'"
echo "generating Valgrind run UMB output in: '${OUT_DIR}'"

${VALGRIND} --leak-check=full --track-origins=yes --error-exitcode=1 \
  "${UMB_GEN}" "${DATA_FILES[@]}" \
  --cpp-out="${OUT_DIR}" \
  --uscript-out="${OUT_DIR}"
