/*
 * ihex.js  -  lettura e controllo di un file Intel HEX del firmware Oblio.
 *
 * Perché esiste
 * -------------
 * L'utility deve (1) trasformare il file .hex in un'immagine di 61440 byte da scrivere nella flash, (2) rifiutare file sbagliati PRIMA di
 * cancellare la tastiera, (3) leggere dall'immagine la versione del firmware (struttura "OBLIOFW1" di core/version.h) per mostrarla
 * accanto a quella installata. Questo modulo non tocca il browser né l'USB: sono funzioni pure, provate da tests/web/ihex.test.mjs.
 *
 * Il formato Intel HEX in due righe: ogni riga è  ":" LL AAAA TT DD...DD CC  (esadecimale) con LL = numero di byte dati, AAAA = indirizzo
 * a 16 bit, TT = tipo (00 dati, 01 fine file, 02/04 indirizzi estesi, 03/05 indirizzo di partenza), CC = checksum (la somma di tutti i
 * byte della riga, checksum compreso, deve fare 0 modulo 256).
 *
 * Cosa fa come sinowisp (la riga di comando che usiamo oggi) e cosa fa di più
 * ---------------------------------------------------------------------------
 *  - Come sinowisp: l'immagine è lunga FIRMWARE_SIZE (0xF000 = 61440 byte), i buchi e la coda sono riempiti con 0x00, i record 02/04
 *    (indirizzi estesi) sono rifiutati (un chip a 64 KB non ne ha bisogno), i record 03/05 sono ignorati.
 *  - In più (sicurezza): un file con dati da 0xF000 in su è rifiutato (sarebbe il bootloader), e un file con dati nell'area di keymap,
 *    impostazioni e marcatore del bootloader (da CODE_LIMIT a 0xEFFF) pure. Nel firmware Oblio il codice finisce a 0xDA00 (0xDC00 senza
 *    registro batteria): niente di legittimo sta più in alto.
 */

// ---------------------------------------------------------------------------------------------------------------------------
// Costanti della flash del SH68F90A (docs/design.md, mappa della flash). Cambiano solo se cambia il chip.
// ---------------------------------------------------------------------------------------------------------------------------
export const FIRMWARE_SIZE = 0xf000; // byte che si scrivono: dalla 0x0000 alla 0xEFFF. Da 0xF000 c'è il bootloader, che non si tocca mai.
export const CODE_LIMIT = 0xdc00;    // il codice dell'applicazione finisce prima (0xDA00 con il registro batteria, 0xDC00 senza)
export const MAGIC = "OBLIOFW1";     // le otto lettere con cui comincia la struttura di versione (core/version.h)

// Disposizione della struttura di versione dopo il magic (core/version.h, version_info_t): sono offset in byte dall'inizio del magic.
const OFFSET_MAJOR = 8;
const OFFSET_MINOR = 9;
const OFFSET_PATCH = 10;
const OFFSET_FLAGS = 11;
const OFFSET_REV = 12;   // sette caratteri del commit, seguiti da uno zero
const REV_LENGTH = 7;
const OFFSET_DIRTY = 20;
const VERSION_STRUCT_SIZE = 21;

// Il bit 0 dei flag dice che quella build contiene il registro batteria.
export const FLAG_BATTERY_LOG = 0x01;

/** Errore con un codice stabile: l'interfaccia lo traduce nella lingua scelta (js/i18n.js). `details` porta i numeri utili. */
export class HexError extends Error {
  constructor(code, details = {}) {
    super(code + (Object.keys(details).length ? " " + JSON.stringify(details) : ""));
    this.name = "HexError";
    this.code = code;
    this.details = details;
  }
}

/**
 * Legge il testo di un file Intel HEX.
 *
 * Restituisce { data, size, ranges }:
 *   data   Uint8Array di FIRMWARE_SIZE byte, pronta da scrivere (buchi e coda a 0x00 come fa sinowisp)
 *   size   l'indirizzo più alto con dati, più uno (la lunghezza "vera" del codice)
 *   ranges elenco di [inizio, fine) dei tratti che il file contiene davvero (serve ai controlli: un tratto fuori mappa è un errore)
 * Lancia HexError se il file è malformato.
 */
