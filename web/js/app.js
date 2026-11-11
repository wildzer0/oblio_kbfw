/*
 * app.js  -  la logica dell'interfaccia: cosa succede a ogni click e in che ordine. È il file da leggere per primo.
 *
 * Il flusso, in breve
 * -------------------
 *   1. "Collega la tastiera" (click) -> finestra di scelta di Chrome -> la tastiera (258a:dead) o, se è già in modalità aggiornamento,
 *      il bootloader (0603:1020/1021). Se è la tastiera, se ne legge la versione sul canale raw HID.
 *   2. Il firmware da installare: una release del menu (scaricata dallo stesso sito, vedi releases.json) oppure un file .hex trascinato.
 *      Il file è letto, controllato (js/ihex.js) e se ne mostra la versione; si confronta con quella installata.
 *   3. "Aggiorna la tastiera": conferma -> si manda la tastiera nel bootloader (feature report 5) -> il dispositivo cambia VID/PID e il browser
 *      lo vede come nuovo: serve un SECONDO click ("Seleziona il bootloader") a meno che il permesso sia già stato dato in passato ->
 *      flashImage() (js/isp.js) cancella, scrive, verifica, attiva -> si aspetta che la tastiera riappaia e se ne rilegge la versione.
 *
 * Tutto lo stato sta nell'oggetto `state`; render() lo disegna. Nessun framework: si modifica il DOM con textContent/hidden/disabled.
 */

import { webHidAvailable, requestDevices, grantedDevices, waitForDevices, findRawHid, findIspEntry, readVersionPacket, enterIsp, WebHidLink,
         KEYBOARD_FILTER, BOOTLOADER_FILTERS } from "./hid.js";
import { parseIntelHex, validateImage, versionFromReply, formatVersion, compareVersions, sha256Hex } from "./ihex.js";
import { flashImage } from "./isp.js";
import { t, errorText, setLanguage, initialLanguage, language } from "./i18n.js";

// ---------------------------------------------------------------------------------------------------------------------------
// Stato
// ---------------------------------------------------------------------------------------------------------------------------
const state = {
  keyboardDevices: [],     // i HIDDevice della tastiera in funzione (258a:dead): più interfacce = più elementi
  bootloaderDevices: [],   // i HIDDevice del bootloader (0603:1020/1021) già autorizzati e presenti
  installed: null,         // la versione letta dalla tastiera ({major, minor, patch, rev, dirty, batteryLog}) o null
  installedRead: false,    // true se si è provato a leggerla (per distinguere "non letta" da "non leggibile")
  firmware: null,          // { name, source, parsed, validation, sha256 } del file scelto
  releases: [],            // l'elenco di releases.json
  busy: false,             // un aggiornamento è in corso: i pulsanti si bloccano
};

const $ = (id) => document.getElementById(id);

// Il registro tecnico (dentro "Dettagli tecnici"): utile per capire dove si è fermato un aggiornamento e per chiedere aiuto.
function log(line) {
  const el = $("log");
  el.textContent += `${new Date().toLocaleTimeString()}  ${line}\n`;
  el.scrollTop = el.scrollHeight;
}

// ---------------------------------------------------------------------------------------------------------------------------
// Disegno dell'interfaccia
// ---------------------------------------------------------------------------------------------------------------------------

function versionText(v) {
  return formatVersion(v) + (v.batteryLog === false ? ` · ${t("fw.noBatteryLog")}` : "");
}

/** Il confronto fra la versione installata e quella da installare: { key, params, tone } o null se non c'è nulla da dire. */
function comparison() {
  const to = state.firmware?.validation.version;
  if (!to) return null;
  if (!state.installed) return { key: "cmp.unknown", params: {}, tone: "warn" };

  const params = { from: formatVersion(state.installed), to: formatVersion(to) };
  const order = compareVersions(to, state.installed);
  if (order > 0) return { key: "cmp.newer", params, tone: "ok" };
  if (order < 0) return { key: "cmp.older", params, tone: "warn" };
  if (to.rev !== state.installed.rev || to.dirty !== state.installed.dirty) return { key: "cmp.otherBuild", params, tone: "warn" };
  return { key: "cmp.same", params, tone: "ok" };
}

/** Un'immagine si può installare se ha passato i controlli, oppure se l'utente ha forzato un errore "forzabile" (file senza versione Oblio). */
function installable() {
  const fw = state.firmware;
  if (!fw) return false;
  if (fw.validation.ok) return true;
  return fw.validation.errors.every((e) => e.forceable) && $("forceBox").checked;
}

function tone(el, name) {
  el.classList.remove("ok", "warn", "error");
  if (name) el.classList.add(name);
}

