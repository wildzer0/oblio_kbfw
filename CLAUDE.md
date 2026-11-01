# CLAUDE.md — Oblio (firmware Yunzii B68)

Leggi questo file all'inizio di ogni sessione. Rispondi **in italiano**. Per una panoramica completa e aggiornata (cosa fa il firmware, stato verificato, problemi aperti) leggi [docs/firmware-report.md](docs/firmware-report.md). Il README è per il pubblico (inglese, niente milestone); lo stato dei lavori sta in [CHANGELOG.md](CHANGELOG.md) e qui.

## Cos'è
Firmware nuovo, solo per la Yunzii B68 (SH68F90A + radio BK3632), scritto da zero: stile QMK-Vial ma snello e veloce.
Vincoli: codice fino a 0xDC00 (56.320 B; dal 2026-11-01, prima era 0xAC00 perché 16 KB erano dei profili), la keymap in flash da 0xDC00, impostazioni a 0xEC00, 4 KB di XDATA e 223 B di stack interno.
Repo: https://github.com/wildzer0/oblio_kbfw (privato), cartella `~/dev/oblio_kbfw`, branch `main`. Licenza GPL-2.0-or-later.

## Origine
Il porting funzionante sta nel repo vecchio `~/dev/smk` (fork SMK di wildzer0, branch `port/yunzii-b68`). È la **fonte di verità** per:
hardware e pin (`docs/keyboards/yunzii-b68*.md`, copiati qui in `docs/`), driver radio (`src/keyboards/yunzii-b68/b68radio*.c`), LED
(`layouts/default/indicators.c`), profili (`src/smk/keymap_store.c`), sleep (`user_sleep.c`, `src/smk/sleep.c`), test host (`tests/host/`),
dump del firmware stock (`~/dev/smk/backups/dump/`). **Riscrittura pulita**: si legge SMK per i concetti, i moduli si scrivono nuovi, niente
copia-incolla; dove si adatta un file altrui si mantiene la sua licenza e si elenca in NOTICE.md. SMK si cita sempre (NOTICE.md, README).

