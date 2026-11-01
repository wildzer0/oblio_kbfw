# Guida: catturare la comunicazione MCU ↔ radio BK3632 (quando potrai aprire la tastiera)

> **Non serve più (2026-11-01):** il protocollo è stato ricavato dal disassemblaggio dello stock e dalla sonda software, e il driver funziona (vedi [keyboards/yunzii-b68-radio.md](keyboards/yunzii-b68-radio.md)).
> Resta valida solo se si vuole osservare cosa fa la radio con comandi ancora ignoti (04, 05, 08, 0B–0E): due fili seriali a 260870 baud (`P5.5`/`P5.6`) più `P0.2` e `P4.7`.

Serve per un solo scopo: vedere i byte che il firmware originale scambia con la radio, così scrivo il driver wireless (2.4G e Bluetooth) senza
indovinare. Si fa **una volta**, con il firmware originale (o con il nostro, non importa: la cattura è sulla radio).

## Cosa si cerca (aggiornato 2026-11-01)

La sonda software ha già confermato il collegamento: **una seriale 8N1 a 260870 baud** (non SPI), con due linee di contorno. Quindi basta registrare **due fili** (più massa),
con un analizzatore logico qualunque a ≥ 2 MS/s e il suo decodificatore UART, oppure con un adattatore USB-seriale a 3,3 V (l'FTDI accetta 260870 baud; un CP2102 a 256000 è entro il 2 %).

| Segnale | Pin MCU | Ruolo |
| --- | --- | --- |
| TX dell'MCU | `P5.5` | dati MCU → radio |
| RX dell'MCU | `P5.6` | dati radio → MCU |
| Selezione | `P0.2` | l'MCU la abbassa durante un frame (utile, non indispensabile) |
| Attenzione | `P4.7` | ingresso MCU: alta = la radio è pronta a ricevere |

I punti `J3` della scheda (CSN, MISO, MOSI, CLK) sono per programmare la radio e **non** servono. Per agganciarsi: i due piedini `P5.5`/`P5.6` del BYK916 (`U1`)
o i piedini corrispondenti del chip radio `U5` (continuità con il multimetro per trovarli). Le catture A–F qui sotto restano valide; quelle che servono di più sono
**C (accoppiamento)**, **D (tasti in wireless)** e il cambio di modalità **B**, perché mostrano comando di modo, accoppiamento e report di tastiera (comandi `04`, `02`, `03`).

## Cosa serve

* Analizzatore logico a 8 canali o più, almeno **10 MS/s** (meglio 24 MS/s), con la sua applicazione (Saleae/PulseView o simile).
* Ganci/clip sottili o fili con punte da saldatore; un multimetro con la prova di continuità.
* Cacciaviti piccoli, plettro per aprire la plastica, fotocamera del telefono.

## Passi

1. **Scollega tutto.** Cavo USB fuori, interruttore d'alimentazione su OFF. Lavora su un tappetino antistatico o tocca un oggetto metallico a terra prima di toccare la scheda.
2. **Apri la tastiera.** Togli le viti sotto (spesso sotto i piedini di gomma), poi apri la scocca con il plettro senza forzare i connettori. **Prima di muovere la scheda,
   scollega la batteria** dal suo connettore (se c'è un connettore) o tienila con il nastro isolante lontana da punte metalliche.
3. **Fotografa, non toccare ancora nulla.** Una foto nitida di **entrambi i lati** della scheda, una del **modulo radio** (il piccolo circuito con il chip BK3632, di solito con piazzole
   sul bordo o un connettore a pettine) e una dell'**MCU** (marcatura `BYK916`) in modo che si leggano i numeri. Mandameli in `backups/foto/`.
4. **Trova i fili del modulo radio.** Con il multimetro in continuità: un puntale su un contatto del modulo radio, l'altro sui pin dell'MCU, finché trovi quali contatti arrivano all'MCU.
   Ti aspetti 4–6 fili di segnale più alimentazione e massa. Annota per ciascuno **il numero del pin dell'MCU** (conta dal punto di riferimento, di solito un puntino sul chip) e il contatto sul modulo.
   Scrivi tutto in una tabella a mano, come:

   | contatto modulo | pin MCU | note |
   | --- | --- | --- |
   | 1 | 17 | |

5. **Collega l'analizzatore.** La **massa** dell'analizzatore alla massa della scheda (un piedino di un connettore, il guscio della USB o una piazzola GND). Un canale per filo di segnale,
   **senza collegare nient'altro** (l'alimentazione sale dalla USB). Se ti manca il modo di agganciarti ai contatti, saldare un filo sottile (30 AWG) a una piazzola va bene, ma **mai** a un pin dell'MCU.
6. **Impostazioni di cattura:** frequenza massima (≥ 10 MS/s), tensione di soglia 1,65 V (la logica è a 3,3 V), trigger sul fronte di discesa del filo di selezione (`P0.2`) se lo hai
   riconosciuto; se no, avvia la cattura a mano **subito prima** di ogni azione sotto.
7. **Cattura queste azioni** (una cattura per ciascuna, durata 10–20 s), con la tastiera collegata via USB e l'interruttore di modalità sulla radio che indico:
   * **A.** Accensione con l'interruttore su **2.4G** e il ricevitore USB inserito nel PC (cattura dall'attimo in cui colleghi l'USB).
   * **B.** Dalla modalità 2.4G passa a **Bluetooth 1** e aspetta 10 s.
   * **C.** In Bluetooth: tieni premuto il tasto di **accoppiamento** (di solito Fn + 1/2/3) finché lampeggia, e accoppia con il PC.
   * **D.** Dopo l'accoppiamento: scrivi qualche lettera, premi un tasto multimediale (volume), tieni premuto un tasto per 2 s.
   * **E.** Lascia la tastiera ferma 30 s (lo stato di riposo/sonno), poi premi un tasto per svegliarla.
   * **F.** (Se esiste sul tuo modello) indicatore della batteria: Fn + il tasto che lo mostra.
8. **Salva ogni cattura** (formato CSV/binario dell'applicazione, o l'esportazione "digital" completa) in `backups/radio/` con un nome tipo `A-2.4G-accensione.sal`. Scrivi in un file di testo
   `backups/radio/note.txt` quale canale è collegato a quale filo (anche se non sei sicuro: dimmelo).
9. **Richiudi.** Ricollega la batteria, rimonta la scheda, chiudi la scocca. Verifica che la tastiera si accenda e scriva.

Cosa ricavo da tutto questo: i fili giusti, la velocità del clock, la forma dei frame (intestazione, lunghezza, checksum), l'elenco dei comandi, e come la radio segnala un tasto o un cambio di collegamento.
Con questo scrivo il driver e la prima prova su hardware è un firmware che si limita a collegarsi e a mandare i tasti.

## Se non puoi usare l'analizzatore

Basta anche **un oscilloscopio a due canali** per la parte facile (verificare che `P0.2` scenda quando premi un tasto in modalità wireless e vedere il clock), ma non per decodificare i frame.
In alternativa ho preparato una sonda software (vedi `docs/radio-software-probe.md`) che non richiede di aprire la tastiera ma scopre meno cose.
