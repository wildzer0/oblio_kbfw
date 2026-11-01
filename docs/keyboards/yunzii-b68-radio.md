# Yunzii B68 — la radio BK3632 (2.4G e Bluetooth): protocollo e driver

Riferimento unico per il collegamento tra la MCU (BYK916 = SH68F90A) e la radio BK3632 della B68, come è stato ricavato (dal disassemblaggio del firmware originale e da
prove sulla tastiera chiusa) e come lo usa il driver. Stato: **tastiera funzionante in 2.4G e in Bluetooth** (verificato dall'utente il 2026-11-01).
Il driver è in [`src/keyboards/yunzii-b68/`](../../src/keyboards/yunzii-b68/) (`b68radio.c`, `b68radio_hw.c`, `kb.c`); l'analisi dello stock in
[yunzii-b68-firmware-stock.md](yunzii-b68-firmware-stock.md).

## 1. Hardware

| Cosa | Dove | Note |
| --- | --- | --- |
| Seriale MCU → radio (TXD) | `P5.5` | EUART0 |
| Seriale radio → MCU (RXD) | `P5.6` | EUART0 |
| Selezione `SEL` | `P0.2` | uscita della MCU: bassa mentre esce un frame, poi rilasciata (ingresso con 1 esterno) |
| Attenzione `ATT` | `P4.7` | ingresso: **alta = la radio può ricevere un frame**, bassa = occupata (elabora o sta parlando) |
| Selettore di modalità | `P0.4`, `P0.3` | ingressi con pull-up: `P0.4` bassa = 2.4G, `P0.3` bassa = Bluetooth, entrambi alti = USB (cavo) |

Sul PCB (foto, tastiera aperta): `J2` = ICP del BYK916 (VDD, TCK, TDI, TMS, TDO, GND); `J3` = SPI del chip radio (GND, CSN, MISO, MOSI, CLK, VCC), che **non** è il collegamento con la MCU
(serve per programmare la radio). L'SPI hardware della MCU non è usato dallo stock.

## 2. La seriale

EUART in **modo 1** (8N1 asincrono), impostata come lo stock (a `0xAFA0`):

* `SCON = 0x50` (modo 1, ricevitore acceso) e subito dopo **`PCON |= 0x40` (`SSTAT`)**: con `SSTAT` i bit 7–5 di `SCON` sono i flag di errore (`FE`, `RXOV`, `TXCOL`) e non i bit del modo.
  Senza `SSTAT`, azzerarli (`ANL SCON,#0x1F` come fa lo stock nel suo interrupt) porta la seriale in modo 0 (registro a scorrimento che riceve di continuo).
* `SBRTH = 0xFF` (`SBRTEN` + 0x7F), `SBRTL = 0xFB`, `SFINE = 0x0C` → baud = Fsys / (16·5 + 12) = 24 MHz / 92 = **260870**.
* **Priorità: la seriale è al livello 3, il massimo** (`IPH1 |= 0x40`, `IPL1 |= 0x40`; lo stock scrive `IPH1 = 0x42`, `IPL1 = 0x41` a `0xEC0F`). Un byte dura 38 µs; il tick dei LED/tasti di SMK dura ~0,35 ms.
  Con la seriale allo stesso livello del tick i byte uscivano con vuoti, le risposte si perdevano e la radio rifiutava o prendeva a caso i report (tasti che restavano premuti).
* TX e RX in interrupt (`uart_interrupt_handler`, vettore 13): il TX manda il byte successivo a ogni `TI`; l'ultimo `TI` libera il flag di occupato. L'RX riempie un anello da 32 byte.

## 3. Frame

* **MCU → radio:** `01 <comando> <a> <b> <c> <chk>` (6 byte) o più lunghi; `chk = 0x55 − Σ(byte precedenti)` (mod 256).
* **Radio → MCU:** intestazione `02` (risposte), `03` (notifiche, 6 byte), `08` (22 byte, ultimo = checksum); stesso checksum.
  * Conferma di un comando: `02 <comando> 00 00 00 chk` (es. `02 02 00 00 00 51` per un report, `02 01 00 00 00 52` per il modo).
  * "Non ora / nulla da fare": `02 00 01 00 00 52` (la radio rifiuta un frame che arriva mentre è occupata o che non cambia nulla; anche la risposta a frame di formato sbagliato).
  * Stato (risposta al comando `06`): `02 06 00 <b3> <b4> <b5> ff 03 01 chk` (10 byte).
  * Un frame con intestazione `03` (notifica) va confermato con `01 F0 00 00 00 chk`; uno con `08` con `01 F1 00 00 00 chk`.

### Comandi MCU → radio (da stock, `78 33 76 01 08 76 <cmd>` in `0xA2DB`, `0xACF5`, `0xAEB2`, `0xAEC7`, `0xAF04`, `0xAF27`, `0xB069`, `0xB089`, `0xB12A`, `0xB1AE`)

| Cmd | Frame | Stato |
| --- | --- | --- |
| `01` | `01 01 <accoppiamento> <modo> 00 chk`; modo 0 = 2.4G, 1–3 = Bluetooth slot 1–3; accoppiamento 1 = avvia | **usato e verificato** (modo e accoppiamento) |
| `02` | report di tastiera, 30 byte (§5) | **usato e verificato** |
| `06` | `01 06 00 00 00 chk`, richiesta di stato | usato (ogni 200 ms a riposo) |
| `F0`, `F1` | `01 F0 00 00 00 chk`, `01 F1 00 00 00 chk`: conferma delle notifiche `03` / `08` | usati |
| `03` | report consumer, 13 byte: `01 03 <usage lo> <usage hi> 00 <7 byte a 0> chk` (usage dallo stock: `E2` mute, `E9` volume +, `EA` volume −; `XDATA[0x09B3]`, `[0x09B4]`) | **usato, da provare sulla tastiera** (2026-11-01); il system control (power/sleep) non è mappato e resta sul cavo |
| `04` | `01 04 <v> 00 00 chk`, `v` = 2 o 3; confermato `02 04 00 00 00 4f` | significato ignoto |
| `05` | `01 05 00 00 00 chk` | ignoto |
| `08` | 23 byte, 20 di dati | ignoto |
| `09` | nome Bluetooth, 32 byte: `01 09 <tipo> <lunghezza> <nome> 00… chk` (tipo 0 = BLE, 1 = classico; stock: "B68 5.0 " e "B68 3.0") | **usato** con `B68RADIO_NAME` = "OBLIO", una volta per accensione dopo la prima risposta al modo (da provare sul telefono) |
| `0B`, `0C`, `0D`, `0E` | `0B <v>`, `0C <a> <b>`, `0D <v>`, `0E` | ignoti |

## 4. Regole di scambio (come lo stock)

1. **Si manda un frame solo con `ATT` alta** (e non mentre un altro è in uscita). Lo stock controlla `P4.7` in ogni funzione di invio.
2. **`SEL` scende subito prima del primo byte e sale subito dopo l'ultimo** (lo stock lo fa nell'interrupt, a `0xA478`). La radio risponde **dopo** il rilascio di `SEL`: tenerlo basso a lungo non dà risposte.
3. **Le conferme non si aspettano.** Lo stock elabora solo lo stato (`0x05EA`); conferme e "non ora" li ignora. Il frame successivo parte appena `ATT` torna alta (il driver aspetta almeno 1 ms).
4. **Le notifiche della radio (`03`, `08`) si confermano** con `F0` / `F1`, con priorità sui report.
5. A riposo una richiesta di stato `06` ogni 200 ms (lo stock: ogni 100 giri dello scheduler).
6. Un "non ora" su un report va riproposto dopo ~10 ms con lo stato più recente, salvo che non cambi nulla rispetto all'ultimo accettato.