## Architettura voluta
- `hal/`: solo i registri usati (header minimi dal datasheet e dal dump, non l'intera sino51lib): gpio, tick, flash, watchdog, power, extint, PWM, UART.
- `core/`: C puro testato sull'host con gcc (matrice+debounce, layer, tapping LT/MT/OSM/OSL/TD, report, macro, store profili, scheduler radio).
- `board/`: pin, LED, radio hw, sleep, selettore modo, tasti personalizzati.
- Una sola scheda, niente `#ifdef` per più schede, allocazione statica, budget di cicli per l'interrupt LED.
- Recupero Esc+1+2 nei primissimi istanti del `main`, prima di USB e radio. Bootloader stock a 0xF000 invariato (flash con `sinowisp`).
- Build: Nix + SDCC + Makefile (una scheda, un hex). CI: GitHub Actions (test host, build, budget flash/XDATA, hex come artifact, Release al tag).

## Fasi (stato in CHANGELOG.md)
0 bootstrap (fatta) → 1 CI/build/budget → 2 HAL + matrice 5×16 → 3 logica pura con test host → 4 USB/raw HID (**solo dopo l'arrivo del
programmatore ICP**, Arduino Nano, per non rischiare di aprire la tastiera) → 5 Vial/VIA → 6 radio/LED/profili/sleep → 7 parità e passaggio.

## Stato al 2026-11-01
- Repo creato e pubblicato; `make test` gira ma non ci sono ancora test; la CI non è ancora stata vista girare su GitHub.
- Nel repo vecchio è stato aggiunto lo **sleep a runtime**: Fn+O / Fn+P (`B68_SLP_DN/UP`) scorrono 1, 5, 10, 30 min, mai (predefinito 10 min);
  i tasti 1–5 della riga numerica lampeggiano il passo. **L'utente lo sta provando sulla tastiera**: attendere l'esito prima di portare sleep/radio qui.
  Il vecchio repo ha quelle modifiche non committate (build OK: flash 41953/44032, XDATA 3957/4096).
- Fase 1 fatta (build SDCC, `make size`, CI con artifact e Release al tag; CI remota non ancora vista girare). Fase 2 scritta (HAL, tick, matrice con debounce, recupero): build e test host OK, **non provata sull'hardware**.
- Decisione dell'utente: **non si aspetta l'ICP**; si riscrive tutto ciò che c'è nel porting SMK (`~/dev/smk`, branch `port/yunzii-b68`) e si prova sulla tastiera quando il progetto è stabile e completo.
- Fase 3 fatta (keycodes, layer, report, tapping, macro, profili/store con flash finta nei test), commit e push a ogni passo (richiesta dell'utente). La CI remota è passata sui commit delle fasi 1–2.
- Fase 4 fatta (descrittori, macchina a stati EP0 testata sull'host, driver USB, coda dei report su EP1, raw HID su EP2, comando ISP → bootloader): **nessuna parte provata sull'hardware**.
- Fase 5 fatta (core/via+vial testati sull'host, `board/vial.json` + `tools/vial_gen.py` + `check_vial.py`): non provata con l'app Vial sull'hardware.
- Fase 6 fatta (encoder, impostazioni a 0xEC00, LED con tick alternato scansione/sottoquadri, radio BK3632 con selettore e slot BT, sleep): **nessuna parte provata sull'hardware**; flash 41194/44032 (2,8 KB di margine).
- Fase 7 fatta: correzioni (watchdog), HUD portato, controlli CI, diagnostica (`tools/diag.py`), build a gradini (`make stages`), ucsim (`make sim`), indicatore batteria (sorgente del dato ancora ignota), documenti. Flash 42549/44032.
- Prossimo passo: la **prima prova sulla tastiera**, a gradini, seguendo `docs/first-boot.md` (rischio dichiarato: senza ICP il recupero è solo Esc+1+2). Poi decodificare la batteria dai dati di `diag.py --radio`. Dopo una compilazione fallita controllare che `build/` non abbia un `.ihx` vecchio (ora c'è `.DELETE_ON_ERROR`). In ogni sessione aggiornare CHANGELOG.md.

## Come lavora l'utente (preferenze confermate)
- Scrivo **io** tutto il codice e la documentazione; l'utente fa solo le prove sull'hardware. Ogni modifica va spiegata in modo che possa rivederla.
- Per funzioni non banali: **modalità piano** e approvazione prima di scrivere codice. Non cambiare decisioni già prese senza accordo.
- **Niente tentativi alla cieca** sull'hardware: ragionare dal dump stock e dal codice di riferimento, strumentare (tracce leggibili) e dire chiaramente
  cosa è ipotesi e cosa verificato. Chiedere prove hardware solo se rispondono a una domanda precisa.
- **Simulatore (ucsim)**: l'utente l'ha ammesso dove serve a provare più a fondo (2026-11-01). `make sim` (`tests/sim/`, richiede `nix develop`, ~30 s) esegue il firmware vero nel simulatore del chip: verifica l'ordine degli eventi, non le durate (il Timer 2 del modello è a cadenza virtuale). Non sostituisce la prova sull'hardware.
- Documentazione, milestone e schede dei compiti le tengo io aggiornate; schede semplici (descrizione + cosa mi aspetto), niente quiz di revisione.
- Commit e push solo se richiesti. Cartella `.vscode/` non tracciata. Le modifiche utili alla memoria di sessione vanno anche qui, perché la memoria
  automatica è legata alla cartella del vecchio repo.

## Comandi
```
nix develop --command make test            # test host
# vecchio repo, build di rilascio B68:
cd ~/dev/smk && nix develop --command bash -c 'meson setup build -Dboards=yunzii-b68 --buildtype=release && meson compile -C build'
sinowisp write --platform sh68f90 --vendor_id 0x258a --product_id 0xdead --format ihex --isp_iface_num 0 -f <file>.hex
```
Mappa flash del porting: applicazione fino a 0xAC00; profili 0–3 (2 copie × 4 settori) 0xAC00–0xEBFF; impostazioni 0xEC00; marcatore 0xEE00; bootloader stock 0xF000.
Recupero: Esc+1+2 mentre si inserisce l'USB.

## Aggiornamento 2026-11-01 (notte)
- **Profili tolti** (decisione dell'utente: non li usava): erano 4 keymap × 2 copie × 2 KB = **16 KB di flash di indirizzamento**. Ora una sola keymap
  con le sue due copie a **0xDC00–0xEBFF** e il codice può arrivare a **0xDC00 (56.320 B)**: +12,3 KB. Flash 43528/56320, XDATA 3772/4096.
  La keymap salvata è ripartita dai default una volta (cambia l'indirizzo del deposito) e i tasti `Fn+Z/X/C/V` non esistono più.
- **Switch Win/Mac**: è **P0.5** (ingresso con pull-up, **alto = Mac**, basso = Windows — misurato sulla tastiera). Non è un semplice swap:
  Windows usa i layer 0/1, Mac i layer **5/4** (base con Alt↔GUI scambiati e **Fn suo**), tutti e quattro editabili da Vial.
- **Prossimo passo deciso dall'utente**: batteria + Caps Lock wireless dal frame `08` della radio (mai decodificato), poi combos/key overrides,
  mouse dall'encoder, Vial RGB. Serve la misura: `diag.py --radio` a carica piena / uso normale / quasi scarica.
  **Fatto il 2026-11-02**: mouse dall'encoder (`core/mouse.c`, report ID 7, solo USB). Flash 44945/56320, XDATA 3788/4096, test host 23.
  Il protocollo Vial autentico (opcode `dynamic_vial_*`, struct `vial_combo_entry_t` 10 B, `vial_key_override_entry_t` 10 B) è in `docs/vial.md`.
  **Fatti il 2026-11-02**: mouse dall'encoder (`core/mouse.c`, report ID 7, solo USB) e **combos** (`core/combo.c`, scheda di Vial, 8 accordi da 4
  tasti, tasti trattenuti al massimo 50 ms). Flash 46989/56320, XDATA 3907/4096, test host 24. Restano: batteria + Caps Lock wireless (serve la
  misura `diag.py --radio`), key overrides, Vial RGB.
- CI GitHub: il job `test` passa sempre; il job `simulation` falliva per **timeout** del harness su un runner lento (non un bug del firmware):
  ora il test aspetta 320 scansioni invece di 900 e il timeout del socket è 180 s.
- Bluetooth: funziona (il telefono vede `OBLIO`), il PC dell'utente no — sospetto un legame vecchio su Windows, non il firmware.

## Aggiornamento 2026-11-01 (sera)
- Il gradino 2 sull'hardware ha fallito (LED a scatti, nessun tasto): causa = ISR troppo lunghe (4697/8963 istruzioni). Corretto con tabella DUTY2 precalcolata, ISR a indirizzi costanti, slot di scansione 500 µs, ritiro dei LED se il main non gira, contatori diag senza sblocco. Flash 43477/44032, XDATA 3703/4096. **Da riprovare sull'hardware**; se fallisce, `tools/diag.py` (senza sblocco) dice causa reset, slot in ritardo, scansioni perse.
- Flicker in USB (2026-11-01): la griglia fissa di slot troncava lo slot LED dopo ogni scansione lunga (~800 µs misurati). Ora gli slot sono temporizzati dall'inizio della propria ISR come in SMK (core/sched, hal/tick); orologio ms dal tempo reale. Non provato sull'hardware; flash 43814/44032. Dati utili: `py diag.py --watch 1` (con `hidapi` su Windows).
