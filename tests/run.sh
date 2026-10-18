#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
nanopb=Vendor/Middlewares/Third_Party/nanopb
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
  -I Vendor/Middlewares/Third_Party/tinycrypt/lib/include -I Core/Inc -I Protocol -I "$nanopb" \
  tests/test_protocol.c tests/mock_auth.c Core/Src/control_auth.c Vendor/Middlewares/Third_Party/tinycrypt/lib/source/sha256.c Vendor/Middlewares/Third_Party/tinycrypt/lib/source/hmac.c Vendor/Middlewares/Third_Party/tinycrypt/lib/source/utils.c tests/mock_flash.c Core/Src/settings_store.c Core/Src/steadyview.c Core/Src/control_protocol.c \
  Protocol/steadyview.pb.c "$nanopb/pb_common.c" "$nanopb/pb_encode.c" "$nanopb/pb_decode.c" \
  -o "$test_dir/protocol"
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
  -I tests/transport_mocks -I Core/Inc -I STM32_WPAN/App -I Protocol -I "$nanopb" \
  tests/test_transport.c -o "$test_dir/transport"
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
  -I Core/Inc -I tests tests/test_settings.c tests/mock_flash.c Core/Src/settings_store.c \
  -o "$test_dir/settings"
# LeakSanitizer cannot operate under the sandbox's ptrace; ASan/UBSan remain enabled.
ASAN_OPTIONS=detect_leaks=0 "$test_dir/protocol"
ASAN_OPTIONS=detect_leaks=0 "$test_dir/transport"

ASAN_OPTIONS=detect_leaks=0 "$test_dir/settings"

python3 tests/test_ble_codec.py
