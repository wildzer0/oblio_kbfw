# Stato della sessione — dove riprendere

Ultimo commit: **vedi `git log`**. Chiusura del 2026-11-02: tutte le prove dell'utente sulla tastiera sono andate bene (USB, salvataggi senza buio, batteria, remote wakeup, NKRO, combo, key override). Si riprende solo se compare un problema (branch `main`, tutto pushato). Questo file è il punto di ingresso per chi riprende: dice dove siamo, cosa aspetta
una prova sulla tastiera, cosa è verificato e le trappole già pagate. La storia sta in `CHANGELOG.md`, il quadro tecnico in
`docs/firmware-report.md`, le regole di lavoro in `CLAUDE.md`.

Per chi inizia: **[guida-sviluppatore.md](guida-sviluppatore.md)**. Documentazione di riferimento: **[design.md](design.md)** (architettura e design, per moduli) e **[api.md](api.md)** (riferimento delle API generato dagli header: `make api`, `make check-api`).

## Dove siamo

Il firmware è **completo e usabile**: USB e radio (2.4G e Bluetooth), backlight con effetti, Vial completo (keymap, tap dance, combo, key
overrides, tab Lighting con colore pieno), NKRO, Caps Word, Leader, mouse dall'encoder, switch Win/Mac su P0.5, gauge della batteria.

| | |
| --- | --- |
| Flash | **54676 / 55808** con il registro batteria; 53232 / 56320 con `BATTERY_LOG=0` |
| XDATA | **3654 / 4096** (442 B liberi); la build di profilo: 56316/56320 B di flash (4 B liberi!) |
| Deposito keymap | **1709 / 2048** (339 B liberi) |
| Test host | **29 / 29** (`make test`, include il controllo della definizione Vial) |
| Simulatore ucsim | **23 / 23** (`make sim`) |
| Vettori interrupt | 4, nessun overlay statico (`make check-interrupts`) |

**Mappa flash**: codice fino a **0xDC00**, keymap con due copie **0xDC00–0xEBFF**, impostazioni **0xEC00** (log append-only), **0xEE00 = marcatore del bootloader, mai toccare**, bootloader stock a 0xF000. `hal/flash.c` rifiuta le scritture fuori da 0xDC00–0xF000; `make size` impone il limite di codice.

## LED vicino al Caps — chiuso (limite dell'hardware)

Il LED è sullo stesso incrocio del rosso del tasto C (vedi `docs/keyboards/yunzii-b68-firmware-stock.md`): segue il rosso del C e non si può comandare da solo. Se un giorno si vuole il Caps Lock come nello stock: forzare il rosso del C a pieno mentre Caps Lock/Caps Word è attivo (una riga in `paint_one`, livello regolabile).

## Trappola pagata il 2026-11-12: la posizione del codice USB

La 0.9.1 non veniva riconosciuta via USB per colpa della **posizione** del codice USB nella flash (non del Caps Word): vedi [design.md §14.2](design.md) per le misure. Cura: i moduli USB sono **collegati per primi** (`Makefile`, `USB_SRC`) e `tools/check_usb_layout.py` fallisce se si spostano. Dopo qualunque modifica al codice USB: provare la build sulla tastiera con l'utility web **prima** di aggiornare `tools/usb_layout.txt`. La versione **0.9.2** porta questa cura insieme al Caps Word che tiene il `_`.

## Utility web di aggiornamento (2026-11-11) — provata dall'utente

**Provata sulla tastiera** il 2026-11-11: l'utente ha flashato la 0.9.0 direttamente dalla pagina (https://wildzer0.github.io/oblio_kbfw/). Per pubblicare una versione nuova: [pubblicare-una-release.md](pubblicare-una-release.md).
Quello che segue è la procedura di prova originale, tenuta per riferimento (la sonda `web/probe.html` resta utile se qualcosa cambia nel browser o nel bootloader).

### (storico) Da provare sull'hardware

