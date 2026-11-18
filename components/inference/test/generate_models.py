"""Generate tiny ONNX test fixtures with protobuf wire encoding; no onnx package needed."""
from pathlib import Path
import struct
import sys


def varint(n):
    out = bytearray()
    while n > 127:
        out.append((n & 127) | 128)
        n >>= 7
    return bytes(out + bytes([n]))


def integer(field, value):
    return varint(field << 3) + varint(value)


def data(field, value):
    value = value.encode() if isinstance(value, str) else value
    return varint((field << 3) | 2) + varint(len(value)) + value


def value(name, dynamic=False, dtype=1):
    dim = data(1, data(2, "batch") if dynamic else integer(1, 1)) + data(1, integer(1, 3))
    tensor = integer(1, dtype) + data(2, dim)
    return data(1, name) + data(2, data(1, tensor))


def model(dynamic=False, dtype=1, divide=False):
    node = data(1, "x") + (data(1, "zero") if divide else b"") + data(2, "y") + data(4, "Div" if divide else "Identity")
    graph = data(1, node) + data(2, "inference-test") + data(11, value("x", dynamic, dtype)) + data(12, value("y", dynamic, dtype))
    if divide:
        tensor = integer(1, 1) + integer(1, 3) + integer(2, 1) + data(4, struct.pack("fff", 0, 0, 0)) + data(8, "zero")
        graph += data(5, tensor)
    return integer(1, 8) + data(7, graph) + data(8, integer(2, 13))


root = Path(sys.argv[1])
root.mkdir(parents=True, exist_ok=True)
for name, content in {"identity": model(), "dynamic": model(dynamic=True), "integer": model(dtype=7), "nonfinite": model(divide=True)}.items():
    (root / (name + ".onnx")).write_bytes(content)
