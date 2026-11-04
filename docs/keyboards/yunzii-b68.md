# Yunzii B68

> Stato: **porting in corso** (branch `port/yunzii-b68`). Piano: [porting-plan.md](../porting-plan.md).
> Task: [tasks/](../tasks/README.md). Questo file raccoglie i fatti verificati sulla tastiera;
> le sezioni vuote si riempiono nelle milestone indicate.

## Specs

- MCU: BYK916 (SH68F90A) — *da confermare fisicamente (M0.4)*
- Wireless: BK3632 (BT + 2.4G) — *il protocollo potrebbe differire da NuPhy (rischio R1)*
- Matrice, LED, indicatori, switch: *da rilevare (M0.4 / M2)*

## Ambiente (M0.1, chiuso)

| Strumento | Versione |
| --- | --- |
| SDCC | 4.5.0 (#15242) |
| Meson / Ninja | 1.10.2 / 1.13.2 |
| gcc (host) | 15.2.0 |
| ucsim-sh68f90 | 4.5.0 (patchato) |
| Commit di partenza | `dbd5c54` |

Ambiente fornito da `nix develop` (`use flake`). Build dir: `build/` (release), `build-debug/` (debug).

## Baseline di build (nuphy-air60, target di riferimento)

| Immagine | Build | XRAM usata | Flash usata | Stack libero |
| --- | --- | --- | --- | --- |
| firmware `default_smk` | release | 1053 B / 4096 | 21415 B / 60416 | 222 B |
| firmware `default_smk` | debug | 1234 B / 4096 | 24764 B / 60416 | 222 B |
| bootloader `boot` | release/debug | 96 B / 4096 | 3773 B / 3840 | 174 B |

Il bootloader ha ~67 B di margine. Altri target (release): example 11874 B flash, eyooso-z11 14129 B,
genesis-thor-300 13999 B, ciy-x77 15080 B. Le cifre combaciano con [architecture.md §17](../architecture.md).

## Esito dei controlli

| Controllo | Release | Debug |
| --- | --- | --- |
| `boot_check` (4 bootloader) | OK | OK |
| `check_interrupts` (6 firmware) | OK, nessuna collisione di overlay | OK |
| `meson test` (75 test) | 74 OK, 1 skipped, **1 errore noto** | non eseguita la suite completa |

**Errore noto:** `test_boot.TestFullBoot.test_boot_runs_full_init_to_main_loop` fallisce sulla build release
con `KeyError: 'console$console_buf not found in .map'`, perché la console esiste solo nella build debug.
Non è un difetto del firmware. Con la build debug il test passa:

```sh
cd tests && SMK_UCSIM=<ucsim-sh68f90> \
  SMK_FIRMWARE=$PWD/../build-debug/nuphy-air60_default_smk.hex \
  python3 -m unittest test_boot
```

Il report grezzo di M0.1 è in [tasks/M0.1-report.txt](../tasks/M0.1-report.txt).

## USB stock (M1.7, 2026-10-31)

Dati grezzi in [tasks/M1.7-report.txt](../tasks/M1.7-report.txt).

**Tastiera cablata:** `258a:010c`, produttore "BY Tech", prodotto "Gaming Keyboard", nessun seriale, bcdDevice 0x2002,
full-speed, 500 mA, 3 endpoint (EP0 + 2).

| Interfaccia | Classe | Endpoint | Contenuto |
| --- | --- | --- | --- |
| 0 | HID boot, protocollo 1 (tastiera) | EP1 IN 8 B, 1 ms | Tastiera 6KRO standard + 5 LED in uscita (via EP0) |
| 1 | HID, nessun boot | EP2 IN 16 B, 1 ms | Report con ID: 1 system control (3 bit), 2 consumer (16 bit), 3 vendor (3 B), 4 NKRO (120 bit, usage 4–0x70), 5 **feature 5 B** (ISP), 6 vendor: input 7 B + **feature 519 B**, 7 mouse (5 pulsanti, X/Y 16 bit, wheel, pan) |

Osservazioni utili al porting:

* La struttura è **la stessa di SMK/NuPhy** (EP1 8 B boot + EP2 16 B composito, ISP = feature report 5 sull'interfaccia 1),
  per cui il preset di sinowisp `--isp_iface_num=1 --isp_report_id=5` è confermato.
* **Non esistono endpoint OUT**: i LED e tutta la configurazione viaggiano su EP0 (SET_REPORT/GET_REPORT). Il
  software del produttore usa il **feature report ID 6 da 519 byte** come canale di configurazione: prova pratica che
  il canale a feature report via EP0 (piano B di D8 e spike S1) funziona su questo chip.
* La mia ipotesi D1 (EP2 IN+OUT come Raw HID da 32 B per VIA/Vial) resta da validare con lo spike S1: lo stock non usa
  EP OUT e non dimostra che funzionino.
* Il mouse report (ID 7) è presente: se lo vogliamo (mousekeys) possiamo copiare questo layout.

**Dongle 2.4G (non richiesto, utile):** `3554:FA09`, produttore "Compx", prodotto "2.4G Wireless Receiver"; low-speed;
interfaccia 0 boot tastiera con EP1 IN 8 B (inclusa una riga vendor), interfaccia 1 boot mouse con EP2 IN 25 B e un
feature report vendor ID 6 da 7 B. È un dispositivo separato: la tastiera in modalità 2.4G parla con il dongle via BK3632,
non con il PC.

**Preset sinowisp:** non verificati. Il test è stato lanciato in modo errato (`yunizii-al66` con refuso e senza il
sottocomando `read`). Non serve: con `--platform sh68f90 --vendor_id 0x258a --product_id 0x010c` sinowisp funziona già.

## Matrice (probe, 2026-10-31)

Misurata sulla scheda con il layout `probe` (ogni tasto digita `cNNrR`), tasti premuti riga per riga da sinistra. **Conferma
l'ipotesi ricavata dal firmware stock**: 16 colonne × 5 righe, tutti i tasti rispondono sulle celle previste. Risultato
(colonna, riga) per ogni tasto fisico, da sinistra a destra:

| Riga | Colonne dei tasti presenti | Tasti |
| --- | --- | --- |
| 0 | 0–13, 15 | Esc 1 2 3 4 5 6 7 8 9 0 - = Backspace · c15 |
| 1 | 0–13, 15 | Tab Q W E R T Y U I O P [ ] \ · c15 |
| 2 | 0–11, 13, 15 | Caps A S D F G H J K L ; ' · Enter · c15 |
| 3 | 0–10, 13, 14, 15 | Shift Z X C V B N M , . / · Shift · Su · c15 |
| 4 | 0, 1, 2, 5, 8, 9, 13, 14, 15 | Ctrl Win Alt · Spazio · Alt Fn · Sx Giù Dx |

Sono **67 celle**: 66 tasti più la **pressione dell'encoder rotativo** (`c15r0`). Non manca nessun tasto (la riga 4 ha 9
celle). Il layout `default` e `vial.json` usano le larghezze di una 65% ANSI; sotto l'encoder, `c15r1..r3` sono assunti
Del, PgUp, PgDn (da confermare). La **rotazione** dell'encoder non fa parte della matrice: i suoi contatti vanno trovati
(layout `probe`, righe `pNN=L` e `iNN`). Il chord di recupero è Esc + 1 + 2 (celle `c0r0`, `c1r0`, `c2r0`) e **è stato
verificato sul chip reale**: la tastiera compare come `0603:1020` ("Gaming KB", report 5 e 6 del bootloader stock). Lo
sblocco Vial è Esc + Invio.

## Encoder rotativo (probe v3, 2026-10-31)

I contatti sono su **P0.6 (A) e P0.7 (B)**, ingressi con pull-up (P0.5–P0.7 sono le linee radio sulla Air60, qui no). Catturato
girando la manopola: in un senso `p06=1 p07=1 p06=0 p07=0` (A sale, B sale, A scende, B scende), nell'altro
`p07=0 p06=0 p07=1 p06=1` (B scende per primo): quadratura normale, quattro variazioni per ciclo. Il primo senso è
trattato come orario. La pressione della manopola è `c15r0` (nella matrice). Il decoder è in `src/smk/encoder.c`, campionato
a ogni tick del timer; il layout `default` mappa {antiorario, orario} = {Vol−, Vol+} sul livello base.

Il testo della sonda si interrompeva dopo ~5 s perché la sonda scrive al massimo 60 eventi (`MAX_EVENTS`): non era un guasto.

## Illuminazione (dichiarata dall'utente)

LED **RGB** sotto i tasti, montati sul lato **sud** dell'interruttore (la parte verso l'utente); un **LED di stato** vicino al tasto Caps Lock. Probabile uso del LED di stato: Caps Lock e/o indicatori di
batteria o di collegamento. Da confermare osservando cosa fa con il firmware stock (accensione, cambio di modalità, batteria scarica, Caps Lock).

