/*
 * i18n.js  -  i testi dell'interfaccia in italiano e in inglese.
 *
 * Come funziona
 * -------------
 *  - Ogni testo ha una chiave ("step1.title"). I testi fissi della pagina stanno in index.html con l'attributo data-i18n="chiave": setLanguage()
 *    li riscrive tutti quando si cambia lingua.
 *  - I testi che compongono numeri e nomi usano segnaposto {nome}: t("fw.sha", { value: "..." }).
 *  - Gli errori dei moduli (ihex.js, isp.js, hid.js) portano un `code` stabile ("hex.checksum", "isp.verify"...): errorText() li traduce.
 *    Aggiungere un errore = aggiungere il codice nel modulo e le due frasi qui sotto.
 *  - La lingua iniziale è quella del browser (it* -> italiano, altrimenti inglese); la scelta dell'utente si ricorda in localStorage.
 * Per aggiungere una lingua basta aggiungere un oggetto con le stesse chiavi in TEXTS e un pulsante in index.html.
 */

const TEXTS = {
  it: {
    "app.title": "Aggiornamento firmware Oblio",
    "app.subtitle": "Yunzii B68 · aggiorna la tastiera direttamente dal browser",
    "app.unsupported": "Questo browser non supporta WebHID. Usa Chrome, Edge o Opera (non Firefox né Safari), su una pagina https o localhost.",

    "step1.title": "1 · Tastiera",
    "step1.connect": "Collega la tastiera",
    "step1.help": "Premi il pulsante e scegli «Oblio B68» nella finestra del browser. Se la tastiera non risponde, scollega l'USB e ricollegala tenendo premuti Esc + 1 + 2: entra in modalità aggiornamento.",
    "kb.none": "Nessuna tastiera collegata.",
    "kb.connected": "Tastiera collegata.",
    "kb.installed": "Versione installata: {version}",
    "kb.unknownVersion": "Versione installata: non leggibile (firmware precedente alla 0.9.0, oppure il canale di diagnostica non risponde).",
    "kb.bootloader": "La tastiera è in modalità aggiornamento (bootloader): non ha un firmware attivo, si può aggiornare subito.",
    "kb.noIsp": "Il browser non permette di mandare la tastiera in modalità aggiornamento da qui. Scollega l'USB e ricollegala tenendo premuti Esc + 1 + 2, poi premi «Collega la tastiera» e scegli il bootloader.",

    "step2.title": "2 · Firmware",
    "fw.release": "Release pubblicate",
    "fw.releaseNone": "(nessuna release pubblicata)",
    "fw.releaseLoadFailed": "Non riesco a leggere l'elenco delle release ({reason}). Puoi comunque trascinare un file .hex.",
    "fw.drop": "Trascina qui un file .hex",
    "fw.or": "oppure",
    "fw.choose": "scegli un file",
    "fw.loading": "Carico {name}…",
    "fw.loaded": "File: {name} ({size} byte di codice)",
    "fw.toInstall": "Versione da installare: {version}",
    "fw.noVersion": "Versione da installare: non presente nel file.",
    "fw.sha": "SHA-256: {value}",
    "fw.batteryLog": "con registro batteria",
    "fw.noBatteryLog": "senza registro batteria",
    "fw.force": "Forza l'installazione di un file senza versione Oblio (solo per esperti)",

    "cmp.same": "Questa versione è già installata.",
    "cmp.newer": "Aggiornamento: da {from} a {to}.",
    "cmp.older": "Attenzione: la versione scelta ({to}) è PIÙ VECCHIA di quella installata ({from}).",
    "cmp.otherBuild": "Stessa versione, ma costruita da un altro commit ({from} → {to}).",
    "cmp.unknown": "Non posso confrontare con la versione installata.",

    "step3.title": "3 · Aggiornamento",
    "upd.button": "Aggiorna la tastiera",
    "upd.confirm": "Sto per cancellare e riscrivere il firmware della tastiera.\n\nLa keymap e le impostazioni tornano ai valori predefiniti. Non scollegare l'USB fino alla fine.\n\nContinuare?",
    "upd.entering": "Passo la tastiera in modalità aggiornamento…",
    "upd.needBootloader": "La tastiera è ora in modalità aggiornamento. Premi «Seleziona il bootloader» e scegli il dispositivo nella finestra (cambia nome: il browser lo vede come un dispositivo nuovo).",
    "upd.selectBootloader": "Seleziona il bootloader",
    "upd.step.erase": "Cancellazione",
    "upd.step.write": "Scrittura",
    "upd.step.verify": "Verifica",
    "upd.step.enable": "Attivazione e riavvio",
    "upd.step.wait": "Attendo la tastiera",
    "upd.done": "Aggiornamento completato.",
    "upd.doneVersion": "Ora la tastiera riporta la versione {version}.",
    "upd.doneNoRead": "Tastiera riavviata. Se non compare qui, ricollegala con «Collega la tastiera».",
    "upd.failed": "Aggiornamento non riuscito: {reason}",
    "upd.recovery": "La tastiera non è danneggiata: il bootloader è sempre presente. Scollega l'USB e ricollegala tenendo premuti Esc + 1 + 2, poi riprova.",
    "upd.details": "Dettagli tecnici",
    "upd.busy": "Aggiornamento in corso: non scollegare l'USB.",

    "lang.it": "Italiano",
    "lang.en": "English",

    "hex.badLine": "riga {line} del file non valida",
    "hex.checksum": "checksum errato alla riga {line}: il file è corrotto",
    "hex.afterEnd": "dati dopo la fine del file (riga {line})",
    "hex.beyond64k": "dati oltre i 64 KB (riga {line})",
    "hex.unsupportedRecord": "tipo di record {type} non supportato (riga {line})",
    "hex.noEnd": "il file non ha il record di fine: è troncato",
    "hex.empty": "il file non contiene dati",
    "image.beyondCode": "il file ha dati agli indirizzi {start}-{end}, oltre il limite del codice ({limit}): sono l'area di keymap, impostazioni e bootloader",
    "image.resetVector": "il primo byte non è un salto (LJMP): non è un firmware per questa tastiera",
    "image.notOblio": "il file non contiene la versione Oblio: non è un firmware Oblio (o è una build di profilo)",
    "image.severalVersions": "il file contiene {count} versioni: immagine anomala",
    "isp.badSize": "immagine di lunghezza sbagliata ({length} byte)",
    "isp.badReply": "il bootloader ha risposto in modo anomalo alla pagina {page}",
    "isp.verify": "la verifica non coincide al byte {offset} (atteso {expected}, letto {found}): il firmware NON è stato attivato",
    "hid.noBootloaderReports": "il dispositivo scelto non è un bootloader (mancano i report 5 e 6)",
    "hid.noBootloaderChosen": "non è stato scelto nessun bootloader",
    "generic.error": "{message}",
  },

  en: {
    "app.title": "Oblio firmware updater",
    "app.subtitle": "Yunzii B68 · update the keyboard straight from the browser",
    "app.unsupported": "This browser does not support WebHID. Use Chrome, Edge or Opera (not Firefox or Safari), on an https or localhost page.",

    "step1.title": "1 · Keyboard",
    "step1.connect": "Connect the keyboard",
    "step1.help": "Press the button and choose “Oblio B68” in the browser's window. If the keyboard does not answer, unplug the USB and plug it in again holding Esc + 1 + 2: it enters update mode.",
    "kb.none": "No keyboard connected.",
    "kb.connected": "Keyboard connected.",
    "kb.installed": "Installed version: {version}",
    "kb.unknownVersion": "Installed version: not readable (firmware older than 0.9.0, or the diagnostics channel does not answer).",
    "kb.bootloader": "The keyboard is in update mode (bootloader): it has no active firmware, you can update right away.",
    "kb.noIsp": "The browser does not let this page put the keyboard in update mode. Unplug the USB and plug it in again holding Esc + 1 + 2, then press “Connect the keyboard” and choose the bootloader.",

    "step2.title": "2 · Firmware",
    "fw.release": "Published releases",
    "fw.releaseNone": "(no release published)",
    "fw.releaseLoadFailed": "Cannot read the list of releases ({reason}). You can still drop a .hex file.",
    "fw.drop": "Drop a .hex file here",
    "fw.or": "or",
    "fw.choose": "choose a file",
    "fw.loading": "Loading {name}…",
    "fw.loaded": "File: {name} ({size} bytes of code)",
    "fw.toInstall": "Version to install: {version}",
    "fw.noVersion": "Version to install: not present in the file.",
    "fw.sha": "SHA-256: {value}",
    "fw.batteryLog": "with battery log",
    "fw.noBatteryLog": "without battery log",
    "fw.force": "Force the installation of a file without an Oblio version (experts only)",

    "cmp.same": "This version is already installed.",
    "cmp.newer": "Update: from {from} to {to}.",
    "cmp.older": "Warning: the chosen version ({to}) is OLDER than the installed one ({from}).",
    "cmp.otherBuild": "Same version, built from a different commit ({from} → {to}).",
    "cmp.unknown": "Cannot compare with the installed version.",

    "step3.title": "3 · Update",
    "upd.button": "Update the keyboard",
    "upd.confirm": "I am about to erase and rewrite the keyboard's firmware.\n\nThe keymap and the settings go back to their defaults. Do not unplug the USB until the end.\n\nContinue?",
    "upd.entering": "Putting the keyboard in update mode…",
    "upd.needBootloader": "The keyboard is now in update mode. Press “Select the bootloader” and choose the device in the window (it has a new name: the browser sees it as a new device).",
    "upd.selectBootloader": "Select the bootloader",
    "upd.step.erase": "Erasing",
    "upd.step.write": "Writing",
    "upd.step.verify": "Verifying",
    "upd.step.enable": "Activating and restarting",
    "upd.step.wait": "Waiting for the keyboard",
    "upd.done": "Update complete.",
    "upd.doneVersion": "The keyboard now reports version {version}.",
    "upd.doneNoRead": "Keyboard restarted. If it does not show up here, reconnect it with “Connect the keyboard”.",
    "upd.failed": "Update failed: {reason}",
    "upd.recovery": "The keyboard is not damaged: the bootloader is always there. Unplug the USB and plug it in again holding Esc + 1 + 2, then try again.",
    "upd.details": "Technical details",
    "upd.busy": "Update in progress: do not unplug the USB.",

    "lang.it": "Italiano",
    "lang.en": "English",

    "hex.badLine": "line {line} of the file is not valid",
    "hex.checksum": "wrong checksum on line {line}: the file is corrupt",
    "hex.afterEnd": "data after the end of the file (line {line})",
    "hex.beyond64k": "data beyond 64 KB (line {line})",
    "hex.unsupportedRecord": "record type {type} not supported (line {line})",
    "hex.noEnd": "the file has no end record: it is truncated",
    "hex.empty": "the file has no data",
    "image.beyondCode": "the file has data at addresses {start}-{end}, beyond the code limit ({limit}): that is the keymap, settings and bootloader area",
    "image.resetVector": "the first byte is not a jump (LJMP): this is not a firmware for this keyboard",
    "image.notOblio": "the file has no Oblio version: it is not an Oblio firmware (or it is a profile build)",
    "image.severalVersions": "the file contains {count} versions: unusual image",
    "isp.badSize": "image of the wrong length ({length} bytes)",
    "isp.badReply": "the bootloader answered page {page} in an unexpected way",
    "isp.verify": "the verification differs at byte {offset} (expected {expected}, read {found}): the firmware was NOT activated",
    "hid.noBootloaderReports": "the chosen device is not a bootloader (reports 5 and 6 are missing)",
    "hid.noBootloaderChosen": "no bootloader was chosen",
    "generic.error": "{message}",
  },
};

