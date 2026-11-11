/*
 * isp.js  -  il protocollo con il bootloader ISP del SH68F90A ("Sinowealth ISP"), lo stesso che usa sinowisp.
 *
 * Perché è fatto così
 * -------------------
 * Questo file è la traduzione fedele di quello che sinowisp fa quando lanciamo
 *     sinowisp write --platform sh68f90 --vendor_id 0x258a --product_id 0xdead --format ihex --isp_iface_num 0 -f build/oblio.hex
 * (sorgente: github.com/karstx/sinowisp, src/isp_device.rs e src/flasher.rs). Non inventa niente: se una cosa qui sembra strana, sta nel
 * programma originale, che è provato sulla tua tastiera. NON è ancora provato dal browser: la prova sull'hardware è la tappa che lo conferma.
 *
 * Il modulo NON conosce il browser né WebHID. Parla con un "collegamento" (link) che gli si passa, con tre operazioni:
 *     link.command(bytes)    manda il feature report 5 (comandi): bytes = i 5 byte di dati, SENZA il report id
 *     link.writePage(bytes)  manda il feature report 6 (trasferimento) con 2049 byte di dati: [0x77, 2048 byte di pagina]
 *     link.readPage()        legge il feature report 6: restituisce 2049 byte di dati: [0x72, 2048 byte di pagina]
 * In produzione il collegamento è quello di js/hid.js (WebHID); nei test (tests/web/isp.test.mjs) è un finto bootloader in memoria. Così
 * l'ordine dei comandi, la verifica e la gestione degli errori si provano senza tastiera.
 *
 * La flash del chip, in breve
 * ---------------------------
 *   0x0000 .. 0xEFFF  area dell'applicazione (60 KB = 30 pagine da 2 KB): qui si scrive.
 *   0xF000 .. 0xFFFF  il bootloader: non si scrive MAI (sinowisp non lo fa e il chip lo protegge).
 * Il bootloader tiene il vettore di reset (i byte 0..2 a 0x0000) puntato su di sé, e salta al firmware con un LJMP che mette alla fine
 * dell'area (a FIRMWARE_SIZE - 5). Per questo i byte 1 e 2 del NOSTRO vettore di reset (l'indirizzo di partenza vero del firmware) non vengono
 * scritti dove stanno ma COPIATI a FIRMWARE_SIZE - 4 e - 3; il comando ENABLE, alla fine, completa il giro. Vedi flashImage().
 */

// ---------------------------------------------------------------------------------------------------------------------------
// Costanti del protocollo. Il significato dei byte è quello di sinowisp (isp_device.rs).
// ---------------------------------------------------------------------------------------------------------------------------
export const REPORT_CMD = 5;          // feature report dei COMANDI (5 byte di dati)
export const REPORT_XFER = 6;         // feature report del TRASFERIMENTO di una pagina (1 + 2048 byte di dati)

export const CMD_ENTER_ISP = 0x75;    // al firmware in funzione (non al bootloader): "salta nel bootloader"
export const CMD_ENABLE = 0x55;       // "attiva il firmware scritto": piazza l'LJMP a FIRMWARE_SIZE-5 verso l'indirizzo salvato
export const CMD_INIT_READ = 0x52;    // "comincia a leggere da questo indirizzo" (indirizzo a 16 bit, byte basso poi alto)
export const CMD_INIT_WRITE = 0x57;   // "comincia a scrivere da questo indirizzo"
export const CMD_ERASE = 0x45;        // cancella tutta l'area dell'applicazione e rimette il vettore di reset sul bootloader
export const CMD_REBOOT = 0x5a;       // riavvia il chip (il dispositivo sparisce dal bus: un errore di disconnessione qui è NORMALE)

export const XFER_WRITE = 0x77;       // primo byte dei dati di una pagina scritta
export const XFER_READ = 0x72;        // primo byte dei dati di una pagina letta (il bootloader lo ripete come "eco")

export const PAGE_SIZE = 0x800;       // 2048 byte per pagina
export const FIRMWARE_SIZE = 0xf000;  // 61440 byte: l'area scrivibile
export const PAGES = FIRMWARE_SIZE / PAGE_SIZE; // 30
export const SETTLE_MS = 2000;        // attesa dopo ERASE e dopo REBOOT (sinowisp: SETTLE_DELAY)

/** Errore del protocollo con un codice stabile (tradotto da js/i18n.js). */
export class IspError extends Error {
  constructor(code, details = {}) {
    super(code + (Object.keys(details).length ? " " + JSON.stringify(details) : ""));
    this.name = "IspError";
    this.code = code;
    this.details = details;
  }
}

const defaultSleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

/** I cinque byte di dati di un comando: [comando, indirizzo basso, indirizzo alto, 0, 0]. Solo INIT_READ e INIT_WRITE usano l'indirizzo. */
export function commandBytes(command, address = 0) {
  return [command, address & 0xff, (address >> 8) & 0xff, 0, 0];
}

