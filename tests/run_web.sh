#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
"${NODE:-node}" tests/test_web.mjs "$work/vectors.json"
"${NODE:-node}" tests/test_ble_client.mjs
python3 -m grpc_tools.protoc -I Protocol --python_out="$work" Protocol/steadyview.proto
PYTHONPATH="$work" python3 tests/check_web_vectors.py "$work/vectors.json"
