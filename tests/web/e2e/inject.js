/*
 * inject.js  -  prova di punta a punta dell'interfaccia (web/js/app.js) in Chrome senza tastiera.
 *
 * Come funziona: run.mjs serve la vera web/index.html con questo script iniettato PRIMA di app.js. Qui si sostituisce `navigator.hid` con una
 * tastiera finta (e il finto bootloader di tests/web/fake_bootloader.mjs), e poi si recita la sequenza di un utente: collega la tastiera,
 * trascina il file .hex, preme "Aggiorna", sceglie il bootloader. Alla fine scrive l'esito in <div id="e2e-result"> che run.mjs legge.
 *
 * Parametro ?split=1: il bootloader appare come DUE dispositivi (uno per il report 5, uno per il 6), come potrebbe fare Chrome su Windows.
 */
import { FakeBootloader } from "../fake_bootloader.mjs";
import { findVersions } from "../../../web/js/ihex.js";

const split = new URLSearchParams(location.search).get("split") === "1";
const fromRelease = new URLSearchParams(location.search).get("release") === "1"; // il firmware viene dal menu e non dal trascinamento
localStorage.setItem("oblio-lang", "en"); // testi in inglese: le verifiche cercano frasi inglesi

// ---- il mondo finto -------------------------------------------------------------------------------------------------------
const world = { keyboardPresent: true, bootPresent: false, granted: new Set(), log: [] };
const boot = new FakeBootloader({ rebootThrows: true });
let installed = { major: 0, minor: 8, patch: 0, rev: "oldrev0", flags: 1 }; // la versione che la tastiera dichiara all'inizio

class MockDevice extends EventTarget {
  constructor(info, isPresent, handlers = {}) {
    super();
    Object.assign(this, info);
    this.opened = false;
    this.isPresent = isPresent;
    this.handlers = handlers;
  }
  async open() { this.opened = true; }
  async close() { this.opened = false; }
  async sendFeatureReport(id, data) { world.log.push(`feature ${id} (${data.length} B)`); return this.handlers.sendFeature?.(id, data); }
  async receiveFeatureReport(id) { const bytes = await this.handlers.receiveFeature(id); return new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength); }
  async sendReport(id, data) { return this.handlers.sendReport?.(id, data, this); }
}

const collection = (usagePage, featureIds = []) => ({ usagePage, usage: 1, type: 1, inputReports: [], outputReports: [],
  featureReports: featureIds.map((reportId) => ({ reportId, items: [] })) });

// la tastiera in funzione: due dispositivi, come li vedrebbe Chrome (raw HID, e la collezione vendor con il report 5)
const rawDevice = new MockDevice({ vendorId: 0x258a, productId: 0xdead, productName: "Oblio B68", collections: [collection(0xff60)] }, () => world.keyboardPresent, {
  sendReport(id, data, self) {
    // risposta al comando 0xF1 tipo 2: [F1, 2, major, minor, patch, flags, commit x8, dirty]
    const reply = new Uint8Array(32);
    reply.set([0xf1, 2, installed.major, installed.minor, installed.patch, installed.flags, ...[...installed.rev].map((c) => c.charCodeAt(0)), 0, 0]);
    setTimeout(() => self.dispatchEvent(Object.assign(new Event("inputreport"), { data: new DataView(reply.buffer) })), 0);
  },
});
const ispDevice = new MockDevice({ vendorId: 0x258a, productId: 0xdead, productName: "Oblio B68", collections: [collection(0xff00, [5])] }, () => world.keyboardPresent, {
  sendFeature(id, data) {
    if (id === 5 && data[0] === 0x75) { world.keyboardPresent = false; world.bootPresent = true; } // salta nel bootloader: la tastiera sparisce
  },
});

