import argparse
import datetime as dt

ALPHABET = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_"
SECRET = b"StrikeSenseOEMKey2026"

TYPE_ALIASES = {
    "24h": "24h",
    "24小时": "24h",
    "7d": "7d",
    "7天": "7d",
    "1m": "1m",
    "一个月": "1m",
    "6m": "6m",
    "半年": "6m",
    "1y": "1y",
    "一年": "1y",
    "10y": "10y",
    "十年": "10y",
    "50y": "50y",
    "五十年": "50y",
    "forever": "forever",
    "永不失效": "forever",
}


def normalize_type(kind: str) -> str:
    value = TYPE_ALIASES.get(kind.strip())
    if not value:
        raise ValueError("未知期限类型")
    return value


def fnv1a(text: str) -> int:
    h = 2166136261
    for b in text.encode("utf-8"):
        h ^= b
        h = (h * 16777619) & 0xFFFFFFFF
    return h


def encode64(raw: bytes) -> str:
    out = []
    val = 0
    bits = -6
    for b in raw:
        val = (val << 8) + b
        bits += 8
        while bits >= 0:
            out.append(ALPHABET[(val >> bits) & 63])
            bits -= 6
    if bits > -6:
        out.append(ALPHABET[((val << 8) >> (bits + 8)) & 63])
    return "".join(out)


def xor_payload(raw: bytes) -> bytes:
    data = bytearray(raw)
    for i, b in enumerate(data):
        data[i] = b ^ SECRET[i % len(SECRET)] ^ ((i * 29 + 17) & 255)
    return bytes(data)


def make_key(stamp: str, kind: str) -> str:
    if len(stamp) != 8 or not stamp.isdigit():
        raise ValueError("时间戳必须是 YYYYMMDD")
    normalized = normalize_type(kind)
    body = f"SSOEM1|{stamp}|{normalized}"
    check = format(fnv1a(f"{body}|StrikeSense"), "x")
    return encode64(xor_payload(f"{body}|{check}".encode("utf-8")))


def main() -> None:
    parser = argparse.ArgumentParser(description="StrikeSense OEM key generator")
    parser.add_argument("--stamp", default=dt.date.today().strftime("%Y%m%d"), help="YYYYMMDD, default is today")
    parser.add_argument("--type", default=None, help="24小时/7天/一个月/半年/一年/十年/五十年/永不失效")
    args = parser.parse_args()
    kind = args.type or input("请输入期限类型：").strip()
    print(make_key(args.stamp, kind))


if __name__ == "__main__":
    main()
