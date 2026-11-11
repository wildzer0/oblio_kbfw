/*
 * hid.js  -  tutto quello che riguarda il browser e WebHID (navigator.hid): trovare la tastiera, parlarle, aspettare che riappaia.
 *
 * Le regole di WebHID che condizionano il disegno (vanno sapute prima di modificare questo file)
 * ------------------------------------------------------------------------------------------------
 *  1. Solo Chrome, Edge e Opera (basati su Chromium). Firefox e Safari non hanno WebHID. La pagina deve stare su https o su localhost.
 *  2. Un dispositivo si vede solo dopo che l'utente lo ha scelto in una finestra del browser: navigator.hid.requestDevice(), che si può
 *     chiamare SOLO dentro un gestore di click. Il permesso ricordato vale per quel dispositivo (VID, PID, numero di serie): la volta dopo
 *     navigator.hid.getDevices() lo restituisce senza chiedere.
 *  3. Il bootloader ha un VID/PID DIVERSO dalla tastiera (0603:1020 invece di 258a:dead): è un altro dispositivo, quindi serve una SECONDA
 *     scelta. È la ragione dei due passi nell'interfaccia ("Collega la tastiera", poi "Seleziona il bootloader").
 *  4. Chrome nasconde le collezioni HID di tastiere e mouse (per non permettere a una pagina di spiare i tasti). Il report 5 con cui si
 *     entra in ISP sta in una collezione "vendor" (pagina d'uso 0xFF00) sulla stessa interfaccia della tastiera boot: se Chrome la nasconde,
 *     enterIsp() non trova nulla e l'interfaccia propone le alternative (Esc+1+2). Lo verifica la pagina probe.html.
 *  5. WebHID passa il report id a parte: nei dati di sendFeatureReport/receiveFeatureReport il report id NON c'è.
 */

import { REPORT_CMD, REPORT_XFER, CMD_ENTER_ISP, XFER_READ, PAGE_SIZE } from "./isp.js";

// ---------------------------------------------------------------------------------------------------------------------------
// Identificativi. Se cambiano si cambiano solo qui.
// ---------------------------------------------------------------------------------------------------------------------------
export const KEYBOARD_FILTER = { vendorId: 0x258a, productId: 0xdead };   // firmware Oblio (core/usb_desc.h)
export const BOOTLOADER_FILTERS = [                                       // bootloader Sinowealth (sinowisp: GAMING_KB_PRODUCT_ID e _V2)
  { vendorId: 0x0603, productId: 0x1020 },
  { vendorId: 0x0603, productId: 0x1021 },
];
export const RAW_HID_USAGE_PAGE = 0xff60;   // interfaccia raw HID di VIA/Vial: quella dei comandi di diagnostica 0xF1
export const RAW_HID_SIZE = 32;             // byte per pacchetto raw HID
const VERSION_TIMEOUT_MS = 1000;            // quanto si aspetta la risposta alla richiesta di versione

/** WebHID c'è in questo browser? */
export function webHidAvailable() {
  return typeof navigator !== "undefined" && "hid" in navigator;
}

// ---------------------------------------------------------------------------------------------------------------------------
// Scelta e ricerca dei dispositivi
// ---------------------------------------------------------------------------------------------------------------------------

/** Apre la finestra di scelta del browser. DEVE essere chiamata da un click. Restituisce l'elenco dei dispositivi scelti (anche vuoto). */
export async function requestDevices(filters) {
  return navigator.hid.requestDevice({ filters });
}

/** I dispositivi già autorizzati in passato che corrispondono a uno dei filtri. Non apre finestre: si può chiamare quando si vuole. */
export async function grantedDevices(filters) {
  const all = await navigator.hid.getDevices();
  return all.filter((d) => filters.some((f) => d.vendorId === f.vendorId && d.productId === f.productId));
}

const hasFeatureReport = (device, reportId) =>
  device.collections.some((c) => (c.featureReports ?? []).some((r) => (r.reportId ?? 0) === reportId));

/** Tra i dispositivi indicati, quello con l'interfaccia raw HID (pagina d'uso 0xFF60), cioè quello da cui si legge la versione. */
export function findRawHid(devices) {
  return devices.find((d) => d.collections.some((c) => c.usagePage === RAW_HID_USAGE_PAGE)) ?? null;
}

/** Tra i dispositivi della tastiera in funzione, quello che espone il feature report 5 (l'ingresso in ISP). Può non esserci: vedi regola 4. */
export function findIspEntry(devices) {
  return devices.find((d) => hasFeatureReport(d, REPORT_CMD)) ?? null;
}

// ---------------------------------------------------------------------------------------------------------------------------
// Parlare con il firmware in funzione
// ---------------------------------------------------------------------------------------------------------------------------

async function ensureOpen(device) {
  if (!device.opened) await device.open();
}

/**
 * Chiede al firmware la sua versione (comando di diagnostica 0xF1, tipo 2). Restituisce il pacchetto di risposta (32 byte, Uint8Array) o
 * null se non risponde in tempo. Un firmware più vecchio della 0.9.0 risponde con 0xFF come primo byte: lo decodifica versionFromReply().
 */