/**
 * Scrive un'immagine nel chip, come sinowisp `write`. Passi (e `onStep({step, done, total})` per la barra di avanzamento):
 *   "erase"   ERASE, poi 2 secondi di attesa
 *   "write"   INIT_WRITE(0) e le 30 pagine da 2048 byte, una dopo l'altra (nessun indirizzo per pagina: il bootloader avanza da solo)
 *   "verify"  INIT_READ(0) e rilettura delle 30 pagine, confrontate byte per byte con quello che si doveva scrivere
 *   "enable"  ENABLE (solo se la verifica è riuscita) e REBOOT
 *
 * `image` deve essere lunga esattamente FIRMWARE_SIZE (è l'output di parseIntelHex). Se la verifica fallisce lancia IspError("isp.verify")
 * e NON manda ENABLE: la tastiera resta nel bootloader, con un firmware non attivato, e si può riprovare subito.
 * Dopo ERASE la tastiera non ha più un firmware valido fino a ENABLE: un'interruzione a metà lascia il bootloader (che c'è sempre) e si
 * rifà il flash.
 */
export async function flashImage(link, image, hooks = {}) {
  const sleep = hooks.sleep ?? defaultSleep;
  const step = hooks.onStep ?? (() => {});

  if (image.length !== FIRMWARE_SIZE) throw new IspError("isp.badSize", { length: image.length });

  // Lavoriamo su una copia: l'immagine dell'utente non si modifica.
  const firmware = Uint8Array.from(image);

  // PUNTO DELICATO 1 - il vettore di reset. Il bootloader mantiene per sé i primi 3 byte (l'LJMP verso se stesso) e ignora quelli che gli
  // scriviamo a 0x0000; i byte 1 e 2 del nostro vettore (dove vuole andare il firmware) vanno quindi salvati a FIRMWARE_SIZE-4 e -3, dove
  // ENABLE li troverà. È la riga `firmware.copy_within(1..3, FIRMWARE_SIZE - 4)` di sinowisp.
  firmware.copyWithin(FIRMWARE_SIZE - 4, 1, 3);

  // 1. ERASE. Da qui la tastiera non ha un firmware valido fino a ENABLE.
  step({ step: "erase", done: 0, total: 1 });
  await link.command(commandBytes(CMD_ERASE));
  await sleep(SETTLE_MS); // la cancellazione richiede tempo: sinowisp aspetta 2 s e senza questa attesa i comandi successivi si perdono
  step({ step: "erase", done: 1, total: 1 });

  // 2. Scrittura: INIT_WRITE dall'indirizzo 0, poi le pagine in ordine. Il bootloader non vuole l'indirizzo a ogni pagina.
  await link.command(commandBytes(CMD_INIT_WRITE, 0));
  for (let page = 0; page < PAGES; page++) {
    step({ step: "write", done: page, total: PAGES });
    const packet = new Uint8Array(1 + PAGE_SIZE);
    packet[0] = XFER_WRITE;
    packet.set(firmware.subarray(page * PAGE_SIZE, (page + 1) * PAGE_SIZE), 1);
    await link.writePage(packet);
  }
  step({ step: "write", done: PAGES, total: PAGES });

  // PUNTO DELICATO 2 - quello che si deve RILEGGERE non è identico a quello che si è scritto: leggendo, il bootloader restituisce nei byte
  // 1 e 2 il valore salvato a FIRMWARE_SIZE-4/-3 (cioè il nostro vecchio vettore) e zero in FIRMWARE_SIZE-4/-3. Quindi il riferimento per il
  // confronto ha quei due byte azzerati: è la riga che sinowisp scrive dopo la scrittura delle pagine.
  firmware[FIRMWARE_SIZE - 4] = 0;
  firmware[FIRMWARE_SIZE - 3] = 0;

  // 3. Verifica: rilegge tutto e confronta. Se anche un solo byte differisce NON si attiva il firmware.
  await link.command(commandBytes(CMD_INIT_READ, 0));
  for (let page = 0; page < PAGES; page++) {
    step({ step: "verify", done: page, total: PAGES });
    const got = await link.readPage();
    if (got.length !== 1 + PAGE_SIZE || got[0] !== XFER_READ) throw new IspError("isp.badReply", { page, length: got.length, first: got[0] });
    for (let i = 0; i < PAGE_SIZE; i++) {
      if (got[1 + i] !== firmware[page * PAGE_SIZE + i]) {
        throw new IspError("isp.verify", { offset: page * PAGE_SIZE + i, expected: firmware[page * PAGE_SIZE + i], found: got[1 + i] });
      }
    }
  }
  step({ step: "verify", done: PAGES, total: PAGES });

  // 4. ENABLE: il bootloader mette l'LJMP a FIRMWARE_SIZE-5 verso l'indirizzo salvato. Poi REBOOT: il chip riparte e il dispositivo SPARISCE
  // dal bus mentre il comando viene confermato, per cui un errore qui non è un guasto ("is_expected_error" in sinowisp). Si ignora e si aspetta.
  step({ step: "enable", done: 0, total: 1 });
  await link.command(commandBytes(CMD_ENABLE));
  try {
    await link.command(commandBytes(CMD_REBOOT));
  } catch (error) {
    hooks.onNote?.({ note: "rebootDisconnected", error }); // atteso: solo per il registro
  }
  await sleep(SETTLE_MS);
  step({ step: "enable", done: 1, total: 1 });
}