/** Ridisegna tutto dallo stato. Si chiama dopo ogni cambiamento. */
function render() {
  // --- tastiera
  const hasKeyboard = state.keyboardDevices.length > 0;
  const hasBootloader = state.bootloaderDevices.length > 0;
  $("kbStatus").textContent = hasBootloader ? t("kb.bootloader") : hasKeyboard ? t("kb.connected") : t("kb.none");
  const noIsp = hasKeyboard && !hasBootloader && !findIspEntry(state.keyboardDevices);
  if (noIsp) $("kbStatus").textContent += " " + t("kb.noIsp");

  $("kbVersion").textContent = state.installed ? t("kb.installed", { version: versionText(state.installed) })
                              : (hasKeyboard && state.installedRead ? t("kb.unknownVersion") : "");

  // --- firmware
  const fw = state.firmware;
  $("fwFile").textContent = fw ? t("fw.loaded", { name: fw.name, size: fw.parsed.size }) : "";
  const v = fw?.validation.version;
  $("fwVersion").textContent = fw ? (v ? t("fw.toInstall", { version: versionText(v) }) : t("fw.noVersion")) : "";
  $("fwSha").textContent = fw ? t("fw.sha", { value: fw.sha256 }) : "";

  const list = $("fwErrors");
  list.replaceChildren();
  for (const problem of fw?.validation.errors ?? []) {
    const li = document.createElement("li");
    li.textContent = errorText({ code: problem.code, details: problem.details });
    list.append(li);
  }
  $("forceRow").hidden = !fw || fw.validation.ok || !fw.validation.errors.every((e) => e.forceable);

  // --- confronto e pulsante
  const cmp = comparison();
  $("compare").textContent = cmp ? t(cmp.key, cmp.params) : "";
  tone($("compare"), cmp?.tone);

  // Si può aggiornare con un file valido e una tastiera (in funzione con l'ingresso in ISP, oppure già in bootloader).
  const canEnter = hasBootloader || (hasKeyboard && !!findIspEntry(state.keyboardDevices));
  $("btnUpdate").disabled = state.busy || !installable() || !canEnter;
  $("btnConnect").disabled = state.busy;
  $("releaseSelect").disabled = state.busy;
  $("btnChoose").disabled = state.busy;
}

// ---------------------------------------------------------------------------------------------------------------------------
// La tastiera
// ---------------------------------------------------------------------------------------------------------------------------

/** Rilegge dai permessi del browser quali dispositivi ci sono, e (se è la tastiera in funzione) ne legge la versione. */
async function refreshDevices() {
  state.keyboardDevices = await grantedDevices([KEYBOARD_FILTER]);
  state.bootloaderDevices = await grantedDevices(BOOTLOADER_FILTERS);
  state.installed = null;
  state.installedRead = false;

  const raw = findRawHid(state.keyboardDevices);
  if (raw) {
    try {
      const packet = await readVersionPacket(raw);
      state.installedRead = true;
      state.installed = packet ? versionFromReply(packet) : null;
      log(packet ? `versione letta dalla tastiera: ${state.installed ? formatVersion(state.installed) : "(firmware senza versione)"}`
                 : "la tastiera non ha risposto alla richiesta di versione");
    } catch (error) {
      state.installedRead = true;
      log(`lettura della versione non riuscita: ${error.name}: ${error.message}`);
    }
  }
  render();
}

// "Collega la tastiera": il click apre la finestra di scelta. Si offrono sia la tastiera sia il bootloader (se è già in modalità aggiornamento).
async function onConnect() {
  try {
    const chosen = await requestDevices([KEYBOARD_FILTER, ...BOOTLOADER_FILTERS]);
    log(`scelti ${chosen.length} dispositivi`);
  } catch (error) {
    log(`scelta dispositivo: ${error.name}: ${error.message}`);
  }
  await refreshDevices();
}

// ---------------------------------------------------------------------------------------------------------------------------
// Il firmware
// ---------------------------------------------------------------------------------------------------------------------------

/** Legge, controlla e mostra un file .hex (testo). `source` dice da dove viene: "file" oppure "release". */
async function loadFirmwareText(text, name, source) {
  $("message").textContent = "";
  $("result").textContent = "";
  try {
    const parsed = parseIntelHex(text);
    const validation = validateImage(parsed);
    const sha256 = await sha256Hex(new TextEncoder().encode(text));
    state.firmware = { name, source, parsed, validation, sha256 };
    $("forceBox").checked = false;
    log(`${name}: ${parsed.size} byte di codice, ${validation.ok ? "controlli superati" : validation.errors.map((e) => e.code).join(", ")}`);
  } catch (error) {
    // Un file illeggibile (checksum, record non supportato...): si mostra come errore del file e si scarta.
    state.firmware = { name, source, parsed: { size: 0, data: new Uint8Array(0), ranges: [] }, sha256: "—",
                       validation: { ok: false, errors: [{ code: error.code ?? "generic.error", details: error.details ?? { message: error.message } }], version: null } };
    log(`${name}: ${error.message}`);
  }
  render();
}

