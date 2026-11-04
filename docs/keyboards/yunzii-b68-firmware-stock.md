# Yunzii B68 — analisi del firmware stock (M1.6, prima passata)

> Fonte: `backups/dump/r1.bin` (64 KiB, MD5 `4aea8bec…`). Le disassemblazioni **non** sono nel repo (firmware
> proprietario): qui ci sono solo fatti e indirizzi. Stato: **prima passata**, i punti marcati *TBD* sono aperti.

## Come riprodurre

Ghidra 12 (`~/ghidra`) in headless con JDK 21 (`nix build nixpkgs#jdk21`; Ghidra rifiuta JDK 17), lingua `8051:BE:16:default`,
`BinaryLoader` base 0. Il loader non crea punti d'ingresso: lo script deve fare `disassemble()` + `createFunction()` su
`0x0000`, sui vettori `0x03 + 8n` e su `0xF000`, poi `analyzeAll()` (≈ 8600 istruzioni, 134 funzioni). I nomi dei SFR si
ricavano da `src/sino51lib/sh68f90/sh68f90.h` (`SFR(...)`/`SFRX(...)`). REA non supporta l'8051 (solo 6502/x86/arm/m68k).

## Struttura generale

* Compilato con **Keil C51** (non SDCC): startup che azzera IRAM (`0x9A26`) e XRAM (4 KiB), poi copia/azzera dati da una
  tabella di init a `0x9F42`; `SP=0x7D` (lo stack parte più in alto che in SMK).
* Mappa: reset `0x0000: LJMP 0x9A26`; app in `0x0000–0xEFFF`; bootloader `0xF000–0xFFFF`.
* Vettori (offset `0x03+8n`): `0x03→0x0181`, `0x0B→0xEDB6`, `0x13→0xEDBC`, `0x1B→0xEDC2`, `0x3B→0xAB30`, `0x43→0x0F3A`,
  `0x4B→0x30DC`, `0x53→0xEC41`, `0x63→0x8C5B`, `0x6B→0xA43C`; gli altri sono vuoti/dati. Corrispondenza con le
  sorgenti di interrupt del SH68F90 (`_INT_TIMER2`, `_INT_USB`, …): *TBD* assegnazione precisa.
* Orologio: `PLLCON=0x03`, `CLKCON=0x0C` (PLL acceso, sistema a 24 MHz, come SMK); sequenza di spegnimento
  `CLKCON&=~…; PLLCON&=~3` in più punti (sleep).

## GPIO a riposo (funzione a `0xA627`)

| Porta | `PxCR` (1=uscita) | `PxPCR` (pull-up) | valore `Px` iniziale |
| --- | --- | --- | --- |
| P0 | `0x04` (P0.2) | `0xF8` | `0x04` |
| P1 | `0x3F` (P1.0–5) | `0x3F` | `0x00` |
| P2 | `0x3F` (P2.0–5) | `0x3F` | `0x00` |
| P3 | `0x3F` (P3.0–5) | `0x3F` | `0x00` |
| P4 | `0x2F` (P4.0–3, P4.5) | `0x6F` | `0x2D` |
| P5 | `0x87` (P5.0–2, P5.7) | `0x9F` | `0x87` |
| P6 | `0xFF` | `0xFF` | `0xFF` |
| P7 | `0x70` (P7.4–6) | `0xFF` | `0x00` |

* Pilotaggio: `DRVCON` sblocca e azzera `P1DRV`, `P2DRV`, `P3DRV`, `P5DRV` (come SMK, che imposta 25 mA: *TBD* se lo
  stock usa il default).
* **Nota:** il SH68F90A esposto qui ha anche **P6 e P7** (SMK/NuPhy usano solo P0–P5). Se il package della B68 ha tutti
  quei pin, la matrice può usare porte che nelle altre tastiere non si usano.
