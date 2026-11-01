# Recupero e programmazione via ICP con Arduino Nano

> Procedura di **emergenza** (e di primo install del bootloader SMK) per la Yunzii B68 con un
> programmatore economico basato su Arduino Nano. Collegato a [porting-plan.md](porting-plan.md)
> (M0, M3) e a [REFERENCE.md §7](REFERENCE.md#7-programmazione-e-recupero-del-chip-icp--isp).
>
> **Stato di verifica.** Ho derivato questo testo da: README e `--help` di `sinodude`/`sinowisp`,
> il `firmware/README.md` di sinodude-serial, il workflow CI di sinodude, e dall'ambiente WSL2 di questo
> repo. **Non l'ho eseguito su hardware.** Dove un passo è un'ipotesi lo trovi marcato **[da verificare]**.
> Il pinout ICP del SH68F90A **non** è documentato nei repo che ho letto: va ricavato dal datasheet
> (vedi §6) — non collegare nulla finché non è confermato.

---

## Indice

1. [Quando serve l'ICP (e quando no)](#1-quando-serve-licp-e-quando-no)
2. [L'Arduino che hai trovato va bene?](#2-larduino-che-hai-trovato-va-bene)
3. [Cosa ti serve (lista della spesa)](#3-cosa-ti-serve-lista-della-spesa)
4. [Schema elettrico e collegamenti](#4-schema-elettrico-e-collegamenti)
5. [Preparare l'Arduino (una tantum)](#5-preparare-larduino-una-tantum)
6. [Collegarsi alla B68](#6-collegarsi-alla-b68)
7. [WSL2: far vedere le periferiche USB a Linux](#7-wsl2-far-vedere-le-periferiche-usb-a-linux)
8. [Procedure](#8-procedure)
9. [Diagnosi dei problemi](#9-diagnosi-dei-problemi)
10. [Checklist di sicurezza](#10-checklist-di-sicurezza)

---

## 1. Quando serve l'ICP (e quando no)

Ci sono **due vie** per scrivere nel chip:

| | ISP (via USB) | ICP (via pin di programmazione) |
| --- | --- | --- |
| Come | `sinowisp`, parla col bootloader residente | `sinodude`, parla direttamente col silicio |
| Hardware | solo il cavo USB | programmatore (Arduino Nano) + fili sui pad |
| Cosa può scrivere | l'**applicazione** | **tutto**: bootloader, applicazione, code option |
| Se l'app è rotta | funziona **se** riesci a entrare in ISP | funziona sempre |

Nel ciclo di sviluppo normale usi solo ISP. L'ICP serve in questi casi:

1. **Installare il bootloader SMK** (M3) o sostituire quello stock: il bootloader non può riscrivere se stesso.
2. **Cambiare le code option** (se differiscono da quanto richiede SMK).
3. **Recuperare la scheda** quando non si riesce più a entrare in ISP (app che si pianta *e* chord di recupero non
   raggiungibile, bootloader stock danneggiato).
4. **Backup di tutto il chip** (bootloader stock compreso) — vedi però la nota sotto.

> **Nota sul backup senza ICP.** `sinowisp read` ha l'opzione `-s/--section` con valori `firmware`, `bootloader`,
> `full`. Questo suggerisce che il bootloader stock si possa leggere **via ISP**, quindi senza programmatore **[da
> verificare sulla B68]**. Prova prima questa strada (M0.5): se funziona, hai un backup completo senza saldare nulla.

**Strategia raccomandata:** lavorare in ISP, con il *chord di recupero nell'applicazione* (task di M2) e il
*gate G0* (vedi piano) come protezioni; tenere il programmatore ICP **pronto ma nel cassetto**. Costa pochi euro e
trasforma un possibile brick in un incidente recuperabile.

---

## 2. L'Arduino che hai trovato va bene?

Il prodotto è "ARCELI Nano 3, modulo con connettore Type-C, 5 V, 16 MHz, compatibile Arduino IDE". Non posso aprire
la pagina Amazon, quindi ti do i **criteri di accettazione**; il design di sinodude-serial richiede:

| Requisito (da `firmware/README.md` di sinodude) | Cosa controllare sul tuo modulo |
| --- | --- |
| MCU **ATmega328P** (o 328PB) | Deve essere un **ATmega328P**, non un ATmega168 (alcuni cloni economici montano il 168, con 16 KB di flash: **non** basta). Lo verifichi dopo l'acquisto (§5.2) leggendo la *signature* |
| Pinout Nano standard (D2…D6, 5V, GND) | Tutti i "Nano" lo rispettano |
| Logica a **5 V**, target alimentato a 5 V | I Nano a 5 V/16 MHz vanno bene; **non** serve level shifter |
| Interfaccia seriale verso il PC | Di solito CH340 (cloni) — su questo WSL2 il modulo `ch341` è presente (§7) |
| Bootloader | Vecchio (57600 baud) o nuovo/Optiboot (115200 baud): cambia solo un parametro in §5.3 |

**Verdetto:** *probabilmente sì.* Il tipo (Nano clone, ATmega328P, CH340, Type-C) è quello usato e raccomandato da
sinodude. Tre avvertenze pratiche:

* Controlla nella descrizione/recensioni che sia **ATmega328P** (non 168) e se i pin sono **già saldati** (molti
  Nano arrivano con le strip di pin in bustina, da saldare).
* **Da solo non basta**: serve un piccolo circuito di alimentazione (MOSFET + resistore, §3–§4) che l'Arduino non ha.
* Il connettore Type-C non cambia nulla funzionalmente; usa un buon cavo dati (non solo ricarica).

---

## 3. Cosa ti serve (lista della spesa)

| Qtà | Componente | Note |
| --- | --- | --- |
| 1 | **Arduino Nano** (il tuo ARCELI, se ATmega328P) | + cavo USB-C dati |
| 1 | **MOSFET a canale P** | Il README di sinodude cita **IRF9540**, **IRFU9024** "o simile logic-level". Controlla sul datasheet del tuo componente l'ordine dei piedini Gate/Drain/Source (varia col package) |
| 1 | **Resistore 10 kΩ** | pull-up fra Gate e Source: tiene il MOSFET spento durante il reset dell'Arduino |
| 1 | Breadboard + **cavetti Dupont** | per assemblare il circuito |
| 5–6 | Modo di toccare i pad della B68 | clip da test (SOIC/pogo), **pogo pin** su supporto, oppure **filo smaltato sottile** saldato ai pad. Dipende dai pad (M0.4) |
| 1 | Multimetro | continuità e verifica del circuito di alimentazione |
| — | Saldatore (se i pin del Nano o i pad richiedono saldatura) | |
| — | Nastro Kapton / isolante | per isolare i fili volanti sulla scheda |

> Il tuo stesso pc deve far "vedere" l'Arduino a Linux: su WSL2 serve `usbipd-win` (già presente sul tuo Windows,
> §7).

---

## 4. Schema elettrico e collegamenti

Tutto proviene dal `firmware/README.md` di sinodude-serial (link in REFERENCE §7). Il **link ICP è a 4 fili**
(TDO, TMS, TDI, TCK) più alimentazione e massa.

### 4.1 Collegamenti Arduino ↔ B68

| Pin Arduino Nano | Funzione ICP | Pin sulla B68 |
| --- | --- | --- |
| **D2** | TDO | pad ICP "TDO" |
| **D3** | TMS | pad ICP "TMS" |
| **D4** | TDI | pad ICP "TDI" |
| **D5** | TCK | pad ICP "TCK" |
| **D6** | comando alimentazione (vedi sotto) | — (pilota il MOSFET) |
| **GND** | massa | GND della B68 |
| *(via MOSFET)* | **VCC 5 V** | alimentazione del MCU (VCC/VDD o rail 5 V) |

I nomi TDO/TMS/TDI/TCK sono quelli del programmatore. **Quale pin fisico del SH68F90A corrisponda a ciascuno**
non è nel README di sinodude: lo trovi nel datasheet del chip (§6.1) o nelle note della community.

### 4.2 Circuito di alimentazione (power-cycle)

Per entrare in modalità ICP il programmatore deve **spegnere e riaccendere** il target; lo fa l'Arduino con D6 e un
MOSFET P:

```
 Arduino 5V ──┬───────────────┐
              │               │ S
           [10 kΩ]         ┌──┴──┐
              │       G ───┤  P  │
 Arduino D6 ──┴────────────┤MOSFET│
                           └──┬──┘
                              │ D
                              └──────────► VCC 5 V della B68
```

* **D6 = LOW** → Vgs negativo → MOSFET **acceso** → il target è alimentato.
* **D6 = HIGH o flottante** → il resistore porta il gate al source (Vgs ≈ 0) → MOSFET **spento**.

### 4.3 Regole di alimentazione (importanti)

* **Un'unica fonte di alimentazione**: la B68 viene alimentata *solo* dal Nano attraverso il MOSFET. **Scollega il
  cavo USB della tastiera** e **stacca la batteria** (connettore) prima di collegare i fili.
* Il rail 5 V della B68 alimenta anche altri componenti (LED, radio). Se assorbono molto, il Nano (limite USB ≈ 500 mA)
  o il MOSFET possono soffrire. Prima di cominciare conviene misurare la corrente della scheda a riposo **[da
  verificare]**; se è alta, valuta cosa scollegare.
* Non collegare mai insieme i 5 V del Nano e i 5 V di un altro alimentatore o della USB della tastiera.

---

## 5. Preparare l'Arduino (una tantum)

Lo scopo: caricare sul Nano il firmware **sinodude-serial**, che parla l'ICP e dialoga col PC via seriale.

### 5.1 Procurarti il firmware

Due modi:

* **Hex precompilato.** La CI di sinodude produce `sinodude-serial-atmega328p.hex` (e `…atmega328pb.hex`) come
  artefatto/release. Controlla nella pagina del repository `karstx/sinodude` (sezione *Releases* / *Actions*) se ce n'è
  uno recente **[da verificare]**.
* **Compilarlo tu** (dal workflow CI di sinodude):

```sh
git clone https://github.com/karstx/sinodude && cd sinodude
nix develop                       # toolchain: Rust nightly, avr-gcc, avrdude
cd firmware
RUSTFLAGS="-C target-cpu=atmega328p" \
  cargo build --release --no-default-features --features atmega328p
avr-objcopy -O ihex target/avr-none/release/sinodude-serial.elf sinodude-serial-atmega328p.hex
```

(Per un ATmega328PB: `target-cpu=atmega328pb` e `--features atmega328pb`.)

### 5.2 Verificare che il chip sia un ATmega328P

Collega il Nano al PC (vedi §7 per WSL2), poi:

```sh
avrdude -p atmega328p -c arduino -P /dev/ttyUSB0 -b 115200 -v
```

Cerca `Device signature = 0x1e950f` (ATmega328P). Se leggi `0x1e9516` è il 328PB (va bene, hex dedicato). Se leggi
`0x1e9406` o `0x1e940b` è un ATmega168: **non funziona**, restituisci il modulo. Se la comunicazione fallisce con
115200 prova `-b 57600` (bootloader "vecchio").

### 5.3 Caricare il firmware

```sh
avrdude -p atmega328p -c arduino -P /dev/ttyUSB0 -b 115200 -D \
        -U flash:w:sinodude-serial-atmega328p.hex:i
```

(`-b 57600` se il tuo Nano ha il bootloader vecchio.) Il repo sinodude usa `ravedude` con `board = "nano-new"` a
115200 baud, che corrisponde al caso Optiboot/nuovo.

> Dopo questo passo l'Arduino **non è più programmabile da IDE come prima**: è diventato il programmatore. Per
> ritrasformarlo basta ricaricare un altro sketch.

---

## 6. Collegarsi alla B68

**Questa è la parte non documentata e quindi la più rischiosa.** Procedi in quest'ordine:

### 6.1 Trovare i pin ICP del SH68F90A

1. Procurati il datasheet (REFERENCE §3.1) e cerca nel pinout i pin di programmazione, in particolare i segnali
   corrispondenti a **TDO / TMS / TDI / TCK** (i nomi usati da sinodude; potrebbero comparire con altri nomi,
   p.es. SCK/SDA/…) **[da verificare]**.
2. Confronta con le foto del PCB della Air60 in [keyboards/nuphy-air60.md](keyboards/nuphy-air60.md): spesso i
   produttori lasciano **pad di test** vicino all'MCU.
3. Con il multimetro in continuità, mappa **dal pin del chip al pad/test point** accessibile. Annota tutto in
   `docs/keyboards/yunzii-b68.md` (task M0.4) con foto.

### 6.2 Contatto fisico

* Preferisci **pogo pin / clip** per i test (nessuna saldatura) e filo smaltato saldato solo se i pad sono troppo
  piccoli.
* Isola i fili; fai trazione-sicura (un filo che si stacca durante una scrittura è una causa classica di danni).

### 6.3 Prima di accendere

* Cavo USB della tastiera **scollegato**, batteria **scollegata**.
* Verifica con il multimetro che **non ci sia continuità** fra VCC e GND, e che D2–D5 vadano ai pad giusti.
* Verifica il MOSFET: con D6 non pilotato (HIGH) la tensione verso la B68 deve essere ~0 V.

---

## 7. WSL2: far vedere le periferiche USB a Linux

Il tuo ambiente è WSL2 (kernel 6.18). Ho verificato in questa sessione:

* `usbipd-win` è installato sul Windows (`C:\Program Files\usbipd-win`, versione 5.3).
* Il kernel WSL ha i moduli `CONFIG_USB_SERIAL_CH341=m` (CH340) e anche CP210x e FTDI, più `HIDRAW`/`USB_HID`.
* `/lib/modules/…/ch341.ko` e `usbserial.ko` esistono.

Procedura **[da verificare]** (PowerShell **da amministratore** per `bind`):

```powershell
usbipd list                                  # trova il BUSID dell'Arduino (es. 1-3) e quello della tastiera
usbipd bind   --busid <BUSID>                # una volta sola, richiede admin
usbipd attach --wsl --busid <BUSID>          # ogni volta che lo colleghi (o --auto-attach)
```

Poi in WSL:

```sh
sudo modprobe usbserial ch341                # se il modulo non si è caricato da solo
ls /dev/ttyUSB*                              # deve comparire /dev/ttyUSB0
sudo usermod -aG dialout $USER               # e riapri il terminale, per evitare sudo
```

Per `sinowisp` (la tastiera in ISP) servono `/dev/hidraw*` e permessi (regola udev) — vedi README di sinowisp.

Note pratiche:

* Mentre un dispositivo è "attached" a WSL **Windows non lo vede più**.
* Se riscolleghi il dispositivo devi rifare `attach` (usa `--auto-attach` per automatizzare).
* In alternativa si possono eseguire `sinowisp`/`sinodude` (Rust, multipiattaforma) direttamente su Windows; per un
  flusso coerente col resto del progetto (Nix/Meson) restiamo su WSL.

---

## 8. Procedure

Sintassi dei comandi presa da `sinodude --help` (v. installata nell'ambiente Nix del repo). `--part` per la B68:
**`sh68f90a`** (esiste anche `sh68f90`). Sostituisci la porta seriale.

### 8.1 Primo contatto (sola lettura)

```sh
sinodude read -c sinodude-serial -p sh68f90a --port /dev/ttyUSB0 --flash /percorso/fuori/dal/repo/b68-full-1.hex
```

* Se il chip risponde con un part number diverso, `sinodude` si ferma: **non** usare `--ignore_part_number` finché
  non hai capito perché (potrebbe essere una variante o un collegamento errato).
* Ripeti la lettura e confronta (`sha256sum`) i due file: devono essere identici.

### 8.2 Backup completo (M0.5)

1. Lettura ICP × 2 (sopra).
2. Lettura ISP di confronto (se disponibile):
   `sinowisp read -s full -p sh68f90 --vendor_id … --product_id … <file>` **[da verificare]** (la B68 non ha un
   preset `-d`; esistono `yunzii-al66`/`yunzii-al71` che potrebbero funzionare per analogia).
3. Salva i file **fuori dal repo** (sono firmware proprietario). Annota gli `sha256`.

### 8.3 Ripristino del firmware stock (M0.6)

```sh
sinodude write -c sinodude-serial -p sh68f90a --port /dev/ttyUSB0 --flash /percorso/b68-full-1.hex
```

Poi verifica rileggendo e confrontando l'hash, e controlla che la tastiera funzioni come prima. **Questa prova va
fatta prima di sovrascrivere qualunque cosa con SMK.**

### 8.4 Installare SMK completo (bootloader + app) — M3.3

Dal repository (target Meson già presenti):

```sh
meson setup build
meson configure build -Dicp_port=/dev/ttyUSB0          # se diversa
meson compile -C build yunzii-b68_default_full_flash
```

Scrive l'intera flash (bootloader SMK + app) lasciando intatte le code option. Serve la voce `icp_part` nella
scheda (`'sh68f90a'`).

### 8.5 Sostituire solo il bootloader

```sh
meson compile -C build yunzii-b68_boot_flash
```

(scrive la sola regione del bootloader; funziona solo se l'app è stata scritta via ISP, vedi
[bootloader.md](bootloader.md)).

### 8.6 Recupero da brick

1. Scollega USB e batteria, collega il programmatore (§6).
2. `sinodude read` per verificare che il chip risponda.
3. `sinodude write … --flash b68-full-1.hex` (immagine stock completa) **oppure** `…_full_flash` (SMK completo).
4. Rileggi, confronta, scollega, ricollega la batteria, prova.
5. Se `read`/`write` rifiutano per protezione, guarda `sinodude security --help` e `--key` **[da verificare]**; non
   forzare con `--erase_mode` o erase totale senza un backup.

---

## 9. Diagnosi dei problemi

| Sintomo | Cause probabili | Cosa fare |
| --- | --- | --- |
| `/dev/ttyUSB0` assente | dispositivo non "attached" a WSL; modulo `ch341` non caricato; cavo solo-carica | `usbipd list`/`attach`; `sudo modprobe ch341`; cambia cavo |
| `Permission denied` sulla seriale | utente non nel gruppo `dialout` | `usermod -aG dialout`, nuova shell |
| `avrdude: stk500_getsync()` | baud sbagliato (bootloader vecchio/nuovo), porta sbagliata | prova `-b 57600` / `-b 115200` |
| signature `0x1e9406`/`0x1e940b` | modulo con ATmega168 | non utilizzabile: sostituisci |
| `sinodude`: nessuna risposta dal chip | cablaggio ICP errato o pad sbagliati; target non alimentato; MOSFET collegato male; batteria/USB ancora collegati | ricontrolla §6.3; misura 5 V sul target quando D6 = LOW; riprova il power-cycle |
| part number non corrispondente | variante del chip o fili scambiati | **non** forzare; ricontrolla i collegamenti; confronta marcatura |
| letture diverse a ogni lettura | contatto instabile, alimentazione debole | fissa i fili, accorcia, controlla corrente |
| scrittura riuscita ma tastiera non parte | code option diverse da quelle richieste, reset vector sbagliato, immagine non adatta | riscrivi il dump stock (8.3); verifica con `boot_check`/`full.hex` per SMK |

---

## 10. Checklist di sicurezza

* [ ] Dump completo del chip fatto **due volte** e con hash identici; file fuori dal repo.
* [ ] Prova di ripristino del dump stock eseguita con successo (M0.6).
* [ ] Batteria e USB scollegate prima di collegare il programmatore.
* [ ] Pinout ICP **confermato** da datasheet/continuità prima di dare alimentazione.
* [ ] Nessun `erase`/`security` lanciato senza backup.
* [ ] Fili fissati, isolati; nessun cortocircuito sul rail 5 V.
* [ ] Il programmatore (Nano) è alimentato solo dal suo cavo USB-C.