## LED RGB (sonda 1, 2026-10-31)

Verificato con la sonda `ledprobe` (un canale per volta, colonna alta + linea di scansione bassa):

* **Il pozzo di un LED è la linea di scansione della colonna di tasti**: accendendo il pozzo `s` si illuminano i tasti della colonna `s` (A: tutti i pozzi 0–15 con il canale 0 non accendono nulla perché
  il canale 0 non è una colonna LED; B/C: pozzo 0 → Esc, Tab, Caps, LCtrl, LShift; pozzo 8 → 8, I, K, RAlt, virgola, cioè i tasti della colonna 0 e della colonna 8).
* **Le sorgenti (colonne LED) sono i canali 3–17**, tre per riga di tasti, nell'ordine rosso, verde, blu:

| Riga di tasti | Canali (R, G, B) | Pin |
| --- | --- | --- |
| 0 (Esc 1 2 …) | 3, 4, 5 | `P1.3`, `P1.4`, `P1.5` |
| 1 (Tab Q W …) | 6, 7, 8 | `P2.0`, `P2.1`, `P2.2` |
| 2 (Caps A S …) | 9, 10, 11 | `P2.3`, `P2.4`, `P2.5` |
| 3 (Shift Z X …) | 15, 16, 17 | `P3.3`, `P3.4`, `P3.5` |
| 4 (Ctrl Win Alt …) | 12, 13, 14 | `P3.0`, `P3.1`, `P3.2` |

  (canale `k`: `P1.k` per k<6, `P2.(k−6)` per 6≤k<12, `P3.(k−12)` per k≥12.) La riga 3 e la riga 4 sono quindi scambiate rispetto all'ordine dei canali.
