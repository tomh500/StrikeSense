import argparse
import datetime as _dt

ALPHABET = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_"
KEY = b"StrikeSenseOEMKey2026"


def days_for_type(kind: str) -> int:
    table = {
        "24小时": 1, "24h": 1,
        "7天": 7, "7d": 7,
        "一个月": 31, "1m": 31,
        "半年": 183, "6m": 183,
        "一年": 366, "1y": 366,
        "十年": 3653, "10y": 3653,
        "五十年": 18263, "50y": 18263,
        "永不失效": 365000, "forever": 365000,
    }
    return table.get(kind, 0)


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
        data[i] = b ^ KEY[i % len(KEY)] ^ ((i * 29 + 17) & 255)
    return bytes(data)


def make_key(stamp: str, kind: str) -> str:
    if len(stamp) != 8 or not stamp.isdigit():
        raise ValueError("时间戳必须是 YYYYMMDD")
    if not days_for_type(kind):
        raise ValueError("未知期限类型")
    body = f"SSOEM1|{stamp}|{kind}"
    check = format(fnv1a(f"{body}|StrikeSense"), "x")
    return encode64(xor_payload(f"{body}|{check}".encode("utf-8")))


def main() -> None:
    parser = argparse.ArgumentParser(description="StrikeSense OEM 调试 key 生成器")
    parser.add_argument("--stamp", default=_dt.date.today().strftime("%Y%m%d"), help="YYYYMMDD，默认系统日期")
    parser.add_argument("--type", default=None, help="24小时/7天/一个月/半年/一年/十年/五十年/永不失效")
    args = parser.parse_args()
    kind = args.type or input("请输入期限类型：").strip()
    print(make_key(args.stamp, kind))


if __name__ == "__main__":
    main()
