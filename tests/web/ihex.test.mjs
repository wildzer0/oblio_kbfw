// Prove di web/js/ihex.js: lettura dell'Intel HEX, controlli sull'immagine, ricerca della versione.
// Si lanciano con:  make web-test   (oppure  node --test tests/web/ )
import test from "node:test";
import assert from "node:assert/strict";
import { existsSync, readFileSync } from "node:fs";
import {
  FIRMWARE_SIZE, MAGIC, HexError, parseIntelHex, findVersions, versionFromReply, formatVersion, compareVersions, validateImage, sha256Hex,
} from "../../web/js/ihex.js";

// ---- aiuti per costruire righe Intel HEX valide -------------------------------------------------------------------------
function record(type, address, bytes = []) {
  const raw = [bytes.length, (address >> 8) & 0xff, address & 0xff, type, ...bytes];
  const checksum = (0x100 - (raw.reduce((s, b) => s + b, 0) & 0xff)) & 0xff;
  return ":" + [...raw, checksum].map((b) => b.toString(16).padStart(2, "0")).join("").toUpperCase();
}
const END = record(1, 0);
const hex = (...records) => [...records, END].join("\n") + "\n";

// Una struttura di versione come la produce core/version.c: magic, major, minor, patch, flags, commit (7 caratteri + 0), dirty.
function versionBytes(major, minor, patch, flags = 1, rev = "abc1234", dirty = 0) {
  return [...MAGIC].map((c) => c.charCodeAt(0)).concat([major, minor, patch, flags], [...rev].map((c) => c.charCodeAt(0)), [0, dirty]);
}

// Un'immagine minima valida: LJMP a 0x0000, poi la versione a 0x0100.
const goodHex = () => hex(record(0, 0, [0x02, 0x01, 0x00]), record(0, 0x100, versionBytes(0, 9, 0)));

test("legge i dati agli indirizzi giusti, riempie i buchi e la coda con zero", () => {
  const parsed = parseIntelHex(hex(record(0, 0, [0x02, 0x12, 0x34]), record(0, 0x20, [1, 2, 3])));
  assert.equal(parsed.data.length, FIRMWARE_SIZE);
  assert.deepEqual([...parsed.data.slice(0, 3)], [0x02, 0x12, 0x34]);
  assert.deepEqual([...parsed.data.slice(0x20, 0x23)], [1, 2, 3]);
  assert.equal(parsed.data[5], 0); // un buco
  assert.equal(parsed.data[FIRMWARE_SIZE - 1], 0); // la coda
  assert.equal(parsed.size, 0x23);
  assert.deepEqual(parsed.ranges, [[0, 3], [0x20, 0x23]]);
});

test("accetta le righe con CRLF e righe vuote, ignora i record di indirizzo di partenza", () => {
  const text = [record(5, 0, [0, 0, 0, 0]), "", record(0, 0, [0x02, 0, 0])].join("\r\n") + "\r\n" + END + "\r\n";
  assert.equal(parseIntelHex(text).size, 3);
});

test("rifiuta checksum sbagliati, righe troncate e righe che non cominciano con i due punti", () => {
  const bad = record(0, 0, [1, 2, 3]).slice(0, -2) + "00"; // checksum alterato
  assert.throws(() => parseIntelHex(hex(bad)), (e) => e instanceof HexError && e.code === "hex.checksum");
  assert.throws(() => parseIntelHex(hex(record(0, 0, [1, 2, 3]).slice(0, -3))), (e) => e.code === "hex.badLine");
  assert.throws(() => parseIntelHex("0200000102FB\n" + END), (e) => e.code === "hex.badLine");
  assert.throws(() => parseIntelHex(hex(":0G00000000")), (e) => e.code === "hex.badLine");
});

test("rifiuta gli indirizzi estesi, i dati oltre 64 KB, un file senza fine e un file vuoto", () => {
  assert.throws(() => parseIntelHex(hex(record(4, 0, [0, 1]))), (e) => e.code === "hex.unsupportedRecord");
  assert.throws(() => parseIntelHex(hex(record(2, 0, [0x10, 0]))), (e) => e.code === "hex.unsupportedRecord");
  assert.throws(() => parseIntelHex(hex(record(0, 0xfffe, [1, 2, 3]))), (e) => e.code === "hex.beyond64k");
  assert.throws(() => parseIntelHex(record(0, 0, [1])), (e) => e.code === "hex.noEnd");
  assert.throws(() => parseIntelHex(END), (e) => e.code === "hex.empty");
  assert.throws(() => parseIntelHex(END + "\n" + record(0, 0, [1])), (e) => e.code === "hex.afterEnd");
});