async function loadFirmwareFile(file) {
  $("fwFile").textContent = t("fw.loading", { name: file.name });
  await loadFirmwareText(await file.text(), file.name, "file");
}

/** Scarica la release scelta nel menu (stesso sito: nessun problema di CORS) e la carica come un file. */
async function loadRelease(entry) {
  $("fwFile").textContent = t("fw.loading", { name: entry.file });
  try {
    const response = await fetch(entry.file, { cache: "no-cache" });
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    const text = await response.text();
    // L'impronta dichiarata in releases.json deve coincidere con quella del file scaricato: protegge da un file sostituito o corrotto.
    if (entry.sha256 && (await sha256Hex(new TextEncoder().encode(text))) !== entry.sha256) throw new Error("SHA-256 diverso da releases.json");
    await loadFirmwareText(text, `${entry.tag}/oblio.hex`, "release");
  } catch (error) {
    $("releaseError").hidden = false;
    $("releaseError").textContent = t("fw.releaseLoadFailed", { reason: error.message });
    log(`release ${entry.tag}: ${error.message}`);
  }
}

/** Riempie il menu con le release di releases.json (le più nuove per prime). In locale il file può non esserci: il menu resta vuoto. */
async function loadReleaseList() {
  const select = $("releaseSelect");
  select.replaceChildren();
  try {
    const response = await fetch("releases.json", { cache: "no-cache" });
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    state.releases = (await response.json()).releases ?? [];
  } catch (error) {
    state.releases = [];
    log(`elenco delle release non disponibile: ${error.message}`);
  }

  const placeholder = document.createElement("option");
  placeholder.value = "";
  placeholder.textContent = state.releases.length ? "—" : t("fw.releaseNone");
  select.append(placeholder);
  for (const [i, entry] of state.releases.entries()) {
    const option = document.createElement("option");
    option.value = String(i);
    option.textContent = `${entry.version}  ·  ${entry.date ?? ""}`;
    select.append(option);
  }
}

// ---------------------------------------------------------------------------------------------------------------------------
// L'aggiornamento
// ---------------------------------------------------------------------------------------------------------------------------

const STEP_ORDER = ["erase", "write", "verify", "enable", "wait"];

/** Evidenzia il passo corrente nella lista e muove la barra. Chiamata da flashImage() a ogni avanzamento. */
function showStep({ step, done, total }) {
  $("steps").hidden = false;
  $("bar").hidden = false;
  const at = STEP_ORDER.indexOf(step);
  for (const li of $("steps").children) {
    const index = STEP_ORDER.indexOf(li.dataset.step);
    li.classList.toggle("done", index < at || (index === at && done >= total));
    li.classList.toggle("active", index === at && done < total);
  }
  // La barra copre i quattro passi del flash con pesi uguali al lavoro reale: la scrittura e la verifica sono i passi lunghi.
  const weights = { erase: 5, write: 45, verify: 45, enable: 5, wait: 0 };
  let before = 0;
  for (const name of STEP_ORDER) { if (name === step) break; before += weights[name]; }
  $("bar").value = before + (weights[step] * done) / Math.max(total, 1);
  log(`${step} ${done}/${total}`);
}

/**
 * Dopo che la tastiera è saltata nel bootloader il browser la vede come un dispositivo NUOVO (altro VID/PID): serve il consenso dell'utente.
 * Si mostra il pulsante "Seleziona il bootloader" e si aspetta il click. Restituisce i dispositivi scelti ([] se annulla).
 */
function askForBootloader() {
  return new Promise((resolve) => {
    $("message").textContent = t("upd.needBootloader");
    const button = $("btnBootloader");
    button.hidden = false;
    button.onclick = async () => {
      try {
        const chosen = await requestDevices(BOOTLOADER_FILTERS);
        button.hidden = true;
        resolve(chosen);
      } catch (error) {
        log(`scelta del bootloader: ${error.name}: ${error.message}`);
        button.hidden = true;
        resolve([]);
      }
    };
  });
}

