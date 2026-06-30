const alphabet = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_";
const secret = Buffer.from("StrikeSenseOEMKey2026", "utf8");

const typeAliases = new Map([
  ["24h", "24h"],
  ["24小时", "24h"],
  ["7d", "7d"],
  ["7天", "7d"],
  ["1m", "1m"],
  ["一个月", "1m"],
  ["6m", "6m"],
  ["半年", "6m"],
  ["1y", "1y"],
  ["一年", "1y"],
  ["10y", "10y"],
  ["十年", "10y"],
  ["50y", "50y"],
  ["五十年", "50y"],
  ["forever", "forever"],
  ["永不失效", "forever"],
]);

function normalizeType(kind) {
  const value = typeAliases.get(String(kind).trim());
  if (!value) throw new Error("未知期限类型");
  return value;
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
  let out = "";
  let val = 0;
  let bits = -6;
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
  const out = Buffer.from(buf);
  for (let i = 0; i < out.length; i += 1) {
    out[i] = out[i] ^ secret[i % secret.length] ^ ((i * 29 + 17) & 255);
  }
  return out;
}

function makeKey(stamp, kind) {
  if (!/^\d{8}$/.test(stamp)) throw new Error("时间戳必须是 YYYYMMDD");
  const normalized = normalizeType(kind);
  const body = `SSOEM1|${stamp}|${normalized}`;
  const check = fnv1a(`${body}|StrikeSense`).toString(16);
  return encode64(xorPayload(Buffer.from(`${body}|${check}`, "utf8")));
}

if (typeof module !== "undefined") module.exports = { makeKey, normalizeType };
if (require.main === module) {
  const stamp = process.argv[2] || new Date().toISOString().slice(0, 10).replaceAll("-", "");
  const kind = process.argv[3] || "24小时";
  console.log(makeKey(stamp, kind));
}
