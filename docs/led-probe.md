# Sonda dei LED: come si usa

> **Nota (rimossa dalla build):** il layout di diagnosi descritto qui è stato tolto dal repository; resta nella storia git (commit `bd19894` e precedenti). Esiste una sola build di rilascio.


Serve a scoprire **quali pin accendono quali LED** della B68 (RGB sotto i tasti). Non cambia nulla in memoria e non richiede di aprire la tastiera.

## Cosa fa

Il firmware `yunzii-b68_ledprobe_smk.hex` è il layout normale della B68 più un test che parte da solo **5 secondi dopo l'accensione** (non toccare niente). Per ~85 secondi **la tastiera non
scrive tasti** (il test ferma la scansione) e accende **un solo canale LED per volta** per 1,5 s, con una luminosità ridotta (circa 25 %, pensata per non stressare nessun LED). Prima di ogni passo
**scrive un'etichetta** in un editor di testo aperto: `A00 `, `A01 `, … Alla fine la scansione riparte e la tastiera torna normale.

Ipotesi dallo stock: i 18 pin `P1.0–5`, `P2.0–5`, `P3.0–5` sono le "colonne" dei LED (sorgenti) e le 16 linee di scansione dei tasti sono i pozzi. Un LED si accende se un pin colonna è alto e una linea di scansione è bassa.

| Etichetta | Cosa viene acceso | Cosa mi serve osservare |
| --- | --- | --- |
| `A00`…`A15` | colonna LED 0 (`P1.0`) alta, linea di scansione 0…15 bassa | **quale tasto si accende e di che colore** |
| `B00`…`B17` | linea di scansione 0 bassa, colonna LED 0…17 alta | quale tasto si accende e di che colore |
| `C00`…`C17` | come B ma con la linea di scansione 8 | idem |

## Procedura

1. Flasha `build-b68/yunzii-b68_ledprobe_smk.hex` (stessi comandi di sempre; la tastiera è quella di prima con in più il test).
2. Apri un editor di testo e dagli il focus. Metti il telefono in modo da **filmare la tastiera** oppure tieni carta e penna (non puoi scrivere sulla tastiera durante il test).
3. Collega l'USB e **non toccare niente per 5 s**. Il test parte.
4. Per ogni etichetta che compare nell'editor, annota **quale tasto vedi acceso e di che colore** (rosso/verde/blu/bianco/altro), o "nessuno". Un solo LED per volta, quindi è facile.
   Se il LED è accesso ma debole, scrivilo comunque.
5. Quando le etichette finiscono la tastiera torna normale. Mandami il testo dell'editor e le tue note, per esempio:

```
A00 tasto Esc rosso
A01 tasto Q rosso      (o "Tab", come è scritto sul tasto)
A02 nessuno
…
B00 Esc rosso
B01 Esc verde
B02 Esc blu
B03 1 rosso
…
```

## Come leggo i risultati

* **A**: mostra quale linea di scansione corrisponde a quale colonna di tasti (A03 → tasti della colonna 3). Se i tasti accesi in A sono sempre della stessa colonna, l'ipotesi "scansione = pozzo" è giusta.
* **B** e **C**: mostrano come le 18 colonne LED si ripartiscono in righe e colori (per esempio cinque righe × RGB = 15 canali, più 3 per il LED di stato accanto al Caps).
* Se **nessun LED si accende**, la polarità è opposta (colonne come pozzi e scansione come sorgenti): lo scrivi e preparo una seconda sonda.

## Sicurezza

Un solo canale per volta, a luminosità ridotta. Se noti un tasto che scalda, odore o lampeggi anomali, scollega l'USB e dimmelo.

---

# Seconda sonda (`yunzii-b68_ledprobe2_smk.hex`)

La prima sonda ha dato la mappa dei LED dei tasti (vedi [keyboards/yunzii-b68.md](keyboards/yunzii-b68.md)). Restano due cose: **il LED di stato** accanto al Caps e **come la larghezza dell'impulso PWM diventa
luminosità**. Stessa procedura (flasha, editor aperto, 5 s di mani lontane, ~80 s di test), ma le etichette sono queste:

| Etichetta | Cosa viene acceso | Cosa annotare |
| --- | --- | --- |
| `D00`…`D03` | le uscite `P4.1`, `P7.4`, `P7.5`, `P7.6` alte una alla volta (colonne a riposo) | si accende il LED di stato? di che colore? |
| `D04`…`D06` | colonne `P1.0`, `P1.1`, `P1.2` alte una alla volta (le quattro uscite sopra a riposo, basse) | LED di stato acceso? di che colore? |
| `E00`…`E15` | colonna `P1.1` alta, linea di scansione 0…15 bassa | si accende un LED (anche il LED di stato)? |
| `E16`…`E31` | colonna `P1.2` alta, linea di scansione 0…15 bassa | idem |
| `F00`…`F05` | **PWM hardware** sul rosso del tasto Esc, impulso di larghezza 0, 30, 120, 300, 600, 1100 (DUTY2 = DUTY1 + larghezza) | luminosità di Esc: spento / fioco / medio / forte / pieno |
| `F06`…`F11` | lo stesso con i registri scambiati (DUTY1 = DUTY2 + larghezza) | luminosità di Esc |

Per le righe F ti basta dire, per ciascuna, se Esc è spento, debole, medio o forte, e se è rosso: dal confronto tra F00–F05 e F06–F11 ricavo la polarità dell'uscita PWM e la curva.
Se in `D` o `E` si accende qualcosa di non previsto (per esempio un LED accanto al Caps), scrivilo con l'etichetta.