export function parseIntelHex(text) {
  const data = new Uint8Array(FIRMWARE_SIZE); // già a zero: è il riempimento
  const covered = new Uint8Array(0x10000);     // 1 dove il file ha scritto un byte (lo spazio di indirizzamento intero: 64 KB)
  let size = 0;
  let sawEnd = false;

  const lines = text.split(/\r?\n/);
  for (let n = 0; n < lines.length; n++) {
    const line = lines[n].trim();
    if (line === "") continue;
    if (line[0] !== ":") throw new HexError("hex.badLine", { line: n + 1 });
    if (sawEnd) throw new HexError("hex.afterEnd", { line: n + 1 });

    // Le cifre devono essere esadecimali e in numero pari: altrimenti la riga è troncata o corrotta.
    const digits = line.slice(1);
    if (digits.length % 2 !== 0 || !/^[0-9a-fA-F]+$/.test(digits)) throw new HexError("hex.badLine", { line: n + 1 });
    const raw = new Uint8Array(digits.length / 2);
    for (let i = 0; i < raw.length; i++) raw[i] = parseInt(digits.substr(i * 2, 2), 16);

    // Struttura minima: lunghezza, 2 byte di indirizzo, tipo, dati, checksum.
    if (raw.length < 5 || raw.length !== raw[0] + 5) throw new HexError("hex.badLine", { line: n + 1 });
    if (raw.reduce((sum, b) => (sum + b) & 0xff, 0) !== 0) throw new HexError("hex.checksum", { line: n + 1 });

    const length = raw[0];
    const address = (raw[1] << 8) | raw[2];
    const type = raw[3];

    switch (type) {
      case 0x00: { // record dati: copia i byte all'indirizzo indicato
        for (let i = 0; i < length; i++) {
          const at = address + i;
          if (at >= 0x10000) throw new HexError("hex.beyond64k", { line: n + 1 });
          covered[at] = 1;
          if (at < FIRMWARE_SIZE) data[at] = raw[4 + i]; // sopra 0xF000 non si copia (e il controllo di validate() lo rifiuta)
          if (at + 1 > size) size = at + 1;
        }
        break;
      }
      case 0x01: // fine file
        sawEnd = true;
        break;
      case 0x03: // indirizzo di partenza (segmento): ignorato, come sinowisp
      case 0x05: // indirizzo di partenza (lineare): ignorato
        break;
      default: // 02 e 04 (indirizzi estesi) e tutto il resto: non servono a un chip a 64 KB e sinowisp li rifiuta
        throw new HexError("hex.unsupportedRecord", { line: n + 1, type });
    }
  }

  if (!sawEnd) throw new HexError("hex.noEnd");
  if (size === 0) throw new HexError("hex.empty");

  return { data, size, ranges: rangesOf(covered) };
}

/** Trasforma la mappa "byte coperto / non coperto" in un elenco di intervalli [inizio, fine). */
function rangesOf(covered) {
  const ranges = [];
  let start = -1;
  for (let i = 0; i <= covered.length; i++) {
    const on = i < covered.length && covered[i] === 1;
    if (on && start < 0) start = i;
    if (!on && start >= 0) { ranges.push([start, i]); start = -1; }
  }
  return ranges;
}

/**
 * Cerca nell'immagine la struttura di versione. Restituisce l'elenco di tutte le occorrenze del magic (normalmente una) già decodificate:
 *   { offset, major, minor, patch, flags, rev, dirty, batteryLog }
 * Un firmware senza versione (precedente alla 0.9.0) o una build di profilo non ne hanno: l'elenco è vuoto.
 */
