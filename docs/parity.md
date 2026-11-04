# Completezza del firmware — Oblio contro stock e SMK

Verificato sul codice il 2026-11-02 (commit dopo `c6ff1f9`). Legenda: **P** = provato dall'utente sulla tastiera · **S** = scritto, test host/simulatore verdi, **non provato**
sull'hardware · **M** = manca · **?** = non so cosa faccia lo stock: da confrontare con l'uso reale della tastiera originale.

## Ingresso

| Funzione | Stock / SMK | Oblio | Stato |
| --- | --- | --- | --- |
| Matrice 5×16, debounce | sì | `core/matrix`, debounce a tempo 5 ms | P (digitazione ok) |
| Layer (6), MO/TG/LT/MT/OSM/OSL | SMK sì | `core/layer`, `core/tapping` | S |
| Tap dance, macro, combo, key override, Leader, Caps Word | SMK/QMK | sì, editabili da Vial | S |
| NKRO (USB 112 bit; radio bitmap) | SMK sì | sì | S (USB), P (2.4G/BLE vedi sotto) |
| Encoder (volume, mouse wheel con MS_WH) | stock sì | `core/encoder` | S |
| Tasto dell'encoder = TD(0) mute / play / next | stock: mute | sì | S |
| Switch Win/Mac (P0.5, layer 0/1 e 5/4) | stock sì | `board/os_switch` | P |
| Mouse keys | stock: mouse dall'encoder | `core/mouse`, solo USB | S; **M in wireless** (la radio non ha un frame mouse) |

## Wireless

| Funzione | Oblio | Stato |
| --- | --- | --- |
| 2.4G, Bluetooth, selettore, 3 slot, accoppiamento (Fn+B/N/M, 3 s) | `core/radio`, `core/link` | P (il BLE funziona; più lento del 2.4G: due contatori in `diag.py --radio` per misurare) |
| Nome Bluetooth «OBLIO» | sì | P |
| Caps Lock dell'host sul LED in wireless | dal byte 1 dello stato | S |
| Frame `08` della radio (22 byte) | confermato con `F1`, non decodificato | ? potrebbe portare collegato/carica |
| Tasti multimediali in wireless | frame consumer | S |
| Mouse in wireless | — | M (limite del protocollo noto) |
| Intervallo di connessione BLE | deciso da host e BK3632 | ? nessun comando noto per cambiarlo |

## Energia

| Funzione | Oblio | Stato |
| --- | --- | --- |
| Sleep (1/5/10/30 min, mai; Fn+Tab; Fn+O/P) | `core/sleep`, `board/sleep_hw` | P dorme e si sveglia; ripetere la prova dopo la correzione dei LED |
| Gauge batteria (Fn+Backspace, 10 tasti) | lettura ADC, curva Li‑Po | S, scala **provvisoria** (un punto misurato) |
| Avviso di batteria scarica | `battery_task`: sotto la lettura 800 (~3,3 V) il gauge rosso compare subito e ogni 30 s | S |
| Sleep di protezione a batteria quasi vuota | sotto 752 (~3,09 V) per 10 s va in sleep; un tasto lo sveglia, e se è ancora scarica ridorme dopo 10 s | S |
| Indicatore di carica col cavo | — | ? cosa mostra lo stock (LED di stato vicino a Caps?) |
| Remote wakeup USB (svegliare il PC con un tasto) | `usb_wake_task`: bit WKUP per 10 ms con il bus sospeso e il wakeup abilitato dall'host (come lo stock a `0x9D73`) | S; **da provare** mettendo il PC in sospensione |
| Sospensione USB (corrente del bus) | — | **M**/? |

## Luci

| Funzione | Oblio | Stato |
| --- | --- | --- |
| Effetti, luminosità, velocità, colore (tab Lighting di Vial, Fn+Q…I) | `core/led`, `led_effect`, `vialrgb` | S (colore da riconfermare) |
| LED Caps Lock sul tasto Caps | sì | S |
| Lampi di modo/slot/pairing sulla riga dei numeri | sì | S |
| Flicker della retroilluminazione | ISR USB senza copia (le richieste dell'app), luci nel log delle impostazioni (nessun erase a ogni tasto luce), pausa solo attorno agli erase, back‑off | **da confermare** con `diag.py --watch 1` («timer interrupt: longest start delay» sotto ~50 µs) |

## Configurazione e strumenti

| Funzione | Oblio | Stato |
| --- | --- | --- |
| Vial (keymap, tap dance, combo, override, lighting), VIA | `core/via`, `core/vial` | S per tutto tranne le luci (provate in parte) |
| Keymap/impostazioni in flash (due copie, log append‑only) | `core/store`, `core/settings` | P (le impostazioni restano) |
| Recupero Esc+1+2, ISP | `board/recovery` | P (non usato) |
| Diagnostica (`diag.py`) | contatori, `--radio`, `--watch`, `--daemon` | P |

## Cosa resta

1. **Latenza del BLE**: provare con i contatori nuovi (vedi `docs/session-state.md`) e decidere.
2. Riprovare sull'hardware quel che è solo **S**: Vial (combo, key override), NKRO, Caps Word, Leader, avviso batteria, remote wakeup.
3. I punti «?» (carica col cavo, frame `08`, tasti Fn dello stock) non interessano all'utente: lasciati.
