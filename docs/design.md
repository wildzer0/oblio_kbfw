# Oblio — architettura e design

Documento di riferimento per chi deve capire, modificare o estendere il firmware. Spiega **perché** è fatto così e come i moduli
si parlano; il **cosa** di ogni funzione (firma e commento) sta in [api.md](api.md), generato dagli header (`make api`).
Gli altri documenti: [architecture.md](architecture.md) (sintesi in inglese), [firmware-report.md](firmware-report.md) (stato verificato),
[parity.md](parity.md) (funzioni contro stock/SMK), [session-state.md](session-state.md) (dove si riprende), `docs/keyboards/` (hardware e radio).

Stato dei numeri: 2026-11-02, flash **~52,6 KB su 56,3 KB**, XDATA **~3,7 KB su 4 KB**, stack interno al picco ~150 B su 223 B.

---

## 1. Cos'è e quali vincoli ha

Firmware per **una sola scheda**: la **Yunzii B68** (66 tasti + un encoder con pulsante).

| | |
| --- | --- |
| MCU | Sinowealth **SH68F90A**: core 8051 a 1 ciclo di clock, 24 MHz (PLL), 64 KB di flash, **4096 B di XDATA**, 256 B di RAM interna (stack incluso), USB device, UART, Timer 2, 18 canali PWM |
| Radio | **BK3632** su seriale (UART0, 260870 baud) per 2.4 GHz (dongle) e Bluetooth (3 slot) |
| Alimentazione | batteria Li‑Po 5000 mAh / 4,2 V; la radio ne misura la tensione |
| Matrice | 16 colonne (strobe) × 5 righe (ingressi con pull‑up); le colonne sono **anche** i pozzi dei LED RGB |
| Strumenti | SDCC 4.5 (`--model-large --stack-auto`), gcc per i test, Nix per l'ambiente |
| Recupero | nessun programmatore ICP: **Esc+1+2 mentre si inserisce il cavo** salta al bootloader ISP stock a `0xF000` |

I vincoli che hanno plasmato quasi ogni scelta sono tre: **la flash** (budget 56320 B per il codice, il resto è dati e bootloader), **la XDATA**
(4 KB per tutto: keymap, framebuffer LED, code dei report, buffer) e **lo stack interno** (223 B, condiviso tra main loop e quattro interrupt).

## 2. Principi di progetto

1. **Un core puro, un confine di hook.** Tutto ciò che non tocca un registro è C portabile in `core/` e gira anche sull'host con gcc, quindi è testato
   in millisecondi. Quando il core ha bisogno del mondo esterno dichiara una funzione `modulo_hook_*()` che la scheda (o un test) definisce.
   Esempi: `report_hook_send`, `store_hook_read/erase/program`, `radio_hook_*`, `usb_hook_*`, `settings_hook_led_get/set`.
2. **Il tempo entra come argomento.** Nessun modulo del core legge un timer: riceve `now` (ms, 16 bit con wrap) dal main loop. I test simulano il tempo e il
   wrap dei 65 s.
3. **Allocazione statica, nessun `#ifdef` per altre schede.** Niente `malloc`. Un solo hex.
4. **Chi scrive è uno solo.** Un dato condiviso con un interrupt ha un solo scrittore, è di un byte, oppure si rilegge finché due letture coincidono; il passaggio
   di consegne avviene con un flag. Nessuna funzione chiamata da un interrupt è chiamata anche dal main (`tools/check_interrupts.py` vieta la
   sovrapposizione statica delle variabili locali di SDCC).
5. **Puntatori tipizzati.** In XDATA un puntatore generico costa 3 byte e un accesso lento: nei percorsi dei pacchetti si usa `XDATA uint8_t *` (`core/platform.h`).
   Su questo core divisioni e moduli con variabili sono molto costosi: si evitano nei percorsi caldi e negli interrupt.
6. **Il dato rischioso non si scrive fuori area.** `hal/flash.c` rifiuta qualunque indirizzo fuori da `0xDC00–0xF000`, comunque il chiamante lo passi.
7. **Misurare, non indovinare.** Contatori di diagnostica sempre presenti (`core/diag.c`), un simulatore del chip per i conteggi di istruzioni (`make sim`),
   e le ipotesi marcate come tali nei documenti.

## 3. Struttura del repository

```
core/    logica pura (testata sull'host)          hal/    registri e driver del chip
board/   ciò che è specifico della B68            tools/  generatori e controlli (vial_gen, check_*, budget, diag, gen_api)
tests/   host/ (gcc) e sim/ (ucsim)               docs/   questo documento e il resto
```

| Strato | Regola | Come si prova |
| --- | --- | --- |
| `core/` | niente registri, niente tempo letto da sé; parla solo con gli hook | `make test` (28 programmi, gcc `-Wall -Wextra -Werror`) |
| `hal/` | solo i registri usati, header minimi dal datasheet e dal dump | il simulatore e l'hardware |
| `board/` | pin, matrice, PWM dei LED, seriale della radio, sleep, keycode della scheda, e la **colla** che implementa gli hook del core | `check_vial.py`, il simulatore, l'hardware |