* Un'altra funzione (`0x9C9A–0x9D0F`) imposta dopo il boot un secondo insieme di pin (P0.5–7, P4.0/2/3/5/6, P5.0–2/7,
  P6.3–7, P7.4) in uscita e abilita pull-up su P5.3/P5.4: probabile configurazione di **sleep/wake** o dei pin radio.
  *TBD*.

## Altri dati nella flash

* `0xB15A`: stringhe `"B68 3.0"` e `"B68 5.0"` → **nomi Bluetooth** annunciati (BT 3.0/5.0 → BK3632 in modalità
  Classic e BLE). Utile per il comando "set BT name" del BK3632.
* `0x5A59`: descrittore dispositivo USB (`258A:010C`, bcdDevice `0x2002`); descrittori di configurazione subito prima.
* `0xC480–0xC4FF`: due tabelle simmetriche (0→255→0), tipicamente **curve di luminosità/respirazione dei LED**.
* `0xC500–0xC5FF`: tabelle di mappatura a passi regolari: *TBD* (indici LED?).
* `0xC600–0xC67F`: **blocco impostazioni** con marker `5A A5` a `0xC67E`; contiene i 2 byte che cambiano fra le
  letture (`0xC60A`, `0xC65A`): stato corrente (modalità/effetto LED). È l'equivalente delle impostazioni NVM di SMK.
* **Keymap:** *non* trovata come tabella di codici HID contigua (cercati i pattern QWERTY/ASDF in chiaro). Probabilmente
  codificata in un formato indicizzato; *TBD*, richiede di seguire il codice che costruisce il report tastiera.


## Matrice (ricavata dal codice, da confermare in banco — M1.2)

La ISR del Timer2 (vettore `0x43` → `0x0F3A`) è una macchina a **20 slot** (tabella di `LJMP` a `0x0F74`, un contatore in
XRAM `0x0305`). Ogni slot rilascia la linea precedente (`SETB`) e abbassa la successiva (`CLR`), poi legge le uscite.

* **16 linee strobe, attive basse**, nell'ordine di slot: `P6.0 P6.1 P6.2 P6.3 P6.4 P6.5 P6.6 P6.7`, `P5.0 P5.1 P5.2 P5.7`,
  `P4.0 P4.2 P4.3 P4.5` (gli ultimi 4 slot sono vuoti: servono a completare il periodo di 20 slot, con ogni probabilità
  condiviso con la PWM dei LED, che nello stesso gestore viene commutata: `PWM00CON`).
* **6 linee di lettura** con pull-up: `P5.3`, `P5.4`, `P7.0`, `P7.1`, `P7.2`, `P7.3` (la ISR compone un byte
  `0xC0 | (P5<<1 & 0x30) | (P7 & 0x0F)` e lo salva in XRAM `0x098E + slot`).
* Coerenza con la funzione GPIO a `0xA627`: P6 tutta a uscita, P4/P5 uscite a `0x2F`/`0x87` per le strobe, `P5.3/P5.4` e
  `P7.0–3` ingressi con pull-up. Quindi: **16 colonne** (strobe) e **5 righe fisiche** (`P7.1–3`, `P5.3–4`, le stesse della Air60); `P7.0` è un sesto ingresso letto dallo stock, di funzione ignota, da non trattare come riga finché la sonda non lo spiega.
* La mappatura *tasto → (colonna, riga)* non si ricava da questi dati: la keymap non è in chiaro (vedi sopra).

## Ingresso in ISP dall'applicazione

All'indirizzo `0xED76` l'app stock esegue `CLR EA; MOV B,#0xA5; MOV A,#0x5A; LJMP 0xFF00`: **identico** a `isp_jump()` di
SMK. Nel bootloader stock lo stub a `0xFF00` confronta `A==0x5A` e `B==0xA5`. Quindi l'ingresso ISP di SMK
funziona sulla B68 senza modifiche. L'app stock non ha un reset vector che passa per il bootloader (`LJMP 0x9A26` diretto):
dopo `sinowisp write` il reset vector è quello dell'immagine scritta.