export async function readVersionPacket(rawDevice) {
  await ensureOpen(rawDevice);

  // Il gestore va montato PRIMA di inviare: la risposta può arrivare subito, prima che sendReport() ritorni.
  const reply = new Promise((resolve) => {
    const timer = setTimeout(() => { rawDevice.removeEventListener("inputreport", onReport); resolve(null); }, VERSION_TIMEOUT_MS);
    function onReport(event) {
      clearTimeout(timer);
      rawDevice.removeEventListener("inputreport", onReport);
      resolve(new Uint8Array(event.data.buffer, event.data.byteOffset, event.data.byteLength));
    }
    rawDevice.addEventListener("inputreport", onReport);
  });

  const packet = new Uint8Array(RAW_HID_SIZE);
  packet[0] = 0xf1; // comando di diagnostica (core/via.c: id_oblio_diag)
  packet[1] = 2;    // tipo 2: la versione (core/diag.c)
  await rawDevice.sendReport(0, packet); // l'interfaccia raw HID non usa report id: si passa 0
  return reply;
}

/**
 * Dice al firmware in funzione di saltare nel bootloader: feature report 5, dati 75 00 00 00 00 (core/usb_device.c, ISP_COMMAND_ENTER).
 * Il firmware conferma e salta DOPO: il dispositivo sparisce dal bus, quindi un errore durante o subito dopo l'invio può essere normale.
 * Per questo non si distingue l'errore: chi chiama controlla se il bootloader appare.
 */
export async function enterIsp(device) {
  await ensureOpen(device);
  try {
    await device.sendFeatureReport(REPORT_CMD, Uint8Array.from([CMD_ENTER_ISP, 0, 0, 0, 0]));
  } catch (error) {
    return { error }; // atteso se il dispositivo è già sparito; se invece il browser ha rifiutato, il bootloader non comparirà e si vedrà
  }
  return { error: null };
}

// ---------------------------------------------------------------------------------------------------------------------------
// Attese
// ---------------------------------------------------------------------------------------------------------------------------

/**
 * Aspetta che compaia (tra i dispositivi GIÀ autorizzati) uno che corrisponde ai filtri. Restituisce l'elenco, o [] se scade il tempo.
 * Controlla ogni 250 ms: l'evento "connect" di WebHID esiste, ma il controllo periodico è più semplice e non perde nulla.
 */
export async function waitForDevices(filters, timeoutMs) {
  const deadline = Date.now() + timeoutMs;
  for (;;) {
    const found = await grantedDevices(filters);
    if (found.length > 0) return found;
    if (Date.now() >= deadline) return [];
    await new Promise((resolve) => setTimeout(resolve, 250));
  }
}

// ---------------------------------------------------------------------------------------------------------------------------
// Il collegamento al bootloader (quello che js/isp.js si aspetta)
// ---------------------------------------------------------------------------------------------------------------------------

/**
 * Collegamento a un bootloader: command() manda il feature report 5, writePage() e readPage() usano il 6.
 *
 * Su Windows sinowisp apre report 5 e report 6 come DUE collegamenti separati (sono due collezioni nel descrittore): in WebHID può capitare che
 * Chrome li presenti come due HIDDevice distinti, quindi si cercano entrambi tra i dispositivi scelti. Se è uno solo, serve per entrambi.
 */
export class WebHidLink {
  /** `devices`: i dispositivi scelti dall'utente per il bootloader. Lancia se non si trova chi ha il report 5 o chi ha il 6. */
  constructor(devices) {
    this.commandDevice = devices.find((d) => hasFeatureReport(d, REPORT_CMD)) ?? null;
    this.transferDevice = devices.find((d) => hasFeatureReport(d, REPORT_XFER)) ?? null;
    if (!this.commandDevice || !this.transferDevice) {
      const error = new Error("bootloader senza i report 5 e 6");
      error.code = "hid.noBootloaderReports";
      throw error;
    }
  }

  async open() {
    await ensureOpen(this.commandDevice);
    await ensureOpen(this.transferDevice);
  }

  /** Feature report 5: `bytes` sono i 5 byte di dati (WebHID aggiunge il report id da solo). */
  async command(bytes) {
    await this.commandDevice.sendFeatureReport(REPORT_CMD, Uint8Array.from(bytes));
  }

  /** Feature report 6: `bytes` = [0x77, ...2048 byte di pagina]. */
  async writePage(bytes) {
    await this.transferDevice.sendFeatureReport(REPORT_XFER, bytes);
  }

  /**
   * Legge il feature report 6. Restituisce 2049 byte [0x72, ...pagina].
   * PUNTO DA VERIFICARE SULL'HARDWARE: alcuni sistemi restituiscono il report id come primo byte (allora i byte sono 2050 e il primo vale 6);
   * qui lo si toglie se c'è, così isp.js vede sempre [0x72, pagina].
   */
  async readPage() {
    const view = await this.transferDevice.receiveFeatureReport(REPORT_XFER);
    let bytes = new Uint8Array(view.buffer, view.byteOffset, view.byteLength);
    if (bytes.length === PAGE_SIZE + 2 && bytes[0] === REPORT_XFER && bytes[1] === XFER_READ) bytes = bytes.subarray(1);
    return bytes;
  }
}