Flusso delle dipendenze: `board → core` (chiama le funzioni del core) e `core → board` **solo attraverso hook** (il linker li risolve). `hal` è usato da `board`
e da pochi `core` che hanno un driver proprio nel chip (USB, tick).

## 4. Mappa di memoria

**Flash (64 KB)**

| Indirizzi | Contenuto |
| --- | --- |
| `0x0000–0xD9FF` | applicazione (limite imposto da `make size`: 55808 B con il registro batteria; 56320 B, fino a `0xDBFF`, con `BATTERY_LOG=0`) |
| `0xDA00–0xDBFF` | **registro della batteria** (un settore, 128 record da 4 B): solo se `BATTERY_LOG=1` (predefinito), altrimenti codice |
| `0xDC00–0xEBFF` | **deposito della keymap**: due copie (A/B) da 4 settori da 512 B; in fondo a ogni copia un **log** di 69 record da 5 B |
| `0xEC00–0xEDFF` | **log delle impostazioni** (sleep, slot Bluetooth, luci): 36 record da 14 B, append-only |
| `0xEE00–0xEFFF` | marcatore del bootloader stock: **mai toccato** |
| `0xF000–0xFFFF` | bootloader stock (invariato; si entra con `isp_jump`) |

**XDATA (4096 B)**: i grossi sono la keymap in RAM (~1,7 KB: 6 layer × 80 tasti, encoder, macro 512 B, tap dance, combo, key override, cfg), il framebuffer dei LED (~750 B) e
le code di radio e USB. I buffer degli endpoint USB e i registri PWM stanno a indirizzi fissi (`0x1100…`, `0xFF80…`).

**RAM interna**: variabili in `idata`, banco di registri e stack con `--stack-auto` (i locali stanno sullo stack, quindi le funzioni sono rientranti). Lo stack è dipinto
all'avvio (`diag_stack_paint`) e il massimo raggiunto si legge con la diagnostica.

## 5. Avvio e main loop

`board/main.c`:

```
init():   RSTSTAT letto (causa del reset) → diag_init → REGCON (regolatore) → clock_init (PLL 24 MHz) → board_gpio_init → watchdog
          → recovery_check()  ← prima di USB e radio: se Esc+1+2 sono giù, salta al bootloader
          → matrix_init, keyboard_init → settings_load → store_init (keymap da flash o default) → settings_apply_led → os_switch_init
          → encoder_init → led_init (+ led_hw_init) → radio_hw_init, link_init → sleep_init → tick_init (parte il Timer 2) → USB → EA = 1

loop:     watchdog_kick → millisecondi dal tick → scansione pronta? matrix_feed (debounce) → encoder_poll → eventi → keyboard_event
          → keyboard_task → os_switch_task → store_task → settings_task → usb_wake_task → usb_task → led_task, led_app_task
          → link_task (radio) → battery_task → sleep_hw_task
```

Il main loop è un **ciclo di polling piatto**: non c'è scheduler né RTOS. Un giro costa ~1100 istruzioni a riposo; l'unica cosa che blocca davvero sono le operazioni di flash
(vedi §9). Il **watchdog** è punzecchiato dal main e dai ritardi, **non** dall'attesa della scansione: un main bloccato lo lascia scattare.

## 6. Tempo e interrupt

Gli interrupt sono **quattro** (verificati da `check_interrupts.py`):

| Interrupt | Compito | Priorità |
| --- | --- | --- |
| **Timer 2** (`hal/tick.c`) | suona gli *slot*: un sottoquadro del backlight o una scansione della matrice; conta i millisecondi | normale |
| **USB** (`hal/usb.c`) | EP0 (enumerazione), EP1/EP2; non copia pacchetti: segna e basta | normale |
| **UART0** (`board/radio_hw.c`) | un byte ogni 38 µs dalla/alla radio, anello RX da 32 B | **massima** (come lo stock) |
| **INT4** (`hal/power.c`) | risveglio dal power‑down | — |

**Perché gli slot.** Le 16 linee di colonna sono sia gli strobe dei tasti sia i pozzi di corrente dei LED: una scansione e un sottoquadro di LED **non possono sovrapporsi**.
`core/sched.c` definisce la sequenza: **4 slot LED da 400 µs, poi 1 scansione** (~510–610 µs misurati), e così via. **Ogni slot è temporizzato dall'inizio del proprio
interrupt**: il timer viene riarmato per lo slot che comincia, e a fine scansione si forza l'overflow perché lo slot LED successivo parta subito. Così uno slot LED è sempre di 400 µs pieni
e una scansione lunga non accorcia il successivo (con una griglia fissa una colonna su cinque restava accesa una frazione del tempo: sfarfallio).

Il millisecondo è la **somma del tempo realmente trascorso** tra interrupt, in conteggi del timer (2000 per ms). L'interrupt copia valori pre‑calcolati e scrive registri; tutto il resto
(effetti, debounce, logica) sta nel main.