## 5. Report di tastiera (comando `02`)

`01 02 <mods> <5 tasti> 00 <bitmap da 16 byte> 00 00 00 00 <chk>` (30 byte). La bitmap ha il bit *n* = usage *n* (n < 128).
La radio presenta all'host **due tastiere** (normale e NKRO): un tasto va **in una sola delle due** parti, altrimenti l'host lo scrive due volte. Come la NuPhy:

* fino a 5 tasti → solo nella lista (bitmap tutta a zero);
* 6 tasti → solo nella bitmap (lista a zero). I modificatori sono sempre in `<mods>`.
* Il rilascio è un report tutto a zero. Nella B68 non esiste il "byte 9" della NuPhy (1 a tasti rilasciati): nello stock il byte 8 del frame è sempre 0.
* Ogni cambio di stato è un frame, **in ordine**: un tap (`matrix_tap_keycode`: pressione e rilascio di seguito nello stesso giro del ciclo principale, usato da encoder, tap-hold e macro) deve uscire come due frame,
  altrimenti la pressione si perde. Il driver ha due code (tastiera 4 stati da 7 byte, consumer 4 usage); a coda piena l'ultimo elemento è sostituito (lo stato finale resta giusto). Un "non ora" rimette l'elemento in testa.
  Se si perde un rilascio l'host ripete il tasto all'infinito.

