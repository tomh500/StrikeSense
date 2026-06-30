const readline = require("readline");

const alphabet = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_";
const secret = Buffer.from("StrikeSenseOEMKey2026", "utf8");
const allowedTypes = new Set(["24h", "7d", "1m", "6m", "1y", "10y", "50y", "forever"]);

function fnv1a(text) {
  let value = 2166136261 >>> 0;
  for (const byte of Buffer.from(text, "utf8")) {
    value ^= byte;
    value = Math.imul(value, 16777619) >>> 0;
  }
  return value >>> 0;
}

function xorPayload(raw) {
  const out = Buffer.from(raw);
  for (let i = 0; i < out.length; i += 1) {
    out[i] = out[i] ^ secret[i % secret.length] ^ ((i * 29 + 17) & 0xff);
  }
  return out;
}

function encode64(raw) {
  let out = "";
  let value = 0;
  let bits = -6;
  for (const byte of raw) {
    value = (value << 8) + byte;
    bits += 8;
    while (bits >= 0) {
      out += alphabet[(value >> bits) & 63];
      bits -= 6;
    }
  }
  if (bits > -6) {
    out += alphabet[((value << 8) >> (bits + 8)) & 63];
  }
  return out;
}

function normalizeType(kind) {
  const value = String(kind).trim().toLowerCase();
  if (!allowedTypes.has(value)) {
    throw new Error("Unknown type. Use: 24h, 7d, 1m, 6m, 1y, 10y, 50y, forever");
  }
  return value;
}

function makeKey(stamp, kind) {
  if (!/^\d{12}$/.test(stamp)) {
    throw new Error("Stamp must be YYYYMMDDHHMM");
  }
  const normalized = normalizeType(kind);
  const body = `SSOEM1|${stamp}|${normalized}`;
  const check = fnv1a(`${body}|StrikeSense`).toString(16);
  return encode64(xorPayload(Buffer.from(`${body}|${check}`, "utf8")));
}

function nowStamp() {
  const now = new Date();
  const pad = (n) => String(n).padStart(2, "0");
  return `${now.getFullYear()}${pad(now.getMonth() + 1)}${pad(now.getDate())}${pad(now.getHours())}${pad(now.getMinutes())}`;
}

if (require.main === module) {
  const stamp = process.argv[2] || nowStamp();
  const kind = process.argv[3] || "24h";

  try {
    console.log("Stamp :", stamp);
    console.log("Type  :", normalizeType(kind));
    console.log("Key   :", makeKey(stamp, kind));
  } catch (error) {
    console.log("Error :", error.message);
  }

  const rl = readline.createInterface({ input: process.stdin, output: process.stdout });
  rl.question("Press Enter to exit...", () => rl.close());
}

module.exports = { makeKey, normalizeType };