// il bootloader: uno o due dispositivi
const bootHandlers = {
  sendFeature: async (id, data) => {
    if (id === 5) {
      try { await boot.command(Array.from(data)); } catch (e) { if (!boot.disconnected) throw e; }
      if (boot.disconnected) { // REBOOT: il bootloader sparisce e la tastiera riparte con il firmware appena scritto
        world.bootPresent = false;
        world.keyboardPresent = true;
        const [v] = findVersions(boot.mem);
        installed = { major: v.major, minor: v.minor, patch: v.patch, rev: v.rev, flags: v.flags };
        throw new Error("device disconnected"); // come un dispositivo vero che sparisce durante il comando
      }
    } else if (id === 6) await boot.writePage(data);
  },
  receiveFeature: async () => boot.readPage(),
};
const bootInfo = { vendorId: 0x0603, productId: 0x1020, productName: "ISP" };
const bootDevices = split
  ? [new MockDevice({ ...bootInfo, collections: [collection(0xff00, [5])] }, () => world.bootPresent, bootHandlers),
     new MockDevice({ ...bootInfo, collections: [collection(0xff00, [6])] }, () => world.bootPresent, bootHandlers)]
  : [new MockDevice({ ...bootInfo, collections: [collection(0xff00, [5, 6])] }, () => world.bootPresent, bootHandlers)];

const everything = [rawDevice, ispDevice, ...bootDevices];
const matches = (device, filters) => filters.some((f) => device.vendorId === f.vendorId && device.productId === f.productId);

// navigator.hid è una proprietà di sola lettura del browser: si sostituisce con defineProperty (sull'istanza, non sul prototipo)
Object.defineProperty(navigator, "hid", { configurable: true, value: Object.assign(new EventTarget(), {
  async getDevices() { return everything.filter((d) => world.granted.has(d) && d.isPresent()); },
  async requestDevice({ filters }) {
    const found = everything.filter((d) => d.isPresent() && matches(d, filters));
    found.forEach((d) => world.granted.add(d)); // lo sceglie l'utente: da qui il permesso è ricordato
    return found;
  },
}) });

// ---- l'utente ---------------------------------------------------------------------------------------------------------------
const wait = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
const $ = (id) => document.getElementById(id);
async function until(predicate, what, ms = 60000) {
  for (let waited = 0; waited < ms; waited += 50) { if (predicate()) return; await wait(50); }
  throw new Error("timeout: " + what);
}
const report = (ok, text) => {
  const el = document.createElement("div");
  el.id = "e2e-result";
  el.textContent = (ok ? "PASS " : "FAIL ") + text;
  document.body.append(el);
};

window.confirm = () => true; // l'utente conferma

window.addEventListener("load", async () => {
  try {
    await wait(300);
    $("btnConnect").click();
    await until(() => $("kbVersion").textContent.includes("0.8.0"), "installed version shown");

    if (fromRelease) {
      await until(() => $("releaseSelect").options.length > 1, "release list loaded");
      $("releaseSelect").value = "0";
      $("releaseSelect").dispatchEvent(new Event("change"));
    } else {
      const hexText = await (await fetch("/build/oblio.hex")).text();
      const dt = new DataTransfer();
      dt.items.add(new File([hexText], "oblio.hex"));
      $("dropZone").dispatchEvent(new DragEvent("drop", { dataTransfer: dt, bubbles: true, cancelable: true }));
    }
    await until(() => $("fwVersion").textContent.includes("Version to install"), "firmware version shown");
    await until(() => $("compare").textContent.includes("Update: from 0.8.0"), "comparison shown");
    if ($("btnUpdate").disabled) throw new Error("update button is disabled");

    $("btnUpdate").click();
    // dopo il salto nel bootloader serve il secondo click, come per un utente vero
    const chooser = setInterval(() => { if (!$("btnBootloader").hidden) { clearInterval(chooser); $("btnBootloader").click(); } }, 100);
    await until(() => $("result").textContent.length > 0, "update finished");
    clearInterval(chooser);

    const result = $("result").textContent;
    const wanted = (await (await fetch("/VERSION")).text()).trim();
    const ok = result.includes("Update complete") && result.includes(`reports version ${wanted}`) && boot.enabled
      && world.log.filter((l) => l.startsWith("feature 6")).length === 30;
    report(ok, `${result} | split=${split} release=${fromRelease} | log=${world.log.length}`);
  } catch (error) {
    report(false, `${error.message} | result="${$("result")?.textContent}" | status="${$("message")?.textContent}"`);
  }
});
