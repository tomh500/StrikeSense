import argparse
import datetime as dt

ALPHABET = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_"
SECRET = b"StrikeSenseOEMKey2026"

TYPE_ALIASES = {
    "24h": "24h",
    "7d": "7d",
    "1m": "1m",
    "6m": "6m",
    "1y": "1y",
    "10y": "10y",
    "50y": "50y",
    "forever": "forever",
}


def fnv1a(text: str) -> int:
    value = 2166136261
    for byte in text.encode("utf-8"):
        value ^= byte
        value = (value * 16777619) & 0xFFFFFFFF
    return value


def xor_payload(raw: bytes) -> bytes:
    data = bytearray(raw)
    for index, byte in enumerate(data):
        data[index] = byte ^ SECRET[index % len(SECRET)] ^ ((index * 29 + 17) & 0xFF)
    return bytes(data)


def encode64(raw: bytes) -> str:
    output = []
    value = 0
    bits = -6
    for byte in raw:
        value = (value << 8) + byte
        bits += 8
        while bits >= 0:
            output.append(ALPHABET[(value >> bits) & 63])
            bits -= 6
    if bits > -6:
        output.append(ALPHABET[((value << 8) >> (bits + 8)) & 63])
    return "".join(output)


def normalize_type(kind: str) -> str:
    normalized = TYPE_ALIASES.get(kind.strip().lower())
    if not normalized:
        raise ValueError("Unknown type. Use one of: 24h, 7d, 1m, 6m, 1y, 10y, 50y, forever")
    return normalized


def make_key(stamp: str, kind: str) -> str:
    if len(stamp) != 12 or not stamp.isdigit():
        raise ValueError("Stamp must be YYYYMMDDHHMM")
    normalized = normalize_type(kind)
    body = f"SSOEM1|{stamp}|{normalized}"
    check = format(fnv1a(f"{body}|StrikeSense"), "x")
    return encode64(xor_payload(f"{body}|{check}".encode("utf-8")))


def main() -> None:
    parser = argparse.ArgumentParser(description="StrikeSense OEM key generator")
    parser.add_argument("--stamp", default=dt.datetime.now().strftime("%Y%m%d%H%M"))
    parser.add_argument("--type", default="24h")
    args = parser.parse_args()

    try:
        key = make_key(args.stamp, args.type)
        print("Stamp :", args.stamp)
        print("Type  :", normalize_type(args.type))
        print("Key   :", key)
    except Exception as exc:
        print("Error :", exc)

    input("Press Enter to exit...")


if __name__ == "__main__":
    main()