* Il LED del tasto in colonna `c`, riga `r` si accende con il pozzo `c` basso e il suo canale colore alto. 67 tasti × RGB.
* **PWM (sonda 2, serie F):** l'uscita è **alta tra DUTY1 e DUTY2**, qualunque dei due sia il maggiore; la larghezza `|DUTY2 − DUTY1|` è la luminosità (larghezza 0 = spento, 1100 su 1200 = pieno). Con periodo 1200 conteggi e `PWMxCON = 0x89` per il primo canale di ogni banco (e `0x08` per gli altri) come nello stock.
* **Mappa canale → PWM:** `P1.n ↔ PWM2n`, `P2.n ↔ PWM1n`, `P3.n ↔ PWM0n`.
* **Il LED di stato accanto al Caps non è pilotato da nessun pin provato** (sonda 2, serie D ed E: `P4.1`, `P7.4`, `P7.5`, `P7.6` come sorgenti e pozzi, canali `P1.0–2` con tutti i pozzi): nessun LED si è acceso. Ipotesi: lo accende la radio BK3632 (che ha il suo indicatore di collegamento) o l'integrato di carica
  della batteria. Si scioglie con la guida della radio o osservando quando si accende.

## Hardware (M0.4)

Foto del PCB (2026-11-01, tastiera aperta). Serigrafia in basso a sinistra: `BYK916+BK3…` (MCU BYK916 = SH68F90A, `U1`).