let current = "en";

/** La lingua di partenza: quella ricordata, altrimenti quella del browser. */
export function initialLanguage() {
  try {
    const saved = localStorage.getItem("oblio-lang");
    if (saved && TEXTS[saved]) return saved;
  } catch { /* localStorage può non esserci (modalità privata): non è un problema */ }
  return (navigator.language || "en").toLowerCase().startsWith("it") ? "it" : "en";
}

/** Cambia lingua e riscrive i testi fissi della pagina (gli elementi con data-i18n; data-i18n-title per l'attributo title). */
export function setLanguage(lang) {
  if (!TEXTS[lang]) return;
  current = lang;
  try { localStorage.setItem("oblio-lang", lang); } catch { /* vedi sopra */ }
  document.documentElement.lang = lang;
  for (const el of document.querySelectorAll("[data-i18n]")) el.textContent = t(el.dataset.i18n);
  for (const el of document.querySelectorAll("[data-i18n-title]")) el.title = t(el.dataset.i18nTitle);
}

export const language = () => current;

/** Il testo della chiave nella lingua corrente, con i segnaposto {nome} sostituiti. Una chiave mancante si vede come tale (utile per correggerla). */
export function t(key, params = {}) {
  const text = TEXTS[current][key] ?? TEXTS.en[key] ?? key;
  return text.replace(/\{(\w+)\}/g, (_, name) => (name in params ? String(params[name]) : `{${name}}`));
}

/** Traduce un errore dei moduli (HexError, IspError, errori con `code`); gli altri mostrano il loro messaggio. */
export function errorText(error) {
  if (error && error.code && TEXTS.en[error.code]) return t(error.code, error.details ?? {});
  return t("generic.error", { message: error?.message ?? String(error) });
}