async function onUpdate() {
  if (state.busy || !installable()) return;
  if (!window.confirm(t("upd.confirm"))) return;

  state.busy = true;
  $("result").textContent = "";
  tone($("result"), null);
  $("steps").hidden = true;
  $("bar").hidden = true;
  render();

  try {
    // --- a) il bootloader: se la tastiera è già lì (Esc+1+2) si salta; altrimenti la si manda con il report 5.
    let bootDevices = state.bootloaderDevices;
    if (bootDevices.length === 0) {
      const entry = findIspEntry(state.keyboardDevices);
      if (!entry) throw Object.assign(new Error(t("kb.noIsp")), { code: "generic.error" });
      $("message").textContent = t("upd.entering");
      const { error } = await enterIsp(entry);
      log(`ingresso in ISP inviato${error ? ` (errore atteso: ${error.message})` : ""}`);

      // Se il permesso per il bootloader è già stato dato in una visita precedente, appare da solo; altrimenti serve il secondo click.
      bootDevices = await waitForDevices(BOOTLOADER_FILTERS, 4000);
      if (bootDevices.length === 0) bootDevices = await askForBootloader();
      if (bootDevices.length === 0) throw Object.assign(new Error(t("hid.noBootloaderChosen")), { code: "hid.noBootloaderChosen" });
    }

    // --- b) il collegamento al bootloader e il flash vero e proprio
    $("message").textContent = t("upd.busy");
    const link = new WebHidLink(bootDevices);
    await link.open();
    log("bootloader aperto");
    await flashImage(link, state.firmware.parsed.data, {
      onStep: showStep,
      onNote: (n) => log(`nota: ${n.note}${n.error ? ` (${n.error.message})` : ""}`),
    });

    // --- c) la tastiera riparte: la si aspetta (il permesso per 258a:dead c'è già) e se ne rilegge la versione
    showStep({ step: "wait", done: 0, total: 1 });
    const back = await waitForDevices([KEYBOARD_FILTER], 15000);
    showStep({ step: "wait", done: 1, total: 1 });
    state.busy = false;
    await refreshDevices();
    $("message").textContent = "";
    tone($("result"), "ok");
    $("result").textContent = t("upd.done") + " " + (back.length && state.installed ? t("upd.doneVersion", { version: versionText(state.installed) }) : t("upd.doneNoRead"));
  } catch (error) {
    // Qualunque cosa vada storta: messaggio chiaro e istruzioni di recupero. Il bootloader c'è sempre, la tastiera non si rovina.
    log(`ERRORE: ${error.name}: ${error.message}`);
    $("message").textContent = "";
    $("btnBootloader").hidden = true;
    tone($("result"), "error");
    $("result").textContent = t("upd.failed", { reason: errorText(error) }) + " " + t("upd.recovery");
  } finally {
    state.busy = false;
    await refreshDevices();
  }
}

// ---------------------------------------------------------------------------------------------------------------------------
// Avvio
// ---------------------------------------------------------------------------------------------------------------------------

function setupLanguage() {
  const apply = (lang) => {
    setLanguage(lang);
    for (const button of document.querySelectorAll(".lang button")) button.setAttribute("aria-pressed", String(button.dataset.lang === lang));
    loadReleaseList().then(render); // il testo del segnaposto del menu dipende dalla lingua
  };
  for (const button of document.querySelectorAll(".lang button")) button.onclick = () => apply(button.dataset.lang);
  apply(initialLanguage());
}

function setupDropZone() {
  const zone = $("dropZone");
  zone.addEventListener("dragover", (event) => { event.preventDefault(); zone.classList.add("over"); });
  zone.addEventListener("dragleave", () => zone.classList.remove("over"));
  zone.addEventListener("drop", async (event) => {
    event.preventDefault();
    zone.classList.remove("over");
    const file = event.dataTransfer.files[0];
    if (file && !state.busy) await loadFirmwareFile(file);
  });
  $("btnChoose").onclick = () => $("fileInput").click();
  $("fileInput").onchange = async () => { const file = $("fileInput").files[0]; if (file) await loadFirmwareFile(file); $("fileInput").value = ""; };
}

async function main() {
  setupLanguage();
  setupDropZone();

  $("unsupported").hidden = webHidAvailable();
  $("btnConnect").disabled = !webHidAvailable();
  if (!webHidAvailable()) { render(); return; }

  $("btnConnect").onclick = onConnect;
  $("btnUpdate").onclick = onUpdate;
  $("forceBox").onchange = render;
  $("releaseSelect").onchange = async () => {
    const entry = state.releases[Number($("releaseSelect").value)];
    $("releaseError").hidden = true;
    if (entry) await loadRelease(entry);
  };

  // La tastiera che si collega o scollega mentre la pagina è aperta si vede subito (non durante un aggiornamento: lo gestisce onUpdate).
  navigator.hid.addEventListener("connect", () => { if (!state.busy) refreshDevices(); });
  navigator.hid.addEventListener("disconnect", () => { if (!state.busy) refreshDevices(); });

  log(`lingua ${language()}, WebHID disponibile`);
  await refreshDevices();
}

main();
