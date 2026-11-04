# Guida per iniziare a sviluppare il firmware

Per chi sa programmare ma non ha mai guardato dentro una tastiera. Va letta in ordine: prima **come funziona una tastiera** (parte 1), poi **come l'abbiamo fatta noi**
(parte 2), poi **un percorso di lettura del codice** (parte 3), **esercizi** (parte 4) e **strumenti per capire cosa succede** (parte 5). In fondo un caso reale,
il backlight che si spegne al salvataggio, che mette insieme quasi tutto.

Documenti da tenere accanto: [design.md](design.md) (perché è fatto così), [api.md](api.md) (ogni funzione), [firmware-report.md](firmware-report.md) (cosa è provato).

---

## Parte 1 — Come funziona una tastiera

### 1.1 La matrice
Una tastiera con 66 tasti non ha 66 fili verso il microcontrollore: ne avrebbe bisogno di 66 pin. I tasti sono disposti in una **matrice** di righe e colonne: ogni tasto è un
interruttore tra una riga e una colonna. La B68 ha **16 colonne × 5 righe = 80 incroci** (66 occupati da tasti, più il pulsante dell'encoder).

```
            colonna 0   colonna 1   colonna 2  ...  colonna 15
riga 0        [Esc]       [1]         [2]            [encoder]
riga 1        [Tab]       [Q]         [W]            [Del]
  ...
```

Il firmware **scandisce** la matrice: porta a livello basso **una colonna per volta** (lo «strobe») e legge le 5 righe, che hanno una resistenza di pull‑up e quindi leggono 1 se nessun tasto
le collega alla colonna attiva e **0 se un tasto è premuto**. Ripetuto per tutte le 16 colonne = una scansione completa = lo stato di tutti i tasti. Con 16 + 5 = 21 pin si leggono 80 tasti.
Nel nostro codice: `board/matrix_hw.c` (quali pin sono colonne e righe, come si legge) e `core/matrix.c` (cosa se ne fa).

Due problemi classici:
- **Rimbalzo (bounce).** Un contatto meccanico non commuta in modo pulito: per qualche millisecondo oscilla. Il **debounce** accetta un cambio solo dopo che il valore è rimasto stabile (da noi 5 ms).
- **Ghosting.** Senza un diodo per tasto, premendone tre agli angoli di un rettangolo la matrice ne «vede» un quarto inesistente. Chi progetta la tastiera mette un diodo per tasto; non sappiamo quanti ne abbia la B68 e dal firmware non si può cambiare (se certe combinazioni di tasti non compaiono, può essere questo).

### 1.2 Dal tasto al computer: i keycode e i report HID
Un tasto fisico ha una **posizione** (riga, colonna). La **keymap** dice cosa significa quella posizione: un **keycode**. Questo permette i **layer**: la stessa posizione vale `Q` normalmente e `F1` tenendo premuto Fn.
Il computer non riceve «Q», riceve un **report HID** (una tabella di tasti premuti) tramite la specifica **USB HID**: un pacchetto con i modificatori (Shift, Ctrl…) e i codici dei tasti premuti
(i codici «usage» della tabella HID Usage Tables: `A` = 0x04, `Invio` = 0x28, `Home` = 0x4A…).
- **Boot protocol**: report da 8 byte, al massimo 6 tasti insieme; lo capisce anche il BIOS.
- **NKRO** («N‑key rollover»): una bitmap, 1 bit per tasto, premi quanti ne vuoi. Noi: 112 bit.

L'host fa il **polling**: chiede alla tastiera «hai qualcosa?» ogni 1 ms (USB full speed). La tastiera non può «spingere» dati: li prepara e il controller USB li consegna alla prossima richiesta.

### 1.3 USB in due parole
Quando colleghi il cavo, l'host chiede al dispositivo di **descriversi** (enumerazione): chi è (VID:PID), quante interfacce ha, che report manda (i **descrittori**, in `core/usb_desc.c`).
Una nostra tastiera presenta due interfacce: la tastiera e un canale **raw HID** da 32 byte, usato da Vial e dalla diagnostica per parlare con il firmware.
I pacchetti passano per **endpoint**: EP0 per la configurazione, EP1 per i tasti, EP2 per il canale raw.

### 1.4 Wireless
Il firmware **non** parla Bluetooth né 2.4 GHz: li parla un chip radio a parte (**BK3632**). Il microcontrollore gli manda i tasti sulla **seriale** e la radio li trasmette. Per questo il percorso dei tasti ha due strade:
cavo USB oppure coda della radio (`board/transport.c`).

### 1.5 I LED RGB e il PWM
Il colore e la luminosità di un LED si regolano con un **PWM**: lo si accende e spegne tanto in fretta che l'occhio vede una luminosità media (tempo acceso / periodo). Su questa scheda le **colonne sono anche i pozzi dei LED**:
un LED si accende se il suo canale colore (un PWM) è alto **e** la colonna è bassa. Quindi in ogni istante si accende **una colonna sola**, e il firmware le fa girare in fretta (multiplexing). Ne segue che
**la matrice dei tasti e il backlight condividono le stesse linee e non si possono usare nello stesso istante**: è la ragione di tutto il modello temporale (parte 2).

### 1.6 Vial / VIA
**Vial** è un'app che riconfigura la tastiera senza ricompilare: legge una **definizione** (un JSON, `board/vial.json`, che descrive la forma della tastiera) e parla col firmware via raw HID per leggere e scrivere keymap, macro,
tap dance, combo. È lo stesso protocollo di QMK. I **keycode** sono numeri a 16 bit con intervalli fissi (`core/keycodes.h`): non si cambiano, perché li conosce l'app.

---

## Parte 2 — Come l'abbiamo fatto noi (la mappa mentale)

**Hardware**: un microcontrollore **SH68F90A** (core 8051: 8 bit, 24 MHz, 64 KB di flash, **4 KB di RAM «esterna»** XDATA, 256 B di RAM interna per lo stack) e la radio. Pochissima memoria: ogni scelta nasce da lì.

**Tre strati** (vedi design.md §3):
```
core/   logica pura, senza registri: si prova sul PC con gcc        ← qui si impara di più, qui si sbaglia in sicurezza
hal/    i registri del chip (timer, USB, flash, PWM…)
board/  ciò che è specifico della B68 + la «colla» che collega core e hal
```
Il core non tocca mai l'hardware: quando serve qualcosa dal mondo esterno chiama una funzione **hook** (`qualcosa_hook_*`) che la scheda definisce e che i test sostituiscono con un finto.

**Il tempo**: un **timer** (Timer 2) interrompe il programma di continuo e «suona» una sequenza: 4 sottoquadri di backlight, poi 1 scansione della matrice. Il programma principale (**main loop**, un ciclo infinito)
raccoglie ciò che l'interrupt ha preparato e fa tutto il lavoro lento (debounce, layer, report, USB, radio).
Regola d'oro: **l'interrupt fa pochissimo e in fretta; il main fa il resto.**

**Il percorso di un tasto** (design.md §7), da imparare a memoria:
`scansione (interrupt)` → `matrix_feed` (debounce) → `keyboard_event` → `layer` (che keycode è, su quale layer) → `tapping/combo/override` → `report` (stato dei tasti) → `transport` → USB o radio.

**La persistenza**: la keymap e le impostazioni stanno nella **flash** del chip. Scrivere in flash è lento e (su questo chip) **ferma la CPU** durante la cancellazione: vedi il caso in fondo.

---

## Parte 3 — Percorso di lettura del codice

Leggere con l'editor aperto e `docs/api.md` accanto. Per ogni file: leggi prima il commento in testa all'header, poi il `.c`.

| # | File | Cosa impari | Tempo |
| --- | --- | --- | --- |
| 1 | `core/keycodes.h` | cos'è un keycode, gli intervalli (base, modificatori, layer, macro, QK_*) | 20 min |
| 2 | `core/matrix.h`, `core/matrix.c` | debounce e coda di eventi: la funzione `matrix_feed` | 30 min |
| 3 | `board/matrix_hw.c` | come si scandisce davvero: quali pin sono colonne/righe | 20 min |
| 4 | `core/keymap.h`, `board/keymap_default.c` | la keymap in RAM e la macro `LAYOUT_B68` che mappa i tasti sulla matrice | 30 min |
| 5 | `core/layer.c` | come si sceglie il keycode tra i layer attivi (`layer_lookup`) | 30 min |
| 6 | `core/keyboard.c` | il cuore: `keyboard_event`, `run_keycode`, `send_keycode` | 1 h |
| 7 | `core/report.c`, `core/usb_reports.c` | dallo stato dei tasti ai byte dei pacchetti | 40 min |
| 8 | `core/tapping.c`, `core/combo.c`, `core/override.c` | le funzioni «intelligenti» (tap/hold, accordi, sostituzioni) | 1 h |
| 9 | `board/main.c` | l'avvio e il main loop: ora tutto si lega | 30 min |
| 10 | `core/sched.c`, `hal/tick.c` | gli slot, il modello temporale, perché è fatto così (leggi il commento di `sched.h`) | 1 h |
| 11 | `core/usb_desc.c`, `core/usb_device.c` | i descrittori e l'enumerazione | 1–2 h |
| 12 | `core/raw_hid.c`, `core/via.c`, `core/vial.c` | il canale di configurazione | 1 h |
| 13 | `core/store.c`, `core/settings.c`, `board/store_hw.c` | la persistenza in flash | 1 h |
| 14 | `core/led.c`, `core/led_effect.c`, `board/led_hw.c` | il backlight | 1–2 h |
| 15 | `core/radio.c`, `core/link.c` | la radio | 1–2 h |

Poi: `tests/host/test_keyboard.c` (come si provano i tasti con un finto hardware) e `tests/sim/test_sim.py` (come si prova il firmware vero).

---

## Parte 4 — Esercizi (dal più facile)

Dopo ciascuno: `nix develop --command make test`, poi `make fw size` per vedere quanta flash è costato.

1. **Cambia la keymap di default** (`board/keymap_default.c`): metti `KC_HOME` e `KC_END` al posto di PgUp/PgDn sul layer Fn. Attenzione: cambiare il default **azzera la keymap salvata** (impronta dei default).
   Poi fallo da Vial invece e confronta: dove viene scritto il cambiamento? (`keymap_set` → `store_flush`).
2. **Leggi un test**: apri `tests/host/test_keyboard.c`, trova `test_layer_key_then_layer_key`, cambia un'asserzione per farla fallire e capisci perché fallisce.
3. **Scrivi un test nuovo**: copia un test esistente e prova un caso tuo (per esempio: un tasto premuto durante Fn e rilasciato dopo che Fn è stato lasciato).
4. **Cambia il debounce** (`MATRIX_DEBOUNCE_MS` in `core/matrix.h`) a 1 ms e a 20 ms: i test cosa dicono? Sulla tastiera cosa senti?
5. **Aggiungi un tasto della scheda** (design.md §17): per esempio uno che stampa la versione. Tocca `b68_keys.h`, `vial.json`, `custom_keys.c`, `keymap_default.c`.
6. **Aggiungi un effetto** al backlight in `core/led_effect.c` (matematica pura, la parte più divertente).
7. **Misura**: guarda quanta CPU usa il main (`diag.py`: «slowest main-loop pass») mentre Vial è aperto, poi senza.

---

## Parte 5 — Strumenti per capire cosa succede

| Strumento | A cosa serve |
| --- | --- |
| `make test` | 29 programmi di test sul PC: veloci, sicuri, il primo posto dove provare un'idea |
| `make sim` | l'hex vero nel simulatore del chip (`tests/sim/oblio_sim.py`): ordine degli eventi e **conteggio esatto di istruzioni**, non le durate |
| `make fw size` | la flash e la XDATA usate rispetto al limite: ogni funzione ha un costo, controllalo sempre |
| `make check-interrupts` | i 4 vettori e l'assenza di sovrapposizione statica |
| `make api` | rigenera `docs/api.md` dagli header |
| `tools/diag.py [--radio] [--watch 1]` | i contatori del firmware sulla tastiera vera: reset, stack, slot in ritardo, ritardo di avvio del timer, erase… |
| Vial | cambia la keymap e guarda cosa succede al volo |
| Un tester di tastiere nel browser | mostra i tasti premuti insieme, utile per NKRO |
| Il recupero | **Esc+1+2 mentre inserisci il cavo** → bootloader; non c'è altra rete di sicurezza (nessun ICP) |

Metodo di lavoro consigliato: **piccoli passi**, test sul PC, poi simulatore, poi la tastiera. Se una prova sull'hardware fallisce, `diag.py` dice se è stato un reset, un overflow, un ritardo.
In questo progetto vale la regola «niente tentativi alla cieca»: si ragiona, si strumenta (un contatore, un lampeggio), si misura.

### Dove imparare le basi (cerca per titolo)
- **QMK Docs**: «How a Keyboard Matrix Works» e le pagine su keycode, layer e tapping: è lo stesso modello che usiamo, scritto per gli appassionati.
- **USB in a NutShell** (Beyond Logic): l'enumerazione e gli endpoint spiegati da zero.
- **HID Usage Tables** e **HID Device Class Definition** (usb.org): le tabelle dei codici e il formato dei report.
- Un articolo sul **debouncing** (per esempio la guida di Jack Ganssle «A Guide to Debouncing»).
- **Il manuale di SDCC** (compilatore per 8051): modelli di memoria, `__xdata`, interrupt.
- Per l'8051: un riassunto dell'architettura MCS‑51 (registri, spazio dati/codice/XDATA, interrupt).
- **Vial**: la documentazione sul protocollo e sulla definizione JSON; `docs/vial.md` qui riassume cosa usiamo.

---

## Un caso reale: perché il backlight si spegne quando si scrive in memoria

È l'esempio perfetto per capire come hardware e firmware si condizionano.

**Il sintomo.** Salvare nella flash (la keymap, le impostazioni) fa spegnere i LED per qualche decina di millisecondi.

**La catena delle cause.**
1. Il firmware gira **eseguendo il codice dalla flash**. Per **cancellare un settore** (512 byte) il chip deve usare la flash per l'operazione: durante i ~**32 ms** la CPU **si ferma** e *nessun interrupt* viene eseguito (misurato: «longest sector erase» ≈ 32,6 ms).
2. Il backlight è **multiplexato dalla CPU**: ogni 400 µs l'interrupt del timer spegne una colonna e ne accende un'altra. Se la CPU si ferma, la colonna accesa **resterebbe accesa** (e con essa i canali PWM): una striscia luminosa fissa. Per evitarlo, prima della cancellazione il firmware **spegne il PWM e le colonne** (`store_hook_flush_begin`), e li riaccende dopo (`flush_end`).
3. Risultato: ~32 ms al buio per ogni settore cancellato. Salvare la keymap cancella **quattro settori**: ~130 ms.

**Perché con QMK (e in generale altrove) non succede.**
- Nei QMK su **AVR** i dati stanno in una **EEPROM** separata: scriverla non ferma la CPU (e i LED, se sono WS2812, sono pilotati da un segnale dedicato, non da una scansione).
- Nei QMK su **STM32** la flash si cancella a pagine e ferma la CPU, ma di solito i LED RGB sono controllati da un **chip driver dedicato** (IS31FL…) o da un timer con **DMA**: *il quadro dei LED lo tiene l'hardware*, non il programma. La CPU può fermarsi e i LED restano com'erano.
- Qui invece il quadro lo tiene il programma: **la CPU è il driver dei LED**. Un blocco della CPU è un blocco dei LED.

**E il firmware originale?** Lo abbiamo disassemblato (`tools/dis51.py`, vedi `docs/keyboards/yunzii-b68-firmware-stock.md`): fa **la stessa cosa** — spegne PWM e colonne, cancella il settore, riaccende — con un solo settore da 128 byte,
subito a ogni cambio di impostazione e scrivendo i byte «a goccia» da un interrupt. Il buio c'è anche lì, ma coincide con la pressione del tasto che cambia l'effetto e dura quanto una sola cancellazione. Non ha nessun trucco per
evitarlo: l'hardware non lo permette.

**Cosa abbiamo già fatto.**
- I tasti luce ora salvano in un **log** che aggiunge un record senza cancellare (il settore si cancella una volta ogni 36 salvataggi).
- La pausa dei LED copre **solo le cancellazioni**, non la programmazione dei byte (che ferma la CPU per microsecondi).
- La programmazione della keymap (1,7 KB in un colpo dal ciclo principale) faceva scattare il **back‑off** del backlight (il main «affamato»): ora `store_hook_program` chiama `tick_alive()`.
- La keymap è ora un'**immagine più un log**: una modifica da Vial aggiunge un record da 5 byte in fondo alla copia in uso, senza cancellare nulla (niente buio). Il salvataggio completo (4 cancellazioni, ~130 ms) capita una volta ogni ~70 modifiche, o per operazioni grosse (reset, macro). Vedi `design.md` §9 e `core/store.c` (`journal_append`, `journal_replay`).
- `diag.py` mostra il **contatore persistente dei salvataggi completi** e una stima dei cicli di cancellazione per settore.

**Come esplorarlo da soli**: `diag.py` mostra «longest sector erase (us)»; guarda come cambia dopo aver fatto un salvataggio dalle luci e uno dalla keymap. In `board/store_hw.c` leggi `store_hook_flush_begin/end`
e `store_hook_erase`; in `core/store.c` `store_flush` (sceglie tra log e salvataggio completo); in `core/settings.c` `settings_flush`.

---

## Piccolo glossario

**ISR**: routine di interrupt. **XDATA**: RAM «esterna» dell'8051 (qui 4 KB). **SFR**: registro del chip. **Hook**: funzione che il core dichiara e la scheda definisce. **Keycode**: numero a 16 bit che dice cosa fa un tasto.
**Layer**: un «piano» di keycode sovrapposto agli altri. **Report**: il pacchetto che dice all'host quali tasti sono premuti. **NKRO**: tutti i tasti insieme. **Strobe**: il segnale che attiva una colonna.
**Debounce**: filtro del rimbalzo. **PWM**: luminosità regolata con il tempo acceso. **Enumerazione**: la presentazione del dispositivo USB all'host. **IAP/SSP**: scrittura della flash da parte del programma stesso.