export function findVersions(image) {
  const magic = Array.from(MAGIC, (c) => c.charCodeAt(0));
  const found = [];
  for (let at = 0; at + VERSION_STRUCT_SIZE <= image.length; at++) {
    let ok = true;
    for (let i = 0; i < magic.length && ok; i++) ok = image[at + i] === magic[i];
    if (!ok) continue;
    found.push({
      offset: at,
      major: image[at + OFFSET_MAJOR],
      minor: image[at + OFFSET_MINOR],
      patch: image[at + OFFSET_PATCH],
      flags: image[at + OFFSET_FLAGS],
      rev: String.fromCharCode(...image.slice(at + OFFSET_REV, at + OFFSET_REV + REV_LENGTH)),
      dirty: image[at + OFFSET_DIRTY] !== 0,
      batteryLog: (image[at + OFFSET_FLAGS] & FLAG_BATTERY_LOG) !== 0,
    });
  }
  return found;
}

/**
 * Decodifica la risposta del firmware al comando di diagnostica "tipo 2" (core/diag.c): [0xF1, 2, major, minor, patch, flags, commit x8, dirty].
 * `reply` è il pacchetto di 32 byte (Uint8Array). Restituisce lo stesso oggetto di findVersions, oppure null se il firmware non conosce il comando
 * (risponde con 0xFF al posto di 0xF1: è un firmware più vecchio della 0.9.0).
 */
export function versionFromReply(reply) {
  if (reply.length < 15 || reply[0] !== 0xf1 || reply[1] !== 2) return null;
  return {
    offset: -1,
    major: reply[2],
    minor: reply[3],
    patch: reply[4],
    flags: reply[5],
    rev: String.fromCharCode(...reply.slice(6, 6 + REV_LENGTH)),
    dirty: reply[14] !== 0,
    batteryLog: (reply[5] & FLAG_BATTERY_LOG) !== 0,
  };
}

/** "0.9.0 (abc1234)" oppure "0.9.0 (abc1234, modifiche non committate)". */
export function formatVersion(v) {
  return `${v.major}.${v.minor}.${v.patch} (${v.rev}${v.dirty ? ", +modifiche" : ""})`;
}

/** Confronta due versioni per numero: negativo se a < b, 0 se uguali, positivo se a > b. Il commit non conta per l'ordine. */
export function compareVersions(a, b) {
  return a.major - b.major || a.minor - b.minor || a.patch - b.patch;
}

/**
 * I controlli da fare PRIMA di toccare la tastiera. Restituisce { ok, errors, warnings, version } dove errors e warnings sono elenchi di
 * { code, details } (tradotti dall'interfaccia). `ok` è vero solo senza errori.
 * Un errore blocca l'aggiornamento (a meno della scelta avanzata "forza" per i soli errori marcati `forceable`); un avviso no.
 */
export function validateImage(parsed) {
  const errors = [];
  const warnings = [];
  const { data, ranges } = parsed;

  // 1. Niente dati dove stanno la keymap, le impostazioni, il marcatore e il bootloader. Sono tutti tratti sopra CODE_LIMIT.
  for (const [start, end] of ranges) {
    if (end > CODE_LIMIT) errors.push({ code: "image.beyondCode", details: { start, end, limit: CODE_LIMIT } });
  }

  // 2. Il vettore di reset: il primo byte deve essere 0x02 (LJMP). Il bootloader si basa su questo (vedi isp.js: i byte 1-2 sono il
  //    punto di partenza vero del firmware).
  if (data[0] !== 0x02) errors.push({ code: "image.resetVector", details: { first: data[0] } });

  // 3. La versione: deve esserci, una sola volta. Se manca non è un firmware Oblio (o è la build di profilo): si può forzare.
  const versions = findVersions(data);
  if (versions.length === 0) errors.push({ code: "image.notOblio", forceable: true });
  if (versions.length > 1) errors.push({ code: "image.severalVersions", details: { count: versions.length } });

  return { ok: errors.length === 0, errors, warnings, version: versions.length === 1 ? versions[0] : null };
}

/** SHA-256 in esadecimale (crypto.subtle: disponibile in ogni browser su https/localhost e in Node). Serve a mostrare l'impronta del file. */
export async function sha256Hex(bytes) {
  const digest = await globalThis.crypto.subtle.digest("SHA-256", bytes);
  return Array.from(new Uint8Array(digest), (b) => b.toString(16).padStart(2, "0")).join("");
}
