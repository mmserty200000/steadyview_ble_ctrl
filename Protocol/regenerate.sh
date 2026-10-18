#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
exec python3 Vendor/Middlewares/Third_Party/nanopb/generator/nanopb_generator.py \
    -I Protocol -D Protocol Protocol/steadyview.proto
