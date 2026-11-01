# Sonda software per la radio

> Il protocollo e il driver sono descritti in [keyboards/yunzii-b68-radio.md](keyboards/yunzii-b68-radio.md). Questo file spiega solo come usare il layout `radioprobe` (utile per provare i comandi ancora ignoti).
> La versione attuale della sonda (v12): richieste di stato, comando di modo secondo il selettore, accoppiamento Bluetooth e due report di prova (`z` solo nella bitmap, `x` solo nella lista).


> **Layout `radioprobe` (2026-11-01, versione 2):** `meson compile -C build yunzii-b68_radioprobe_smk.hex`. È la tastiera predefinita più un test che parte
> 6 s dopo l'accensione e **scrive i risultati nel campo di testo con il focus** (aprire un editor prima e non toccare i tasti). Usa la seriale come lo stock
> (modo 1, 260870 baud, `P5.5`/`P5.6`, `P0.2` giù durante il frame, `P4.7` letto). Fase `S`: 3 s in ascolto. Poi dodici scambi `E0…E11` (nessun frame, "set nome
> Bluetooth" dello stock, stato e wake NuPhy, 0x0C e 0x0A in stile stock; i primi sei con `P0.2` giù, gli altri senza) con i byte ricevuti. Nessun byte = la radio non
> risponde in quella modalità: provare con l'interruttore di modalità in ogni posizione. La versione 1 (SPI a bit-bang) non vedeva nulla: il collegamento è una seriale. La versione 2 (prima prova) dava un flusso costante di `FF` identico in tutte le posizioni dell'interruttore: azzerava i flag di errore di `SCON` senza il bit `SSTAT` di `PCON` (0x40) che lo stock imposta, quindi cancellava i bit del modo e lasciava l'EUART in modo 0. Corretto nella versione 3. La versione 4 manda i frame veri dello stock (comandi `06`, `F0`, `F1` da 6 byte e il nome Bluetooth da 32 byte) e mostra fino a 16 byte di risposta. La versione 8 (attuale) dopo il comando di modo manda tre volte la pressione e il rilascio del tasto "z" nel formato dei report dello stock (comando 02): se la radio è collegata a un host (PC con dongle o telefono accoppiato) compaiono delle `z`. La versione 7 manda il comando di modo (`01 01 00 <slot> 00`) secondo la posizione del selettore (2.4G / Bluetooth; in USB non manda nulla) e poi dodici richieste di stato a 300 ms l'una dall'altra: prova con ognuna delle due posizioni wireless e guarda sul telefono (nome `B68 …` tra i dispositivi Bluetooth) o sul PC con il dongle se la tastiera si collega. La versione 6 aggiunge in coda la ricerca del pin del selettore di modalità: `W ` (4 s mani lontane), poi `G ` e si sposta il selettore: ogni cambio di un pin di ingresso viene scritto come `pPB=L` (P4.7 a 1 = `p47=1`). La versione 5 manda in sequenza richiesta di stato (06), comando 04 con 2, 06, 04 con 3, 06, nome, 06, F0, 06, F1, 06 per vedere cosa cambia nello stato dopo ogni comando.
>
> Il testo che segue descrive la vecchia sonda (rimossa), tenuto come riferimento.

## (vecchia) Sonda software per la radio (senza aprire la tastiera)

> **Nota (rimossa dalla build):** il layout di diagnosi descritto qui è stato tolto dal repository; resta nella storia git (commit `bd19894` e precedenti). Esiste una sola build di rilascio.


Cosa si può sapere **senza** analizzatore: quali ingressi si muovono da soli, cioè quali pin dell'MCU sono collegati a qualcosa che parla (la radio). Non permette di decodificare i frame.

Il layout `probe` (build `yunzii-b68_probe_smk.hex`), dopo 4 s dall'accensione senza toccare nulla, scrive una riga come
`n47 n05 n--` : i pin di ingresso che hanno cambiato livello da soli durante quei 4 s (`n47` = `P4.7`). Poi scrive come al solito le variazioni delle altre linee.

Procedura:
1. Flasha la sonda; non toccare niente per 4 s; copia il testo `n.. n..` che compare.
2. Ripeti con l'interruttore di modalità nelle posizioni **2.4G**, **BT** e **USB/spenta** (o come sono etichettate): le linee della radio devono comportarsi diversamente.
3. Incolla i tre risultati. Se `P4.7` (attenzione della radio) compare in modalità wireless e non in USB, l'ipotesi è confermata; `P5.5`/`P5.6`
   appariranno solo se la radio trasmette senza essere interrogata.

Per decodificare i frame serve la guida con l'analizzatore (`docs/radio-capture-guide.md`).