**Cosa disturba i LED** (lezioni pagate):
- Un interrupt (USB) che trattiene il Timer 2 sposta l'inizio di uno slot: la colonna resta accesa più o meno a lungo. Per questo l'USB **non copia** i pacchetti raw HID nell'ISR e la diagnostica ha
  il contatore «timer interrupt: longest start delay».
- Il **back‑off**: se il main non gira per 8 scansioni di fila (CPU affamata), il backlight si fa da parte per 12 slot, così i tasti continuano a funzionare. (Con 3 scattava anche in uso normale, in radio: lampeggio dei LED mentre si scriveva.)
- Le **cancellazioni di flash** fermano la CPU per ~32 ms: il timer e il PWM vengono messi in pausa solo attorno a quelle (§9).

I conteggi di istruzioni sono esatti nel simulatore (sottoquadro 433, scansione 4638) ma **non le durate**: il suo Timer 2 è virtuale.

### 6.1 Dove va il tempo, e come lo si risparmia

Misurato con la build di profilo (`LOOP_PROFILE=1`, §14): l'interrupt del timer prende ~39 % della CPU (la scansione, ~560 µs ogni 2 ms, e i 4 sottoquadri da 433 istruzioni), il resto è il main loop, di cui il ridisegno dei LED
(`led_task`) era quasi metà. Le regole che ne sono uscite, tutte a **comportamento invariato** (i test host e il simulatore lo verificano):

| Dove | Cosa | Perché costa meno |
| --- | --- | --- |
| `board/matrix_hw.c` | `matrix_hw_scan`: ogni colonna tocca solo la propria porta (P6, P5 o P4) | prima ogni colonna faceva tre letture‑modifica‑scrittura, due delle quali con maschera nulla: 5105 → 4638 istruzioni per scansione |
| `core/matrix.c` | `matrix_feed`: una colonna ferma (campione = candidato = stabile, debounce già contato) esce subito | quasi tutte le 16 colonne, quasi sempre, sono ferme; il debounce completo serve solo a chi cambia (750 → ~190 µs) |
| `hal/tick`, `board/slots`, `board/main`, `core/matrix` | il buffer `raw[16]` è in **XDATA tipizzato** (`XDATA uint8_t *`) e non un puntatore generico | un accesso generico passa da `__gptrget/__gptrput`, ~30 istruzioni; uno tipizzato 3–4 |
| `board/sleep_hw.c` | `sleep_hw_task(now, any_key_down)` riusa il valore già calcolato dal main loop | non interroga più la matrice una seconda volta per giro |
| `core/settings.c` | il controllo «i valori delle luci sono cambiati?» ogni 8 ms, non a ogni giro | otto chiamate di hook a giro; il tempo di quiete che segue è di mezzo secondo, pochi ms in più non contano |
| `core/encoder.c` | se lo stato dei pin non è cambiato, `encoder_poll` non fa nulla | quasi tutti i giri |
| `core/led.c` | colori **uniformi** (tinta unita, respiro, pulsazione) calcolati una volta per ridisegno e copiati; onde per colonna (orizzontale) e per riga (verticale) calcolate sulla prima riga/primo tasto; effetto radiale calcolato una volta per anello (12 anelli, `led_effect_ring`) | un LED costava il calcolo completo di un colore (ruota + 3 scale); ora, per la maggior parte dei tasti, una copia di 3 byte |
| `core/led.c` | **provato e tolto**: scritture del framebuffer con puntatori e senza i canali invariati | insieme alle cache dei colori rendeva il ridisegno così rapido che le colonne sfarfallavano sulla tastiera (con ogni effetto); con una sola delle due cose no. I dati scritti sono identici: è una questione di tempi, causa non trovata. Resta il ciclo semplice |
| `core/led_effect.c` | `led_color_wheel` senza divisione per 43 (confronti e sottrazioni) | la divisione è una chiamata al compilatore, un centinaio di istruzioni, a ogni colore |
| `core/led.c` | la fase dell'animazione è fotografata all'inizio del ridisegno (`cur_phase`) | un ridisegno non mescola più due fasi se questa avanza a metà |

**Attenzione ai tempi del ridisegno.** Rendere il ridisegno molto più rapido non è gratis: la combinazione «cache dei colori + framebuffer veloce» ha fatto sfarfallare le colonne (bisezione fatta con due hex, ciascuno dei due cambi da solo andava bene). Dopo un'ottimizzazione dei LED, guardare i LED sulla tastiera, non solo i contatori (che restavano a zero).

Regole di pollice per chi continua: (1) in modello *large* ogni `static` è in XDATA e ogni accesso costa; (2) un puntatore generico è 10 volte più caro di uno tipizzato; (3) la moltiplicazione e la divisione a 16 bit sono chiamate al compilatore;
(4) se un valore non cambia da un giro all'altro, non rifare il lavoro; (5) misura prima (`make sim`, `tools/diag.py --profile`).

