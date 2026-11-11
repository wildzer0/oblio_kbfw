// Prova di punta a punta dell'interfaccia in Chrome (senza tastiera): vedi inject.js. Si salta da sola se Chrome o build/oblio.hex mancano.
// Lancio: node --test tests/web/e2e/run.mjs   (parte anche con  make web-test)
import test from "node:test";
import assert from "node:assert/strict";
import { createServer } from "node:http";
import { existsSync, readFileSync } from "node:fs";
import { spawn } from "node:child_process";
import { extname, join, normalize } from "node:path";
import { homedir } from "node:os";
import { createHash } from "node:crypto";

const root = process.cwd(); // la radice del repository: serve web/, tests/, build/ e VERSION
const TYPES = { ".html": "text/html", ".js": "text/javascript", ".mjs": "text/javascript", ".css": "text/css", ".json": "application/json", ".hex": "text/plain" };

// Dove trovare Chrome: variabile CHROME, poi i nomi comuni, poi quello di Playwright se c'è.
function findChrome() {
  const candidates = [process.env.CHROME, "/usr/bin/google-chrome", "/usr/bin/google-chrome-stable", "/usr/bin/chromium", "/usr/bin/chromium-browser",
    join(homedir(), ".cache/ms-playwright/chromium-1254/chrome-linux64/chrome")];
  return candidates.find((p) => p && existsSync(p)) ?? null;
}

// Il server: /__e2e/ è la vera web/index.html con inject.js messo prima di app.js e <base href="/web/"> perché style.css e js/ si trovino;
// tutto il resto è servito dalla radice del repository.
function serve() {
  return new Promise((resolve) => {
    const server = createServer((req, res) => {
      const url = new URL(req.url, "http://x");
      if (url.pathname === "/__e2e/") {
        let html = readFileSync(join(root, "web/index.html"), "utf8");
        html = html.replace("<head>", '<head><base href="/web/">')
                   .replace('<script type="module" src="js/app.js"></script>',
                            '<script type="module" src="/tests/web/e2e/inject.js"></script><script type="module" src="js/app.js"></script>');
        res.writeHead(200, { "content-type": "text/html" }).end(html);
        return;
      }
      // Il sito delle release, finto: releases.json e il file della release 0.9.0 (è il build/oblio.hex di adesso), come li pubblicherebbe la CI.
      if (url.pathname === "/web/releases.json" || url.pathname === "/web/releases/v0.9.0/oblio.hex") {
        const hex = readFileSync(join(root, "build/oblio.hex"));
        const body = url.pathname.endsWith(".json")
          ? JSON.stringify({ releases: [{ tag: "v0.9.0", version: "0.9.0", date: "2026-11-11", file: "releases/v0.9.0/oblio.hex",
                                          sha256: createHash("sha256").update(hex).digest("hex"), size: hex.length }] })
          : hex;
        res.writeHead(200, { "content-type": url.pathname.endsWith(".json") ? "application/json" : "text/plain" }).end(body);
        return;
      }
      const file = normalize(join(root, url.pathname));
      if (!file.startsWith(root) || !existsSync(file)) { res.writeHead(404).end("not found"); return; }
      res.writeHead(200, { "content-type": TYPES[extname(file)] ?? "text/plain" }).end(readFileSync(file));
    });
    server.listen(0, "127.0.0.1", () => resolve(server));
  });
}

function runChrome(chrome, url) {
  return new Promise((resolve, reject) => {
    const child = spawn(chrome, ["--headless=new", "--no-sandbox", "--disable-gpu", "--virtual-time-budget=120000", "--dump-dom", url], { stdio: ["ignore", "pipe", "ignore"] });
    let out = "";
    child.stdout.on("data", (d) => (out += d));
    child.on("error", reject);
    child.on("close", () => resolve(out));
  });
}

const chrome = findChrome();
const skip = !chrome ? "Chrome non trovato (imposta CHROME)" : !existsSync("build/oblio.hex") ? "manca build/oblio.hex (make fw)" : false;

// split: il bootloader come uno o due dispositivi; release: il firmware dal menu delle release invece che trascinato
for (const [split, release] of [[0, 0], [1, 0], [0, 1]]) {
  test(`aggiornamento completo dall'interfaccia, bootloader in ${split ? "due dispositivi" : "un dispositivo"}, firmware ${release ? "dal menu delle release" : "trascinato"}`, { skip, timeout: 180000 }, async () => {
    const server = await serve();
    try {
      const html = await runChrome(chrome, `http://127.0.0.1:${server.address().port}/__e2e/?split=${split}&release=${release}`);
      const match = html.match(/<div id="e2e-result">([^<]*)<\/div>/);
      assert.ok(match, "la pagina non ha prodotto un esito");
      assert.match(match[1], /^PASS /, match[1]);
    } finally {
      server.close();
    }
  });
}
