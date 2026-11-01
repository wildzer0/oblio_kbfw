# Changelog — Oblio

## Piano (fasi)
0. Bootstrap (repo, flake, CI, docs) — **fatto**
1. CI: build del firmware, budget flash/XDATA, hex come artifact, Release al tag — **fatto** (CI remota non ancora vista girare)
2. HAL minimo e matrice 5×16 — **scritta, non provata sull'hardware** (build e test host OK)
3. Logica pura con test sull'host (layer, tapping, report, macro, profili)
4. USB e raw HID (dopo l'arrivo del programmatore ICP)
5. Vial/VIA
6. Radio, LED, profili, sleep
7. Parità con il porting SMK e passaggio

Il controllo di recupero Esc+1+2 va all'inizio del `main`, prima di USB e radio.

## 2026-11-01
- Repo creato. Documenti hardware, radio e LED portati dal porting SMK (`docs/`).

### Fase 1 — build, budget, release
- `board/main.c`: `main` segnaposto (loop vuoto) per esercitare compilatore, linker e budget; lo sostituisce la fase 2.
- `Makefile`: nuovi target `fw` (SDCC → `build/oblio.ihx` e `build/oblio.hex` con `packihx`), `size` (controllo budget); `clean` rimuove anche `build/`. Flag SDCC ripresi dal vecchio repo: modello large, `--stack-auto`, XDATA 4096, codice fino a 0xAC00.
- `tools/budget.py`: legge il `.mem` del linker, stampa «flash usata/limite, XDATA usata/dimensione» ed esce con errore se uno dei due è superato.
- `.github/workflows/ci.yml`: dopo i test host eseguono `make fw` e `make size`; l'hex è caricato come artifact `oblio-hex`; nuovo job `release` che sui tag `v*` allega `oblio.hex` alla Release.
- Verifica locale: `make test` (0 test), `make fw size` → flash 174/44032, XDATA 0/4096; il controllo fallisce correttamente con un limite troppo basso. Simulatore non usato. CI su GitHub non verificata.

### Fase 2 — HAL minimo e matrice 5×16
Si procede senza aspettare l'ICP, riscrivendo ciò che il porting SMK già fa; la prima prova sulla tastiera avverrà a riscrittura completa e stabile.
- `hal/sh68f90.h`: solo i registri usati (porte 0–7 con direzione e pull-up, drive, watchdog, clock/PLL, Timer 2, EA/ET2); indirizzi del chip, gli stessi di dump e porting.
- `hal/gpio.h`, `hal/watchdog.h` (kick compilato solo con `-DWATCHDOG_ENABLE`, come nel porting), `hal/clock.[ch]` (PLL a 24 MHz), `hal/delay.[ch]` (attese in µs/ms con un ciclo da 24 cicli: ipotesi da misurare con l'oscilloscopio).
- `hal/tick.[ch]`: tick da 1 ms con Timer 2 (2000 conteggi a 2 MHz, ricavato dai 801 conteggi = 400 µs del porting). L'interrupt incrementa solo un contatore (niente chiamate: evita la collisione di overlay di SDCC); il `main` consuma i tick con `tick_take()`, quindi un giro lento si recupera. Il vettore è dichiarato in `tick.h` perché SDCC lo cerca nel modulo del `main`.
- `core/matrix.[ch]` (C puro): debounce per colonna (un valore nuovo vale dopo `MATRIX_DEBOUNCE_SCANS` = 5 scansioni uguali di fila), coda di 16 eventi `{col,row,pressed}`. Con la coda piena il cambio non si perde: resta in sospeso e si accetta alla scansione dopo (`matrix_deferred()` conta i rinvii).
- `board/matrix_hw.[ch]`: stato dei pin a riposo uguale allo stock (`board_gpio_init`) e scansione delle 16 colonne con tabelle di maschere per porta (niente `switch` da 16 casi); doppia lettura delle righe, colonna scartata se le due letture differiscono.
- `board/recovery.[ch]`: Esc+1+2 tenuti per 6 campioni da 10 ms, poi salto a 0xFF00 con A=0x5A, B=0xA5 e interrupt spenti. Chiamato subito dopo clock e GPIO, prima di tick/USB/radio. **Non provato sull'hardware.**
- `board/main.c`: sostituisce lo stub; per ora gli eventi si scartano (li userà la fase 3).
- `tests/host/test_matrix.c` e `tests/host/check.h`: 7 test (pressione/rilascio dopo il debounce, rimbalzi, glitch che riavvia la finestra, due tasti nella stessa colonna e ordine, colonne indipendenti, bit oltre l'ultima riga, coda piena senza perdite).
- `Makefile`: i sorgenti del firmware sono `core/`, `hal/`, `board/`; `test_X.c` si compila con `core/X.c`.
- Verifica: `make test` 1 file, 7 test OK; `make fw size` → flash 1821/44032, XDATA 101/4096; vettori controllati nell'hex (0x0000 → startup, 0x0003 → `tick_isr`). Simulatore non usato. Non verificati: periodo del tick, ritardi, recupero, timing della scansione (tutto richiede l'hardware). Non portati ancora: LDO per l'USB, trappola per interrupt inattesi.

### Fase 3, passo 1 — codici tasto, keymap in RAM, layer
- `core/keycodes.h`: codici a 16 bit con gli stessi intervalli di QMK/Vial (servono già alla fase 5), solo ciò che si usa: intervalli, macro MO/TO/TG/DF/OSL/OSM/TT/TD/LT/MT/LM con relativa decodifica, maschere dei modificatori, tasti della mappa predefinita.
- `core/keymap.[ch]`: 8 layer × 5×16 e i due codici di rotazione dell'encoder per layer, in RAM (1280 B + 32 B); parte dalla mappa predefinita fornita dalla scheda; contatore di versione per le modifiche.
- `core/layer.[ch]`: layer attivi sopra il layer predefinito, trasparenza, MO/TO/TG/DF/OSL/OSM/TT/LM; il codice trovato alla pressione è quello usato al rilascio (nessun tasto resta bloccato se il layer cambia nel frattempo). Il tempo entra come argomento (`now`), il core non legge nessun timer.
- `core/keyboard.[ch]`: collega gli eventi della matrice ai layer; cerca il tasto *prima* di `layer_key_pressed()` perché un OSL deve ancora essere attivo per il tasto che aspetta. Gli hook dei modificatori sono vuoti finché non c'è il report (passo 2).
- `core/platform.h`: unica differenza tra 8051 e host per il codice puro (`ROM` = `__code` su SDCC).
- `board/keymap_default.c` e `board/b68_keys.h`: mappa predefinita della B68 (base + Fn, encoder = volume, tap dance 0 sul tasto dell'encoder) e codici personalizzati 0x7E00–0x7E13, stessi numeri del porting SMK.
- `board/main.c`: contatore dei millisecondi, gli eventi della matrice vanno a `keyboard_event`.
- Test: `test_layer.c` (11 test: ricerca, trasparenza, rilascio con lo stesso codice, TG/TO/DF, layer non valido, OSL, OSM anche a destra, LM, TT con 5 tocchi e con pausa, encoder) e `test_default_keymap.c` (67 celle come misurate sulla scheda, nessun tasto Fn dove la base è vuota). Il `Makefile` ricompila sempre i test e accetta moduli extra per test (`LINK_x`).
- Verifica: `make test` 3 file OK; `make fw size` → flash 5031/44032, XDATA 1590/4096. Simulatore non usato; nessuna prova sull'hardware.
