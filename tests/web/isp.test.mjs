// Prove di web/js/isp.js contro il finto bootloader (tests/web/fake_bootloader.mjs).
import test from "node:test";
import assert from "node:assert/strict";
import { flashImage, commandBytes, IspError, FIRMWARE_SIZE, PAGES, CMD_ERASE, CMD_INIT_WRITE } from "../../web/js/isp.js";
import { FakeBootloader } from "./fake_bootloader.mjs";

// Un'immagine di prova riconoscibile: ogni byte è funzione della posizione; il vettore di reset è LJMP 0x1234.
function sampleImage() {
  const image = new Uint8Array(FIRMWARE_SIZE);
  for (let i = 0; i < FIRMWARE_SIZE - 8; i++) image[i] = (i * 7 + 3) & 0xff;
  image.set([0x02, 0x12, 0x34], 0);
  return image;
}
const noWait = { sleep: async () => {} }; // niente attese vere nei test

test("commandBytes: indirizzo a 16 bit, byte basso poi alto", () => {
  assert.deepEqual(commandBytes(CMD_ERASE), [0x45, 0, 0, 0, 0]);
  assert.deepEqual(commandBytes(CMD_INIT_WRITE, 0x1234), [0x57, 0x34, 0x12, 0, 0]);
});

test("scrive, verifica e attiva: ordine dei comandi e contenuto della flash", async () => {
  const boot = new FakeBootloader();
  const image = sampleImage();
  await flashImage(boot, image, noWait);

  // l'ordine: erase, init-write, 30 pagine, init-read, 30 letture, enable, reboot
  const expected = ["erase", "init-write 0", ...Array.from({ length: PAGES }, (_, i) => `write ${i}`), "init-read 0",
    ...Array.from({ length: PAGES }, (_, i) => `read ${i}`), "enable", "reboot"];
  assert.deepEqual(boot.log, expected);

  // il codice è nella flash, tranne i primi tre byte (del bootloader) e i due che hanno cambiato posto
  assert.deepEqual([...boot.mem.slice(3, FIRMWARE_SIZE - 5)], [...image.slice(3, FIRMWARE_SIZE - 5)]);
  assert.deepEqual([...boot.mem.slice(0, 3)], [0x02, 0xf0, 0x00]); // il vettore di reset è ancora quello del bootloader

  // il punto di partenza vero (i byte 1-2 dell'immagine) è salvato a FIRMWARE_SIZE-4/-3 e l'LJMP a -5 lo raggiunge dopo ENABLE
  assert.equal(boot.enabled, true);
  assert.equal(boot.mem[FIRMWARE_SIZE - 5], 0x02);
  assert.deepEqual([...boot.mem.slice(FIRMWARE_SIZE - 4, FIRMWARE_SIZE - 2)], [0x12, 0x34]);
});

test("non modifica l'immagine che gli si passa", async () => {
  const image = sampleImage();
  const before = Uint8Array.from(image);
  await flashImage(new FakeBootloader(), image, noWait);
  assert.deepEqual(image, before);
});

test("se la rilettura differisce NON manda ENABLE e dice dove", async () => {
  const boot = new FakeBootloader({ corruptAt: 5000 });
  await assert.rejects(flashImage(boot, sampleImage(), noWait), (e) => e instanceof IspError && e.code === "isp.verify" && e.details.offset === 5000);
  assert.equal(boot.enabled, false);
  assert.ok(!boot.log.includes("enable") && !boot.log.includes("reboot"));
});

test("il REBOOT che fallisce perché il dispositivo sparisce è atteso, e uno che riesce va bene lo stesso", async () => {
  const notes = [];
  await flashImage(new FakeBootloader({ rebootThrows: true }), sampleImage(), { ...noWait, onNote: (n) => notes.push(n.note) });
  assert.deepEqual(notes, ["rebootDisconnected"]);
  await flashImage(new FakeBootloader({ rebootThrows: false }), sampleImage(), noWait);
});

test("un errore prima di ERASE o durante la scrittura si propaga e non attiva nulla", async () => {
  const boot = new FakeBootloader();
  const original = boot.writePage.bind(boot);
  let pages = 0;
  boot.writePage = async (bytes) => { if (++pages === 3) throw new Error("USB gone"); return original(bytes); };
  await assert.rejects(flashImage(boot, sampleImage(), noWait), /USB gone/);
  assert.equal(boot.enabled, false);
});

test("rifiuta un'immagine della lunghezza sbagliata senza toccare la tastiera", async () => {
  const boot = new FakeBootloader();
  await assert.rejects(flashImage(boot, new Uint8Array(1000), noWait), (e) => e.code === "isp.badSize");
  assert.deepEqual(boot.log, []);
});

test("una risposta con l'eco sbagliato è un errore (non si fida di una lettura anomala)", async () => {
  const boot = new FakeBootloader();
  const original = boot.readPage.bind(boot);
  boot.readPage = async () => { const page = await original(); page[0] = 0x00; return page; };
  await assert.rejects(flashImage(boot, sampleImage(), noWait), (e) => e.code === "isp.badReply");
});

test("riporta l'avanzamento di ogni passo", async () => {
  const seen = [];
  await flashImage(new FakeBootloader(), sampleImage(), { ...noWait, onStep: (s) => seen.push(`${s.step}:${s.done}/${s.total}`) });
  assert.equal(seen[0], "erase:0/1");
  assert.ok(seen.includes("write:29/30") && seen.includes("write:30/30"));
  assert.ok(seen.includes("verify:30/30"));
  assert.equal(seen[seen.length - 1], "enable:1/1");
});
