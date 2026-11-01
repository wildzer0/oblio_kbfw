# Stato della sessione — dove riprendere

Ultimo commit: **`45732e0`** (branch `main`, CI verde). Questo file dice dove siamo, cosa aspetta una prova sulla tastiera e cosa
resta da decidere. Non sostituisce `CHANGELOG.md` (la storia) né `docs/firmware-report.md` (il quadro tecnico).

**Da dove riprendere, in una riga**: la tastiera ha un firmware completo e non provato — si flasha `build/oblio.hex`, si prova la lista qui
sotto, e le due cose che aspettano una misura sono la **scala della batteria** (registro a bordo, `diag.py --blog`) e la **causa del flicker**
(contatori, `diag.py --watch 1`).

## Dove siamo

Il firmware è **stabile e usabile**: USB e radio (2.4G e Bluetooth), backlight con effetti, Vial completo (keymap, tap dance, combo,
key overrides, tab Lighting), NKRO, Caps Word, Leader, mouse dall'encoder, switch Win/Mac letto da P0.5.

| | |
| --- | --- |
| Flash | **53069 / 56320** (3,3 KB liberi) |
| XDATA | **3934 / 4096** (162 B liberi) |
| Deposito keymap | **1709 / 2048** (339 B liberi) |
| Test host | **28 / 28** |
| Simulatore (ucsim) | **24 / 24** |
| Gradini | 1–3 costruiti, vettori interrupt ok (4) |

## Cosa è stato fatto in questa sessione

| Commit | Cosa |
| --- | --- |
| `1322478` | 6 layer (da 8) e colore per tasto Vial RGB |
| `db5ecf7` | **Tab Lighting** (serve `"lighting": "vialrgb"` nella definizione, non il byte 12), **NKRO** 112 bit, **Caps Word**, **Leader** |
| `ce85989` | **Flicker**: la pausa del salvataggio copre solo le cancellazioni (~100 → ~20 ms), ritiro del backlight 32 → 12 slot |
| `26816ae` | **Colore giusto**: la ruota è quella HSV standard (era rosso→blu→verde, l'arancione usciva viola) |
| `8088c75` | **Tinta + saturazione** in una parola da 16 bit: colore completo, **0 byte di memoria**, +333 B di flash |
| `38ed7a8` | **Batteria**: `0xFF` col cavo è una sentinella (non 100%), e lo stato letto **a batteria** ora si legge con `diag.py --radio` |
| `45732e0` | **Logger di batteria a bordo** (`core/blog.c`, 192 B di XDATA): il registro si scrive da solo mentre la tastiera è a batteria e si legge con `diag.py --blog`; `diag.py --daemon N file.csv` per lasciare un demone sul cavo |

## Da provare sulla tastiera (in quest'ordine)

1. **Flash** con Vial chiusa:
   `sinowisp write --platform sh68f90 --vendor_id 0x258a --product_id 0xdead --format ihex --isp_iface_num 0 -f build/oblio.hex`
2. **Tab Lighting**: c'è, gli effetti ci sono (Disable, Solid Color, Gradient Up/Down, Left/Right, Breathing, Cycle Out In), luminosità,
   velocità e **colore** devono agire subito. **Arancione deve uscire arancione**, e un colore pastello deve uscire pastello. Stacca e
   riattacca il cavo: le impostazioni restano.
3. **NKRO**: sette lettere tenute premute insieme in un editor → devono comparire tutte.
4. **Caps Word**: assegna `Caps Word` a un tasto, premi, scrivi `ciao-mondo` senza Shift, poi spazio.
5. **Leader**: assegna `Leader`, premi e poi `E S C` → deve uscire Esc (le sequenze stanno in `board/leader.c`).
6. **Batteria — ora c'è il registro a bordo.** Stacca il cavo e **usa la tastiera**: scrive da sola nel suo registro (un record quando la
   lettura cambia, e comunque ogni 5 minuti, con i minuti di accensione). Quando vuoi leggere, riattacca e lancia
   `py .\diag.py --blog` → tabella `minuti / byte batteria / livello / flags`. Annota la carica reale accanto alla riga: due o tre sessioni a
   cariche diverse e la scala è nota. Il registro vive in RAM: **spegnere la tastiera lo azzera**, quindi si legge dopo una scarica, non dopo
   un power cycle.
   In alternativa, **col cavo** (che ricarica, quindi la batteria non serve): `py .\diag.py --daemon 60 oblio-log.csv` scrive i contatori ogni
   minuto in CSV, e lo si lascia girare la notte per inseguire il flicker.
