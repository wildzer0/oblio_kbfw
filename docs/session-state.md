# Stato della sessione — dove riprendere

Ultimo commit: **`41eb8b0`** (branch `main`, tutto pushato). Questo file è il punto di ingresso per chi riprende: dice dove siamo, cosa aspetta
una prova sulla tastiera, cosa è verificato e le trappole già pagate. La storia sta in `CHANGELOG.md`, il quadro tecnico in
`docs/firmware-report.md`, le regole di lavoro in `CLAUDE.md`.

## Dove siamo

Il firmware è **completo e usabile**: USB e radio (2.4G e Bluetooth), backlight con effetti, Vial completo (keymap, tap dance, combo, key
overrides, tab Lighting con colore pieno), NKRO, Caps Word, Leader, mouse dall'encoder, switch Win/Mac su P0.5, registro della batteria in flash.

| | |
| --- | --- |
| Flash | **53595 / 55296** (1,7 KB liberi) |
| XDATA | **3779 / 4096** (317 B liberi) |
| Deposito keymap | **1709 / 2048** (339 B liberi) |
| Test host | **28 / 28** (`make test`, include il controllo della definizione Vial) |
| Simulatore ucsim | **24 / 24** (`make sim`) |
| Gradini | 1–3 costruiti (`make stages`), vettori interrupt ok (4) |

**Mappa flash (cambiata il 2026-11-02)**: codice fino a **0xD800**, **registro batteria 0xD800** (un settore), keymap con due copie
**0xDC00–0xEBFF**, impostazioni **0xEC00** (log append-only), **0xEE00 = marcatore del bootloader, mai toccare**, bootloader stock a 0xF000.
`hal/flash.c` rifiuta le scritture fuori da 0xD800–0xF000; `make size` impone il limite di codice.

## Cosa è stato fatto in questa sessione (dal più vecchio)

| Commit | Cosa |
| --- | --- |
| `1322478` | 6 layer (da 8), colore per tasto Vial RGB (poi ridimensionato) |
| `db5ecf7` | **Tab Lighting** in Vial (serve `"lighting": "vialrgb"` nella *definizione*, non il byte della risposta), **NKRO** 112 bit, **Caps Word**, **Leader** |
| `ce85989` | **Flicker**: la pausa del salvataggio copre solo le cancellazioni (~100 → ~20 ms), ritiro del backlight 32 → 12 slot |
| `26816ae` | **Colore giusto**: la ruota colori è quella HSV standard (era rosso→blu→verde: l'arancione usciva viola) |
| `8088c75` | **Tinta + saturazione** in una parola da 16 bit (0 byte di memoria in più), colore completo dalla tavolozza |
| `38ed7a8` | **Batteria**: `0xFF` col cavo è una sentinella (non 100%); lo stato letto a batteria si legge con `diag.py --radio` |
| `45732e0` | **Registro batteria a bordo** (`core/blog.c`), letto con `diag.py --blog`; `diag.py --daemon N file.csv` per un demone sul cavo |
| `5780a8b` | **Bug del cavo**: `usb_device_configured()` non tornava mai a 0 quando il cavo usciva (ora conta gli start-of-frame, `usb_hook_sof_ticks`) |
| `6e3ea10` | **Debounce a tempo** (`MATRIX_DEBOUNCE_MS` 5 ms, come QMK) e **scansione più corta** (`SETTLE_US` 10→5: −39% di istruzioni in ISR) |
| `41eb8b0` | **Registro in flash** (settore a 0xD800, 128 record) e **scrittura incondizionata**: sopravvive al reset che il cavo provoca |

## Da provare sulla tastiera (in ordine)

Flash con Vial chiusa:
`sinowisp write --platform sh68f90 --vendor_id 0x258a --product_id 0xdead --format ihex --isp_iface_num 0 -f build/oblio.hex`

1. **Registro della batteria** (deve funzionare adesso): staccare il cavo, premere qualche tasto, aspettare ~30 s, riattaccare,
   `py .\diag.py --blog`. Deve mostrare record con i minuti di accensione, il byte batteria e i flag. I record presi **col cavo** hanno il
   byte `0xff` e la scritta "cable in: charging"; quelli **a batteria** hanno il livello vero. Se resta vuoto, il prossimo sospetto è l'indirizzo
   del settore o l'ordine di `blog_init()` (vedi `core/blog.c`), non il gate.
2. **Tab Lighting**: c'è, con gli effetti (Disable, Solid Color, Gradient Up/Down, Left/Right, Breathing, Cycle Out In), luminosità, velocità e
   colore. **Arancione deve uscire arancione**, e un pastello deve restare pastello. Staccare e riattaccare: le impostazioni restano.
3. **NKRO**: sette lettere tenute premute insieme in un editor → devono comparire tutte.
4. **Caps Word**: assegnare `Caps Word` a un tasto, premere, scrivere `ciao-mondo` senza Shift, poi spazio.
5. **Leader**: assegnare `Leader`, premere e poi `E S C` → deve uscire Esc (le sequenze stanno in `board/leader.c`).
6. **Flicker**: quando la retroilluminazione si spegne e torna, `py .\diag.py --watch 1`. Se sale **`backlight back-offs`** è la fame del main
   loop; se resta fermo ed era appena cambiata un'impostazione era la pausa del salvataggio. Annotare **`longest sector erase (us)`** (un erase
   di settore misurato costa ~32 ms: quattro settori sono ~130 ms di buio per salvataggio).