## Radio BK3632

L'analisi completa (seriale a 260870 baud su `P5.5`/`P5.6`, `P0.2` selezione, `P4.7` attenzione, frame, comandi, controllo di flusso, priorità degli interrupt, selettore di modalità su `P0.4`/`P0.3`,
indirizzi nello stock) è in [yunzii-b68-radio.md](yunzii-b68-radio.md). Qui resta solo la nota di metodo: la prima lettura del dump ("modo 0", poi "SPI") era sbagliata; l'inizializzazione a `0xAFA0` scrive
`SCON = 0x50`, `PCON |= 0x40`, `SBRT = 0x7FFB`, `SFINE = 0x0C` (modo 1, 8N1, 260870 baud), e le priorità a `0xEC00` mettono la seriale al livello 3.

## LED (analisi statica)

* Il firmware pilota **tre banchi PWM da 6 canali**: `PWM0x`, `PWM1x`, `PWM2x`, cioè 18 canali, uno per pin di `P1`, `P2`, `P3` (le tre porte che a riposo sono tutte uscite a 0).
  Sono le **colonne** della matrice dei LED (18 colonne).
* **Ipotesi più forte:** il Timer2 alterna scansione dei tasti e sotto-frame dei LED sugli **stessi 20 slot**: per ogni slot l'ISR carica da una tabella (`0x059C + 0x24 × slot`, 36 byte = 18 duty da 16 bit) i 18 canali PWM, mentre la linea di scansione dello slot è bassa. Quindi le **16 linee di scansione dei tasti (`P6.0–7`, `P5.0/1/2/7`, `P4.0/2/3/5`) sono anche i pozzi dei LED**, e i 18 canali PWM (`P1`, `P2`, `P3`) le sorgenti: 5 righe di tasti × RGB = 15 canali + 3 per il LED di stato. Da confermare con la sonda (`docs/led-probe.md`).
* Le **righe** (pozzi di corrente) non sono ancora identificate con certezza. Candidati: uscite che lo stock imposta ma non usa per la matrice dei tasti: `P4.1`, `P7.4`, `P7.5`, `P7.6`, `P0.2`
  (quest'ultima è la linea della radio). Il tick del Timer2 alterna una scansione dei tasti (`~320 µs`) e un sotto-frame dei LED.
* Non si sa ancora se la retroilluminazione è monocromatica o RGB, né se esistono LED di stato separati: dipende dalla tastiera, non dal codice.

## Cosa manca per chiudere M1.6

1. ~~Funzione di scansione matrice~~ → fatto, vedi sopra (resta il ritardo di settling e la mappa tasto→(col,riga)).
2. Keymap e layer (formato, dimensione, dove si salva la personalizzazione fatta dal software del produttore).
3. Protocollo verso BK3632 (linee, frame, comandi): confronto con `rf_controller.c`.
4. Driver LED (PWM, pin, frequenza) e sequenza di sleep/wake (INT4).

Le voci 1–3 si possono chiudere più in fretta con una **cattura con analizzatore logico** o misure sui pin
(M1.2/M1.5); la voce 4 è analisi statica. Il Nano non serve per questo.

## Come lo stock scrive in flash (2026-11-02, riletto con `tools/dis51.py`)

Domanda: perché con il firmware originale il backlight non «sfarfalla» quando si salva? Risposta dal dump (`backups/dump/r1.bin`): **lo stock fa le stesse cose di Oblio**, con un'organizzazione diversa.

| Cosa | Dove nel dump | Com'è |
| --- | --- | --- |
| Primitiva di scrittura IAP | `0x9700–0x9786` | `R7` = operazione (`0xE6` cancella, `0x6E` programma), `XPAGE` = settore × 2; **rifiuta di partire se `EA` è acceso** (`JB AFh`): serve il chiamante con gli interrupt spenti. Chiavi `05 0A 09 06` come in Oblio |
| Interblocco | `0x9700`, `0xAD6C`, `0xEC55` | solo i settori **0x62–0x75** (`0xC400–0xEBFF`) e solo se coincidono con un «gettone» in `XDATA[0x0300]`, messo dal chiamante (per le impostazioni vale `0x63`) |
| Cancellazione di un settore | `0xAD6C` | `CLR EA` → `XPAGE = settore×2` → op `0xE6`; **`EA` non viene riacceso qui** |
| Salvataggio delle impostazioni | `0xA4A5`, chiamata dal ciclo principale a `0x8F9A` | copia 128 byte (`XDATA[0x0307…]` → buffer `0x09B5…`), `EA` spento, **`0xECEA`** (spegne il PWM: `PWM0CON &= ~0x40`; `P4 = 0xFF` = colonne spente; `P3 \|= 0x2D`), **cancella il settore `0xC600`**, poi `0xB1DA` (riaccende il PWM: `0x89` ai tre canali di testa e bit `0x40`) e riabilita `EA` |
| Scrittura dei 128 byte | coda del Timer 2 a `0x1071–0x10C1`, `0xEC55`, `0x96D7` | **un byte per interrupt del timer**, con contatore in `XDATA[0x72/0x73]` fino a `0x80`; il flag `49h` («scrittura in corso») impedisce un nuovo salvataggio |
| Quando salva | flag `50h` («impostazioni cambiate»), messo da 23 punti del codice | `0x8F94`: `se 50h e non 49h → A4A5`. **Subito, senza tempo di quiete** |
| Dove | settore `0xC600`, marcatore `5A A5` a `0xC67E` | **un blocco da 128 B in un settore da 512 B, senza log**: ogni salvataggio cancella |

**Cosa ne segue**
1. Lo stock **spegne il backlight durante la cancellazione**, esattamente come fa Oblio (PWM spento e colonne alte, poi PWM riacceso): anche lì c'è un buio, di ~un settore (quanto dura una cancellazione, ~32 ms nelle nostre misure).
2. Salva **subito** a ogni cambio, ma **un solo settore** (non quattro) e **poi scrive i byte a goccia** nell'interrupt, quindi il ciclo principale non si blocca. Il buio coincide con la pressione del tasto che cambia l'effetto, e la variazione stessa lo mimetizza.
3. Non esistono né un log né una pausa di quiete: Oblio è migliore per frequenza (log, un erase ogni 36 salvataggi) e per l'impatto sulla flash.
4. Una differenza che **sì** ci riguardava: Oblio scrive la keymap (1,7 KB) in un colpo dal ciclo principale. Per tutto quel tempo il ciclo non gira e il rilevatore di «main affamato» (3 scansioni senza passaggi) metteva da parte il backlight, **allungando il buio oltre l'erase**. Corretto (`tick_alive()` in `store_hook_program`).



## Cavo e carica nello stock (2026-11-04, riletto con `tools/dis51.py` su `r1.bin`)

Domanda: come sa lo stock se c'è il cavo e se la batteria è in carica, e cosa mostra? Risposta dal dump (indirizzi nel dump, bit di flag di Keil in `20h–2Fh`):

| Cosa | Dove | Com'è |
| --- | --- | --- |
| Sondaggio dei due pin | `0xA058`, chiamata dal ciclo principale a `0x9B4D` | **P4.4** (bit `B4h`): alto per ≥ 5 campioni di fila → flag `60h` = 1 («cavo»), basso per ≥ 5 → `60h` = 0 (e il valore mostrato `19h` torna a `0xFF`). **P7.7** (bit `FFh`): basso per ≥ 20 campioni → flag `5Bh` = 1 («in carica»), alto per ≥ 200 → `5Bh` = 0. Ogni cambio mette `4Ah` (evento) |
| Direzione dei pin | `0xA627` (GPIO a riposo) | P4.4 è ingresso **senza pull-up** (`P4PCR = 0x6F`: l'hardware lo pilota); P7.7 è ingresso con pull-up (`P7PCR = 0xFF`; nello sleep va a uscita bassa, `0x7EAB`). Coerente con una linea «cavo/VBUS» e con l'uscita aperta di stato di un caricatore (come il CHRG dei TP4056) |
| Uscita `P7.6` | `0xA09E`/`0xA0A2` | segue il cavo: **bassa col cavo, alta senza**; bassa nello sleep (`0x7EB0`). Funzione ignota (non è il LED vicino al Caps) |
| Percentuale | `0x7CF0` (gauge), `0x1EBC–0x1F7B` (soglie) | `0151 = (lettura − 715) × 10 / 22` (0–100). Col cavo (`60h`) il valore mostrato `19h` *sale* verso 100 di 1 punto alla volta (ritmo da una tabella a `0xB16A`: 20 giri per punto vicino alla meta, meno lontano) e **si ferma a 99 finché P7.7 dice «in carica»** (`5Bh`), poi 100. Senza cavo scende lentamente, con salto se la differenza è ≥ 20/30. Cioè: **lo stock non legge la carica col cavo (la lettura satura), la ricostruisce**. Isteresi: il blocco di batteria scarica (`5Ch`) si toglie dopo 100 letture sopra 912 |
| Cosa mostra sulla tastiera | `0x1E85`, `0x1F3C` | **tasto Esc** (canali del tasto 0,0 nella tabella dei duty `0x05A2…`): **rosso fisso** col cavo in carica (`60h` e `5Bh`); **lampeggiante** a batteria sotto la lettura 800 (`0x0320`, contatore `0397`). A carica finita niente. Nessun altro LED di stato |
| Cosa manda al PC | `0x41EF`, `0x44E8`, `0x55FE`, `0x6687` (pacchetti per il software del produttore) | la percentuale (`0C2B`) e un byte di flag: `0x10` col cavo, `1` quando la carica è finita |
| Sleep | `0x1EBC` e dintorni | sotto la lettura `0x02F0` (752) per 200 giri: `5Ch`, `39h`, poi lo sleep (come l'avviso di Oblio). Con il cavo non si addormenta |

**Il LED di stato vicino al Caps.** Nessun pin tra `P7.4–6` e `P4.1` lo pilota (usati solo per lo sleep e per `P7.6`, che segue il cavo). Osservazione dell'utente su Oblio: il LED si accende e **segue lo stato dei LED dei tasti** (cioè fa parte del backlight, non è un indicatore indipendente di carica). Quindi non serve per mostrare la carica: lo stock la mostra con il tasto Esc.

**Il LED vicino al Caps è sullo stesso incrocio del rosso del tasto C (2026-11-04, misurato a occhio dall'utente + dump).** Lo stock lo accende col Caps Lock scrivendo un solo duty, a `0x0626` = tabella dei duty, colonna 3, canale 15 = rosso della riga 3 = rosso del tasto C. Prova sulla tastiera: a colore pieno il LED si accende **solo con il rosso** (non con verde né blu) e il tasto C si accende in tutti i colori. Conclusione (inferenza, non verificata col multimetro): il LED di stato è in parallelo al LED rosso del C, sulla stessa rete. **Non si può pilotare separatamente**: o segue il rosso del C (come nello stock), o si spegne il rosso del C per sempre. Le due opzioni (A: come lo stock, il Caps Lock forza il rosso del C; B: rosso del C sempre spento) non sono implementate: l'utente ha deciso di lasciare com'è.

**Stato (2026-11-04):** misurato dall'utente con `diag.py` sul cavo: **P4.4 alto con il cavo** e **P7.7 basso mentre carica** (il gauge è azzurro; verde a carica finita, vista in precedenza). Non ancora misurati: P4.4 a batteria (atteso basso) e P7.7 a carica finita direttamente dai pin. Prima era solo lettura del codice. Oblio le legge (`board/charger_hw.c`) e le espone in `diag.py` per confermarle in tre stati: a batteria, cavo in carica, cavo a carica finita.
