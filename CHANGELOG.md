# Changelog — Oblio

## Piano (fasi)
0. Bootstrap (repo, flake, CI, docs) — **fatto**
1. CI: build del firmware, budget flash/XDATA, hex come artifact, Release al tag — **fatto** (CI remota non ancora vista girare)
2. HAL minimo e matrice 5×16 — **scritta, non provata sull'hardware** (build e test host OK)
3. Logica pura con test sull'host (layer, tapping, report, macro, profili) — **fatta** (passi 1–5, 8 file di test)
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

### Fase 3, passo 2 — report HID
- `core/report.[ch]`: stato di ciò che l'host deve sapere e costruzione dei byte. Report boot da 8 byte (modificatori + 6 tasti nell'ordine di pressione; il settimo non entra), bitmap NKRO (modificatori + 120 bit, come il descrittore stock), modificatori a sinistra nei bit 0–3 e a destra 4–7, uso consumer e system (uno alla volta) con le tabelle tasto → uso (HUT 1.12).
- Il core non invia nulla: chiama `report_hook_send(kind)` a ogni cambio, una volta per cambio, così un tasto premuto e rilasciato nello stesso giro produce comunque due report. Il trasporto (USB, fase 4; radio, fase 6) implementa l'hook; per ora `board/transport.c` è un hook vuoto.
- `core/keyboard.c`: gli hook dei modificatori del layer ora scrivono nel report; `send_keycode` gestisce tasti, modificatori (KC_LCTL…), `LSFT(KC_A)`-style (modificatori prima del tasto alla pressione, dopo alla fine), consumer e system. Alla fine del tasto il report esce prima che i modificatori one-shot vengano tolti.
- `core/keycodes.h`: aggiunti tasti media/system, `LCTL/LSFT/LALT/LGUI/RCTL/…(kc)`, macro `IS_*_KEYCODE`, `MOD_BIT`, `MODS_FROM_MASK` (spostata da layer.c, ora condivisa).
- Bug trovato dai test: `LSFT(kc)` sommava per errore `QK_MODS` (che vale già `MOD_LCTL << 8`), dando 0x0305 invece di 0x0205; il test `test_keycode_encoding` ora fissa i valori usati da Vial.
- Test: `test_report.c` (ordine e limite dei 6 tasti, NKRO, modificatori, tabelle degli usi, valori dei codici), `test_keyboard.c` (tasto semplice, modificatore, mods+tasto, tasto di layer con rilascio dopo il cambio, consumer/system, OSM e OSL su un solo tasto).
- Verifica: `make test` 5 file OK; `make fw size` → flash 6333/44032, XDATA 1616/4096. Simulatore non usato; nessuna prova sull'hardware.

