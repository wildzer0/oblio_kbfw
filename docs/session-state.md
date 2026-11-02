# Stato della sessione — dove riprendere

Ultimo commit: **vedi `git log`** (la sessione del 2026-11-02 ha chiuso la batteria e tolto registro/diagnostica superflua) (branch `main`, tutto pushato). Questo file è il punto di ingresso per chi riprende: dice dove siamo, cosa aspetta
una prova sulla tastiera, cosa è verificato e le trappole già pagate. La storia sta in `CHANGELOG.md`, il quadro tecnico in
`docs/firmware-report.md`, le regole di lavoro in `CLAUDE.md`.

## Dove siamo

Il firmware è **completo e usabile**: USB e radio (2.4G e Bluetooth), backlight con effetti, Vial completo (keymap, tap dance, combo, key
overrides, tab Lighting con colore pieno), NKRO, Caps Word, Leader, mouse dall'encoder, switch Win/Mac su P0.5, gauge della batteria.

| | |
| --- | --- |
| Flash | **51797 / 56320** (4,5 KB liberi) |
| XDATA | **3755 / 4096** (341 B liberi) |
| Deposito keymap | **1709 / 2048** (339 B liberi) |
| Test host | **27 / 27** (`make test`, include il controllo della definizione Vial) |
| Simulatore ucsim | **24 / 24** (`make sim`) |
| Gradini | 1–3 costruiti (`make stages`), vettori interrupt ok (4) |

**Mappa flash**: codice fino a **0xDC00**, keymap con due copie **0xDC00–0xEBFF**, impostazioni **0xEC00** (log append-only), **0xEE00 = marcatore del bootloader, mai toccare**, bootloader stock a 0xF000. `hal/flash.c` rifiuta le scritture fuori da 0xDC00–0xF000; `make size` impone il limite di codice.

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
4. **Da provare**: con l'app HUD collegata i LED non devono più sfarfallare (guardare il nuovo contatore con `diag.py --watch 1`: dovrebbe restare sotto ~50 µs);
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
nix develop --command make sim               # ~60 s, il firmware vero nel simulatore (24 test)
nix develop --command make stages            # build a gradini 1-3

py tools\diag.py [--watch 1] [--radio] [--daemon N file.csv]     # Windows
py tools\diag.py --daemon 60 oblio-log.csv                                # contatori in CSV, uno ogni 60 s
python3 tools/diag.py [...]                                               # Linux
```

Recupero, se qualcosa va storto: **Esc+1+2 mentre si inserisce il cavo** → bootloader ISP (nessun ICP: è l'unica via).