Pronta e provata contro un finto bootloader e un finto `navigator.hid` (`make web-test`), **non** con la tastiera. Ordine di prova:
1. `cd web && python3 -m http.server 8000`, Chrome su `http://localhost:8000/probe.html`: «Cerca la tastiera» (c'è un dispositivo con il report 5?), «Leggi la versione (raw HID)», poi «Invia 75» e «Cerca il bootloader» (VID/PID, report 5 e 6 insieme o separati?). Incolla il registro.
2. Se il report 5 non si vede: ripiego (comando Vial `0x0B` con sblocco, oppure Esc+1+2 all'inserimento dell'USB e poi la pagina vera).
3. Aggiornamento vero da `http://localhost:8000/` con un `.hex` trascinato (ricorda: keymap e impostazioni tornano al default). Una volta pubblicato il sito (tag `v0.9.0`, GitHub Pages con sorgente «GitHub Actions»): da `https://wildzer0.github.io/oblio_kbfw/`.
Punti da confermare: offset dei dati di `receiveFeatureReport` (isp/hid: si toglie il report id se c'è), il secondo click per il bootloader, l'attesa dei 2 s dopo ERASE.

## Da provare sulla tastiera (2026-11-04)

1. **Canale raw HID**: scollega/ricollega il cavo più volte e dopo ogni volta `py diag.py`; prova Vial. Prima falliva a intermittenza («did not answer», Vial muto, «no Oblio keyboard found»).
2. **Linee del caricatore** (confermate col cavo: P4.4 alto; in carica P7.7 basso e gauge azzurro; la carica finita è stata vista come gauge verde): `py diag.py` mostra «charger: P4.4 …, P7.7 …». Leggile in tre stati: a batteria (staccato), cavo mentre carica, cavo a carica finita. Attesi (dallo stock): P4.4 alto col cavo; P7.7 basso in carica, alto a carica finita. Se sono invertiti, dimmelo: si cambia in `board/charger_hw.c` in una riga.
3. **Gauge** (Fn+Backspace): col cavo dieci tasti azzurri (in carica) / verdi (finita); a batteria il livello.
4. **Sleep col cavo**: in 2.4G con il cavo inserito e il PC acceso, la tastiera non deve sparire da Windows nemmeno dopo il tempo di inattività (default 10 min).

## Ottimizzazioni del 2026-11-06/07 — da confrontare sull'hardware

Prima ondata (già misurata dall'utente con la build di profilo): passate 183–231 → **343 al secondo**, `matrix_feed` 750 → 189 µs, chunk di pittura 3285 → 1722 µs, ISR del timer 40,9 → 39,0 %.
Seconda ondata (commit successivo, **non ancora misurata**): cache dei colori nei LED (uniformi, onde, anelli del radiale), ruota senza divisione, scritture del framebuffer per puntatore, Caps Word nel Caps.
Il simulatore non dà un conteggio utile per i LED (nella sua finestra il ridisegno quasi non scatta): la misura è `diag.py --profile-reset`, scrivere ~30 s, `--profile` e confrontare «led: paint chunk», `led_task` e le passate al secondo con i numeri sopra.
Dettagli e motivazioni: [design.md §6.1](design.md).

## Batteria — chiusa il 2026-11-02 (scala provvisoria)

La batteria è una **lettura ADC a 10 bit** nei byte 4 (basso) e 5 (alto) dello stato radio, ricavata dal firmware stock (gestore a `0x05EA`, gauge a `0x7CF0`);
il vecchio «livello a 3 bit» era il layout NuPhy. Misurato: **4,0378 V a riposo = lettura 981** (~243 conteggi/V), il cavo satura a 1023. Cella: Li-Po
5000 mAh, 4,2 V di carica. `board/battery.c` usa una **curva di scarica Li-Po** (800 = 3,30 V = 0 % … 1020 = 4,20 V = 100 %), ignora le letture sotto 600
(ADC non assestato all'accensione) e quelle ≥ 1021 (cavo). **Per confermare**: la lettura subito dopo aver tolto il caricatore dovrebbe essere ~1015–1020.
Il registro in flash, il logger da tastiera e la diagnostica `--blog/--ports/--monitor` sono stati **tolti** (erano strumenti per trovare questo).

## Prossimi passi concordati

1. **Latenza del BLE — misurata il 2026-11-02: non è il firmware.** Con i contatori nuovi: **0 rifiuti** («non ora») sia in BLE sia in 2.4G, e il massimo da tasto a
   «accettato dalla radio» è 33 ms (BLE) contro 40 ms (2.4G): uguale nei due modi, quindi il ritardo in più del BLE sta **dopo** la radio (intervallo di
   connessione BLE, deciso dall'host con la BK3632; latenza dello slave). Nessun comando noto della radio lo cambia (nello stock `04 02` dopo il cambio di modo e
   `04 03` dopo ~80 s sono probabilmente avvio/fine dell'advertising, non parametri di connessione). Prove utili: lo stesso BLE su un telefono o un altro PC
   (se cambia, è l'host); primo tasto dopo una pausa contro raffica (se solo il primo è lento, è lo slave latency). Resta da capire il massimo di 33–40 ms (ATT bassa
   a tratti?), uguale nei due modi.
2. Provare **avviso di batteria** (`battery_task`: sotto 800 gauge rosso ogni 30 s, sotto 752 per 10 s sleep) e **remote wakeup USB** (PC in sospensione, un tasto lo sveglia).
3. (fatto) diagnostica ridotta a 9 contatori e `--radio` (+ «timer interrupt: longest start delay»).
5. **Da provare (log della keymap)**: modifica un tasto da Vial: i LED **non** devono spegnersi; `diag.py` mostra «keymap: records in the log» che sale di 1 a modifica e «whole saves ever» che resta fermo; dopo ~70 modifiche un solo buio di ~130 ms. Spegni/riaccendi: le modifiche restano.
6. **Da provare (SOF a 16 bit)**: slider su USB con il cavo: i LED non devono più spegnersi a intervalli e i tasti devono sempre rispondere.
   i tasti luce non devono più spegnere i LED per ~130 ms 2 s dopo (ora solo ogni 36 salvataggi, e quando si salva la keymap da Vial).

## Da provare sulla tastiera

Flash con Vial chiusa: `sinowisp write --platform sh68f90 --vendor_id 0x258a --product_id 0xdead --format ihex --isp_iface_num 0 -f build/oblio.hex`

1. **Gauge**: Fn+Backspace a batteria (4,04 V ≈ 83 %: ~8 tasti verdi). Appena accesa: dieci tasti bianchi lampeggianti finché l'ADC non si assesta.
2. **Tab Lighting**, **NKRO**, **Caps Word**, **Leader**, **flicker** (`py tools\diag.py --watch 1`, annotare `backlight back-offs` e `longest sector erase`): come nelle sessioni precedenti, ancora da confermare.

> Cambiando predefiniti e definizione Vial, **la keymap salvata riparte dai default** e il PC può reinstallare il dispositivo: è normale.

## Domande aperte

1. **Causa del flicker** (la pausa del flash è scesa a ~20 ms; confermare con i contatori).
2. **Sequenze del Leader**: due esempi (Esc, mute); le vere le decide l'utente in `board/leader.c`.
3. **Saturazione sugli effetti animati**: oggi lo slider agisce sulla tinta unita; per gli arcobaleni costa ~30 B.
4. **Bluetooth con il PC**: ora **funziona** (verificato dall'utente 2026-11-02); resta la latenza (sopra).

## Trappole già pagate (non ripeterle)

- **`usb_device_configured()` è "cavo dentro *e* host che pilota il bus"**: sul chip niente segnala il cavo staccato, quindi si guarda il
  contatore di start-of-frame ogni 250 ms (`usb_device_bus_check`). Fidarsi del solo `configured` ha fatto perdere una notte di misure.
- **Un contatore che serve a vedere se «si è mosso» non deve poter fare il giro nel tempo di una finestra**: il contatore SOF a 8 bit (256 ms a 1 kHz) dichiarava il cavo staccato quando la finestra era di 256 ms. Confrontare con differenza, o usare un contatore largo abbastanza.
- **Il simulatore non sa misurare i tempi**: il suo Timer 2 è un flag virtuale. Un debounce puramente a tempo non si chiude lì, per questo
  `MATRIX_DEBOUNCE_SAMPLES` esiste come pavimento.
- **Il rapporto slot LED / scansioni resta 4**: portarlo a 8 rompe il test dei tasti nel simulatore (con 8:1 il test che tiene premuta una riga
  non riceve più report su EP1; verificato con un bisect, in entrambi gli ordini di scansioni e passate).
- **`SETTLE_US` = 5** funziona (verificato nel simulatore dopo il cambio), ma se si tocca la scansione ricordarsi la sonda in
  `board/matrix_hw.c`: due letture concordi per colonna.
- **Attaccare il cavo resetta la scheda** (`reset cause 0x00`): la RAM muore proprio quando si leggono i dati. E con il cavo la batteria carica: la
  lettura satura. Per misurare a batteria serviva un canale senza cavo (la tastiera che digita la lettura) o un registro in flash: entrambi tolti a
  misura fatta. Un comando diagnostico che richiede lo sblocco **si limita a rimandare indietro la richiesta**: un registro «vuoto» era illeggibile, non vuoto.
- **L'ADC ondeggia di 1–2 conteggi** a ogni risposta: ogni logica «scrivi quando cambia» va con una soglia, o consuma la flash.
- **`0xED00` non è libero**: le impostazioni occupano tutto il settore `0xEC00`–`0xEDFF`.

## Fatti verificati sulla tastiera

- **Switch Win/Mac = P0.5**, `OS_SWITCH_IS_MAC 1` in `board/os_switch.c` è la polarità giusta (alto = Mac). Windows usa i layer 0/1, Mac i
  layer 5/4 (base con Alt↔GUI scambiati e Fn suo), tutti e quattro editabili da Vial.
- **P7.0 non è lo switch**: il firmware stock lo legge come sesta riga, ma sulla tastiera resta alto e non cambia con le strobe.
- **Byte 4 dello stato radio**: `0xff` col cavo (in carica), `0x03` in uso, `0x01` all'accensione.
- **Il dongle 2.4G non ha canale diagnostico**: si presenta come tastiera, senza l'interfaccia raw HID di Vial e `diag.py`.
- Il byte dei **LED host** è confermato dal dump stock (`0x06B6`).
- **NKRO**: il buffer hardware di EP1 è di **16 byte**, quindi il report è di 112 bit (non 120 come QMK) contando l'ID.
- **Latenza dei tasti**: scansione ~510 µs ogni ~2,4 ms + debounce 5 ms + USB 1 ms ≈ **5–9 ms**, contro i ~6–7 ms di QMK (che usa
  `MATRIX_IO_DELAY` 30 µs, `DEBOUNCE` 5 ms e `USB_POLLING_INTERVAL_MS` 1 ms).

## Comandi

Tutto dentro `nix develop` (SDCC 4.5, gcc, python, `ucsim-sh68f90`): **senza `nix develop` `make fw` non trova `sdcc`**.

```
nix develop --command make test              # 28 test host + definizione Vial
nix develop --command make fw size           # build/oblio.hex, flash e XDATA contro il budget
nix develop --command make check-interrupts  # i quattro vettori, nessun overlay statico
nix develop --command make sim               # ~60 s, il firmware vero nel simulatore (23 test)

py tools\diag.py [--watch 1] [--radio] [--daemon N file.csv]     # Windows
py tools\diag.py --daemon 60 oblio-log.csv                                # contatori in CSV, uno ogni 60 s
python3 tools/diag.py [...]                                               # Linux
```

Recupero, se qualcosa va storto: **Esc+1+2 mentre si inserisce il cavo** → bootloader ISP (nessun ICP: è l'unica via).