7. **Flicker**: quando la retroilluminazione si spegne e torna, `py .\diag.py --watch 1`. Se sale **`backlight back-offs`** è la fame del
   main loop; se resta fermo ed era appena cambiata un'impostazione, era la pausa del salvataggio. Annotare anche
   **`longest sector erase (us)`**.

> Cambiando predefiniti e definizione Vial, **la keymap salvata riparte dai default** e il PC può reinstallare il dispositivo: è normale.

## Domande aperte

1. **Scala della batteria.** Il byte è quello giusto (byte 4 dello stato radio), ma i valori visti non sono una carica che scende:
   `0xFF` col cavo (sentinella, ora ignorata), `0x03` in uso, `0x01` appena accesa. Il registro a bordo raccoglie i punti che servono
   (`diag.py --blog`, con la carica reale annotata accanto). Se la scala è un'altra si cambiano due punti: `RADIO_STATUS_BATTERY_AT` in
   `core/radio.h` e il calcolo in `board/battery.c`.
2. **Causa del flicker**: la correzione riduce il buio, ma la conferma arriva dai contatori (punto 7 sopra).
3. **Sequenze del Leader**: oggi ci sono due esempi (Esc, mute). Le sequenze vere le decide l'utente: si scrivono in `board/leader.c`.
4. **Saturazione sugli effetti animati** (arcobaleni che sbiadiscono verso il bianco con lo slider): oggi vale solo per la tinta unita,
   costa ~30 B perché la conversione esiste già.
5. **Bluetooth con il PC**: il telefono vede `OBLIO`, il PC no (sospetto un legame vecchio su Windows, non il firmware).

## Cosa non è ancora stato provato

Tutto quello che è entrato dopo l'ultima prova dell'utente: tab Lighting con colore completo, NKRO, Caps Word, Leader, la pausa del flash
accorciata, lo switch Win/Mac nella polarità corretta, il registro della batteria. Il resto (digitazione, backlight, radio, Vial, encoder,
mouse) è già stato visto funzionare su build precedenti.

## Fatti verificati sulla tastiera

- **Switch Win/Mac = P0.5**, e `OS_SWITCH_IS_MAC 1` in `board/os_switch.c` è la polarità giusta (alto = Mac). Windows usa i layer 0/1,
  Mac i layer 5/4 (base con Alt↔GUI scambiati e Fn suo), tutti e quattro editabili da Vial.
- **P7.0 non è lo switch**: è letto come sesta riga dal firmware stock, ma sulla tastiera resta alto e non cambia con le strobe
  (`diag.py --ports` con lo switch nelle due posizioni).
- **Byte 4 dello stato radio**: `0xFF` col cavo, `0x03` a batteria, `0x01` all'accensione.
- **Il dongle 2.4G non ha canale diagnostico**: si presenta al PC come tastiera, senza l'interfaccia raw HID con cui parlano Vial e `diag.py`.
  Per questo il registro della batteria sta **nella tastiera** e non in un demone sul PC.
- Il byte dei **LED host** è confermato dal dump stock (`0x06B6`).
- **NKRO**: il buffer hardware di EP1 è di 16 byte, quindi il report è di 112 bit (non 120 come QMK) contando l'ID.

## Comandi

```
nix develop --command make test              # 28 test host + definizione Vial
nix develop --command make fw size           # build/oblio.hex, flash e XDATA contro il budget
nix develop --command make check-interrupts  # i quattro vettori, nessun overlay statico
nix develop --command make sim               # ~50 s, il firmware vero nel simulatore (24 test)
nix develop --command make stages            # build a gradini 1-3
py tools\diag.py [--watch 1] [--radio] [--ports] [--blog] [--monitor]         # Windows
py tools\diag.py --daemon 60 oblio-log.csv      # contatori in CSV, uno ogni 60 s
python3 tools/diag.py [...]                                                   # Linux
```

Recupero, se qualcosa va storto: **Esc+1+2 mentre si inserisce il cavo** → bootloader ISP (nessun ICP, è l'unica via).