> Cambiando predefiniti e definizione Vial, **la keymap salvata riparte dai default** e il PC può reinstallare il dispositivo: è normale.

## Domande aperte

1. **Batteria — risolta sulla carta (2026-11-02 notte), da confermare sulla tastiera.** Il dump stock dice: lettura ADC a 10 bit = byte5:byte4 del
   frame di stato, `% = (lettura − 715) × 10 / 22` (0x7CF0). Dettagli in `core/radio.h` e CHANGELOG. Prova: Fn+Backspace a batteria deve dare un valore
   plausibile (la lettura `0x0303` vista = ~25 %); `py tools\diag.py --blog` mostra lettura e %. Col cavo la lettura è satura (0x03FF) e si ignora.
2. **Causa del flicker.** La pausa del flash è già scesa (~100 → ~20 ms) e la ISR della scansione pesa un terzo in meno, ma la conferma arriva
   dai contatori del punto 6.
3. **Sequenze del Leader**: due esempi (Esc, mute). Le sequenze vere le decide l'utente, si scrivono in `board/leader.c` (compile-time, come QMK).
4. **Saturazione sugli effetti animati**: oggi lo slider agisce sulla tinta unita; per gli arcobaleni costa ~30 B (la conversione c'è già).
5. **Bluetooth con il PC**: il telefono vede `OBLIO`, il PC no (sospetto un legame vecchio su Windows, non il firmware).

## Trappole già pagate (non ripeterle)

- **`usb_device_configured()` è "cavo dentro *e* host che pilota il bus"**: sul chip niente segnala il cavo staccato, quindi si guarda il
  contatore di start-of-frame ogni 250 ms (`usb_device_bus_check`). Fidarsi del solo `configured` ha fatto perdere una notte di misure.
- **Il simulatore non sa misurare i tempi**: il suo Timer 2 è un flag virtuale. Un debounce puramente a tempo non si chiude lì, per questo
  `MATRIX_DEBOUNCE_SAMPLES` esiste come pavimento.
- **Il rapporto slot LED / scansioni resta 4**: portarlo a 8 rompe il test dei tasti nel simulatore (con 8:1 il test che tiene premuta una riga
  non riceve più report su EP1; verificato con un bisect, in entrambi gli ordini di scansioni e passate).
- **`SETTLE_US` = 5** funziona (verificato nel simulatore dopo il cambio), ma se si tocca la scansione ricordarsi la sonda in
  `board/matrix_hw.c`: due letture concordi per colonna.
- **Il registro della batteria è in flash** perché attaccare il cavo **resetta** la scheda (`reset cause 0x00`): un registro in RAM muore
  esattamente quando lo si legge.
- **`0xED00` non è libero**: le impostazioni occupano tutto il settore `0xEC00`–`0xEDFF`. Il settore del registro è `0xD800`, preso dallo spazio
  del codice (il limite è sceso lì).

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

py tools\diag.py [--watch 1] [--radio] [--ports] [--blog] [--monitor]     # Windows
py tools\diag.py --daemon 60 oblio-log.csv                                # contatori in CSV, uno ogni 60 s
python3 tools/diag.py [...]                                               # Linux
```

Recupero, se qualcosa va storto: **Esc+1+2 mentre si inserisce il cavo** → bootloader ISP (nessun ICP: è l'unica via).