Il budget è stretto: la build di profilo (che ha la propria statistica in XDATA) usa **4096/4096 B** di XDATA e 56 297/56 320 B di flash (con `BATTERY_LOG=0`); un'aggiunta qui va compensata.

## 7. La pipeline dei tasti

```
 scansione (ISR) ─► raw[16 colonne] ─► tick_scan_take ─► matrix_feed ─► eventi (matrix_event_t)
                                                                           │
   encoder_poll ───────────────────────────────────────────────────────────┤
                                                                           ▼
                     keyboard_event ─► (combo) ─► (leader) ─► tapping_intercept ─► layer_key_down ─► tapping_event ─► run_keycode
                                                                           │               (override, macro, mouse, tasti custom)
                                                                           ▼
                              send_keycode ─► report (stato) ─► report_hook_send ─► transport
                                                                                     ├─ link_wireless()  → coda della radio (core/radio.c)
                                                                                     └─ USB              → coda EP1 (core/usb_reports.c)
```

- **`matrix`** — debounce **a tempo** (5 ms, come QMK) con un pavimento di 3 campioni concordi; produce eventi di pressione/rilascio con coda. `matrix_row_bits` serve al tester della matrice di Vial.
- **`layer`** — pila di layer attivi sopra un layer di default; il keycode trovato alla pressione è lo stesso usato al rilascio (nessun tasto si blocca se il layer cambia nel mezzo).
  Il Win/Mac cambia il layer di default (0 su Windows, 5 su Mac, con coppie Fn 1 e 4).
- **`tapping`** — LT/MT/TD (tap dance): finché un tasto è indeciso gli eventi successivi vanno in una piccola coda e vengono riprodotti a decisione presa. Decide il rilascio (tap) o il timeout (hold).
- **`combo`** — tiene fermi al massimo 50 ms i tasti che sono ingressi di un accordo; 6 accordi da 4 ingressi, corrispondenza esatta, un accordo alla volta.
- **`override`** — 4 sostituzioni dipendenti dai modificatori (Shift+Backspace = Delete), con livelli, modificatori vietati e soppressi.
- **`leader`** — sequenze compilate in `board/leader.c`; abbandono dopo 400 ms.
- **`macro`** — buffer da 512 B (16 macro), eseguito un passo al millisecondo, senza bloccare.
- **`mouse`** — tasti `MS_*` (solo USB: la radio non ha un frame mouse).
- **`report`** — tiene lo stato (modificatori, tasti, consumer, sistema) e costruisce i byte: **NKRO a 112 bit** in report protocol, 6 tasti in boot protocol.
- **`transport`** (`board/transport.c`) decide la strada: radio se `link_wireless()`, altrimenti USB.

## 8. Moduli, uno per uno

Una riga per modulo; le firme sono in [api.md](api.md).

