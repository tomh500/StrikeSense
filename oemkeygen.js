const alphabet = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_";
const key = "StrikeSenseOEMKey2026";

function daysForType(type) {
  return {
    "24小时": 1, "24h": 1,
    "7天": 7, "7d": 7,
    "一个月": 31, "1m": 31,
    "半年": 183, "6m": 183,
    "一年": 366, "1y": 366,
    "十年": 3653, "10y": 3653,
    "五十年": 18263, "50y": 18263,
    "永不失效": 365000, "forever": 365000
  }[type] || 0;
}

function fnv1a(text) {
  let h = 2166136261 >>> 0;
  for (const c of Buffer.from(text, "utf8")) {
    h ^= c;
    h = Math.imul(h, 16777619) >>> 0;
  }
  return h >>> 0;
}

function encode64(buf) {
  let out = "", val = 0, bits = -6;
  for (const c of buf) {
    val = (val << 8) + c;
    bits += 8;
    while (bits >= 0) {
      out += alphabet[(val >> bits) & 63];
      bits -= 6;
    }
  }
  if (bits > -6) out += alphabet[((val << 8) >> (bits + 8)) & 63];
  return out;
}

function xorPayload(buf) {
  const k = Buffer.from(key, "utf8");
  const out = Buffer.from(buf);
  for (let i = 0; i < out.length; i++) out[i] = out[i] ^ k[i % k.length] ^ ((i * 29 + 17) & 255);
  return out;
}

function makeKey(stamp, type) {
  if (!/^\d{8}$/.test(stamp)) throw new Error("时间戳必须是 YYYYMMDD");
  if (!daysForType(type)) throw new Error("未知期限类型");
  const body = `SSOEM1|${stamp}|${type}`;
  const check = fnv1a(`${body}|StrikeSense`).toString(16);
  return encode64(xorPayload(Buffer.from(`${body}|${check}`, "utf8")));
}

if (typeof module !== "undefined") module.exports = { makeKey, daysForType };
if (require.main === module) {
  const stamp = process.argv[2] || new Date().toISOString().slice(0, 10).replaceAll("-", "");
  const type = process.argv[3] || "24小时";
  console.log(makeKey(stamp, type));
}
