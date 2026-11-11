# CLAUDE.md — Oblio (firmware Yunzii B68)

Leggi questo file all'inizio di ogni sessione; per riprendere il lavoro da dove era rimasto c'è [docs/session-state.md](docs/session-state.md)
(ultimo commit, cosa aspetta una prova sulla tastiera, domande aperte, misure da fare). Rispondi **in italiano**. Per una panoramica completa e aggiornata (cosa fa il firmware, stato verificato, problemi aperti) leggi [docs/firmware-report.md](docs/firmware-report.md). Il README è per il pubblico (inglese, niente milestone); lo stato dei lavori sta in [CHANGELOG.md](CHANGELOG.md) e qui.

## Cos'è
Firmware nuovo, solo per la Yunzii B68 (SH68F90A + radio BK3632), scritto da zero: stile QMK-Vial ma snello e veloce.
Vincoli: codice fino a **0xDA00** (55.808 B; 56.320 B senza registro batteria), la keymap in flash da 0xDC00, impostazioni a 0xEC00, 4 KB di XDATA e 223 B di stack interno.
Repo: https://github.com/wildzer0/oblio_kbfw (pubblico, `gh` lo dà PUBLIC), cartella `~/dev/oblio_kbfw`, branch `main`. Licenza GPL-2.0-or-later.

## Origine
Il porting funzionante sta nel repo vecchio `~/dev/smk` (fork SMK di wildzer0, branch `port/yunzii-b68`). È la **fonte di verità** per:
hardware e pin (`docs/keyboards/yunzii-b68*.md`, copiati qui in `docs/`), driver radio (`src/keyboards/yunzii-b68/b68radio*.c`), LED
(`layouts/default/indicators.c`), profili (`src/smk/keymap_store.c`), sleep (`user_sleep.c`, `src/smk/sleep.c`), test host (`tests/host/`),
dump del firmware stock (`~/dev/smk/backups/dump/`). **Riscrittura pulita**: si legge SMK per i concetti, i moduli si scrivono nuovi, niente
copia-incolla; dove si adatta un file altrui si mantiene la sua licenza e si elenca in NOTICE.md. SMK si cita sempre (NOTICE.md, README).

## Architettura
- `hal/`: solo i registri usati (gpio, tick, flash, watchdog, power, PWM, USB, clock).
- `core/`: C puro testato sull'host con gcc (matrice+debounce, layer, tapping, combo, override, leader, macro, report, store/settings, radio, link, VIA/Vial/diag, USB).
- `board/`: pin, matrice, LED, radio hw, sleep, selettore modo, switch Win/Mac, batteria, tasti personalizzati.
- Una sola scheda, niente `#ifdef` per più schede, allocazione statica. Recupero Esc+1+2 nei primissimi istanti del `main`. Bootloader stock a 0xF000 invariato (flash con `sinowisp`).
- Build: Nix + SDCC + Makefile (un hex). CI: GitHub Actions (test host, riferimento API, build, budget, vettori, simulazione; hex come artifact; Release al tag).
- Per il perché di ogni scelta: [docs/design.md](docs/design.md); per le funzioni: [docs/api.md](docs/api.md) (generato: `make api`); per iniziare: [docs/guida-sviluppatore.md](docs/guida-sviluppatore.md).

## Stato al 2026-11-02 (chiusura)
Il firmware è **completo e provato dall'utente sulla tastiera**: USB e radio (2.4G e Bluetooth), Vial (keymap, tap dance, combo, key override, Lighting), NKRO, Caps Word, Leader, mouse dall'encoder,
switch Win/Mac (P0.5), batteria (lettura ADC a 10 bit dalla radio, curva Li-Po, avviso e sleep di protezione), remote wakeup USB, backlight stabile (USB e salvataggi), keymap salvata come immagine + log.
Aggiornamento dal browser: `web/` (utility WebHID, vedi docs/design.md §14.1), versione in `VERSION` + `core/version.c`, sito su GitHub Pages dal tag. Ultimi numeri: flash ~54,9/55,8 KB (log batteria incluso), XDATA ~3,7/4 KB, 29 test host, 23 test nel simulatore. Cronologia in [CHANGELOG.md](CHANGELOG.md), punti ancora aperti in [docs/session-state.md](docs/session-state.md)
(la scala della batteria poggia su un solo punto misurato; le build a gradini sono state tolte; le sequenze del Leader le decide l'utente in `board/leader.c`).
Se compare un problema: `py tools\diag.py` (contatori, `--radio`) e `docs/session-state.md` → "Trappole già pagate".

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
nix develop --command make test            # test host (27) + definizione Vial
nix develop --command make fw size         # build/oblio.hex e budget flash/XDATA
nix develop --command make check-interrupts check-api sim web-test   # web-test: utility web (Node + Chrome senza testa)
sinowisp write --platform sh68f90 --vendor_id 0x258a --product_id 0xdead --format ihex --isp_iface_num 0 -f build/oblio.hex
```
Mappa flash: applicazione fino a 0xDA00 (0xDC00 con `make BATTERY_LOG=0`), registro batteria 0xDA00 (opzionale); keymap (2 copie × 4 settori, con log) 0xDC00–0xEBFF; impostazioni 0xEC00; marcatore del bootloader 0xEE00 (mai toccare); bootloader stock 0xF000.
Recupero: Esc+1+2 mentre si inserisce l'USB. Il vecchio repo `~/dev/smk` (branch `port/yunzii-b68`) resta la fonte per hardware e radio; il dump stock è in `~/dev/smk/backups/dump/` (`tools/dis51.py` lo disassembla).
