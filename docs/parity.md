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
| 2.4G, Bluetooth, selettore, 3 slot, accoppiamento (Fn+B/N/M, 3 s) | `core/radio`, `core/link` | P (il BLE funziona; più lento del 2.4G: da misurare) |
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
| **Avviso di batteria scarica** | — | **M**: lo stock avvisa sotto ~3,29 V (lettura 800) e va critico sotto ~3,09 V (752); Oblio non fa niente |
| **Spegnimento di protezione** a batteria quasi vuota | — | **M**: la cella ha la sua protezione, ma meglio dormire prima |
| Indicatore di carica col cavo | — | ? cosa mostra lo stock (LED di stato vicino a Caps?) |
| Remote wakeup USB (svegliare il PC con un tasto) | il descrittore lo dichiara (`0xA0`), il firmware non lo usa | **M** |
| Sospensione USB (corrente del bus) | — | **M**/? |

## Luci

| Funzione | Oblio | Stato |
| --- | --- | --- |
| Effetti, luminosità, velocità, colore (tab Lighting di Vial, Fn+Q…I) | `core/led`, `led_effect`, `vialrgb` | S (colore da riconfermare) |
| LED Caps Lock sul tasto Caps | sì | S |
| Lampi di modo/slot/pairing sulla riga dei numeri | sì | S |
| Flicker della retroilluminazione | pausa del salvataggio ~20 ms, back‑off | **da confermare** con `diag.py --watch 1` |

## Configurazione e strumenti

| Funzione | Oblio | Stato |
| --- | --- | --- |
| Vial (keymap, tap dance, combo, override, lighting), VIA | `core/via`, `core/vial` | S per tutto tranne le luci (provate in parte) |
| HUD (overlay desktop) | `core/hud` | S |
| Keymap/impostazioni in flash (due copie, log append‑only) | `core/store`, `core/settings` | P (le impostazioni restano) |
| Recupero Esc+1+2, ISP | `board/recovery` | P (non usato) |
| Diagnostica (`diag.py`) | contatori, `--radio`, `--watch`, `--daemon` | P |

## Cosa farei, in ordine

1. **Latenza del BLE** (già concordata): misurare prima, poi decidere.
2. **Avviso di batteria scarica** (lettura sotto 800: tasti rossi lampeggianti; sotto 752: sleep): ora che la batteria è decodificata costa poche righe.
3. **Remote wakeup USB**, se vuoi svegliare il PC dalla tastiera.
4. **Confrontare con lo stock** i punti «?»: carica col cavo, cosa fa il LED di stato, cosa fanno i tasti Fn originali. Servono le tue osservazioni sulla tastiera originale.
5. Riprovare sull'hardware quel che è solo **S**: soprattutto Vial (combo, key override), NKRO, Caps Word, Leader.