**`hal/`** — `clock` (PLL e risveglio), `delay` (attese a ciclo, con e senza watchdog), `gpio` (macro per porta), `flash` (lettura, cancellazione di settore e programmazione via SSP,
confinate nell'area scrivibile), `pwm` (18 canali, tre banchi da 6), `power` (power‑down e sorgente di risveglio INT4), `tick` (Timer 2, orologio, back‑off, `tick_pause/resume`),
`usb` (controller: EP0/EP1/EP2, sospensione, **remote wakeup** con `USBCON.WKUP`), `watchdog`, `sh68f90.h` (mappa dei registri usati).

**`core/`**

| Modulo | Responsabilità |
| --- | --- |
| `matrix` | debounce e coda eventi |
| `sched` | la sequenza degli slot e l'orologio in conteggi (puro) |
| `keycodes` | i keycode QMK/Vial a 16 bit (gli intervalli non si cambiano: li usa l'app) |
| `keymap` | la keymap in RAM (6 layer, encoder, macro, TD, combo, override, cfg) e i suoi accessori; `keymap_version` sale a ogni modifica **salvabile** |
| `layer`, `tapping`, `combo`, `override`, `leader`, `macro`, `mouse`, `encoder` | la logica dei tasti (§7) |
| `keyboard` | la colla: riceve gli eventi, instrada ai moduli sopra, esegue il keycode, chiama `keyboard_hook_custom` per i tasti della scheda |
| `report`, `usb_reports` | lo stato da dire all'host e la coda dei pacchetti EP1 (un tap = due report in ordine) |
| `radio`, `link` | il protocollo della BK3632 e lo scheduler dei frame (§11); la modalità (USB / 2.4G / BT), gli slot e l'accoppiamento |
| `usb_desc`, `usb_device`, `raw_hid` | descrittori come dati; macchina a stati di EP0; il tubo raw HID da 32 byte (l'ISR segna, il main legge e risponde) |
| `via`, `vial`, `vialrgb`, `diag` | i protocolli sul raw HID (§10) |
| `led`, `led_effect` | framebuffer, effetti per tasto, sequenza delle 16 colonne, gauge della batteria sulla riga dei numeri |
| `store`, `settings` | persistenza (§9) |
| `sleep` | quando dormire (timeout in secondi, livelli 1/5/10/30 min/mai) |
| `blog` | **registro della batteria** (opzionale, `BATTERY_LOG`): lettura ADC, minuti di veglia e flag (cavo, risveglio) in un settore di flash; vedi `core/blog.h` |

**`board/`** — `main` (§5), `matrix_hw` (scansione delle colonne), `led_hw` (PWM + strobe: un sottoquadro = colonna spenta, larghezze caricate, colonna accesa), `led_app` (colla dei LED),
`radio_hw` (seriale, SEL/ATT), `link_app` (selettore P0.4/P0.3), `os_switch` (P0.5, Win/Mac), `usb_app` (colla USB, ISP, remote wakeup), `sleep_hw` (sequenza di power‑down),
`store_hw` (hook di flash con la pausa dei LED), `battery` (gauge, avviso, sleep di protezione), `custom_keys` (i tasti `B68_*`), `keymap_default`, `leader`, `recovery`, `isp`,
`diag_hw`, `slots`, `timebase`, `transport`, `vial.json` (definizione che l'app Vial scarica).

## 9. Persistenza

**Keymap** (`core/store.c`). Due copie A/B da 4 settori. Un **salvataggio completo** cancella la copia che non è in uso, scrive il payload (~1,7 KB di 2 KB) e **scrive l'intestazione per ultima**
(magic, versione, layer, righe, colonne, **numero di sequenza**, **CRC‑16 del payload**, impronta dei default). Una copia vale solo se completa; al caricamento vince la valida con la sequenza più recente
(confronto con wrap). Un'interruzione durante il salvataggio lascia intatta la copia precedente. L'**impronta dei default** invalida le copie scritte da un firmware con una keymap di default diversa
(altrimenti nasconderebbero le novità). I salvataggi partono dopo 2 s senza modifiche e senza tasti giù.

**Il log della keymap.** Il salvataggio completo è raro. Nello spazio rimasto in fondo a ogni copia dopo il payload (~340 B) c'è un **log di record da 5 byte**: `[offset lo, offset hi, valore lo, valore hi, controllo]`, uno per ogni
parola modificata. Tutta la keymap è una sequenza di parole da 16 bit (tasti, encoder, macro, tap dance, combo, key override, impostazioni: `core/keymap.h`), e **ogni modifica annota l'offset che ha cambiato**
(`note()` in `core/keymap.c`, fino a 24 voci, senza doppioni). Al salvataggio: se la lista è breve e il log ha posto (**69 record**), si **aggiunge un record per parola**: nessuna cancellazione, nessuna pausa dei LED,
pochi byte programmati; se la lista è troppo lunga (reset, macro grandi, molte modifiche insieme), se il log è pieno o se non c'è ancora nessuna copia, si fa un salvataggio completo nell'altra copia e il log riparte vuoto.
Al caricamento si applica il payload e poi i record **nell'ordine**, uno sopra l'altro. Un record tagliato da un'interruzione (controllo sbagliato, mai 0xFF) viene saltato e il log prosegue dopo di lui;
un'interruzione a metà di un gruppo di record lascia ogni parola o vecchia o nuova, mai corrotta. Il risultato: **un salvataggio da Vial non spegne i LED** e la flash viene cancellata ~70 volte meno.
Il numero di sequenza dell'intestazione è anche il **contatore persistente dei salvataggi completi** (`store_saves()`, in `diag.py`): ogni salvataggio completo cancella i 4 settori di una copia, quindi ogni settore ha subito circa metà di quel numero.

**Registro della batteria** (`core/blog.c`, opzionale). Un settore a `0xDA00` con 128 record da 4 byte (minuti di veglia dall'accensione, lettura, flag con la marcatura `0xA` nel nibble alto). Scrive quando la lettura si muove di 8 conteggi,
quando il cavo entra o esce, alla prima lettura dopo un'accensione o un **risveglio dallo sleep**, e comunque ogni 5 minuti. Durante lo sleep il chip non ha orologio e non campiona: un record WAKE dice che prima di lui c'è un buco,
e la scarica di una notte si legge confrontando la sua lettura con l'ultimo record prima dello sleep. Si legge con `py tools\diag.py --blog`. Lo spegne `make BATTERY_LOG=0 fw` (direttiva `BATTERY_LOG`, default in `core/options.h`):
~1,4 KB di codice in meno e il settore torna all'applicazione (limite di codice da `0xDA00` a `0xDC00`). Un settore con altro dentro (resti di un firmware precedente) viene cancellato al primo uso.

**Impostazioni** (`core/settings.c`). Record da 14 B (magic, versione 2, sleep, slot Bluetooth, **effetto, luminosità, velocità, colore**, CRC) in **un solo settore usato come log**: ogni salvataggio
**appende** al primo slot vuoto e vince l'ultimo record valido. Si cancella solo quando il settore è pieno (ogni 36 salvataggi). Così un salvataggio frequente (i tasti luce) **non cancella nulla** e
un'interruzione non perde il precedente. Un record danneggiato viene saltato.

**Perché le luci sono qui e non nella keymap.** Stanno nei campi `cfg` della keymap in RAM (così `led.c` e Vial restano semplici) ma `keymap_cfg_set` **non conta come modifica** per questi quattro
campi (la versione non sale): li salva `settings`. Se stessero nel deposito, ogni pressione di un tasto luce cancellerebbe 4 settori (~130 ms di LED spenti).

**Il costo della flash.** Una cancellazione di settore ferma la CPU ~32 ms (nessun interrupt gira); la programmazione ferma la CPU per microsecondi a byte, non percepibili. `store_hook_flush_begin/end`
mettono in pausa **timer e PWM solo attorno alle cancellazioni** e fanno `watchdog_kick` e `tick_alive` prima di ogni operazione. Un salvataggio completo della keymap (4 settori, ~130 ms al buio) ora capita una volta ogni ~70 modifiche.

## 10. USB e protocolli sul raw HID

**Descrittori** (`core/usb_desc.c`, VID:PID `258a:dead`, "Oblio B68"). Interfaccia 0: tastiera HID (boot) su **EP1 IN** con report ID: tastiera 3, sistema 1, consumer 2, mouse 7, feature 5 (comando ISP).
Interfaccia 1: **raw HID** (pagina 0xFF60) su **EP2** da 32 byte, senza ID. Windows memorizza i descrittori per VID:PID+bcdDevice: **si cambia `USB_BCD_DEVICE` ogni volta che cambiano**.

**EP0** (`core/usb_device.c`): macchina a stati pura (SETUP → dati → stato) guidata dal driver; il chip porta SETUP, OUT e «IN preso». **Cavo staccato**: il chip non lo segnala, quindi `usb_device_bus_check` guarda
il contatore degli start‑of‑frame ogni 250 ms (`usb_device_configured()` = configurato *e* host che pilota il bus).

**Raw HID** (`core/raw_hid.c`): una richiesta alla volta. L'ISR segna che un pacchetto è arrivato **senza copiarlo**; il main lo legge dal buffer dell'endpoint, lo elabora e risponde; l'EP2 OUT non è riarmato
finché non si risponde, quindi non si perde e non si accoda nulla.

**Smistamento** (`core/via.c`, primo byte): comandi VIA `0x01–0x13` (versione, keymap dinamica, macro, bootloader…), `0xF1` **diagnostica**, `0xFE` **Vial** (id, definizione, sblocco, QMK settings, tap dance,
combo, key override), `0xFF` non gestito. Il Vial protegge con **sblocco** i comandi pericolosi (salto al bootloader, scrittura macro) e il keylogger; la diagnostica è in sola lettura.
`vialrgb` mappa gli effetti dell'app sui nostri (la scheda *Lighting* appare perché la **definizione** dice `"lighting": "vialrgb"`).

(L'**HUD**, un protocollo `0xF0` per un'app esterna di overlay, è stato tolto il 2026-11-04: nessuno lo usava. Il suo posto, il primo keycode della scheda `0x7E00`, resta riservato e non fa nulla, per non spostare i numeri degli altri tasti salvati nelle keymap.)

**Diagnostica** (`core/diag.c`, `tools/diag.py`): kind 0 = contatori (causa reset, stack, slot LED/scansioni in ritardo, passata più lenta, overflow coda USB, back‑off, erase più lungo, frame radio, ritardo di avvio del timer),
kind 1 = i 6 byte di stato della radio, kind 4 = il registro della batteria (solo con `BATTERY_LOG=1`). Il pacchetto da 32 byte regge al massimo 14 contatori.

**ISP** (`board/isp.c`): il comando (feature report 5, o Vial sbloccato) salta a `0xFF00` con `A=0x5A, B=0xA5`, come fa lo stock.

## 11. Radio e modalità

**Seriale** a 260870 baud, frame `[01][cmd][a][b][c][chk]` con checksum `0x55 − somma`; la linea **SEL** (P0.2) scende prima del primo byte e sale con l'ultimo, **ATT** (P4.7) alta = la radio accetta un frame.
La radio risponde `02 <cmd> …` *dopo* il rilascio di SEL; le sue notifiche `03` si confermano con `F0`, i suoi frame lunghi `08` con `F1`, altrimenti resta occupata o rifiuta i report.

**Scheduler** (`radio_task`, ordine di priorità): conferme → comando di modo → nomi Bluetooth (una volta) → report tastiera → report consumer → rimando del modo (se non risposto) → richiesta di stato `06` ogni 200 ms.
Un frame esce solo con ATT alta e ≥ 1 ms dopo l'ultimo. Le code (8 stati tastiera, 4 consumer) sono in ordine: un tap sono due frame; a coda piena l'ultimo elemento è sostituito (lo stato finale resta giusto).
Un «non ora» a un report lo rimette in testa e lo riprova dopo 10 ms.

**Report tastiera**: fino a 5 tasti in lista, 6 tasti nella bitmap (mai in entrambi: l'host scriverebbe due volte); il rilascio è un report a zero.

**Modalità** (`core/link.c` + `board/link_app.c`): il selettore fisico (P0.4 = 2.4G, P0.3 = Bluetooth, entrambi alti = USB, filtrato 50 ms) sceglie dove vanno i report. Cambiando modo si rilascia ciò che la vecchia strada teneva giù.
Tre slot Bluetooth (Fn+B/N/M: tocco = scegli, 3 s = accoppia); lo slot è un'impostazione persistente. In USB la radio viene fermata dopo un report vuoto.

**Stato** (6 byte, ogni 200 ms): byte 1 = LED dell'host (bit 0–2), **byte 4–5 = tensione della batteria come lettura ADC a 10 bit** (basso, alto).

## 12. Backlight

`core/led.c` tiene un **framebuffer** di livelli a 8 bit per canale e la sequenza delle 16 colonne; `led_task` ridisegna **pochi LED per giro** (un repaint non trattiene mai i tasti). `led_effect.c` calcola i colori
(tinta unita con tinta+saturazione in una parola da 16 bit, gradienti, respiro, ciclo). In interrupt si fa solo: `led_next_column()` → carica le 15 larghezze PWM da una tabella `duty2` pre‑calcolata → accende la colonna.
**Come si ridisegna.** `led_task` ridisegna 8 LED per giro (`RENDER_CHUNK`). All'inizio di un ridisegno `read_settings` legge una volta effetto, luminosità, tinta, saturazione e **fase** (`cur_phase`: fotografata, un passo
della fase a metà ridisegno ne chiede un altro). Poi `paint_one` prende il colore del tasto: per gli effetti **uniformi** (tinta unita, respiro, pulsazione) il colore è calcolato una volta in `base`; per l'onda
**orizzontale** il colore di una colonna è calcolato sulla riga 0 e riusato sulle altre (`color[col]`); per la **verticale** sul primo tasto di ogni riga (`base`, che non serve ad altro in quel caso); per il **radiale** una volta
per anello (`color[anello]`, `color_known` dice quali sono pronti; `led_effect_ring` dà l'anello 1–15 di un tasto: le distanze della tabella sono multipli di 17, quindi `distanza >> 4` è unico); diagonale e altri
calcolano ogni tasto. Il framebuffer (`level`, `duty2`, `lit`) è scritto per intero, canale per canale (la variante con puntatori e salto dei canali invariati è stata tolta: vedi §6.1).
Il tasto Caps si accende fisso quando l'host ha il Caps Lock (via USB dal report LED, in wireless dal byte 1 dello stato) **o quando Caps Word è attivo** (`keyboard_caps_word()`; `led_task` guarda entrambi per accorgersi del cambio e ridisegnare). La riga dei numeri mostra il **gauge** (Fn+Backspace), lampeggi di modo/slot/pairing e i passi dello sleep.
Il PWM suona una colonna alla volta: il «pieno» dei LED è la frazione di tempo, non una corrente continua.

## 13. Energia e batteria

- **Sleep** (`core/sleep.c` + `board/sleep_hw.c`): solo in wireless e **solo senza cavo**; dopo 1/5/10/30 min o mai (Fn+O/P), o subito con Fn+Tab. Sequenza copiata dallo stock: frame `01 0C 08 07 00` e `01 0B 00` alla radio, seriale
  spenta, PWM a 1, pin in stato di riposo, INT4 armato, `power_enter_powerdown`. Il tasto che sveglia non viene inviato.
- **Batteria** (`board/battery.c`): lettura ADC dalla radio (**~243 conteggi/volt**, misurata: 4,0378 V ↔ 981), percentuale da una **curva di scarica Li‑Po** (800 = 3,30 V = 0 % … 1020 = 4,20 V = 100 %), non la retta dello stock.
  Col cavo la lettura satura (1023) e si ignora; all'accensione l'ADC legge 1–3 (non assestato) e si ignora. **Avviso** sotto 800 (gauge rosso subito e ogni 30 s); **sleep di protezione** sotto 752 per 10 s.
  La scala poggia su un solo punto misurato: **provvisoria** (conferma: lettura ~1015–1020 appena tolto il caricatore).
- **Remote wakeup USB** (`board/usb_app.c`): con il bus sospeso e il wakeup abilitato dall'host, un tasto preme `USBCON.WKUP` per 10 ms (come lo stock a `0x9D73`); il tasto non esce. Da provare con un PC in sospensione.

## 14. Build, test, qualità

| Comando | Cosa fa |
| --- | --- |
| `make test` | 28 programmi host + controllo della definizione Vial |
| `make fw size` | `build/oblio.hex` e budget (flash 56320 B, XDATA 4096 B) |
| `make check-interrupts` | esattamente 4 vettori, nessun overlay statico |
| `make sim` | l'hex vero nel simulatore `ucsim-sh68f90` (24 test: avvio, slot, stack, recupero, descrittori, VIA/Vial, report, ISP); ~45 s |
| `make api` / `make check-api` | rigenera / verifica `docs/api.md` dagli header |
| `make BATTERY_LOG=0 fw` | build senza il registro della batteria (opzione in `core/options.h`) |
| `make LOOP_PROFILE=1 BATTERY_LOG=0 fw` | **build di profilo**: misura ogni passo del ciclo principale (15 passi: corse, minimo, media, massimo, quota di tempo), la lunghezza dei giri per fasce e la CPU presa da ogni interrupt (timer, USB, seriale della radio). Si legge con `py tools\diag.py --profile`, si riparte con `--profile-reset`. Costa ~2 KB di flash e ~180 B di XDATA e rallenta un po' ogni giro: solo per misurare. Va con `BATTERY_LOG=0` (non ci starebbe) |

CI (GitHub Actions): test host, build, budget, vettori, simulazione; l'hex è un artifact e il tag `v*` pubblica una Release. Il simulatore mostra l'**ordine** degli eventi e i conteggi di istruzioni, non le durate.

## 15. Decisioni di design e compromessi

| Decisione | Perché | Costo / limite |
| --- | --- | --- |
| Riscrittura pulita invece del fork SMK | codice snello e testabile; SMK resta la fonte di verità per hardware e radio (citato in `NOTICE.md`) | molto lavoro di re‑derivazione |
| Profili tolti (erano 4 keymap × 2 copie) | 16 KB di flash per layout che nessuno usava | una sola keymap |
| Slot a sequenza fissa (4 LED + 1 scansione) | niente sovrapposizioni tra tasti e LED, orologio indipendente dalle durate | frame del backlight non fisso (~8–10 ms) |
| Debounce a tempo | il tempo di risposta non dipende dalla frequenza di scansione | i campioni scartati sono innocui (la matrice è uno stato) |
| Log delle impostazioni append‑only | niente erase a ogni salvataggio, nessuna perdita per interruzioni | 36 salvataggi per settore |
| Luci fuori dal deposito della keymap | il deposito cancella 4 settori (~130 ms al buio) | `keymap_cfg_set` ha un'eccezione per 4 campi |
| Keymap salvata come immagine + log | una modifica da Vial = 5 byte programmati, niente erase, niente buio; ~70× meno cicli di cancellazione | ~1,1 KB di flash in più; un salvataggio a metà di un gruppo può lasciare un gruppo applicato in parte |
| ISR USB senza copia | non trattiene il timer dei LED | il main legge dal buffer dell'endpoint (che resta fermo finché non si risponde) |
| Curva Li‑Po e non la retta dello stock | lo stock chiama «pieno» tutto sopra ~3,85 V | scala provvisoria |
| Nessun mouse via radio | la BK3632 non ha un frame mouse | encoder come rotella solo via cavo |
| Diagnostica sempre presente ma minima | serve per il flicker e per i problemi sul campo | 9+1 contatori, ~0,5 KB |

## 16. Limiti noti e punti aperti

- Scala della batteria da confermare con un secondo punto; avviso e sleep di protezione da provare.
- Remote wakeup USB da provare (il significato del bit `WKUP` è dedotto dallo stock).
- Latenza BLE: misurato che **non** è del firmware (0 rifiuti, stesso tempo tasto→radio del 2.4G); dipende dall'intervallo di connessione deciso da host e radio.
- Flicker (con un'app che interroga il raw HID, e al salvataggio): corretti (ISR senza copia; luci nel log).
- Frame `08` della radio, comandi `04/05/0E` e tasti Fn dello stock: non decodificati (non interessano).
- Funzioni scritte con test verdi ma non viste sull'hardware: combo, key override, NKRO, Caps Word, Leader, tab Lighting.

## 17. Come estendere

- **Un tasto della scheda**: aggiungi il keycode a `board/b68_keys.h` (numerazione continua da `0x7E00`, controllata da `check_vial.py`), la voce in `board/vial.json` (`tools/vial_gen.py`), il caso in `board/custom_keys.c` e,
  se serve, un posto nella keymap di default. **Cambiare keymap di default o definizione resetta la keymap salvata** (impronta dei default).
- **Un comando raw HID**: un nuovo ID in `core/via.c` (smistamento), la logica in un modulo `core` con i suoi hook e un test in `tests/host/`.
- **Un effetto del backlight**: in `core/led_effect.c` (matematica pura) e nell'elenco di `core/vialrgb.c` se deve comparire nell'app.
- **Un'impostazione persistente**: un campo in `settings_t` e nel record (alza `VERSION`: i record vecchi vengono ignorati), il setter con `changed()`; non nel deposito della keymap se cambia spesso.
- **Una nuova ISR**: no (l'infrastruttura ne ammette quattro e controlla lo stack); metti il lavoro nel main e un flag nell'ISR esistente.
- **Prima di ogni commit**: `make test`, `make fw size`, `make check-interrupts`, `make sim`, `make check-api` e aggiornare `CHANGELOG.md` e `docs/session-state.md`.