* **`J2` = pad ICP del BYK916** (6 fori, dall'alto): `VDD`, `TCK`, `TDI`, `TMS`, `TDO`, `GND`. Sono i quattro fili del programmatore Arduino
  Nano ([recovery-icp.md](../recovery-icp.md)): il recupero non richiede saldature sul chip.
* **`J3` = punti SPI del chip radio** (6 fori vicino all'antenna, dall'alto): `GND`, `CSN`, `MISO`, `MOSI`, `CLK`, `VCC`. Dal firmware originale il collegamento
  MCU↔radio risulta una **seriale a 260870 baud** (vedi il file del firmware stock), quindi `J3` è probabilmente per programmare la radio. Le righe `OPT1`/`OPT2` accanto sono ponticelli di opzione.
* Radio: il chip a 16 piedini `U5` accanto all'antenna (`ANT`) e al quarzo `Y3` (marcatura illeggibile nella foto).
* Da fare con il multimetro in continuità: quali pin del `U1` arrivano a `CSN`, `MISO`, `MOSI`, `CLK` di `J3`, e se c'è una linea di
  attenzione (MOT/ACK come sulla Air60) che in `J3` non è portata fuori.

## Dump del firmware stock (M0.5–M0.7)

Dump eseguito via ISP (M0.5), file in `backups/` (ignorato da git, fuori dal repo: tenere anche una copia su altro disco).

| File | Contenuto | sha256 |
| --- | --- | --- |
| `full.hex` | flash completa 0x0000–0xFFFF (64 KiB) | `ce6f7b83…d1ddcf9184` |
| `firmware.hex` | = `full` 0x0000–0xEFFF | `2aad0baf…c96060` |
| `bootloader.hex` | = `full` 0xF000–0xFFFF, ma indirizzato da 0x0000 nell'hex | `e1f44081…37ddab` |

Verifica incrociata fatta da Claude: firmware e bootloader sono coerenti con `full`. Osservazioni sullo stock:
reset vector `0x0000: LJMP 0x9A26` (punta all'applicazione, non al bootloader come in SMK); bootloader stock a 0xF000
inizia con `clr EA; mov SP,#0x70`; nessuno stub tipo `ISP_ENTRY` evidente a 0xFF00. Da usare per il gate G0.
**Attenzione:** per ripristinare il bootloader da `bootloader.hex` va rispostato a 0xF000.

Code option e prova di ripristino: M0.6/M0.7 (ancora da fare).

## Gate G0 (M0.9)

**Esito (Claude, 2026-10-31): NON superato — il Nano diventa obbligatorio prima del primo flash (R11).**

* In ISP si entra **tramite l'applicazione stock**: `sinowisp` apre il dispositivo in esecuzione `258a:010c`
  (interfaccia 1, feature report 5), la tastiera si resetta nel bootloader, si legge, si riavvia. Non è stato provato
  nessun ingresso indipendente dall'app (tasto/ponticello a freddo), né sappiamo cosa fa il bootloader stock se
  l'app è assente o non valida.
* Quindi con un'app che non parte non c'è un percorso di recupero provato. Per sbloccare il primo flash serve l'ICP
  (M0.3), oppure un test controllato futuro.
* Preset da usare: `--platform sh68f90 --vendor_id 0x258a --product_id 0x010c` (isp_iface_num=1, isp_report_id=5 di
  default, identici a SMK).

**Anomalia dump — risolta (M0.10):** `dump/r1.bin` e `dump/r2.bin` sono identici (MD5 `4aea8bec8f13356a66a90788be58f179`,
64 KiB) e coincidono con la rilettura di M0.9. Rispetto al primo `full.hex` differiscono solo 2 byte: `0xC60A`
(0x11→0x08) e `0xC65A` (0x04→0x01), cioè dati di stato scritti dal firmware stock in una zona a 0xC600.
Il backup è affidabile. **Dump canonico: `backups/dump/r1.bin`** (binario, 65536 B); gli `.hex` restano come copie.

## Build di rilascio (unica)

Esiste un solo layout, `default`, con interfaccia raw HID (VIA, Vial), `bcdDevice` 0x0200 e retroilluminazione RGB.
Per compilare **solo la B68** (senza NuPhy e le altre schede):

```
nix develop --command bash -c 'meson setup build -Dboards=yunzii-b68 --buildtype=release && meson compile -C build'
```

Il file da flashare è `build/yunzii-b68_default_smk.hex`. Il flicker dei LED: `LED_SUBFRAMES_PER_SCAN` è 4 (cornice di 16 colonne
≈ 8 ms, 125 Hz; prima 55–100 Hz).


## Tap dance (click e pressione lunga dell'encoder)

Il tasto dell'encoder (cella `c15r0`, in alto a destra) ora è `TD(0)`, un *tap dance* di Vial. Si modifica nella scheda **Tap Dance**
di Vial (8 voci): *Tap* = un tocco, *Hold* = tenuto oltre il tempo (200 ms, o il "Custom Tapping Term" della voce), *Double Tap* =
due tocchi (se è impostato, il tocco singolo aspetta un tempo per capire se ne arriva un secondo). Il campo *Tap + Hold* si salva
ma non è ancora usato. Per usare un'altra voce, assegna `TD(n)` al tasto dalla scheda Keycodes > Tap Dance.
Predefinito di `TD(0)`: tocco = Mute, tenuto = Play/Pausa, doppio tocco = traccia successiva.
Le azioni possono essere qualunque codice (tasti, media, livelli `MO`, macro, tasti luce), non un altro `TD`.

## Tastiera salvata e firmware nuovo

La tastiera salvata in flash porta l'impronta della tastiera predefinita del firmware che l'ha scritta. Se un firmware nuovo cambia la
tastiera predefinita (per esempio aggiunge i tasti luce), la copia salvata non viene più usata e si riparte dalla predefinita:
le modifiche fatte con Vial vanno rifatte una volta. Prima di questo cambiamento il flash teneva la tastiera vecchia, quindi mancavano
i tasti luminosità.

## Tasti luce (livello Fn)

Fn+Q / W = effetto − / +, Fn+E / R = luminosità − / +, Fn+T / Y = velocità − / +, Fn+U / I = colore − / + dell'effetto "tinta unita" (bianco, poi 16 colori della ruota: rosso, magenta, lilla, blu, ciano, verde, giallo…; per profilo).

## Wireless (2.4G e Bluetooth)

Il selettore fisico vicino all'USB-C è letto dalla MCU su `P0.4` (2.4G) e `P0.3` (Bluetooth); entrambi alti = USB. In wireless la tastiera manda i report alla radio BK3632
(seriale a 260870 baud; protocollo e driver in [yunzii-b68-radio.md](yunzii-b68-radio.md)); i tasti multimediali e l'encoder per ora escono solo dal cavo, e il LED Caps Lock probabilmente non segue l'host in wireless (lo stato dei LED dell'host arriva dalla radio nei frame `08`, non ancora letti).
**Fn+B / N / M** = slot Bluetooth 1 / 2 / 3: un tocco seleziona lo slot, tenuto 3 s avvia l'accoppiamento (solo con il selettore su Bluetooth). Il driver è in
`src/keyboards/yunzii-b68/b68radio*.c`, la sonda per le prove in `layouts/radioprobe` (docs/radio-software-probe.md). La traccia del collegamento (Fn+Backspace) è disponibile solo compilando con `-DB68RADIO_TRACE=1`. **Sleep:** dopo 10 minuti di inattività (o Fn+Tab) in 2.4G/Bluetooth, mai con il cavo.

## Switch Win/Mac (2026-11-01)

Lo switch fisico Win/Mac **non è nella matrice**: è **`P0.5`**, un ingresso con pull-up (`P0CR = 0x04` lo lascia ingresso, `P0PCR = 0xF8` lascia il
pull-up). Misurato sulla scheda con `tools/diag.py --ports`: con lo switch su **Mac** il pin legge **1**, su **Windows** **0**, e il livello non dipende
dalle colonne (non è cablato nella matrice). (Le due catture da sole non dicevano quale posizione fosse quale: è stata la tastiera a dirlo,
perché al primo tentativo il layer Mac usciva sul lato sbagliato.) Il candidato che veniva dal dump (`P7.0`, letto dalla ISR insieme alle righe) resta alto in tutte e
due le posizioni: non è lui.

Conferma dal firmware stock: a `0xA9D9` c'è il debounce di `P0.5` (due contatori, dieci letture uguali di fila) che tiene lo stato in
`XDATA[0x0307]`; quello stato sceglie fra **due insiemi di tabelle della keymap** (`DPTR = 0x7Cxx + 4 × indice` dopo il test di `0x0307`), cioè
il venditore cambia il layer di base. In Oblio fa lo stesso, ma con **due tastiere complete**: su Windows i layer 0 (base) e 1 (Fn), su Mac i
layer 7 (base con Alt e GUI scambiati) e 6 (il suo Fn, che il tasto Fn del lato Mac seleziona al posto dell'1). Tutti e quattro sono
modificabili da Vial, quindi il lato Mac può avere anche un Fn completamente diverso.

## Profili

Quattro profili completi: ognuno ha la sua tastiera (8 livelli), encoder, tap dance, macro, effetto/luminosità/velocità dei LED e tempo
di tap/hold. Si scelgono con **Fn+Z / X / C / V** (tasti `PF1…PF4`, assegnabili anche da Vial). Il cambio avviene quando tutti i tasti sono
rilasciati; il tasto scelto diventa bianco per circa mezzo secondo. Il profilo in uso è ricordato dopo lo spegnimento (`user_settings.profile`).
Un profilo mai modificato parte dalla tastiera predefinita.

Il tempo di tap/hold si cambia dalla scheda *Settings* di Vial (impostazione "Tapping Term", id 7) e vale per il profilo in uso.

Flash: ogni profilo ha le sue due copie A/B da 4 settori; i profili 0–3 stanno in 0xAC00–0xEBFF (`keymap_profiles = 4` in meson.build),
l'applicazione deve restare sotto 0xAC00 (oggi 0x8C4F). In RAM c'è un solo profilo (XDATA 3709 su 4096).