test("trova la versione dentro l'immagine e la decodifica", () => {
  const { data } = parseIntelHex(goodHex());
  const found = findVersions(data);
  assert.equal(found.length, 1);
  assert.deepEqual(
    { ...found[0] },
    { offset: 0x100, major: 0, minor: 9, patch: 0, flags: 1, rev: "abc1234", dirty: false, batteryLog: true },
  );
  assert.equal(formatVersion(found[0]), "0.9.0 (abc1234)");
  assert.equal(formatVersion({ ...found[0], dirty: true }), "0.9.0 (abc1234, +modifiche)");
});

test("la versione senza il registro batteria ha il flag a zero", () => {
  const parsed = parseIntelHex(hex(record(0, 0, [2, 0, 0]), record(0, 0x80, versionBytes(1, 2, 3, 0, "deadbee", 1))));
  const [v] = findVersions(parsed.data);
  assert.equal(v.batteryLog, false);
  assert.equal(v.dirty, true);
  assert.equal(formatVersion(v), "1.2.3 (deadbee, +modifiche)");
});

test("confronta le versioni per numero e non per commit", () => {
  const v = (major, minor, patch, rev = "aaaaaaa") => ({ major, minor, patch, rev });
  assert.ok(compareVersions(v(0, 9, 1), v(0, 9, 0)) > 0);
  assert.ok(compareVersions(v(0, 10, 0), v(0, 9, 9)) > 0); // 10 > 9: numeri, non testo
  assert.ok(compareVersions(v(1, 0, 0), v(0, 99, 99)) > 0);
  assert.equal(compareVersions(v(0, 9, 0, "aaaaaaa"), v(0, 9, 0, "bbbbbbb")), 0);
});

test("decodifica la risposta del firmware al comando di versione", () => {
  const reply = new Uint8Array(32);
  reply.set([0xf1, 2, 0, 9, 1, 1, ...[..."1a2b3c4"].map((c) => c.charCodeAt(0)), 0, 0]);
  assert.equal(formatVersion(versionFromReply(reply)), "0.9.1 (1a2b3c4)");
  reply[14] = 1;
  assert.equal(versionFromReply(reply).dirty, true);
  // un firmware che non conosce il comando risponde 0xFF: nessuna versione
  assert.equal(versionFromReply(Uint8Array.from([0xff, 2, ...new Array(30).fill(0)])), null);
});

test("validateImage accetta un'immagine buona", () => {
  const result = validateImage(parseIntelHex(goodHex()));
  assert.equal(result.ok, true);
  assert.equal(formatVersion(result.version), "0.9.0 (abc1234)");
});

test("validateImage blocca dati nell'area di keymap, impostazioni e bootloader", () => {
  for (const address of [0xdc00, 0xec00, 0xee00, 0xf000, 0xfff0]) {
    const result = validateImage(parseIntelHex(hex(record(0, 0, [2, 0, 0]), record(0, 0x100, versionBytes(0, 9, 0)), record(0, address, [1, 2]))));
    assert.equal(result.ok, false, `0x${address.toString(16)}`);
    assert.ok(result.errors.some((e) => e.code === "image.beyondCode"));
  }
  // l'ultimo byte del codice ammesso (0xDBFF) va bene
  assert.equal(validateImage(parseIntelHex(hex(record(0, 0, [2, 0, 0]), record(0, 0x100, versionBytes(0, 9, 0)), record(0, 0xdbff, [1])))).ok, true);
});

test("validateImage blocca un file senza vettore di reset LJMP", () => {
  const result = validateImage(parseIntelHex(hex(record(0, 0, [0x75, 0, 0]), record(0, 0x100, versionBytes(0, 9, 0)))));
  assert.ok(result.errors.some((e) => e.code === "image.resetVector"));
});

test("validateImage: senza versione non è Oblio (si può forzare), con due versioni è un errore", () => {
  const none = validateImage(parseIntelHex(hex(record(0, 0, [2, 0, 0]), record(0, 0x100, [1, 2, 3]))));
  assert.equal(none.ok, false);
  assert.equal(none.errors[0].code, "image.notOblio");
  assert.equal(none.errors[0].forceable, true);

  const twice = validateImage(parseIntelHex(hex(record(0, 0, [2, 0, 0]), record(0, 0x100, versionBytes(0, 9, 0)), record(0, 0x200, versionBytes(0, 9, 0)))));
  assert.ok(twice.errors.some((e) => e.code === "image.severalVersions"));
});

test("sha256Hex dà l'impronta nota", async () => {
  assert.equal(await sha256Hex(new TextEncoder().encode("abc")), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
});

// ---- il file vero, se la build c'è -------------------------------------------------------------------------------------
test("build/oblio.hex (se esiste) è valido e dice la versione del file VERSION", { skip: !existsSync("build/oblio.hex") }, () => {
  const result = validateImage(parseIntelHex(readFileSync("build/oblio.hex", "utf8")));
  assert.deepEqual(result.errors, []);
  const wanted = readFileSync("VERSION", "utf8").trim();
  assert.equal(`${result.version.major}.${result.version.minor}.${result.version.patch}`, wanted);
});
