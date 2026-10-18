import importlib.util
from pathlib import Path
spec = importlib.util.spec_from_file_location("ble_codec", Path(__file__).resolve().parents[1] / "Protocol/ble_codec.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
for size in (1, 16, 17, 263, 512):
    payload = bytes(i % 256 for i in range(size))
    count = (size + 15) // 16
    assembler = module.ResponseAssembler()
    for index in range(count):
        result = assembler.feed(bytes([0xa1, 255, index, count]) + payload[index*16:(index+1)*16])
        assert (result == payload) if index == count-1 else (result is None)
for invalid in (b"", b"\x00"*20, bytes([0xa1,1,1,2,0]), bytes([0xa1,1,0,0,0])):
    try:
        module.ResponseAssembler().feed(invalid)
    except ValueError:
        pass
    else:
        raise AssertionError("invalid frame accepted")
a = module.ResponseAssembler()
a.feed(bytes([0xa1,1,0,3]) + bytes(16))
try:
    a.feed(bytes([0xa1,1,2,3,0]))
except ValueError:
    pass
else:
    raise AssertionError("missing fragment accepted")
print("BLE response reassembly: passed")