## 6. Modo, slot, accoppiamento, USB

* Il selettore è letto dalla MCU (50 ms stabile). Passando a 2.4G o Bluetooth: `01 01 00 <slot> 00` (slot 0 = 2.4G, 1–3 = Bluetooth); la radio si ricollega all'ultimo host. Il comando si ripete ogni 300 ms (fino a 8 volte)
  finché la radio non risponde (all'accensione può non essere pronta).
* **Accoppiamento:** `01 01 01 <slot> 00` (verificato: il telefono vede la tastiera e si accoppia). Tasti **Fn+B / N / M** = slot 1 / 2 / 3: tocco = seleziona, tenuto 3 s = accoppia (solo con il selettore su Bluetooth).
  Lo slot è in `user_settings.bt_slot`.
* In USB la radio viene fermata dopo aver mandato un report vuoto (altrimenti l'host wireless terrebbe il tasto premuto). Lo stock in USB disabilita l'interrupt della seriale (`IEN1 &= 0xBF`).
* Stato: byte `b3` = 1 in 2.4G, `b4` = 1 in Bluetooth, `b5` = 2 a riposo / 1 Bluetooth / 3 2.4G (ipotesi dalle osservazioni); `ff 03 01` costanti; in USB `03` → `00`. Il significato completo (batteria, collegato, accoppiato) **non è stato decodificato**.

## 7. Il driver

| File | Contenuto |
| --- | --- |
| `b68radio.h` | API e hook hardware |
| `b68radio.c` | protocollo e scheduler (puro, senza hardware: testato sul PC): `b68radio_build_command/_report`, `b68radio_set_link`, `b68radio_send_keys`, `b68radio_task`, `b68radio_stop`, traccia |
| `b68radio_hw.c` | seriale (init, priorità, interrupt, anello RX, TX), `SEL`, `ATT`, orologio; attivo con `-DRADIO_UART=1` (layout `default`); senza, hook vuoti (la sonda) |
| `kb.c` | selettore, instradamento dei report (wireless → radio, USB → cavo), tasti `B68_LK_1..3`, tasto della traccia |
| `src/sino51lib/sh68f90/interrupts.h` | riserva il vettore della seriale quando `RADIO_UART` è definito |
| `tests/host/test_b68radio.c` | 9 gruppi di test con una radio simulata (frame, modo e ripetizione, `ATT`, nessuna attesa delle conferme, `F0`/`F1`, rifiuti, stop, dati sporchi) |

Scheduler (`b68radio_task`, ogni giro del ciclo principale): elabora l'RX → se un frame è in uscita aspetta la fine (timeout 50 ms) → altrimenti, **con `ATT` alta e ≥ 1 ms dall'ultimo frame**, sceglie in quest'ordine:
conferma `F0`, conferma `F1`, comando di modo, nomi Bluetooth (una volta), report di tastiera, report consumer, rimando del modo, richiesta di stato.
`SEL` scende prima del TX e sale con la fine del TX. Memoria: ~200 byte di XDATA (anello, buffer, code, traccia da 24 eventi), ~4,4 KB di flash per `b68radio.c` (oggi restano ~1,4 KB sotto 0xAC00: per lo sleep servirà recuperare spazio, per esempio togliendo la traccia o restringendo la zona dei profili).

### Traccia per il debug (non nella build di rilascio)
Costava ~1,5 KB di flash e ~170 byte di XDATA, quindi è compilata solo con `-DB68RADIO_TRACE=1` (da aggiungere agli `args` del layout `default` in `src/keyboards/yunzii-b68/meson.build`). Descrizione:
**Fn+Backspace** (`B68_TRACE`, solo con `B68RADIO_TRACE`) scrive via cavo `c` e otto contatori da quattro cifre (report mandati, "non ora", frame `03`, frame `08`, volte in cui `ATT` è scesa, durata massima di `ATT` bassa in ms,
giro più lento del ciclo principale in ms, timeout della seriale), poi gli ultimi 40 eventi come `<ms basso><codice><argomento>` (10 modo mandato, 11 report, 12 `F0`/`F1`, 20 conferma, 21 "non ora", 23/24 frame `03`/`08`, 30/31 `ATT` scesa/risalita, 13 consumer mandato, 14 nome mandato, 40/41 **cambio dei byte di stato** con i byte 3-4 e 5). Dopo i contatori c'è una riga `s` con i 6 byte dell'ultimo stato: serve per decodificare "collegato / in accoppiamento" annotandola nelle varie situazioni.
Se un tasto è bloccato nel frattempo il testo si riempie di lettere ripetute: vanno tolte (non sono cifre esadecimali) prima di leggere i dati.

### Sonda
Il layout `radioprobe` (`meson compile -C build yunzii-b68_radioprobe_smk.hex`, [radio-software-probe.md](../radio-software-probe.md)) ferma il tick e prova a mano frame e pin, scrivendo i risultati nell'editor. È servita per tutta la scoperta
e resta per provare i comandi ancora ignoti.

## 8. Dove nello stock (dump `backups/dump/app_r1.bin`)

| Indirizzo | Cosa |
| --- | --- |
| `0xEC00–0xEC15` | `IEN0`, `IEN1 = 0x21`, priorità (`IPH1 = 0x42`, `IPL1 = 0x41`) |
| `0xAFA0` | init della seriale |
| `0xA43C` | interrupt della seriale: RX → buffer `0x54…` (max 23 byte); TX del frame da `0x33…`; a fine frame libera il flag di occupato e `P0.2` |
| `0x4236–0x45A0` | scheduler dei frame: code di record da 28 byte a `0x0C4C`; tipi 02 (report) e 03 (consumer) |
| `0xA999` | invio generico (lunghezza in `R5`, checksum `0x55 − Σ`) |
| `0x4210–0x4235`, `0xB124` | contatore a riposo, richiesta di stato `06` |
| `0x05EA` | gestore dei frame ricevuti: stato `02 06`, `03` → `F0`, `08` → `F1` |
| `0x864C–0x86FD` | lettura del selettore con debounce, variabile di modo `XDATA[0x0315]` (0 USB, 1 2.4G, 2 Bluetooth), slot `XDATA[0x0313]` |
| `0xB1A8` | comando di modo `01` |

## 9. Cosa è andato storto (per non rifarlo)

1. **Letto come "modo 0"** l'azzeramento dei flag di `SCON`: lo stock imposta `SSTAT`. La seriale è asincrona a 260870 baud, non SPI né a scorrimento.
2. **Sonda con lo stesso errore** (senza `SSTAT`): un flusso continuo di `FF`.
3. **`SEL` tenuta bassa durante l'ascolto**: nessuna risposta (la radio risponde dopo il rilascio).
4. **Frame a raffica senza guardare `ATT`** e senza rispondere ai frame `03`: radio occupata, report rifiutati.
5. **Seriale alla stessa priorità del tick** (la causa vera dei tasti persi e dei rilasci lunghi): sulla sonda il tick era fermo, nel firmware no.
6. **Tasto in lista e in bitmap**: lettere doppie.
7. Ritentare subito un report rifiutato e poi scartarlo: lasciava il tasto premuto. Ora si riprova dopo 10 ms.

## 10. Sleep (scritto, da provare sulla tastiera)

* **Quando:** solo in 2.4G e Bluetooth e **solo senza il cavo** (`usb_is_configured()` falso: con il cavo riconosciuto dal PC la tastiera resta sveglia). Dopo **10 minuti** senza tasti né encoder
  (`SLEEP_TIMEOUT 18750` × `SLEEP_FRAME_DIV 4` × ~8 ms di cornice LED, in `kbdef.h`; il contatore di SMK è a 16 bit e a 8 ms si fermerebbe a 8,7 minuti), oppure subito con **Fn+Tab** (`B68_SLEEP`, assegnabile da Vial) per provarlo.
* **Sequenza** (`src/keyboards/yunzii-b68/user_sleep.c`, dal dump `0x7E3C–0x7EF3`, `0xAE46`, `0x9C82`):
  1. alla radio, con la seriale ancora accesa e il tick fermo (attese a ritardi): `01 0C 08 07 00` e `01 0B 00` (`b68radio_sleep`; nello stock il secondo frame cambia in `01 0B 01`, senza il primo, secondo un flag interno `XDATA[0x0E2A]`: se il Bluetooth non si ricollega dopo il risveglio provare l'altra forma);
  2. `IEN1 &= ~ES0`, `SCON &= 0xBF`;
  3. tutti i 18 `PWMxxCON` (`0xFF80–0xFF91`) a 1;
  4. righe `P7.0–3` e `P5.3–4` ingressi con pull-up; latch a 0 e uscite per `P6` (tutto), `P5.0–2/7`, `P4.0/2/3/5/6`, `P7.4`, `P0.5–7`; `P0.0–1/3–7` e `P7.4/7` uscite a 0 senza pull-up; `P4.1` a 1 (la linea di risveglio), `P7.6` a 0;
  5. INT4 armato (`extint_wake_arm`: `IENC = 0xF3`, `EXF0 = 0x40`, gli stessi valori dello stock a `0x7EB5`); poi `power_enter_powerdown` (USB, clock, regolatore, `PCON |= 2`).
* **Risveglio:** `extint_wake_disable`, `user_gpio_init` (come `0xA627` dello stock), `b68radio_init` e il selettore riapplicato (`kb_link_wake`): la radio riparte con il comando di modo.
  Il tasto che sveglia non viene mandato (la coda si azzera con il nuovo comando di modo).
* **Rischio:** se un pin è sbagliato la tastiera non si risveglia: staccare e ricollegare l'USB (Esc+1+2 entra nel bootloader). Va provato con la batteria, senza cavo, con un tester in serie per misurare la corrente.

## 11. Da fare / ignoto

* **LED Caps/Num e indicatore "collegato"/batteria in wireless:** esclusi per scelta dell'utente (i frame `08` e i byte di stato non sono decodificati; la traccia permette di farlo).
* **Comandi ignoti:** `04`, `05`, `08` (23 byte), `0B`/`0C` oltre allo sleep, `0D`, `0E`; il system control (power/sleep dall'host) non è mappato.
* **LED di stato** accanto al Caps: probabilmente pilotato dalla radio; non richiesto.