### Fase 3, passo 3 — tapping (LT, MT, tap dance)
- `core/tapping.[ch]`: tasti tap-or-hold `LT(layer, kc)`, `MT(mods, kc)` e `TD(n)`. Un tocco più corto del tapping term invia `kc`; tenuto oltre il term attiva il layer o tiene i modificatori. Mentre il tasto è indeciso gli eventi successivi finiscono in una coda di 8 e vengono riprodotti a decisione presa, così un tasto battuto durante l'attesa cade sul layer giusto. Niente permissive hold: decide il rilascio (tap) o il timeout (hold). Coda piena → l'attesa diventa un hold subito.
- Tap dance: tap, hold (se vuoto si tiene l'azione del tap), doppio tocco (il tocco singolo aspetta allora un altro term); term proprio per ogni tap dance (campo 4); un'azione che è a sua volta un `TD` non fa nulla (evita la ricorsione).
- `core/keymap.[ch]`: 8 tap dance × 5 parole (80 B) e il tapping term generale (200 ms) tenuti in RAM, con valori iniziali dalla scheda; `board/keymap_default.c` definisce TD(0) = encoder (tocco mute, hold play/pausa, doppio tocco traccia successiva).
- `core/keyboard.c`: pipeline `tapping_intercept` → ricerca nel layer → `tapping_event` → azione; hook del tapping (replay, tap, modificatori, layer, keycode) implementati qui; nuova `keyboard_task(now)` chiamata dal `main` a ogni giro.
- Il tempo è sempre un `uint16_t` in ms passato come argomento; le differenze usano l'aritmetica a 16 bit, quindi il giro dell'orologio (65536 ms) non rompe il term.
- Test `test_tapping.c` (11 test a livello di report: LT tap/hold, tasti battuti durante l'attesa su entrambi i layer, MT tap/hold con il tasto sotto, TD tap con attesa, doppio tocco, hold, TD senza doppio tocco, coda piena, giro dell'orologio, tapping term modificato). Verificato che falliscano: con una mutazione temporanea (hold scambiato con tap) danno 13 errori.
- Verifica: `make test` 6 file OK; `make fw size` → flash 8626/44032, XDATA 1749/4096. Simulatore non usato; nessuna prova sull'hardware.

### Fase 3, passo 4 — macro
- `core/keymap.[ch]`: il buffer delle macro di Vial (16 macro in 512 B, ognuna chiusa da uno 0) vive in RAM con `keymap_macro_byte/read/write/clear`.
- `core/macro.[ch]`: interprete del buffer. Testo digitato come su tastiera US (lettere, cifre, punteggiatura con shift, `\b \t \n ESC DEL`), azioni dopo il prefisso 0x01 (tap/down/up di un tasto, ritardo, le stesse tre con codice a 16 bit; i codici con byte basso 0 sono memorizzati come 0xFF0n, come fa Vial), buffer con ultimo byte diverso da 0 = in riscrittura o rovinato, non si suona niente; azioni troncate chiudono la macro.
- **Differenza rispetto al porting SMK:** la macro non ferma la tastiera. SMK la eseguiva fino in fondo con attese bloccanti (la scansione si fermava, e con un tick a 8 bit un'attesa >255 ms avrebbe fatto perdere tempo); qui `macro_task(now)` fa un passo al millisecondo, quindi ogni report arriva all'host prima del successivo (nessuna coda di report necessaria nel trasporto USB) e un ritardo è una scadenza. Una macro avviata mentre un'altra è in corso aspetta il suo turno (coda di 4; oltre, scartata).
- `core/keyboard.c`: i tasti `QK_MACRO` avviano la macro alla pressione (e contano come tasto premuto/rilasciato per i one-shot); gli hook della macro usano lo stesso `send_keycode`; `keyboard_task` fa avanzare anche le macro.
- Test `test_macro.c` (10 test: testo, ogni carattere stampabile ha un tasto, azioni a 8 e 16 bit, codici con byte zero, ritardo come scadenza anche oltre 255 ms, un passo per ms, scelta della macro, coda, buffer rovinato, azioni troncate) e un test d'integrazione in `test_keyboard.c` (tasto macro → report).
- Verifica: `make test` 7 file OK; `make fw size` → flash 10555/44032, XDATA 2291/4096. Simulatore non usato; nessuna prova sull'hardware.

### Fase 3, passo 5 — profili e store in flash
- `core/store.[ch]`: 4 profili completi (tasti, encoder, macro, tap dance, impostazioni) in coppie di slot A/B da 4 settori da 512 B, 0xAC00–0xEBFF (poi le impostazioni a 0xEC00). Un salvataggio cancella lo slot *non* in uso, scrive il contenuto e per ultimo l'intestazione con numero di sequenza e CRC-16: lo slot vale solo se completo e al caricamento vince quello valido con la sequenza più recente (confronto sicuro al giro del contatore). Un taglio di corrente lascia la copia precedente intatta. Il contenuto è letto/scritto attraverso le funzioni del keymap, non da copie parallele.
- **Differenze dal porting SMK:** magic `OB` versione 1 (gli slot scritti da SMK, `KM`, non vengono interpretati); la validità si controlla con la sola CRC prima di applicare (SMK applicava ogni slot mentre li validava); l'accesso alla flash passa da tre hook (`store_hook_read/erase/program`), sostituiti nei test da una flash finta.
- Impronta della mappa predefinita nell'intestazione, come nel porting: se il firmware nuovo cambia i valori predefiniti, la copia salvata non si usa (le modifiche fatte da Vial vanno rifatte una volta). Da rivalutare in fase 7.
- Salvataggio dopo 2 s senza modifiche e senza tasti premuti (`store_task`, tempo in ms); il cambio profilo (`store_request`) avviene a tasti tutti rilasciati e scrive prima il profilo che si lascia; `store_hook_loaded` avverte il firmware (la scheda azzera lo stato dei layer).
- `core/keymap.[ch]`: impostazioni per profilo (5 parole: effetto LED, luminosità, velocità, tapping term, colore) con valori predefiniti dalla scheda (`0, 160, 4, 200, 0`), `keymap_reset()`; il tapping term ora è l'impostazione `CFG_TAP_TERM` (0 = 200 ms). `matrix_any_pressed()` nel core della matrice.
- `hal/flash.[ch]`: scrittura della flash da applicazione (registri SSP). **Protezione:** cancella/programma solo tra 0xAC00 e 0xF000, qualunque indirizzo riceva (l'applicazione e il bootloader stock non si toccano). Interrupt spenti per ogni operazione, quindi qualche tick va perso durante un salvataggio (decine di ms). **Non provato sull'hardware.**
- `core/keyboard.c`: i codici della fascia `QK_KB` passano da `keyboard_hook_custom`; `board/custom_keys.c` gestisce i tasti profilo 1–4 (`store_request`). `board/main.c` carica il profilo all'avvio e chiama `store_task`. Il profilo in uso *non* viene ancora ricordato dopo lo spegnimento: serve l'area impostazioni (fase 6); per ora si parte dal profilo 0.
- Test `test_store.c` (14 test: flash vuota → default, modifiche dopo il riavvio, slot alternati e vince il più recente, slot più recente rovinato → quello precedente, impronta diversa, slot di SMK ignorato, **taglio di corrente a ogni 7ª operazione di un salvataggio** → sempre profilo vecchio o nuovo completo, mai misto (anche al primo salvataggio), profili indipendenti, cambio a tasti rilasciati, hook di caricamento, salvataggio solo dopo la quiete e senza tasti, giro dell'orologio, reset dei tasti che non tocca le macro). La flash finta segnala scritture che la vera non farebbe (bit riportato a 1 senza cancellare, accessi fuori dall'area). Verificato che falliscano: saltando l'ultimo settore da cancellare i test danno 9 errori.
- Verifica: `make test` 8 file OK; `make fw size` → flash 14767/44032, XDATA 2378/4096. Simulatore non usato; nessuna prova sull'hardware.

### Fine fase 3
Logica pura completa: matrice, layer, report, tapping, macro, profili. Resta da collegare all'hardware: USB e raw HID (fase 4), Vial/VIA (5), LED/radio/sleep/impostazioni (6).
