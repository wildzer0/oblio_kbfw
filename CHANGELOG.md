# Changelog — Oblio

## Piano (fasi)
0. Bootstrap (repo, flake, CI, docs) — **fatto**
1. CI: build del firmware, budget flash/XDATA, hex come artifact, Release al tag — **fatto** (CI remota non ancora vista girare)
2. HAL minimo e matrice 5×16 — **scritta, non provata sull'hardware** (build e test host OK)
3. Logica pura con test sull'host (layer, tapping, report, macro, profili) — **fatta** (passi 1–5, 8 file di test)
4. USB e raw HID — **scritta, non provata sull'hardware**
5. Vial/VIA — **scritta, non provata sull'hardware**
6. Radio, LED, profili, sleep — **scritta, non provata sull'hardware**
7. Parità con il porting SMK e passaggio — **fatta** (passi 1–8, vedi sotto); la prova sulla tastiera resta da fare

Il controllo di recupero Esc+1+2 va all'inizio del `main`, prima di USB e radio.

## 2026-11-02 (notte, 4) — la batteria è una lettura ADC a 10 bit, formula dallo stock

- **Trovato nel dump stock, senza aspettare ore di registro**: il gestore del frame di stato (`0x05EA–0x0690`) costruisce `XDATA[0x02E6..7] = byte5:byte4`
  (byte 4 basso, byte 5 alto), e la routine del gauge (`0x7CF0`) calcola `% = (lettura − 715) × 10 / 22` (0 sotto 715, 100 da 935 in su). Non è un
  livello a 3 bit (quello era il layout NuPhy, ipotesi sbagliata). Le soglie dello stock: lettura < 800 = avviso, < 752 = critica. Lo stock **non usa**
  l'ADC/LPD del chip (`0xEDCE` azzera solo `LPDCON/LPDSEL`): la tensione la misura la radio.
- Rilettura dei dati vecchi: `00 01 00 03 ff 03` col cavo = `0x03FF` (ADC saturo); «`0x03` in uso» = `0x0303` = 771 ≈ 25 %. Nessun mistero: non scendeva
  perché la lettura era un byte basso di un numero più grande.
- `board/battery.c` usa la curva dello stock; col cavo la lettura satura e si ignora (resta l'ultima presa a batteria). `radio_battery_level()` e
  `radio_low_power()` (il bit 7 era un'ipotesi) tolti; c'è `radio_battery_raw()`. Registro e `diag.py` mostrano lettura e % (i 2 bit alti stanno nei
  bit 4–5 del byte dei flag). Flash 53664/55296, XDATA 3779/4096, test host 28. **Da provare sulla tastiera**: Fn+Backspace a batteria.

## 2026-11-02 (notte, 5) — transitorio dell'ADC e registro che non consuma la flash

- **Prova dell'utente**: appena accesa il gauge dava «rosso su 1», dopo qualche secondo «verde su 0». La lettura all'accensione è 1–3 (ADC non
  assestato), poi salta al valore vero. `board/battery.c` ora ignora le letture sotto 600 (`BATTERY_RAW_FLOOR`): finché non c'è una lettura vera il gauge
  dice «sconosciuto» (dieci tasti bianchi lampeggianti) invece di inventare uno 0 %.
- **Registro**: l'ADC ondeggia di 1–2 conteggi a ogni risposta, e il registro scriveva a ogni cambio: il settore (128 record) si riempiva in pochi
  decine di secondi e veniva cancellato di continuo (usura della flash + 32 ms di blocco a ogni erase). Ora un cambio vale solo se la lettura si è mossa di
  `BLOG_STEP` (8) conteggi o i flag cambiano; altrimenti un record ogni 5 minuti. Flash 53762/55296.
- `--opt-code-size` al posto di `--opt-code-speed` risparmia solo 26 byte: non vale il rischio sul budget dell'ISR.

## 2026-11-02 (notte, 6) — il registro resta vuoto: contatori per capire perché

- Dati dell'utente (nuovo hex, `--radio --blog` dopo la prova del carico): «on battery» = lettura **1023** (satura) e **registro vuoto**. La cattura
  «a batteria» avviene nell'attimo in cui si infila il cavo, prima che il PC enumeri: non è una lettura a batteria. Ora `radio.c` la scarta se è satura
  (≥ 0x3F0). Il registro vuoto invece non ha spiegazione nel codice: dopo il reset del cavo la radio gira e il primo stato valido deve scrivere un record.
- Due contatori (al posto di «passi del main tra due scansioni» e «risposte al comando di modo», che servivano poco; il pacchetto raw HID ne
  regge al massimo 14): **battery log: writes tried** e **writes that did not hold** (la scrittura rilegge il record). Se «tried» è 0 il registro non
  scrive; se «tried» > 0 e «did not hold» > 0 la flash a 0xD800 non tiene i dati.

## 2026-11-02 (notte, 7) — il registro non era vuoto: era illeggibile

- Contatori dell'utente: **267 scritture tentate, 0 fallite**, eppure `--blog` diceva «empty». Causa: `core/via.c` lasciava passare senza sblocco solo i tipi
  0, 1 e 3 della diagnostica; il tipo 4 (registro) veniva rimandato indietro con conteggio 0. Ora anche il tipo 4 è libero (sola lettura).
- Da capire ancora: 267 scritture in pochi minuti col cavo sono troppe per la soglia di 8 conteggi: guardare il registro (fino a 128 record).

## 2026-11-02 (notte, 8) — la tastiera digita la lettura della batteria (log dal vivo)

- Per misurare senza cavo e senza ore di attesa: **Fn+Backspace due volte di fila** (entro 700 ms) accende/spegne un log: la tastiera digita ogni secondo la
  lettura ADC grezza in decimale + Invio, nel campo in primo piano, a 2.4G/Bluetooth (l'unica strada senza cavo: il dongle è solo una tastiera).
  `tools/battery-log.html` (pagina locale, nessun server) ascolta i tasti, registra ora e lettura, disegna la curva, scarica il CSV. Un carattere ogni
  30 ms (la coda della radio è di 4); la tastiera non va in sleep mentre scrive. Costa ~520 B di flash (54415/55296). È uno strumento di debug: da
  togliere insieme al registro in flash quando la scala della batteria è confermata.

## 2026-11-11 — la versione del firmware e l'utility web per aggiornarlo dal browser
- **Versione** (`VERSION` = 0.9.0, `core/version.[ch]`): una struttura in ROM che comincia con `OBLIOFW1` (major, minor, patch, flag, commit, dirty), leggibile dal `.hex` e dalla tastiera (comando `0xF1` tipo 2, `diag.py --version`). `tools/check_version.py` e `make check-version` controllano che compaia una volta e coincida con `VERSION` e con il tag. Non c'è nella build di profilo.
- **Utility web** (`web/`, commentata a fondo, senza dipendenze): collega la tastiera con WebHID, mostra versione installata e da installare, scelta della release o trascinamento del `.hex`, controlli sull'immagine, aggiornamento con lo stesso protocollo di sinowisp (rilettura obbligatoria prima di ENABLE), IT/EN. Pagina di sonda `web/probe.html`.
- **CI**: test Node e un aggiornamento completo in Chrome senza testa (`make web-test`); workflow `pages.yml` che pubblica il sito con tutte le release (`tools/make_site.py`), **su `main`** (l'ambiente `github-pages` rifiuta i tag: il primo tentativo sul tag `v0.9.0` fallì così) e avviato dalla Release con `workflow_dispatch`; controllo tag/versione; **l'artifact `oblio-hex` ora è preso prima della build senza registro batteria** (prima il passo `BATTERY_LOG=0` sovrascriveva `build/oblio.hex` e la Release avrebbe pubblicato quella).
- **Caps Word**: il carattere `_` (`LSFT(KC_MINS)`) mantiene attivo il Caps Word, come il trattino; test in `test_keyboard.c`.
- Piccole riduzioni di spazio nel profilo (il bit «host USB configurato» del contatore `DIAG_CHARGER`, tolto).
- **Non provato sull'hardware**: l'aggiornamento dal browser (usare `web/probe.html`, poi un aggiornamento vero).

## 2026-11-04 — il cavo e la carica come nello stock; il canale raw HID si riarma sempre
Segnalazione: dopo ore col cavo, il gauge dava ~10 % su USB e ~20 % in radio; poi `diag.py` e Vial non rispondevano più (la tastiera scriveva), «no Oblio keyboard found», a volte «did not answer»; si riprendeva staccando il cavo, ma non in modo deterministico.
- **Dal dump stock** (`docs/keyboards/yunzii-b68-firmware-stock.md`, sezione «Cavo e carica»): lo stock legge **P4.4** (cavo, alto) e **P7.7** (in carica, basso) con antirimbalzo, mostra la carica col **tasto Esc rosso fisso** e ricostruisce la percentuale (la lettura satura col cavo). Il LED vicino al Caps non è un indicatore di carica: segue il backlight (osservazione dell'utente).
- **Causa probabile del buio USB** (ipotesi coerente coi dati, non provata): Windows sospende la porta, `usb_device_bus_check` dichiara l'host «perso», `usb_device_configured()` diventa falso e la tastiera, in 2.4G, si addormenta dopo il tempo di inattività con l'USB spento. Ora `battery_cable()` (lettura satura, linee del caricatore) lo impedisce.
- **Difetto trovato nel codice** (spiega «scrive ma Vial muto»): `usb_task` riarmava il canale raw HID solo `if (usb_device_configured())`; dopo un reset del bus l'endpoint è vuoto e, col verdetto «nessun host» ancora in piedi (fino a 250 ms dopo un cavo inserito o una porta risvegliata), il SET_CONFIGURATION passava senza riarmarlo. Ora si usa `usb_device_enumerated()` (stato del dispositivo). Il simulatore non lo riproduce (non azzera l'endpoint al reset del bus), quindi non c'è un test automatico: la conferma viene dalla tastiera (staccare/riattaccare il cavo, diagnostica e Vial sempre pronti).
- **Nuovi moduli:** `core/charger.[ch]` (antirimbalzo, `tests/host/test_charger.c`), `board/charger_hw.[ch]` (legge i pin), `battery_cable()` e `battery_show()` in `board/battery.c`.
- **Gauge:** a batteria il livello; col cavo dieci tasti azzurri (in carica) o verdi (carica finita). `led_show_battery` accetta `LED_BATTERY_CHARGING`/`LED_BATTERY_CHARGED`.
- **Diagnostica:** nuovo contatore `DIAG_CHARGER` (pin grezzi, filtrati, conclusione) con le righe «charger» in `diag.py`; `--radio` non stampa più «~100 %» per una lettura satura.
- **Spazio:** tolta la tabella `level` del framebuffer (240 B di XDATA; `led_level` esiste solo per i test host), tolte `report_mouse_buttons` e `tick_ms_now` (inutilizzate), `profile_end()` al posto di tre istruzioni in ogni passo, sonde del profilo ridotte a un solo punto (il tratto che dipinge, passo 16). La build di profilo ora sta in **56.316/56.320 B** di flash (4 B liberi).
- Numeri: flash 54.676/55.808, XDATA 3.654/4.096 (53.232/56.320 senza log); 29 test host, 23 nel simulatore.

## 2026-11-07 (sera) — il flicker era della build di profilo
Sulla tastiera il flicker delle colonne si vedeva con `oblio-profile.hex` e non con la release (`1f51c85`, provata dall'utente). Un commit di prova (`1c18c00`) aveva tolto la scrittura veloce del framebuffer sulla base di un confronto tra un profilo e due release: annullato. Il profilo non va usato per giudicare i LED.

## 2026-11-07 — i LED non rifanno lo stesso lavoro 80 volte; Caps Word accende il Caps
- **`core/led.c`:** il colore degli effetti uniformi (tinta unita, respiro, pulsazione) è calcolato una volta per ridisegno; le onde orizzontale (per colonna), verticale (per riga) e il radiale (per anello, nuovo `led_effect_ring`) riusano i colori già calcolati; le scritture nel framebuffer vanno per puntatore e saltano i canali invariati; la fase è fotografata a inizio ridisegno (`cur_phase`).
- **`core/led_effect.c`:** `led_color_wheel` senza la divisione per 43.
- **Caps Word** (modifica dell'utente in `led_task`/`paint_one`): il tasto Caps si accende anche con Caps Word, e `led_task` si accorge del suo cambio (prima solo del Caps Lock: il cambio non ridisegnava).
- **Profilo:** la tabella dei passi parte da 1 (19 voci, non 20: 10 B di XDATA in meno); `PROFILE_STEPS` è l'ultimo passo valido.
- **Test:** `test_led` controlla Caps Word, gli effetti uniformi (stesso colore su ogni tasto, `duty2` coerente col livello) e che le onde con cache diano gli stessi colori dell'effetto calcolato tasto per tasto.
- Numeri: flash 54.136/55.808 (52.692/56.320 senza log), XDATA 3.884/4.096; profilo (`LOOP_PROFILE=1 BATTERY_LOG=0`) 56.297/56.320 e XDATA 4.096/4.096 (al limite).

## 2026-11-06 (sera) — scansione, debounce, sleep, impostazioni, encoder: meno lavoro, stessi risultati
- **Scansione** (`matrix_hw_scan`): ogni colonna tocca solo la sua porta (P6, P5 o P4) invece di tre letture-modifica-scrittura: da 5105 a 4638 istruzioni per scansione (simulatore), -9 % sull'ISR più pesante.
- **`matrix_feed`**: percorso rapido per la colonna ferma (campione uguale a candidato e a stabile, debounce già contato); puntatori `XDATA` tipizzati invece dei generici (~30 istruzioni a accesso).
- **`sleep_hw_task`** riusa `any_key_down` del ciclo principale (prima interrogava la matrice di nuovo).
- **`settings_task`**: il confronto dei valori delle luci ogni 8 ms, non a ogni passata (8 chiamate degli hook a passata).
- **`encoder_poll`**: se lo stato dei pin non è cambiato, niente da fare.
- Il comportamento non cambia: 28 test host, 23 nel simulatore, flash 53.548/55.808, XDATA 3.829/4.096.

## 2026-11-06 — il ridisegno dei LED costava 1730 istruzioni a LED: ora ~1000

- Le sonde dentro `led_task` (utente, radio, 36 s): **il blocco da 8 LED costa 2959 µs al minimo e 3285 µs di media (43,9 % del tempo)**; il resto di `led_task` e di ogni altro passo è trascurabile.
  Nel simulatore (contando le istruzioni tra due chiamate di `led_effect_rgb`) un LED costava **~1730 istruzioni**, di cui 6 chiamate a `__mulint` (la moltiplicazione a 16 bit del compilatore, ~230 istruzioni
  l'una: 3 in `scale()` per la luminosità dei canali e 3 in `LED_WIDTH` per la larghezza del PWM) e ~14 chiamate ai puntatori generici (`__gptrget/put`, ~30 istruzioni) per gli array `out[3]` dei colori.
  (Una prima lettura nel simulatore, ~1000 istruzioni per 8 LED, era sbagliata: avevo preso un giro parziale.)
- Correzioni, senza cambiare nessun valore calcolato (test **esaustivo**: `test_the_fast_arithmetic_equals_the_plain_formulas` confronta, per tutte le 256×256 coppie fase/luminosità e tutti i 256 livelli, con le formule semplici):
  `LED_WIDTH` con shift e somme; `scale()` senza moltiplicazione per 0 e 255 (255·b/256 = b−1) e con shift-add per il resto; i colori passano come `XDATA uint8_t *` (puntatori a 2 byte, accesso diretto)
  e il vettore `rgb` di `paint_one` è statico in XDATA. **~1000 istruzioni a LED (−42 %), flash −488 B** (53400/55808). Il blocco da 8 LED passa da ~3,3 ms a ~1,4 ms di tempo di orologio.
- Resta `matrix_feed` (15 %, ~750 µs per scansione): da guardare dopo (`raw` è un puntatore generico, 16 `__gptrget` per scansione, e ogni colonna fa ~190 istruzioni).

## 2026-11-05 (notte) — profilo con orologio corretto: `led_task` è il 45 % del tempo

- Misura dell'utente (radio, 49 s, orologio corretto; le quote ora tornano: passi ~75 % + interrupt fuori dai passi): **203 giri/s (4,9 ms a giro)**, timer 40,8 % della CPU, USB 0,9 %, seriale 0,2 %.
  `led_task`: **media 2196 µs, massimo 8,8 ms, 44,6 %** del tempo; `matrix_feed` 743 µs (min 480), 15 % (una volta per scansione); tutti gli altri passi 40–135 µs di media (radio inclusa: `link_task` 135 µs, max 2,6 ms).
  I giri da 6,4–12,8 ms sono il 4 %. Il simulatore dava ~1000 istruzioni per ridisegnare 8 LED (~0,15 ms): sull'hardware è ~15× di più, non capito.
- Sonde dentro `led_task` (passi 16–19: `frame_passed`, controllo del caps lock, `read_settings`, blocco di 8 LED) per vedere dove va il tempo (`PROBE` in `core/led.c`, solo nella build di profilo).

## 2026-11-05 (sera) — l'orologio del profilo sbagliava: corretto

- Prima misura dell'utente (radio, 76 s): le quote dei passi sommavano al **190 %**, e ogni passo, anche da 12 µs, aveva massimo ~32,4 ms e media gonfiata di ~280 µs. Colpa dell'orologio (`profile_hook_now`): fermava l'interrupt del
  timer per leggere; se l'overflow cadeva in quei pochi istruzioni il conteggio ripartiva dal valore di reload e il tempo risultava indietro di uno slot, la differenza andava sotto zero e in aritmetica a 16 bit diventava ~32 ms
  (circa una misura su cento). Ora non si ferma più nulla: si rilegge se l'interrupt è passato nel mezzo (numero di sequenza) e un overflow non ancora servito (flag TF2) conta lo slot appena finito.
- Dati ancora validi di quella corsa: ~231 giri/s (4,3 ms a giro), CPU degli interrupt **timer 40,4 %**, USB 0,9 %, seriale 0,2 %; `led_task` il passo più lungo (si ridisegna di continuo con l'effetto animato). Da rifare le medie.

## 2026-11-05 — build di profilo completa (`LOOP_PROFILE`)

- Richiesta dell'utente: vedere i tempi di ogni compito del ciclo principale (minimo, media, massimo; la deviazione standard non serve). Sostituisce il contatore del «passo più lento» con un profilo vero:
  `core/profile.[ch]` (statistiche pure, con test host: `test_profile`), orologio a 0,5 µs in `hal/tick.c` (`profile_hook_now`: tempo all'inizio dello slot + conteggio del Timer 2, letti con l'interrupt del timer fermo per
  pochi istruzioni), passi misurati con `STEP(n, ...)` in `board/main.c`, tempo degli interrupt (timer, USB, seriale radio) misurato dentro le loro ISR dal conteggio grezzo del timer.
- Diagnostica `diag` kind 5 (passo: corse, min, max, somma), 6 (fasce di lunghezza dei giri + tempo degli interrupt), 7 (ricomincia), 8 (finestra in ms); `diag.py --profile` stampa tabella (corse, min, media, max, quota %),
  fasce dei giri e % di CPU per interrupt; `--profile-reset` azzera. Ricordare: i tempi sono di orologio (un passo include gli interrupt che girano), la quota degli interrupt è a parte; la seriale gira anche dentro l'ISR del timer.
- Si compila con `make LOOP_PROFILE=1 BATTERY_LOG=0 fw` (col registro non ci sta nel limite 0xDA00): flash 55426/56320, XDATA 3973/4096. Hex: `build/oblio-profile.hex`. Spento di default; il resto invariato.
- **Misura nel simulatore** (istruzioni esatte, solo il codice; niente interrupt reali né radio; valore tipico = minimo): `encoder_poll` 345, `settings_task` 359, `sleep_hw_task` 203, `keyboard_task` 147, `battery_task` 90,
  `usb_task` 78, `led_app_task` 74, `led_task` 63 (1018 con un ridisegno di 8 LED), `blog_task` 49, `link_task` 50, `os_switch_task` 42, `store_task` 44, `usb_wake_task` 25: un giro a vuoto costa ~1570 istruzioni, ~0,2 ms di CPU.
  Il passo più grosso è `matrix_feed`, **~1900–3200 istruzioni una volta per scansione** (~0,3–0,4 ms). **Nessun passo è pesante da solo**: i picchi di 9–12 ms visti sull'hardware non vengono dal codice dei passi ma da
  raffiche di interrupt o dalla radio, cose che la build di profilo misura. Candidati a un piccolo risparmio: `settings_task` (359 a giro, per il controllo delle luci a ogni giro) e `encoder_poll`.

## 2026-11-04 (notte) — il back-off scatta meno: 3 → 8 scansioni

- Misura con `LOOP_PROFILE` (scrivendo in radio): **led_task 9 ms**, giro più lento 12 ms, back-off 7. In simulatore un ridisegno di 8 LED costa ~1000 istruzioni (~0,3 ms): i 9 ms sono quindi tempo di orologio
  allungato da raffiche di interrupt (radio/USB/timer) più che lavoro di `led_task`; la causa precisa della raffica non è stata isolata.
- Quel che si vede e che si corregge: il rilevatore di «ciclo principale affamato» scattava dopo 3 scansioni (~6,6 ms) e metteva da parte il backlight per 12 slot (~5 ms): un lampo. Un giro di 9–12 ms non
  mette in pericolo i tasti (li scansiona l'interrupt; ritarda solo il debounce). Soglia a **8 scansioni (~18 ms)**: la vera affamatura (il ciclo che non gira più) è ancora colta, ben dentro il watchdog.
- **Da verificare**: scrivendo in radio il contatore «backlight back-offs» non deve più salire, e il lampo sparire. Resta da capire cosa allunga i giri del ciclo (picchi di 9–12 ms: anche latenza dei tasti).

## 2026-11-04 (sera) — profilo del ciclo principale (`LOOP_PROFILE`)

- Flicker mentre si scrive in modalità radio. I contatori dell'utente: i **back-off** salgono di 1–5 al secondo insieme ai frame radio (30 s di scrittura: da 2 a 33). Il back-off (`hal/tick.c`) mette da parte il
  backlight per 12 slot se il ciclo principale non fa un giro per 3 scansioni (~6,6 ms), quindi in quei momenti un giro del ciclo dura più di 6 ms (già visto: «slowest main-loop pass» 11 ms). Quale passo, non lo so.
- Strumento, non correzione: opzione **`LOOP_PROFILE`** (default 0; `make LOOP_PROFILE=1 fw`): `board/main.c` misura in ms ogni passo del ciclo (`STEP(n, ...)`, 15 passi) e tiene il più lento in `DIAG_LOOP_WORST`
  (passo ≪ 8 | ms); `diag.py` lo stampa col nome del passo. `tick_ms_now()` espone il contatore dei ms. Costa un po' di tempo a ogni giro, per questo è spento. Hex del profilo: `build/oblio-profile.hex`.

## 2026-11-04 — tolto l'HUD

- L'utente lo ha provato e non gli serve. Via `core/hud.[ch]`, il suo test, il comando `0xF0` in `core/via.c`, `hud_hook_host_leds`, il caso `B68_HUD_TOG`, `docs/hud-protocol-v2.md`, i riferimenti nei documenti; via anche
  `layer_oneshot_layer()` / `layer_oneshot_mods()` (le usava solo l'HUD).
- **Il primo keycode della scheda, `0x7E00`, resta come `B68_UNUSED`** (nella definizione Vial: «(unused)»): non fa nulla, ma così gli altri tasti `B68_*` non cambiano numero, e le keymap salvate in flash
  (e l'impronta dei default) restano valide. Rinumerarli resetterebbe la keymap salvata dell'utente: si può fare in futuro se si accetta di rifare le modifiche.
- Un comando `0xF0` ora risponde «non gestito» (test in `test_via`). Flash 53879/55808 con il registro batteria (+1,4 KB liberi rispetto a prima), 52443/56320 senza; 27 test host, `make sim` ok (23 test: tolto quello dell'HUD).

## 2026-11-03 — il registro della batteria torna, opzionale (`BATTERY_LOG`)

- Richiesta dell'utente: la tastiera è rimasta scollegata una notte e un giorno e il gauge segnava ancora 90 % (verosimile: dormendo consuma pochissimo, ma non c'era modo di vederlo). Il registro in flash è reintegrato,
  **sotto la direttiva `BATTERY_LOG`** (default 1 in `core/options.h`; `make BATTERY_LOG=0 fw` lo toglie; il Makefile ricostruisce tutto se l'opzione cambia, e la CI compila anche la variante senza).
- Diverso dalla prima versione: il settore sta a **0xDA00** (non 0xD800: il codice arriva a 0xDA00 invece di 0xDC00 solo con il registro acceso); i record portano la marcatura `0xA` (un settore con resti del firmware stock/vecchi
  viene cancellato al primo uso invece di essere elencato); ogni record ha i flag **CAVO** e **WAKE** (prima lettura dopo accensione o risveglio dallo sleep: prima di lui c'è un buco, perché senza orologio durante lo sleep i minuti
  contano solo la veglia); le letture non assestate (<600) non si scrivono; legge con `diag.py --blog` (kind 4, sola lettura, nessuno sblocco). `blog_task` riceve la lettura (`battery_reading()`), non i byte di stato.
- Flash 55326/55808 con il registro (+1,4 KB), 53890/56320 senza. 28 test host (`test_blog` nuovo, kind 4 in `test_diag`), `make sim` ok, vettori ok in entrambe le varianti.
- Il test `test_led` non linkava per la modifica non committata dell'utente in `core/led.c` (`keyboard_caps_word()`): aggiunto lo stub nel test; `core/led.c` non è toccato né incluso nel commit.

## 2026-11-02 (notte, 22) — tolta la build a gradini

- Eliminati `board/stage.h`, i blocchi `#if STAGE_LED/RADIO/SLEEP` di `main.c`, `led_hw.c` e `slots.c`, `OBLIO_STAGE` e i bersagli `make stage` / `make stages`, il passo e l'artifact dei gradini nella CI.
  Il Makefile perde anche `STAGE`, `NOT_LINKED` e `CHECK_REL` (vuoti da tempo). **L'hex è identico a prima** (md5 `60c1117c304a12070d2a36aecab14b56`): lo stadio 4 compilava già tutto.
- `docs/first-boot.md` riscritto: non è più la procedura a gradini del primo avvio ma «come flashare e cosa guardare» (comando, recupero, contatori sani, giro funzionale).

## 2026-11-02 (notte, 21) — chiusura

- Tutte le prove dell'utente sull'hardware sono andate bene. Pulizia finale: tolti gli hex di bisezione, `CLAUDE.md` riscritto con lo stato attuale (era fermo al 2026-11-01), numeri di `session-state.md` aggiornati.
- Resta aperto solo ciò che non è un difetto: scala della batteria su un solo punto misurato (conferma: lettura ~1015–1020 appena tolto il caricatore), sequenze del Leader a scelta dell'utente, build a gradini eliminabili.

## 2026-11-02 (notte, 20) — in USB i LED si spegnevano e i tasti cadevano per ~250 ms: contatore dei SOF a 8 bit

- **Bug** (latente da quando c'è `usb_device_bus_check`): il contatore degli start-of-frame era a 8 bit e a 1 SOF al millisecondo gira esattamente in 256 ms. Il controllo del bus parte ogni ~250 ms e confronta il contatore
  con quello dell'ultima volta: se la finestra durava 256 ms (basta un giro del main un po' lungo) i due valori erano **uguali** e la tastiera dichiarava «host sparito» (`usb_device_configured()` falso) fino al controllo
  successivo: **LED spenti (in USB si accendono solo con l'host) e tasti scartati** per 250 ms, a intervalli casuali. Spariva in wireless (anche col cavo) perché lì i LED e i report non dipendono dall'USB.
  Corretto: contatore a 16 bit (`usb_hook_sof_ticks`), letto due volte fino a coincidere. Test: `test_a_full_turn_of_the_counter_is_not_a_host_that_went_away` prova ogni lunghezza di finestra da 250 a 770 ms.
- Dati dell'utente che lo escludevano da altre cause: nessun salvataggio completo (sequenza 1), nessun erase, reset cause 0 (nessun reset).
- La bisezione dell'utente ha confermato che non è una regressione recente: il problema c'è già in `d8c7df2.hex` e col backlight spento (quindi non sono né i LED né la corrente). Il bug esiste da `5780a8b`, quando è nato il controllo del bus.
- Nuovo contatore `DIAG_USB_GONE` («USB: times the host was declared gone»): con il cavo nel PC acceso deve restare 0.

## 2026-11-02 (notte, 19) — la keymap è un'immagine più un log; contatore dei salvataggi

- **Log della keymap** (`core/store.c`, `core/keymap.c`): le modifiche da Vial non salvano più la keymap intera (4 cancellazioni, ~130 ms di LED spenti). Ogni modifica annota l'offset della parola cambiata (`note()`, fino a 24);
  al salvataggio si **aggiunge un record da 5 byte** (offset, valore, controllo) nel log in fondo alla copia in uso (**69 record**, nello spazio libero dopo il payload) e non si cancella nulla. Salvataggio completo (copia
  alternata, log vuoto) solo se il log è pieno, se cambia molto insieme (reset, macro) o se non c'è ancora nulla. Il caricamento applica il payload e poi i record in ordine; un record tagliato si salta. Formato dell'immagine
  invariato: la keymap salvata resta valida. La keymap in RAM è ora una sola sequenza di parole (`words[]`), con `keymap_word_get/set` per offset: il codice di caricamento/salvataggio è più corto.
- **Contatore dei salvataggi**: l'intestazione ha già un numero di sequenza persistente = salvataggi completi di sempre; `diag.py` lo mostra (`DIAG_STORE_SAVES`), con una stima dei cicli di cancellazione per settore (metà),
  e i record presenti nel log (`DIAG_STORE_LOG`).
- Test: `test_store` riscritto per immagine + log (modifica = record senza erase né pausa dei LED, ordine di riproduzione, log pieno → altra copia dopo 69 record, molte parole → salvataggio completo, record tagliato saltato,
  interruzione di corrente in ogni punto del gruppo → ogni parola vecchia o nuova, mai corrotta, luci non sono una modifica). Flash 53755/56320 (+1,1 KB), XDATA 3778/4096, 27 test host, `make sim` ok.

## 2026-11-02 (notte, 18) — reverse engineering del salvataggio dello stock; fix del back-off durante la programmazione

- Riscritto il disassemblatore come `tools/dis51.py` (con `--xref`). Il salvataggio dello stock (vedi `docs/keyboards/yunzii-b68-firmware-stock.md`): primitiva IAP a `0x9700` che rifiuta di partire con `EA` acceso,
  interblocco sui settori 0x62–0x75, salvataggio a `0xA4A5` (PWM spento, colonne alte, **cancella il solo settore 0xC600**, PWM riacceso), scrittura dei 128 byte **uno per tick** nella coda dell'ISR del Timer 2,
  subito a ogni cambio (flag `50h`). Lo stock **spegne il backlight durante l'erase come noi**; non ha trucchi per evitarlo.
- **Trovata una causa nostra**: la programmazione della keymap (1,7 KB) gira in un colpo nel main, che non incrementa `alive`: il rilevatore di «main affamato» (3 scansioni) faceva scattare il back-off e allungava il
  buio oltre l'erase. `store_hook_program` ora chiama `tick_alive()`. Flash 52612/56320, 27 test host, `make sim` ok.

## 2026-11-02 (notte, 17) — guida per sviluppatori; Fn+PgUp/PgDn era lo switch su Mac

- `docs/guida-sviluppatore.md`: come funziona una tastiera (matrice, debounce, keycode, report HID, USB, wireless, PWM, Vial), la mappa mentale del firmware, un percorso di lettura del codice con i tempi,
  esercizi, strumenti, risorse esterne e il caso del backlight che si spegne al salvataggio (perché QMK non lo fa, cosa abbiamo fatto, cosa resta).
- Fn+PgUp/PgDn con Home/End "non funzionava": lo switch era su Mac (layer 5/4); l'edit era sul layer 1. Aggiunto `test_a_key_edited_on_the_fn_layer_is_what_fn_sends`.

## 2026-11-02 (notte, 16) — documentazione di architettura e riferimento delle API

- **`docs/design.md`** (italiano): vincoli, principi, mappa di memoria, avvio e main loop, tempo e interrupt, pipeline dei tasti, moduli uno per uno, persistenza, USB e protocolli, radio, backlight, energia
  e batteria, build e test, decisioni di design e compromessi, limiti noti, come estendere.
- **`docs/api.md`**: riferimento di tutte le funzioni, macro e tipi dei 54 header di `hal/`, `core/` e `board/`, **generato** da `tools/gen_api.py` leggendo i commenti `//` che già stanno sopra ogni dichiarazione
  (Doxygen legge solo `///` e `/** */`: avrebbe richiesto di riscrivere i commenti). `make api` lo rigenera, `make check-api` fallisce se è vecchio.
- Commenti vecchi corretti negli header (NKRO a 112 bit, diagnostica senza sblocco, riferimenti a funzioni tolte); combo: 6, non 8, nei documenti; `architecture.md` e `parity.md` allineati.

## 2026-11-02 (notte, 15) — HUD e salvataggio: i LED non devono più sfarfallare

- **HUD**: l'interrupt USB copiava i 32 byte di ogni richiesta due volte con puntatori generici (lenti), e teneva fermo l'interrupt del timer dei LED: ogni richiesta
  dell'app (decine al secondo) spostava l'inizio di uno slot e la colonna si accendeva più o meno a lungo. Ora l'interrupt non copia niente: segna che il pacchetto è arrivato
  e il main lo legge dal buffer dell'endpoint (che non è riarmato finché non si risponde). Nuovo contatore **«timer interrupt: longest start delay (us)»** per verificarlo
  (sopra ~50 µs si vede).
- **Salvataggio**: i tasti luce (effetto, luminosità, velocità, colore) scrivevano nel **deposito della keymap**, che a ogni salvataggio cancella 4 settori: ~130 ms di LED spenti,
  2 s dopo l'ultimo tocco. Ora stanno nel **log delle impostazioni** (un append, nessun erase salvo una volta ogni 36 salvataggi): `keymap_cfg_set` non conta più come modifica
  della keymap per questi quattro campi, `core/settings` li salva e li rimette dopo il caricamento della keymap (`settings_apply_led`). Il pause dei LED sta solo attorno all'erase.
  Il record delle impostazioni passa a 14 byte (v2): sleep e slot Bluetooth tornano ai predefiniti una volta; la luce resta (viene dal deposito).
  Restano ~130 ms al buio solo quando si salva la **keymap** da Vial (tasti, combo, ecc.) o ogni 36 salvataggi della luce.
- Flash 52601/56320, XDATA 3723/4096, 27 test host, `make sim` ok.

## 2026-11-02 (notte, 14) — diagnostica ridotta all'essenziale

- Tolti: i contatori «scansioni sovrascritte» (innocuo), «slot LED più lungo» e «scansione più lunga» (misure nell'interrupt: ora costa meno), «rifiuti della radio» e
  «tasto → radio» (la latenza del BLE è chiusa), e il frame `08` (mai decodificato) con il suo buffer. Restano 9 contatori: causa del reset, stack, slot LED e scansioni in
  ritardo (il rivelatore del flicker), passata più lenta del main, overflow della coda USB, back-off della retroilluminazione, erase più lungo, frame ricevuti dalla radio;
  e `--radio` (stato della radio con la lettura della batteria). Flash 52049/56320, XDATA 3735/4096, 27 test host, `make sim` ok (24).

## 2026-11-02 (notte, 13) — latenza del BLE: la misura dice che non è il firmware

- Dati dell'utente: rifiuti della radio ai report **0** in BLE e in 2.4G; massimo da tasto a «accettato dalla radio» **33 ms (BLE)** e **40 ms (2.4G)**. Il percorso del
  firmware è lo stesso nei due modi: il ritardo percepito in più del BLE è dell'intervallo di connessione (host + BK3632). Comandi dello stock mai usati (`04`, `05`, `0E`)
  guardati nel dump: `04 02/03` seguono il cambio di modo e un timeout, `0E` il modo USB; nessuno ha l'aria di un parametro di connessione. Nessuna modifica al codice.

## 2026-11-02 (notte, 12) — avviso di batteria, remote wakeup USB, misure per il BLE

- **Avviso di batteria** (`board/battery.c`, `battery_task`): sotto la lettura 800 (~3,3 V) il gauge (rosso, un tasto) si mostra subito e ogni 30 s; sotto 752 (~3,09 V) per 10 s la
  tastiera va in sleep per non scaricare la cella fino alla protezione (un tasto la sveglia; se è ancora scarica ridorme dopo 10 s). Solo a batteria, con lettura valida.
- **Remote wakeup USB** (`board/usb_app.c`, `hal/usb.c`): con il bus sospeso dall'host e il wakeup abilitato (il descrittore lo dichiarava già), un tasto preme `USBCON.WKUP` per
  10 ms, come lo stock (`0x9D73`/`0x7F11`); il tasto non esce, serve solo a svegliare. **Non provabile nel simulatore**: da provare mettendo il PC in sospensione.
- **Latenza BLE**: nessuna modifica alla cieca. Due contatori nuovi (14 su 14 nel pacchetto): rifiuti della radio ai report e il massimo da tasto a «accettato dalla radio»
  (l'intervallo BLE dopo è della radio). Flash 52381/56320, XDATA 3785/4096.

## 2026-11-02 (notte, 11) — controllo di completezza

- `docs/parity.md`: tabella funzione per funzione (stock/SMK contro Oblio), con provato / non provato / mancante. Mancano: **avviso di batteria scarica** e spegnimento di
  protezione (lo stock avvisa sotto la lettura 800 e va critico sotto 752), **remote wakeup USB** (il descrittore lo dichiara, il firmware non lo usa), mouse in wireless
  (limite del protocollo). Da confrontare con lo stock: indicatore di carica, frame `08`.
- `core/diag.c`: la lettura dello stato della radio non copia più 6 byte da un puntatore nullo prima della prima risposta.
- Numeri del `firmware-report.md` aggiornati (27 test, flash 51827/56320).

## 2026-11-02 (notte, 10) — pulizia: via il registro, il logger e la diagnostica superflua

- Tolti: `core/blog.[ch]`, `board/blog_hw.c`, il suo test, il logger da tastiera (`battery_key`/`battery_log_task`) e `tools/battery-log.html`, la diagnostica
  di tipo 2 (monitor radio col cavo), 3 (porte) e 4 (registro), la cattura «a batteria» della radio (`radio_status_on_battery`, `radio_monitor`,
  `link_monitor`), i due contatori del registro, e il blocco dello sblocco sulla diagnostica (ora tutto è sola lettura e libero).
- Il settore a 0xD800 torna al codice: **limite 0xDC00**, `FLASH_WRITE_MIN` 0xDC00. Flash 51797/56320 (4,5 KB liberi), XDATA 3755/4096, test host 27, `make sim` ok.
- `diag.py` ora: contatori, `--radio` (stato, lettura e % della batteria, frame 08), `--watch`, `--daemon`. Restano i contatori (flicker, latenza BLE).
- Documenti allineati (CLAUDE.md, architecture, firmware-report, first-boot, session-state).

## 2026-11-02 (notte, 9) — la scala della batteria dalla misura e dalla targa

- Misure dell'utente: **4,0378 V a riposo** (tastiera spenta) e lettura **981** a tastiera accesa → ~**243 conteggi/volt** (il 222,6 della pagina era un'ipotesi
  sbagliata). Targa: Li-Po **5000 mAh**, carica 4,2 V, nominale 3,7 V. A 243 conteggi/volt il fondo scala dell'ADC (1023) è ~4,2 V: spiega la saturazione col cavo.
- `board/battery.c`: la percentuale segue una **curva di scarica Li-Po** (800 = 3,30 V = 0 % … 1020 = 4,20 V = 100 %), non la retta dello stock (715–935, che
  chiama «pieno» tutto sopra ~3,85 V). 4,04 V → ~83 %. **Provvisoria**: poggia su un punto solo; il secondo (lettura appena tolto il caricatore, attesa ~1015–1020)
  la conferma. Soglia «cavo» 1021. Flash 54646/55296.

## 2026-11-02 (notte, 3) — il registro della batteria va in flash, e scrive sempre

- **I numeri dell'utente** dicevano questo: `reset cause 0x00` (power-on) e `radio: frames received 1945` — cioè la scheda era accesa da **pochi
  minuti**, non da una notte. Quindi **attaccare il cavo resetta la scheda**, e un registro in RAM muore proprio quando lo si vuole leggere.
- **In flash, in un settore suo**: `core/blog.[ch]` ora scrive in **0xD800** (un settore di 512 B, **128 record da 4 byte**), append-only, cancellato
  solo quando è pieno. Il limite del codice scende da 0xDC00 a **0xD800** (`make size` lo impone), `FLASH_WRITE_MIN` con lui; la keymap resta a
  0xDC00.
- **Scrive sempre**: prima registrava solo senza cavo (e con `usb_device_configured()` mai azzerato non registrava mai). Ora registra **ogni
  risposta** del radio, marcando `BLOG_FLAG_CABLE` quando il cavo è dentro: una notte attaccata dà una riga piatta di `0xFF` con la marca, che è
  comunque un'informazione; a batteria dà i livelli veri. Il primo record dopo un reset c'è sempre.
- **Il settore si scrive raramente** (un record per cambio, altrimenti ogni 5 minuti) e si cancella una volta ogni 128 record: usura trascurabile.
  La cancellazione (32 ms) ferma timer e backlight come per la keymap.
- **Test riscritto** `test_blog` con una flash finta: niente senza risposta, ogni risposta scritta e marcata, cambio immediato, soglia dei 5
  minuti, **sopravvivenza a un reset** (la ragione per cui è in flash), settore che riparte quando è pieno.
- **Memoria**: XDATA **3971 → 3779** (l'anello in RAM da 192 B è sparito), flash 53595/**55296** (il budget è più stretto di 1 KB perché un settore
  ora è del registro).

`make test` 28/28, `make sim` 24/24, `make stages` ok, vettori ok.

## 2026-11-02 (notte, 2) — il debounce diventa un tempo (come QMK), e la scansione costa un terzo in meno

- **Cosa fa QMK** (verificato sul suo codice e sui suoi documenti): la matrice si scansiona **nel main loop**, senza un ritmo fisso, con
  `MATRIX_IO_DELAY` = **30 µs per colonna** (più lento dei nostri 10×2); il **debounce è a tempo** — `DEBOUNCE` default **5 ms**, con algoritmo
  scegliibile (`sym_defer_pk` il predefinito) — e l'altra manopola è `USB_POLLING_INTERVAL_MS` (default **1 ms**). Latenza tipica: ~0,5 ms di
  scansione + 5 ms di debounce + 1 ms di USB ≈ **6–7 ms**.
- **Da qui la correzione giusta**: da noi il debounce era a **conteggio di scansioni** (3), quindi la latenza era *incollata* al ritmo di scansione
  e i due parametri non si potevano muovere separatamente. Ora `MATRIX_DEBOUNCE_MS` = **5 ms** (il default di QMK): una colonna conta quando è
  stabile da 5 ms. Con una soglia di **3 campioni** come pavimento, per il simulatore (il cui Timer 2 è un flag virtuale) e per non accettare nulla
  da meno di tre letture concordi.
- **Costo**: +32 B di XDATA (32 timestamp da 16 bit invece di 16 contatori), +2 B per la soglia; flash +71 B.
- **La scansione costa meno**: `SETTLE_US` da 10 a 5 µs (l'attesa dopo lo strobo, due volte per colonna) — 320 µs dei 753 misurati. Nel
  simulatore la scansione passa da **8305 a 5105 istruzioni** (-39%), cioè la ISR che affamava il main loop pesa un terzo in meno.
- **Cosa non ho fatto, e perché**: il rapporto slot LED/scansioni da 4 a 8 (l'altra metà della "soluzione B"). L'ho provato e **rompe la copertura
  del simulatore**: con 8:1 il test che preme una riga e aspetta i report su EP1 non vede più nulla (verificato con un bisect: 4:1 passa, 8:1
  no, in entrambi gli ordini di scansioni/passate). Il guadagno era per giunta ipotetico: `matrix_any_pressed()` non scansiona nulla (legge lo
  stato filtrato), quindi la fame del loop non era quella. Meglio un terzo di ISR in meno **e** il test dei tasti funzionante.

Budget: flash **53290/56320** (3,0 KB liberi), XDATA **3971/4096** (125 B liberi). `make test` 28/28, `make sim` 24/24, `make stages` ok.

## 2026-11-02 (notte) — perché il registro era vuoto: la tastiera credeva di essere collegata

- **Il bug**: `configured` viene messo a 1 da `SET_CONFIGURATION` e **azzerato solo** da un'altra `SET_CONFIGURATION(0)` o da un bus reset. Quando
  si **stacca il cavo** non succede né l'uno né l'altro: la tastiera restava convinta di essere attaccata al PC per sempre. Con quel flag,
  `radio_hook_on_battery()` (wireless **e** niente cavo) restava **falso**, quindi il registro della batteria non ha scritto **niente** per tutta
  la notte. Non era il gate sbagliato: era lo stato "cavo dentro" che non tornava mai indietro.
- **La correzione**: quello che il chip può dire è se **qualcuno sta pilotando il bus**. Ora `hal/usb.c` conta gli **start-of-frame** (uno al
  millisecondo con un host vivo) e `core/usb_device.c` guarda il contatore ogni 250 ms (`usb_device_bus_check`, chiamata da `usb_task`): se non è
  avanzato, il cavo è fuori. `usb_device_configured()` significa quindi "cavo dentro **e** qualcuno che pilota il bus" — così anche la logica dei
  LED e l'invio dei report USB vedono la verità.
- Il core non dipende da `hal/`: il conteggio arriva dall'hook `usb_hook_sof_ticks()`, come gli altri `usb_hook_*`, e il test può farlo salire e
  fermare a piacere.
- Test nuovo `test_a_host_that_goes_away_leaves_the_keyboard_unconfigured`: configura, il bus batte, il bus tace (→ non configurata), il bus torna
  (→ configurata di nuovo, senza un nuovo `SET_CONFIGURATION`).
- **Conseguenza per la notte persa**: il registro è in RAM e il cavo si attacca per leggerlo; se attaccare il cavo resetta la scheda, il registro
  muore proprio quando serve. Prima di riprovare una notte conviene verificarlo con una prova da un minuto (sotto).

Budget: flash **53177/56320** (3,1 KB liberi), XDATA **3939/4096** (157 B liberi). `make test` 28/28, `make sim` 24/24, vettori ok.

## 2026-11-02 (notte) — il registro della batteria dentro la tastiera

- **Il problema fisico, detto chiaro**: la diagnostica viaggia sull'USB della tastiera, e col cavo la batteria **si carica** (il byte risponde
  `0xFF`). Il dongle 2.4G, dal canto suo, si presenta al PC come una tastiera normale: **non espone l'interfaccia raw HID**, quindi nessun demone
  sul PC può interrogare la tastiera mentre è wireless. Le letture che servono si possono fare solo senza cavo, cioè quando niente può leggerle.
- **La soluzione**: la tastiera se le scrive da sola. `core/blog.[ch]` tiene **48 record da 4 byte** (192 B di XDATA): i minuti di accensione,
  il byte batteria e il byte dei flag dello stato radio. Un record quando la lettura cambia, e comunque ogni **5 minuti**; è un anello, si
  sovrascrive il più vecchio. Vive in RAM: un power cycle lo azzera (una scarica = una sessione).
- Il tempo è quello **da svegli** (i minuti si fermano nel sonno): è la cosa onesta, perché nel sonno non si campiona nulla e un record non deve
  dichiarare un'età che non ha.
- **Lettura**: comando diag `kind 4`, sei record per pacchetto; `tools/diag.py --blog` stampa la tabella. `--daemon N file.csv` scrive i
  contatori ogni N secondi in CSV con l'orologio (per lasciarlo girare la notte col cavo, che serve per il flicker).
- Test nuovo `test_blog` (niente senza risposta, niente col cavo, record sul cambio, ogni 5 minuti, anello, il sonno non conta) e il caso kind 4
  in `test_diag`. `core/blog.c` non conosce il radio: `blog_task` riceve lo stato, così i test non hanno bisogno di stub.

Budget: flash **53069/56320** (3,3 KB liberi), XDATA **3934/4096** (162 B liberi). `make test` 28/28, `make sim` 24/24, `make stages` ok.

## 2026-11-03 — la batteria: 0xFF col cavo, e lo stato a batteria adesso si può leggere

- **Dati dell'utente** (carica reale ~metà): con il cavo e selettore 2.4G la ghiera diceva **tutte e dieci** (100%), senza cavo **cinque** (43%),
  appena accesa **due rosse** (14%). Quindi il byte che leggiamo vale `0xFF` col cavo, `0x03` in uso, `0x01` all'accensione: **non è una carica che
  scende**, e col cavo `0xFF & 7 = 7` fingeva una batteria piena.
- **`core/radio.[ch]`**: `0xFF` in quel byte è una **sentinella** (in carica), non un livello: `radio_battery_level()` risponde "nessuna lettura".
  Nuovo `radio_status_on_battery()`: l'ultimo stato ricevuto **mentre la tastiera è a batteria**, con l'hook `radio_hook_on_battery()` (scheda:
  wireless e nessun cavo configurato).
- **`board/battery.c`**: la ghiera tiene l'**ultimo livello buono** — col cavo non dice più 100%, mostra quello che aveva visto a batteria; senza
  nessuna lettura dice "sconosciuta".
- **`core/diag.c` kind 1**: nei due byte liberi della risposta ora ci sono **il byte batteria e il byte dei flag come erano a batteria** — è
  l'unico modo di guardare una lettura che richiede il cavo per essere letta. `tools/diag.py` stampa una riga `on battery: ...`.
- **Come si chiude la misura**: staccare il cavo, usarla un po' (la radio interroga lo stato ogni 200 ms), riattaccare e lanciare
  `py .\diag.py --radio`: la riga `on battery` dice cosa leggeva a batteria. Due o tre letture a cariche diverse, e la scala si ricava.

Budget: flash **52017/56320** (4,3 KB liberi), XDATA **3729/4096**. `make test` 27/27, `make sim` 24/24.

## 2026-11-03 — colore completo: tinta **e** saturazione, a costo zero di memoria

- **Il conto, prima di farlo**: il colore del profilo era già una parola da 16 bit che ne usava otto (la tinta, con 0 = bianco). La saturazione
  sta nell'altro byte: **0 byte di RAM, 0 di XDATA, 0 di deposito**. Il costo è solo codice.
- **Costo vero, misurato**: **+333 B di flash** (51321 → 51654) e **+1 B di XDATA** (una variabile di stato). Il primo tentativo, con la conversione
  HSV scritta a sei lati, costava **+1605 B**: riscritta come "prendi il colore puro dalla ruota e mescolalo col bianco" (divisioni per 256, cioè
  shift) è costata un quinto.
- `core/led_effect.c`: `led_color_hsv(hue, saturation, value, out)`; `core/led.c`: il colore del profilo si legge come tinta (byte basso) e
  saturazione (byte alto), e l'effetto tinta unita usa la conversione. I tasti Fn+U/I camminano la tinta a saturazione piena e passano dal bianco
  dopo l'ultima, come prima.
- `core/vialrgb.c`: la tinta **e** la saturazione della tavolozza si salvano e si rileggono uguali, al passo di 1/255.
- Test aggiornati: colore del profilo con saturazione, giro tinta+saturazione → profilo → tinta+saturazione, tasti del colore.

Budget: flash **51654/56320** (4,7 KB liberi), XDATA **3721/4096**. `make test` 27/27, `make sim` 24/24, `make stages` 1–3 ok.

## 2026-11-03 — il colore scelto da Vial non era quello che usciva

- **Causa**: la ruota colori del firmware girava **rosso → blu → verde** (un ordine suo), mentre la tavolozza di Vial è HSV: **rosso → giallo →
  verde → ciano → blu → magenta**. Con la mappatura hue → indice, l'arancione (hue ~32) finiva sul viola. `core/led_effect.c`: la ruota è ora
  quella standard, in sei lati da 43 passi — e anche gli effetti arcobaleno ci guadagnano, perché ora sono arcobaleni veri.
- **Seconda correzione, la precisione**: il colore del profilo era un **indice di 17 colori** (0 = bianco, 1–16 la ruota a passi di 16), quindi
  qualunque tinta finiva sulla più vicina di sedici. Ora `CFG_LED_COLOR` è **la tinta stessa** (0 = bianco, 1–255 la ruota): quello che scegli
  esce, e rileggendolo torna uguale (`vialrgb.c`). I tasti Fn+U/I continuano a saltare di 16, come prima.
- I colori degli effetti sono a saturazione piena: un colore pastello dalla tavolozza esce come la sua tinta pura (documentato in `docs/vial.md`).
- Test aggiornati: la ruota (rosso/giallo/verde/ciano/blu/magenta), i passi dei tasti luce, il giro tinta → profilo → tinta.

Budget: flash **51321/56320**, XDATA 3720/4096. `make test` 27/27, `make sim` 24/24, `make stages` 1–3 ok.

## 2026-11-03 — il backlight che si spegne e si riaccende

- **Sintomo riportato**: ogni tanto la retroilluminazione si spegne e torna. Due cause possibili, e questa build le riduce entrambe:
- **Il salvataggio in flash** (la più probabile): la pausa di timer e backlight copriva **tutto** il salvataggio — quattro cancellazioni più
  ~2 KB programmati byte per byte. Ora copre **solo le cancellazioni**, che sono l'unica parte che blocca davvero il chip per millisecondi;
  la programmazione (uno stallo per byte, microsecondi) gira con il display acceso. Il buio passa da ~100 ms a ~20 ms.
- **Il ritiro del backlight** quando il main loop è affamato (`BACK_OFF_SLOTS`): era di **32 slot (13 ms)**, ora **12 (5 ms)** — abbastanza per
  far girare il loop, molto meno visibile.
- **Nuovo contatore diag** al posto di quello dei frame radio spediti (RX li racconta già): **`longest sector erase (us)`**, misurato con il
  Timer 2, che resta l'unico orologio acceso durante la cancellazione. Serve a sapere quanto dura davvero.
- **Per capire quale delle due è**, con la tastiera che lampeggia: `py .\diag.py --watch 1`. Se sale `backlight back-offs` è la fame del main
  loop; se non sale nulla ed è successo un salvataggio (un cambio di impostazione, o Vial che scrive), era la pausa del salvataggio.

## 2026-11-03 (mattina) — il tab Lighting, NKRO, Caps Word, Leader

### Perché Vial non mostrava il tab Lighting, e cosa può fare davvero
- **Causa**: l'app non guarda il byte 12 della risposta con l'id — guarda **il campo `"lighting": "vialrgb"` della definizione**
  (`keyboard_comm.py`: `self.lighting_vialrgb = self.definition["lighting"] == "vialrgb"`). Aggiunto a `board/vial.json`: il tab compare.
- **Seconda scoperta**: la Vial del 2026-11 **non ha il pittore per tasto**. I suoi comandi sono solo `get_info`, `get_mode`, `get_supported`,
  `set_mode`: niente `direct_fastset`, `get_number_leds`, `get_led_info` (esistono nel firmware di riferimento, aspettano un'app che li usi). Quindi
  il "colore per tasto" che avevo scritto l'app non lo usa: **tolto** (via i 160 B di XDATA e la sezione del deposito), e al suo posto
  `core/vialrgb.c` mappa i **sei effetti** che questa tastiera ha davvero sugli id che l'app conosce (0 spento, 2 tinta unita, 4 gradiente su-giù,
  5 gradiente sx-dx, 6 respiro, 18 anelli dal centro) e il colore sui diciassette della ruota. La scheda comanda effetto, luminosità, velocità e colore.
- I **6 layer restano** (decisi ieri per far posto al pittore): lo spazio liberato è andato a NKRO e Caps Word, e nel deposito restano ~340 B
  (utili per le sequenze del Leader, se vorrai averle modificabili).

### NKRO in USB
- Il report tastiera (ID 3) ora è la **bitmap NKRO**, e il 6KRO resta solo per il boot protocol (un BIOS non sa cosa farsene). `report.c` tiene la
  bitmap, `usb_reports.c` sceglie il formato, il descrittore dichiara 112 usages.
- **112 bit, non 120 come QMK**: il buffer hardware di EP1 è di **16 byte** (`EP1_BUF_SIZE` in `hal/sh68f90.h`) e l'ID ne prende uno; 112 bit
  arrivano a usage 0x6F e coprono lettere, cifre, F1–F12 e navigazione. `bcdDevice` 0x0202 → **0x0203**.
- Test nuovo: sette tasti insieme arrivano tutti in report protocol, e in boot protocol il settimo resta fuori (come deve).

### Caps Word e Leader
- **Caps Word** (`QK_CAPS_WORD_TOGGLE` 0x7C73, il numero di QMK): le lettere escono con Shift finché la parola non finisce — si spegne al primo
  tasto che non è lettera, cifra, meno, backspace o delete. L'app mostra il tasto solo se il firmware dichiara la feature: `core/vial.c` mette il
  bit 0 dell'ultimo byte della risposta delle voci dinamiche.
- **Leader** (`core/leader.[ch]` + `board/leader.c`, il tasto `B68_LEADER` 0x7E10 assegnabile da Vial, la definizione passa a 17 tasti
  personalizzati): premi il tasto, poi una sequenza; le sequenze stanno in `board/leader.c` (compile-time, come in QMK), 400 ms di silenzio la
  annullano. Due esempi: `Leader E S C` = Esc, `Leader M U T E` = mute.
- Test nuovi: `test_leader` (sequenza che agisce e non scrive, tasto fuori sequenza, timeout, tasto che disarma) e il caso Caps Word in
  `test_keyboard`. `test_usb_reports` e `test_sim` aggiornati all'NKRO.

### Batteria
- Niente di nuovo "in giro": il giro di ricerca non trova documentazione sul frame di stato della B68. Resta quello che c'è: byte dei LED
  **confermato** dal dump stock (`0x06B6`), byte della batteria **ipotesi** coerente (0xFF a carica piena = sveglia + 7 di 7, lo stesso layout del
  driver NuPhy). La prova è Fn+Backspace: se resta a 0 o a 7 per giorni, l'offset è un altro ed è una riga in `core/radio.h`.

Budget: flash **51111/56320** (5,2 KB liberi), XDATA **3720/4096**, deposito 1709/2048. `make test` 27/27, `make sim` 24/24, `make stages` 1–3 ok.

## 2026-11-03 — 8 → 6 layer, e Vial RGB per tasto

- **Layer 8 → 6** (decisione dell'utente): libera **320 B di XDATA e 320 B del deposito**, che era il muro per il colore per tasto. La coppia Mac
  passa da 7/6 a **5/4** (Windows resta 0/1); il tasto Fn del lato Mac usa `MO(4)`. Vial mostra 6 layer.
- **Vial RGB per tasto** (`core/vialrgb.[ch]`, `docs/vial.md`): il comando VIA di illuminazione (0x07/0x08/0x09) con i sottocomandi 0x40–0x44.
  Si offrono **due soli effetti**: 0 = spento e 1 = **diretto** (il pittore per tasto). `get_supported` dice la verità, quindi l'app mostra solo
  quelli; gli altri id sono di `rgb_matrix` di QMK, che qui non c'è (i nostri effetti stanno sui tasti Fn).
- I colori arrivano in HSV, vengono convertiti in RGB565 e tenuti **nella keymap** (una parola per cella, 80 celle = 160 B), quindi sopravvivono
  allo spegnimento; `lighting_save` scrive subito. L'effetto che li mostra è **`FX_VIAL`** (in fondo alla lista, raggiungibile anche con Fn+W).
  `get_number_leds` = 80 celle e `get_led_info` marca con 0xFF le celle che non sono tasti, così l'app non le dipinge.
- **`core/via.c`**: i tre comandi di illuminazione; **`core/vial.c`**: byte `[12] = 1` nella risposta con l'id, che dice all'app "qui c'è vialrgb".
- **`tests/host/test_vialrgb.c`** (26° test): info, effetti offerti, modo legato all'effetto del profilo, numero di LED e posizioni, pittura di due
  LED con verifica dei colori, salvataggio. Aggiornati test_via, test_hud, test_store, test_led, test_sim (6 layer).

Budget: flash **52655/56320** (3,7 KB liberi), XDATA **3794/4096** (302 B), deposito 2034/2048. `make test` 26/26, `make sim` 24/24,
`make stages` 1–3 ok, vettori ok.

## 2026-11-02 (tarda notte) — key overrides (Vial), e il deposito quasi pieno

- **`core/override.[ch]`**: fino a 4 voci nel formato di Vial (10 byte: trigger, replacement, layer, modificatori richiesti, modificatori che
  non devono essere premuti, modificatori da togliere mentre agisce, opzioni con bit 7 = abilitata). Shift+Backspace = Delete, per esempio; il
  trigger non arriva mai all'host e il suo rilascio rilascia il replacement. Le opzioni di QMK su *quando* si attiva sono lette e ignorate
  (nessuna cambia l'uso quotidiano).
- **`core/keymap.[ch]`**, **`core/store.c`**: le voci fanno parte della keymap salvata (20 parole, sezione `SEC_OVERRIDES`).
- **`core/vial.c`**: l'operazione 0 riporta anche il numero di key overrides; 5 e 6 leggono e scrivono una voce → **la scheda Key Overrides di
  Vial funziona**.
- **`core/keyboard.c`**: il gancio sta prima dei combos (un override cambia *cosa è* il tasto) e riceve i modificatori attuali.
- **`tests/host/test_override.c`** (25° test): senza i modificatori il tasto resta suo, il trigger diventa il replacement, un modificatore
  soppresso va e torna, i modificatori che non devono esserci, il vincolo di layer, la voce disabilitata, il rilascio di un altro tasto.
- **Spazio**: per far stare gli override i combos scendono da 8 a 6. Il deposito ora è **2029 byte su 2048 di slot** (19 liberi): la prossima
  cosa che va nel profilo richiede di togliere qualcosa (le macro da 512 a 384 B libererebbero 64 byte di parole).
- **XDATA 3960/4096 (136 B liberi)**: è il vincolo più stretto adesso.

### Vial RGB: protocollo documentato, e il muro che c'è davanti
- `docs/vial.md` ora ha anche la sezione **Vial RGB** letta dall'implementazione di riferimento: non passa dal prefisso `0xFE` ma dai comandi
  VIA di illuminazione (`0x07`/`0x08`/`0x09`), con i sottocomandi 0x40–0x44 e la tabella degli id degli effetti (0 spento, 1 diretto, 2 tinta unita,
  4/5 gradienti, 6 respiro, 13–20 cicli, …).
- **Non l'ho implementato** perché non ci sta: i colori per tasto sono 3 byte × 67 tasti = **201 B di XDATA** (liberi: 136) e altrettanti nello
  slot della keymap per sopravvivere allo spegnimento (libero: 14 B). Per farlo serve una decisione: **8 layer → 6** libera 320 B di XDATA e 320 B
  di slot, e sposta la coppia Mac da 7/6 a 5/4. Senza quella, l'unica parte fattibile è il "modo" (effetto, luminosità, velocità, hue/sat da Vial),
  che però duplica i tasti Fn già presenti.

Budget: flash **48897/56320**, XDATA **3960/4096**. `make test` 25/25, `make sim` 24/24, `make stages` 1–3 ok, vettori ok.

## 2026-11-02 (notte) — batteria e Caps Lock wireless: la fonte era il driver di riferimento

- **Trovato senza misurare**: il driver dello **stesso radio** per le NuPhy (`src/peripherals/bk3632/rf_controller.c` del porting) decodifica già i
  byte di stato — `battery_level = status[0] & 0x07` (tre bit, 0–7 livelli), `led_state = status[1] & 0x07` (num/caps/scroll), `connected` bit 3,
  `paired` bit 4, `low_power` bit 7, e il bit 7 dell'altro byte è "radio sveglia". La B68 fa la stessa cosa: nel dump stock, a `0x06B6`, il byte
  `b1` della risposta `02 06` viene copiato nella cella dei LED dell'host quando il selettore è su Bluetooth.
- **`core/radio.[ch]`**: `radio_battery_level()` (0–7, `RADIO_BATTERY_NONE` finché non arriva una risposta), `radio_low_power()`,
  `radio_host_leds()`. Gli offset sono due `#define` con il ragionamento scritto accanto: dalla risposta misurata (`00 01 00 03 ff 03`, carica
  piena, 2.4G) il byte 4 leggeva `0xff` (sveglia + livello 7 di 7) e il byte 1 portava i LED, coerente con quanto fa lo stock.
- **`board/battery.c`**: la percentuale è `livello × 100 / 7` (passi pari, perché il radio non dice a che tensione corrispondono), e il flag di
  batteria critica del radio rende la ghiera rossa qualunque sia il livello. In modalità USB senza radio la ghiera dice "sconosciuta" invece di
  inventare un numero.
- **`board/led_app.c`**: il **Caps Lock dell'host** in wireless ora arriva dalla radio (`radio_host_leds()`), dal cavo dal report dei LED: il tasto
  Caps si illumina anche in 2.4G e Bluetooth.
- **`tests/host/test_radio.c`**: risposta di stato costruita a mano → livello 5, caps lock, batteria critica, num lock: la decodifica è coperta.
- Resta un dubbio dichiarato: se la ghiera resta a 0 o ferma a 7 per giorni, l'offset del byte batteria è un altro — è una riga sola in
  `core/radio.h`. Un'occhiata a Fn+Backspace in wireless lo dice subito.

Budget: flash **47163/56320**, XDATA **3908/4096**. `make test` 24/24, `make sim` 24/24, `make stages` 1–3 ok, vettori ok.

## 2026-11-02 — combos (Vial), e la CI che falliva

### Combos: più tasti insieme danno un altro tasto
- **`core/combo.[ch]`**: fino a 8 accordi da 4 codici l'uno (il formato di Vial, `input[4]` + `output`, 10 byte). Un tasto che è ingresso di un
  accordo configurato viene **trattenuto al massimo 50 ms** in attesa del resto: se l'accordo si chiude i tasti non arrivano all'host e l'uscita
  viene premuta (e rilasciata quando li si lascia), altrimenti escono normalmente. Solo corrispondenze esatte, un accordo alla volta. Un tasto che
  non è ingresso di nessun accordo **non paga niente**: passa subito.
- **`core/keymap.[ch]`**: i combos sono parte della keymap salvata (8 × 5 parole = 80 B), con i predefiniti dalla scheda (nessuno).
- **`core/store.c`**: nuova sezione del payload (`SEC_COMBOS`) fra tap dance e impostazioni: il payload passa da 1929 a 2009 byte, dentro lo slot da
  2048. **Una tantum**: cambiando l'impronta dei predefiniti, la keymap salvata riparte dai default un'altra volta.
- **`core/vial.c`**: l'operazione 0 della richiesta dinamica riporta anche il numero di combos; le operazioni 3 e 4 leggono e scrivono una voce
  (10 byte), come da `docs/vial.md`. La scheda **Combos** di Vial adesso funziona.
- **`core/keyboard.c`**: il motore è chiamato fra il tap-hold e l'invio del tasto, e `combo_task()` nel giro; `combo_hook_send()` manda l'uscita
  dell'accordo per la stessa strada di un tasto normale.
- **`tests/host/test_combo.c`** (24° test): l'accordo che scatta e i suoi tasti che non arrivano, il termine che li manda, un tasto rilasciato prima,
  un tasto estraneo che svuota la coda nell'ordine giusto, un accordo da tre, un combo senza uscita, lo stesso tasto due volte.
- **`tests/host/test_via.c`**, `test_store.c` (limite dei punti di interruzione ora calcolato dallo slot), stub `keymap_default_combo` negli altri test.

### CI: falliva il job della simulazione, non il firmware
- `tests/sim/test_sim.py`: il test degli slot aspettava 900 scansioni (≈2 s di tempo simulato) prima di misurare; su un runner GitHub più lento di
  questa macchina lo `run()` superava il timeout del socket e il test falliva per timeout, non per un conteggio sbagliato. Ora aspettano 320
  scansioni (≈0,55 s, sempre oltre il mezzo secondo dopo cui il backlight è permesso) e `tests/sim/oblio_sim.py` ha il timeout a **180 s** invece
  di 30. `make sim` scende da 56 a 45 s su questa macchina.

### Diagnostica: la lettura della radio non richiede più lo sblocco
- `core/via.c`: il comando `0xF1` kind 1 (i 6 byte di stato e l'ultimo frame `08`) è **solo lettura**, quindi risponde anche a tastiera bloccata,
  come i contatori e il readout dei port. Lo sblocco serve solo al kind 2, che cambia cosa fa la radio. `tools/diag.py --radio` non chiede più
  Esc+Invio: è la misura della batteria senza attrito.

Budget: flash **46995/56320** (9,3 KB liberi), XDATA **3907/4096** (189 B liberi). `make test` 24/24, `make sim` 24/24, `make stages` 1–3 ok,
vettori ok.

## 2026-11-02 — mouse dall'encoder (prima delle feature rimaste)

- **`core/mouse.[ch]`**: i codici mouse di QMK (`KC_MS_UP/DOWN/LEFT/RIGHT`, `KC_MS_BTN1..5`, `KC_MS_WH_UP/DOWN/LEFT/RIGHT`, `KC_MS_ACCEL0..2`,
  0xCD–0xDF, gli stessi numeri di QMK così Vial li conosce). Una direzione tenuta muove il cursore a passi di 8 ms che si allungano tenendola
  premuta (fino a 8 unità) e con `MS_ACCEL1/2`; la ghiera scorre (un tocco = uno scatto, tenuta = scatti ogni 48 ms); i pulsanti tengono il loro bit.
  **Questo è ciò che rende l'encoder una rotella**: in Vial basta assegnare `MS_WH_UP` / `MS_WH_DOWN` alle due direzioni.
- **`core/report.[ch]`**: stato del mouse (pulsanti, X/Y, ghiera) e `report_build_mouse()`: le due assi sono **relative**, quindi il report le
  consuma (un movimento non viene mandato due volte) e i delta si fermano a ±127 per non traboccare.
- **`core/usb_desc.[ch]`**: collezione **Mouse** sull'interfaccia 0, report ID **7**, come quella del firmware stock: 5 pulsanti, X e Y a 16 bit
  relativi, ghiera. `bcdDevice` 0x0201 → **0x0202** (Windows tiene in cache i descrittori).
- **`core/keyboard.c`**: i tasti mouse passano da `mouse_process()` (con il trattamento one-shot come gli altri tasti) e `mouse_task()` nel giro.
- **Radio**: il protocollo non ha un frame per il mouse, quindi in 2.4G/BT i tasti mouse non fanno niente (documentato in `board/transport.c`).
- **`tests/host/test_mouse.c`** (23° test): passo immediato e ripetizione, accelerazione, ghiera a tocco e tenuta, maschera dei pulsanti,
  consumo dei delta, e che un tasto non-mouse non venga toccato.
- Da fare per provarlo: in Vial, sull'encoder, mettere `MS_WH_UP`/`MS_WH_DOWN` (oppure `MS_UP`/`MS_DOWN` per il cursore).

Budget: flash **44945/56320** (11,4 KB liberi), XDATA 3788/4096. `make test` 23/23, `make sim` 24/24, `make stages` 1–3 ok, vettori ok.

## 2026-11-02 — via i profili: 16 KB di flash tornano al codice

- **La decisione**: i 4 profili occupavano 0xAC00–0xEBFF, cioè **16 KB di indirizzamento**, e nessuno li usava. Con **una sola keymap** (con le sue
  due copie A/B, come una QMK normale) il deposito si sposta in fondo — **0xDC00–0xEBFF** — e il codice passa da 44.032 a **56.320 byte**: +12,3 KB.
  Flash usata: **43528/56320** (era 43935/44032, 97 B liberi).
- `core/store.[ch]`: via `STORE_PROFILES`, `store_request()`, `store_profile()`, `switch_profile()`, il profilo in uso e il suo campo nell'header;
  `store_init()` non prende più il profilo. `hal/flash.h` e il Makefile: `FLASH_WRITE_MIN` e `CODE_LIMIT` a 0xDC00.
- `core/settings.[ch]`: tolto `settings.profile` (il byte 3 del record resta, ignorato e riscritto a 0, così i record vecchi si leggono ancora).
- `board/b68_keys.h`, `board/vial.json`, `board/keymap_default.c`: via i quattro tasti `B68_PROF_1..4`; i codici successivi si rinumerano
  (0x7E07–0x7E0F) perché `check_vial.py` vuole la lista senza buchi, e la definizione Vial passa da 20 a **16 tasti personalizzati**. Sul layer Fn
  le posizioni Z X C V tornano trasparenti.
- `core/led.[ch]`, `board/led_app.c`: via il flash del tasto profilo (`led_hook_profile`, `LED_PROFILE_FLASH`).
- Test host aggiornati (store, settings, led): 22/22. `make sim` 24/24, `make stages` 1–3 ok, vettori ok.
- **Una tantum**: la keymap salvata riparte dai default (cambia l'indirizzo del deposito) e un layout Vial che usava i tasti profilo va rifatto.

## 2026-11-01 (notte fonda) — polarità dello switch corretta, e i due lati diventano due tastiere

- **Polarità:** provato sulla tastiera, era al contrario. Adesso **Mac = P0.5 alto**, Windows = basso (`OS_SWITCH_IS_MAC` in
  `board/os_switch.c`, una riga da girare se un'altra scheda si comporta all'opposto). Le due catture di `diag.py --ports` da sole non potevano
  dire quale posizione fosse quale: lo ha detto la tastiera.
- **Due tastiere in una:** il lato Mac non è più solo uno scambio di Alt/GUI. `core/keymap.h`: `KEYMAP_MAC_LAYER` = 7 (base) e
  `KEYMAP_MAC_FN_LAYER` = 6 (il suo Fn). Su Windows si usano i layer 0 e 1, su Mac i layer 7 e 6; il tasto Fn del lato Mac manda a `MO(6)`
  invece che a `MO(1)`. Il layer 6 parte come copia del layer Fn di Windows e il layer 7 come copia del base con Alt e GUI scambiati: **tutti e
  quattro sono modificabili da Vial**, quindi il lato Mac può avere un Fn completamente diverso senza toccare il lato Windows.
- `tests/host/test_default_keymap.c`: controlla i tre modificatori scambiati, il Fn che va al layer 6, il layer 6 copia dell'1 e che Fn+B resti
  il tasto Bluetooth anche sul lato Mac.

Budget: flash **43935/44032** (97 B liberi), XDATA **3777/4096**. `make test` 22/22. **Il margine di flash è agli sgoccioli**: la prossima
funzione richiede di togliere qualcosa (vedi `docs/firmware-report.md` §9).

## 2026-11-01 (notte) — lo switch Win/Mac: trovato, ed è una funzione

### Dov'è e come funziona (misurato, non ipotesi)
- Due letture di `diag.py --ports` con lo switch nelle due posizioni differiscono per **un solo bit**: **P0.5** (1 con Windows, 0 con Mac).
  P7.0, il candidato che veniva dal dump, resta alto in entrambe e non viene mai tirato basso da nessuna colonna: non è lui.
- Nel firmware stock, `0xA9D9` fa esattamente il debounce di P0.5 (due contatori, dieci letture uguali di fila, stato in `XDATA[0x0307]`) e
  quello stato sceglie fra **due insiemi di tabelle della keymap** (`DPTR = 0x7Cxx + 4 × indice` dopo aver testato `0x0307`): il venditore
  usa lo switch per cambiare il layer di base, con una variante Mac. È la stessa idea di quello che abbiamo fatto.
- `diag.py --ports` ora riporta solo i byte grezzi dei port (letti con timer e backlight fermi, così gli strobe non falsano P4/P5/P6).

### Cosa fa adesso
- `board/os_switch.[ch]`: legge P0.5, debounce 50 ms come il selettore di modo, e chiama `layer_set_default(KEYMAP_MAC_LAYER)` su Mac,
  `layer_set_default(0)` su Windows. Al cambio il layer non viene salvato: lo switch è la verità, il profilo resta uno solo.
- `core/keymap.h`: `KEYMAP_MAC_LAYER` = 7. `board/keymap_default.c`: quel layer è la copia del base con **LGUI↔LALT e RGUI↔RALT** scambiati
  (un solo bit, `^ 1`), tutto il resto identico, encoder compreso. Quindi è **modificabile da Vial come qualunque altro layer** (compare come
  layer 7); il layer Fn (1) resta valido su entrambi i lati dello switch.
- `board/main.c`: `os_switch_init()` dopo il caricamento del profilo e `os_switch_task()` nel giro; `board/store_hw.c`: un profilo caricato
  azzera i layer, quindi `os_switch_apply()` li riapplica subito.
- `tests/host/test_default_keymap.c`: controlla che il layer Mac sia il base con i tre modificatori scambiati e nient'altro.

### Attenzione
- Cambiando la tastiera predefinita cambia l'impronta salvata nei profili: **al primo avvio dopo questo aggiornamento la tastiera riparte dai
  valori predefiniti** (è il comportamento documentato in `docs/keyboards/yunzii-b68.md`, "Tastiera salvata e firmware nuovo"). Eventuali
  modifiche fatte da Vial vanno rifatte una volta.

Budget: flash **43838/44032** (194 B liberi), XDATA **3771/4096**. `make test` 22/22.

## 2026-11-01 (sera, dopo la prova dello sleep: LED morti al risveglio)

### Il difetto segnalato: al risveglio i LED restano spenti fino al power cycle
- **Causa (dal codice, non un'ipotesi):** `sleep_and_wake()` richiama `led_hw_init()` per rimettere a posto i canali PWM, e quella funzione
  azzera il permesso di accendere (`allowed`). `led_app_task()` confrontava il permesso con **una propria copia** (`static bool last`), che
  restava `true`: `on == last`, nessuno riaccendeva, e il backlight rimaneva spento fino al riavvio.
- `board/led_hw.[ch]`: nuovo `led_hw_enabled()`; `board/led_app.c` confronta con lo stato vero del driver, quindi dopo ogni
  re-inizializzazione il backlight torna su da solo.
- **Da riprovare sull'hardware**: Fn+Tab (o attesa) in 2.4G/BT e risveglio con un tasto.

### Sonno: altri due difetti della stessa area
- `board/sleep_hw.c`: il conteggio dell'inattività riparte solo sugli **eventi** di un tasto, quindi un tasto tenuto fermo non lo azzerava e la
  tastiera poteva addormentarsi sotto il dito: ora `sleep_hw_task()` non dorme se `matrix_any_pressed()`.
- `board/sleep_hw.c`: al risveglio lo stato del report viene azzerato (`report_clear_keys/mods`). Prima un tasto premuto al sonno e rilasciato
  durante il sonno poteva restare «appeso» all'host radio (nessun evento, nessun report).

### Salvataggio in flash
- `core/store.[ch]`, `board/store_hw.c`: la pausa di timer e backlight è **una sola per tutto il salvataggio** (`store_hook_flush_begin/end`)
  invece di una per ognuno dei ~30 blocchi da 64 byte: meno sfarfallio del backlight, meno lavoro, e il clock perde un intervallo solo.
- `board/store_hw.c`: `watchdog_kick()` prima di ogni cancellazione e di ogni programmazione. Il main loop, che è quello che lo calcia, resta
  bloccato qui dentro: un erase lento poteva far resettare la tastiera a metà salvataggio.
- `core/settings.[ch]`: le impostazioni (profilo in uso, tempo di sonno, slot BT) sono ora un **log nel solo settore 0xEC00**: ogni salvataggio si
  aggiunge alla prima cella cancellata (56 record da 9 byte) e vince l'ultimo record completo. Un salvataggio non cancella più niente, quindi una
  caduta di corrente lascia le impostazioni precedenti invece dei valori predefiniti, e il settore si cancella una volta ogni 56 salvataggi invece
  che ad ognuno (usura). **Nota:** la prima idea era una seconda copia a 0xEE00, ma quel settore è quello del *marker* del bootloader (la `LJMP` a
  0xEFFB: `docs/architecture.md` del porting SMK): non si tocca.

### Altri difetti corretti
- `hal/usb.c`: i tre buffer dell'interrupt USB (`SETUP` 8 B, EP0 8 B, raw HID 32 B) erano **locali**: con `--stack-auto` finivano sullo stack
  interno, che aveva ~38 byte liberi, sopra quello che stava facendo il main loop. Ora sono statici (vanno in XDATA).
- `core/usb_desc.c`: il report consumer dichiarava usages fino a 0x023C, ma `KC_MCTL` (0x29F) e `KC_LPAD` (0x2A0) stanno oltre: un host li
  scarta. Ora 0x03FF. `bcdDevice` 0x0200 → **0x0201** (Windows tiene in cache i descrittori per VID:PID:bcdDevice).
- `core/report.[ch]`: tolto il codice NKRO morto (bitmap mantenuta e mai inviata, `report_build_nkro`, `report_key_pressed`): 16 byte di XDATA e
  un po' di flash. Il limite vero (6 tasti in USB) è ora scritto nel commento.
- `core/radio.c`: la coda dei report passa da 4 a 8 stati, così una sequenza rapida premuto/rilasciato/premuto non sovrascrive uno stato
  intermedio.
- `board/main.c`: `matrix_any_pressed()` una volta per giro invece di due (era il 37% del costo di un giro secondo il report).
- `core/vial.c`: tolta una condizione sempre falsa in `get_definition()`.
- `hal/power.c`: tolta `woke_by_int4` (scritta e mai letta).
- Codice morto rimosso per fare spazio: `crc16_word` (usa due volte `crc16_byte`), `matrix_deferred()` e il suo contatore, `led_render_all`
  (sostituita da `led_refresh()`, il test pompa `led_task()`), `clamp()` delle impostazioni (in linea nel caricamento), e tre contatori
  diagnostici che avevano risposto alla loro domanda (`DIAG_MS_PER_50_SOF` con `tick_ms_now()`, `DIAG_USB_IRQS`, `DIAG_NO_PASS`).
- `tests/host`: aggiornati i test toccati (report, radio, impostazioni con le due copie, led, matrice, diag) e i nuovi stub dei hook.

### Diagnostica per lo switch Win/Mac (indagine, non ancora una funzione)
- `core/diag.[ch]` kind 3 + `board/diag_hw.c`: la risposta riporta i byte grezzi di **P0…P7** e la maschera delle colonne in cui **P7.0** legge
  basso. P7.0 è l'unico ingresso che il firmware stock legge insieme alle righe della matrice senza essere una riga (dump: `mov a,P7; anl a,#0F`
  alle due letture di conferma della ISR del Timer2), ed è quindi il candidato per lo switch. La maschera distingue uno switch verso massa (basso
  in tutte le colonne) da uno cablato su una colonna (una sola cella).
- Il comando non richiede lo sblocco Vial (come i contatori); `tools/diag.py --ports` lo legge.
- **Cosa serve dall'utente:** `py tools\diag.py --ports` (o `python3 tools/diag.py --ports` su Linux) con lo switch in **entrambe** le posizioni.

### Ultime rifiniture della stessa sessione
- `core/led.c`: le impostazioni dell'effetto (effetto, luminosità, colore) si leggono **una volta per repaint** invece di tre `keymap_cfg_get()`
  per ognuno degli 8 LED di un giro del main. Contro ogni aspettativa fa **risparmiare flash** (-140 B: SDCC non le ricopia in linea nel percorso del
  disegno) e toglie lavoro al main loop.
- `board/led_app.c`: il «dopo 500 ms senza host» ora è un flag che si chiude una volta invece di `now >= 500`: con l'orologio a 16 bit che gira
  ogni 65 s, il confronto spegneva il backlight per mezzo secondo a ogni giro quando non c'era host.
- `board/sleep_hw.c`: prima di dormire si scrivono i profili e le impostazioni in sospeso (`store_flush`/`settings_flush`): su batteria la tastiera
  può non tornare, e una modifica fatta mezzo secondo prima del sonno andrebbe persa.
- `.gitignore` + `git rm --cached`: `tests/sim/__pycache__/` non è più tracciato (era nel repo per errore).

Budget: flash **43718/44032** (314 B liberi), XDATA **3767/4096**. `make test` 22/22, `make check-interrupts` ok, `make sim` 24/24
(sottoquadro 433, scansione 8305 istruzioni; stack usato nel simulatore 87 byte su 223).

## 2026-11-01
- Repo creato. Documenti hardware, radio e LED portati dal porting SMK (`docs/`).

### Correzione dopo la prova del gradino 2 (sull'hardware: LED che lampeggiano, nessun tasto)
- **Causa (ipotesi forte, dal simulatore):** il lavoro dell'interrupt non stava nei suoi slot. Con `make sim` il sottoquadro LED costava 4697 istruzioni e la scansione 8963, contro slot da 400/450 µs: il main loop non girava più (niente tasti, LED a scatti, poi reset del watchdog). Lo stesso codice LED era nella build finale, quindi andava corretto, non aggirato.
- `board/led_hw.c`: il percorso dell'interrupt non calcola più nulla né cicla: scrive indirizzi costanti (macro `LED_PWM`/`SWITCHED`) leggendo i DUTY2 già pronti. `core/led.[ch]`: tabella `duty2` precalcolata (+480 B di XDATA, `LED_DUTY1_BASE` = 195). Sottoquadro: 4697 → 433 istruzioni.
- `core/sched.h`: slot di scansione 500 µs (era 450); scansione 8963 → 8305 istruzioni (~6400 sono le attese volontarie di 320 µs).
- `hal/tick.[ch]`: se il main loop non gira per 3 scansioni di fila (`tick_alive()`), i LED cedono il posto per 32 slot, così tasti, USB e watchdog hanno la CPU anche se qualcosa va storto.
- `core/via.c`, `tools/diag.py`: i contatori diagnostici (causa reset, slot in ritardo, scansioni perse, giro più lento) si leggono senza sblocco: servono proprio quando i tasti non funzionano. Radio/monitor richiedono ancora lo sblocco.
- `tests/sim`: nuovo guard sul numero di istruzioni di sottoquadro e scansione (limiti 1200 / 8500); test sul diag aggiornato. Non provato sull'hardware.

### Led che flickerano in modo USB (wireless OK)
- Segnalato dall'utente: con il cavo i LED flickerano forte, in 2.4G e BLE senza cavo va tutto. Causa **non nota**: due sospetti, da distinguere con i dati e non con tentativi alla cieca: (1) il salvagente del main loop spegne i LED perché con il cavo il giro del main è lento; (2) il backlight viene acceso/spento dal flag di sospensione USB (`led_app_task`).
- Due contatori nuovi nella diagnostica: `DIAG_BACKOFF` (volte che il salvagente è scattato) e `DIAG_LED_TOGGLES` (cambi di permesso del backlight). Al posto di `DIAG_FLASH_ERASES` (il pacchetto da 32 B tiene al massimo 14 contatori). Flash 43493/44032.

### Led che flickerano in USB: primi dati dalla tastiera, seconda strumentazione
- Lettura di `diag.py` col cavo: nessun reset (causa 0), 45988 slot in ritardo, 19643 scansioni perse, giro più lento del main 15 ms, stack 187 su 223, **130 back-off**, solo 3 cambi di permesso del backlight. Quindi il sospetto «flag di sospensione che oscilla» è escluso; il lavoro dell'interrupt non sta nel tempo in modo USB (in wireless sì). La causa resta da trovare.
- `tools/diag.py`: funziona anche su Windows/macOS con `hidapi`.
- Strumentazione nuova: durata massima misurata dal Timer 2 (2 MHz) di uno slot LED e di uno slot di scansione, dall'overflow che lo ha iniziato alla fine del lavoro (`DIAG_LED_US_MAX`, `DIAG_SCAN_US_MAX`; se lo slot è in ritardo include il ritardo), e conteggio degli interrupt USB diversi dal SOF (`DIAG_USB_IRQS`). Tolti i contatori radio «rifiuti» e «TX scaduti» (a zero) e i cambi di permesso. Flash 43672/44032 (margine 360 B).

### Led in USB: seconda lettura e conteggi separati
- Seconda lettura (il `diag.py` locale dell'utente era una copia vecchia, i nomi erano sfasati; qui i valori giusti): slot LED più lungo **409 µs** (previsti 400), slot di scansione più lungo **816 µs** (previsti 500), 736 interrupt USB non-SOF, 4 back-off, 13769 slot in ritardo, 5330 scansioni perse, giro del main 17 ms.
- Nel simulatore (`usbcost`) `usb_isr` costa 688–1801 istruzioni per evento (SET_ADDRESS … raw HID OUT): su questo core sono fino a ~200 µs, abbastanza per spiegare un picco di 816 µs sulla scansione quando arriva all'inizio dello slot. Ma un massimo sporcato da un evento isolato non dice cosa succede a regime: **causa del flicker ancora non dimostrata**.
- `DIAG_SLOT_LATE` conta solo gli slot LED in ritardo; nuovo `DIAG_SCAN_LATE` per le scansioni; tolto `DIAG_RADIO_EVENTS`. Con `diag.py --watch 1` si vedono i tassi al secondo mentre i LED flickerano. Flash 43674/44032.

### Led in USB: la scansione sfora sempre (terza lettura)
- A tastiera ferma col cavo (`diag.py --watch 1`): ~479 scansioni in ritardo al secondo, cioè **tutte** (una ogni 2,1 ms); slot LED in ritardo: 1 in tutto; ~185 scansioni perse al secondo; 2 interrupt USB non-SOF al secondo (le richieste di diag). La scansione dura ~800 µs, non i ~440 attesi (il porting SMK la misurò a ~0,35 ms). Un slot LED che segue una scansione così lunga parte in ritardo e resta acceso molto meno: ogni 5 slot una colonna dimezzata, da qui il flicker. Era anche la causa del gradino 2 (slot di scansione 450 µs).
- Non si sa ancora perché dura il doppio e mezzo, né perché non si veda in wireless. Ipotesi da verificare: il Timer 2 non conta ai 2 MHz assunti (allora i miei «µs» non sono µs reali) oppure le istruzioni durano più di quanto assunto. Nuovo contatore `DIAG_MS_PER_50_SOF`: i millisecondi del nostro orologio che passano in 50 SOF dell'USB (1 ms esatto ciascuno): 50 se il timer va come assunto. Al posto di `DIAG_MATRIX_DEFERRED`. Flash 43720/44032.

### Flicker in USB: tempi degli slot come in SMK (piano approvato)
- Causa dell'effetto (dai dati di `diag.py --watch 1`): la scansione dura ~800 µs e **ogni** scansione sfora lo slot da 500 µs della griglia fissa; lo slot LED seguente parte tardi e resta acceso ~100 µs invece di 400: una colonna su cinque dimezzata, a ~100 Hz. Confronto con SMK (`systick.c`, `tick.c`): lì il Timer 2 è **riavviato all'inizio di ogni ISR**, e la scansione è armata corta (~100 µs): lo slot LED dopo parte subito alla fine del lavoro ed è sempre di 400 µs interi. Anche il `delay_us` è identico.
- `core/sched.[ch]`: `sched_step(elapsed, &ms)` prende il tempo realmente trascorso in conteggi del timer (2000 per ms) e restituisce i millisecondi interi passati (0 o più); lunghezza armata per la scansione = massimo di sicurezza 1000 µs; test host riscritti (ms esatti con scansioni di durata qualsiasi, scansione lunga, nessun ms perso).
- `hal/tick.c`: ogni ISR legge il contatore (latenza), riavvia il Timer 2 per il proprio slot, e alla fine della scansione forza subito l'overflow, così lo slot seguente parte appena finita. L'orologio in millisecondi usa il tempo reale (lunghezza dello slot + latenza), non i valori nominali. `DIAG_SCAN_US_MAX` ora è la durata vera del lavoro di scansione. Flash 43814/44032 (margine 218 B).
- **Non provato sull'hardware.** Resta da spiegare perché la scansione duri ~800 µs e non ~440; non cambia l'esito, ma la cornice dei LED non è più fissa (~9,6 ms con scansione da 800 µs). Prova: `diag.py` → `longest scan slot` (durata vera), `own ms per 50 USB SOF` (deve dare 50).

### Dopo la correzione dei tempi: LED stabili, scansioni perse in aumento
- Prova dell'utente (col cavo): **LED stabili**. Contatori: slot LED/scansione in ritardo 0, back-off 0, slot LED più lungo 93 µs, scansione più lunga 609 µs (durata vera), giro più lento del main 7 ms, 49 ms ogni 50 SOF (il Timer 2 va ai 2 MHz assunti, al più con un errore del 2%). **Scansioni perse in aumento** (3910 poi 14594), ~40% come prima della correzione: il main non prende ogni scansione.
- Nel simulatore il giro del main costa ~1170 istruzioni (~180 µs sul chip), quindi in teoria prende ogni scansione molte volte: manca il dato hardware. Nuovi contatori al posto di quelli radio (a zero in USB): giri del main tra le ultime due scansioni (`DIAG_PASSES_LAST`) e scansioni dopo le quali il main non è passato nemmeno una volta (`DIAG_NO_PASS`). Flash 43803/44032.

### Scansioni «perse»: non sono un guasto (confronto con SMK)
- Lettura con `--watch 1`: ~145 scansioni sovrascritte al secondo su ~453 (circa un terzo), e quasi tutte coincidono con «main non passato nemmeno una volta tra due scansioni»; giri del main tra due scansioni: 0, 1, 4, 5. Slot in ritardo 0, LED stabili.
- SMK ha la stessa semantica: la ISR scrive lo **stato** della matrice in `matrix[]` e alza `matrix_updated`; `matrix_task()` prende uno snapshot e lo confronta con il precedente («a transition is split across two main-loop iterations, never lost»). Una scansione sovrascritta non perde nessun tasto: SMK semplicemente non le conta. In Oblio `matrix_feed` è lo stesso modello (stato + debounce su 3 scansioni lette): con un terzo di scansioni saltate il debounce dura alcuni ms in più (~10 ms nel caso peggiore), nient'altro.
- Il contatore `DIAG_SCAN_LOST` e quello «senza passaggio» sono rinominati «(harmless)» in `diag.py`; `docs/first-boot.md` non chiede più 0 scansioni perse. Il giro del main resta lento (0–5 passaggi per scansione, ~400+ µs l'uno sul chip, contro ~180 stimati): non è un problema finché i tasti rispondono, e va rivisto solo se la latenza si sente.

### Report per altri agent
- Nuovo `docs/firmware-report.md` (inglese): cosa fa il firmware, hardware, struttura, comandi, stato verificato/non verificato, cosa è andato storto sull'hardware e perché, diagnostica, punti aperti in ordine di priorità (prova funzionale, interruttore Win/Mac, sorgente della batteria, scansione da ~610 µs, main loop lento, margine flash 229 B), regole di lavoro con l'utente.
- `docs/architecture.md`: il paragrafo sui tempi descriveva ancora la griglia fissa (slot di scansione 450 µs, auto-reload); riscritto per lo schema attuale (slot temporizzati dall'inizio della propria ISR, ms dal tempo reale). `CLAUDE.md` rimanda al report.

### Fase 1 — build, budget, release
- `board/main.c`: `main` segnaposto (loop vuoto) per esercitare compilatore, linker e budget; lo sostituisce la fase 2.
- `Makefile`: nuovi target `fw` (SDCC → `build/oblio.ihx` e `build/oblio.hex` con `packihx`), `size` (controllo budget); `clean` rimuove anche `build/`. Flag SDCC ripresi dal vecchio repo: modello large, `--stack-auto`, XDATA 4096, codice fino a 0xAC00.
- `tools/budget.py`: legge il `.mem` del linker, stampa «flash usata/limite, XDATA usata/dimensione» ed esce con errore se uno dei due è superato.
- `.github/workflows/ci.yml`: dopo i test host eseguono `make fw` e `make size`; l'hex è caricato come artifact `oblio-hex`; nuovo job `release` che sui tag `v*` allega `oblio.hex` alla Release.
- Verifica locale: `make test` (0 test), `make fw size` → flash 174/44032, XDATA 0/4096; il controllo fallisce correttamente con un limite troppo basso. Simulatore non usato. CI su GitHub non verificata.

### Fase 2 — HAL minimo e matrice 5×16
Si procede senza aspettare l'ICP, riscrivendo ciò che il porting SMK già fa; la prima prova sulla tastiera avverrà a riscrittura completa e stabile.
- `hal/sh68f90.h`: solo i registri usati (porte 0–7 con direzione e pull-up, drive, watchdog, clock/PLL, Timer 2, EA/ET2); indirizzi del chip, gli stessi di dump e porting.
- `hal/gpio.h`, `hal/watchdog.h` (kick compilato solo con `-DWATCHDOG_ENABLE`, come nel porting), `hal/clock.[ch]` (PLL a 24 MHz), `hal/delay.[ch]` (attese in µs/ms con un ciclo da 24 cicli: ipotesi da misurare con l'oscilloscopio).
- `hal/tick.[ch]`: tick da 1 ms con Timer 2 (2000 conteggi a 2 MHz, ricavato dai 801 conteggi = 400 µs del porting). L'interrupt incrementa solo un contatore (niente chiamate: evita la collisione di overlay di SDCC); il `main` consuma i tick con `tick_take()`, quindi un giro lento si recupera. Il vettore è dichiarato in `tick.h` perché SDCC lo cerca nel modulo del `main`.
- `core/matrix.[ch]` (C puro): debounce per colonna (un valore nuovo vale dopo `MATRIX_DEBOUNCE_SCANS` = 5 scansioni uguali di fila), coda di 16 eventi `{col,row,pressed}`. Con la coda piena il cambio non si perde: resta in sospeso e si accetta alla scansione dopo (`matrix_deferred()` conta i rinvii).
- `board/matrix_hw.[ch]`: stato dei pin a riposo uguale allo stock (`board_gpio_init`) e scansione delle 16 colonne con tabelle di maschere per porta (niente `switch` da 16 casi); doppia lettura delle righe, colonna scartata se le due letture differiscono.
- `board/recovery.[ch]`: Esc+1+2 tenuti per 6 campioni da 10 ms, poi salto a 0xFF00 con A=0x5A, B=0xA5 e interrupt spenti. Chiamato subito dopo clock e GPIO, prima di tick/USB/radio. **Non provato sull'hardware.**
- `board/main.c`: sostituisce lo stub; per ora gli eventi si scartano (li userà la fase 3).
- `tests/host/test_matrix.c` e `tests/host/check.h`: 7 test (pressione/rilascio dopo il debounce, rimbalzi, glitch che riavvia la finestra, due tasti nella stessa colonna e ordine, colonne indipendenti, bit oltre l'ultima riga, coda piena senza perdite).
- `Makefile`: i sorgenti del firmware sono `core/`, `hal/`, `board/`; `test_X.c` si compila con `core/X.c`.
- Verifica: `make test` 1 file, 7 test OK; `make fw size` → flash 1821/44032, XDATA 101/4096; vettori controllati nell'hex (0x0000 → startup, 0x0003 → `tick_isr`). Simulatore non usato. Non verificati: periodo del tick, ritardi, recupero, timing della scansione (tutto richiede l'hardware). Non portati ancora: LDO per l'USB, trappola per interrupt inattesi.

### Fase 3, passo 1 — codici tasto, keymap in RAM, layer
- `core/keycodes.h`: codici a 16 bit con gli stessi intervalli di QMK/Vial (servono già alla fase 5), solo ciò che si usa: intervalli, macro MO/TO/TG/DF/OSL/OSM/TT/TD/LT/MT/LM con relativa decodifica, maschere dei modificatori, tasti della mappa predefinita.
- `core/keymap.[ch]`: 8 layer × 5×16 e i due codici di rotazione dell'encoder per layer, in RAM (1280 B + 32 B); parte dalla mappa predefinita fornita dalla scheda; contatore di versione per le modifiche.
- `core/layer.[ch]`: layer attivi sopra il layer predefinito, trasparenza, MO/TO/TG/DF/OSL/OSM/TT/LM; il codice trovato alla pressione è quello usato al rilascio (nessun tasto resta bloccato se il layer cambia nel frattempo). Il tempo entra come argomento (`now`), il core non legge nessun timer.
- `core/keyboard.[ch]`: collega gli eventi della matrice ai layer; cerca il tasto *prima* di `layer_key_pressed()` perché un OSL deve ancora essere attivo per il tasto che aspetta. Gli hook dei modificatori sono vuoti finché non c'è il report (passo 2).
- `core/platform.h`: unica differenza tra 8051 e host per il codice puro (`ROM` = `__code` su SDCC).
- `board/keymap_default.c` e `board/b68_keys.h`: mappa predefinita della B68 (base + Fn, encoder = volume, tap dance 0 sul tasto dell'encoder) e codici personalizzati 0x7E00–0x7E13, stessi numeri del porting SMK.
- `board/main.c`: contatore dei millisecondi, gli eventi della matrice vanno a `keyboard_event`.
- Test: `test_layer.c` (11 test: ricerca, trasparenza, rilascio con lo stesso codice, TG/TO/DF, layer non valido, OSL, OSM anche a destra, LM, TT con 5 tocchi e con pausa, encoder) e `test_default_keymap.c` (67 celle come misurate sulla scheda, nessun tasto Fn dove la base è vuota). Il `Makefile` ricompila sempre i test e accetta moduli extra per test (`LINK_x`).
- Verifica: `make test` 3 file OK; `make fw size` → flash 5031/44032, XDATA 1590/4096. Simulatore non usato; nessuna prova sull'hardware.

### Fase 3, passo 2 — report HID
- `core/report.[ch]`: stato di ciò che l'host deve sapere e costruzione dei byte. Report boot da 8 byte (modificatori + 6 tasti nell'ordine di pressione; il settimo non entra), bitmap NKRO (modificatori + 120 bit, come il descrittore stock), modificatori a sinistra nei bit 0–3 e a destra 4–7, uso consumer e system (uno alla volta) con le tabelle tasto → uso (HUT 1.12).
- Il core non invia nulla: chiama `report_hook_send(kind)` a ogni cambio, una volta per cambio, così un tasto premuto e rilasciato nello stesso giro produce comunque due report. Il trasporto (USB, fase 4; radio, fase 6) implementa l'hook; per ora `board/transport.c` è un hook vuoto.
- `core/keyboard.c`: gli hook dei modificatori del layer ora scrivono nel report; `send_keycode` gestisce tasti, modificatori (KC_LCTL…), `LSFT(KC_A)`-style (modificatori prima del tasto alla pressione, dopo alla fine), consumer e system. Alla fine del tasto il report esce prima che i modificatori one-shot vengano tolti.
- `core/keycodes.h`: aggiunti tasti media/system, `LCTL/LSFT/LALT/LGUI/RCTL/…(kc)`, macro `IS_*_KEYCODE`, `MOD_BIT`, `MODS_FROM_MASK` (spostata da layer.c, ora condivisa).
- Bug trovato dai test: `LSFT(kc)` sommava per errore `QK_MODS` (che vale già `MOD_LCTL << 8`), dando 0x0305 invece di 0x0205; il test `test_keycode_encoding` ora fissa i valori usati da Vial.
- Test: `test_report.c` (ordine e limite dei 6 tasti, NKRO, modificatori, tabelle degli usi, valori dei codici), `test_keyboard.c` (tasto semplice, modificatore, mods+tasto, tasto di layer con rilascio dopo il cambio, consumer/system, OSM e OSL su un solo tasto).
- Verifica: `make test` 5 file OK; `make fw size` → flash 6333/44032, XDATA 1616/4096. Simulatore non usato; nessuna prova sull'hardware.

### Fase 3, passo 3 — tapping (LT, MT, tap dance)
- `core/tapping.[ch]`: tasti tap-or-hold `LT(layer, kc)`, `MT(mods, kc)` e `TD(n)`. Un tocco più corto del tapping term invia `kc`; tenuto oltre il term attiva il layer o tiene i modificatori. Mentre il tasto è indeciso gli eventi successivi finiscono in una coda di 8 e vengono riprodotti a decisione presa, così un tasto battuto durante l'attesa cade sul layer giusto. Niente permissive hold: decide il rilascio (tap) o il timeout (hold). Coda piena → l'attesa diventa un hold subito.
- Tap dance: tap, hold (se vuoto si tiene l'azione del tap), doppio tocco (il tocco singolo aspetta allora un altro term); term proprio per ogni tap dance (campo 4); un'azione che è a sua volta un `TD` non fa nulla (evita la ricorsione).
- `core/keymap.[ch]`: 8 tap dance × 5 parole (80 B) e il tapping term generale (200 ms) tenuti in RAM, con valori iniziali dalla scheda; `board/keymap_default.c` definisce TD(0) = encoder (tocco mute, hold play/pausa, doppio tocco traccia successiva).
- `core/keyboard.c`: pipeline `tapping_intercept` → ricerca nel layer → `tapping_event` → azione; hook del tapping (replay, tap, modificatori, layer, keycode) implementati qui; nuova `keyboard_task(now)` chiamata dal `main` a ogni giro.
- Il tempo è sempre un `uint16_t` in ms passato come argomento; le differenze usano l'aritmetica a 16 bit, quindi il giro dell'orologio (65536 ms) non rompe il term.
- Test `test_tapping.c` (11 test a livello di report: LT tap/hold, tasti battuti durante l'attesa su entrambi i layer, MT tap/hold con il tasto sotto, TD tap con attesa, doppio tocco, hold, TD senza doppio tocco, coda piena, giro dell'orologio, tapping term modificato). Verificato che falliscano: con una mutazione temporanea (hold scambiato con tap) danno 13 errori.
- Verifica: `make test` 6 file OK; `make fw size` → flash 8626/44032, XDATA 1749/4096. Simulatore non usato; nessuna prova sull'hardware.

### Fase 3, passo 4 — macro
- `core/keymap.[ch]`: il buffer delle macro di Vial (16 macro in 512 B, ognuna chiusa da uno 0) vive in RAM con `keymap_macro_byte/read/write/clear`.
- `core/macro.[ch]`: interprete del buffer. Testo digitato come su tastiera US (lettere, cifre, punteggiatura con shift, `\b \t \n ESC DEL`), azioni dopo il prefisso 0x01 (tap/down/up di un tasto, ritardo, le stesse tre con codice a 16 bit; i codici con byte basso 0 sono memorizzati come 0xFF0n, come fa Vial), buffer con ultimo byte diverso da 0 = in riscrittura o rovinato, non si suona niente; azioni troncate chiudono la macro.
- **Differenza rispetto al porting SMK:** la macro non ferma la tastiera. SMK la eseguiva fino in fondo con attese bloccanti (la scansione si fermava, e con un tick a 8 bit un'attesa >255 ms avrebbe fatto perdere tempo); qui `macro_task(now)` fa un passo al millisecondo, quindi ogni report arriva all'host prima del successivo (nessuna coda di report necessaria nel trasporto USB) e un ritardo è una scadenza. Una macro avviata mentre un'altra è in corso aspetta il suo turno (coda di 4; oltre, scartata).
- `core/keyboard.c`: i tasti `QK_MACRO` avviano la macro alla pressione (e contano come tasto premuto/rilasciato per i one-shot); gli hook della macro usano lo stesso `send_keycode`; `keyboard_task` fa avanzare anche le macro.
- Test `test_macro.c` (10 test: testo, ogni carattere stampabile ha un tasto, azioni a 8 e 16 bit, codici con byte zero, ritardo come scadenza anche oltre 255 ms, un passo per ms, scelta della macro, coda, buffer rovinato, azioni troncate) e un test d'integrazione in `test_keyboard.c` (tasto macro → report).
- Verifica: `make test` 7 file OK; `make fw size` → flash 10555/44032, XDATA 2291/4096. Simulatore non usato; nessuna prova sull'hardware.

### Fase 3, passo 5 — profili e store in flash
- `core/store.[ch]`: 4 profili completi (tasti, encoder, macro, tap dance, impostazioni) in coppie di slot A/B da 4 settori da 512 B, 0xAC00–0xEBFF (poi le impostazioni a 0xEC00). Un salvataggio cancella lo slot *non* in uso, scrive il contenuto e per ultimo l'intestazione con numero di sequenza e CRC-16: lo slot vale solo se completo e al caricamento vince quello valido con la sequenza più recente (confronto sicuro al giro del contatore). Un taglio di corrente lascia la copia precedente intatta. Il contenuto è letto/scritto attraverso le funzioni del keymap, non da copie parallele.
- **Differenze dal porting SMK:** magic `OB` versione 1 (gli slot scritti da SMK, `KM`, non vengono interpretati); la validità si controlla con la sola CRC prima di applicare (SMK applicava ogni slot mentre li validava); l'accesso alla flash passa da tre hook (`store_hook_read/erase/program`), sostituiti nei test da una flash finta.
- Impronta della mappa predefinita nell'intestazione, come nel porting: se il firmware nuovo cambia i valori predefiniti, la copia salvata non si usa (le modifiche fatte da Vial vanno rifatte una volta). Da rivalutare in fase 7.
- Salvataggio dopo 2 s senza modifiche e senza tasti premuti (`store_task`, tempo in ms); il cambio profilo (`store_request`) avviene a tasti tutti rilasciati e scrive prima il profilo che si lascia; `store_hook_loaded` avverte il firmware (la scheda azzera lo stato dei layer).
- `core/keymap.[ch]`: impostazioni per profilo (5 parole: effetto LED, luminosità, velocità, tapping term, colore) con valori predefiniti dalla scheda (`0, 160, 4, 200, 0`), `keymap_reset()`; il tapping term ora è l'impostazione `CFG_TAP_TERM` (0 = 200 ms). `matrix_any_pressed()` nel core della matrice.
- `hal/flash.[ch]`: scrittura della flash da applicazione (registri SSP). **Protezione:** cancella/programma solo tra 0xAC00 e 0xF000, qualunque indirizzo riceva (l'applicazione e il bootloader stock non si toccano). Interrupt spenti per ogni operazione, quindi qualche tick va perso durante un salvataggio (decine di ms). **Non provato sull'hardware.**
- `core/keyboard.c`: i codici della fascia `QK_KB` passano da `keyboard_hook_custom`; `board/custom_keys.c` gestisce i tasti profilo 1–4 (`store_request`). `board/main.c` carica il profilo all'avvio e chiama `store_task`. Il profilo in uso *non* viene ancora ricordato dopo lo spegnimento: serve l'area impostazioni (fase 6); per ora si parte dal profilo 0.
- Test `test_store.c` (14 test: flash vuota → default, modifiche dopo il riavvio, slot alternati e vince il più recente, slot più recente rovinato → quello precedente, impronta diversa, slot di SMK ignorato, **taglio di corrente a ogni 7ª operazione di un salvataggio** → sempre profilo vecchio o nuovo completo, mai misto (anche al primo salvataggio), profili indipendenti, cambio a tasti rilasciati, hook di caricamento, salvataggio solo dopo la quiete e senza tasti, giro dell'orologio, reset dei tasti che non tocca le macro). La flash finta segnala scritture che la vera non farebbe (bit riportato a 1 senza cancellare, accessi fuori dall'area). Verificato che falliscano: saltando l'ultimo settore da cancellare i test danno 9 errori.
- Verifica: `make test` 8 file OK; `make fw size` → flash 14767/44032, XDATA 2378/4096. Simulatore non usato; nessuna prova sull'hardware.

### Fine fase 3
Logica pura completa: matrice, layer, report, tapping, macro, profili. Resta da collegare all'hardware: USB e raw HID (fase 4), Vial/VIA (5), LED/radio/sleep/impostazioni (6).

### Fase 4, passo 1 — protocollo USB puro (testato sull'host)
- `core/usb_desc.[ch]`: i descrittori come dati. Dispositivo 258a:dead, bcdDevice 0x0200 (stessi id del porting SMK, per `sinowisp` e Vial), stringhe `wildzer0` / `Oblio B68` / seriale `vial:f64c2b3c` (Vial elenca solo le tastiere il cui seriale lo contiene). Interfaccia 0: HID boot tastiera su EP1 IN da 16 B, con report tastiera (ID 3), system (1), consumer (2), LED in uscita e feature ISP (ID 5, 5 B); interfaccia 1: raw HID 0xFF60/0x61, EP2 IN + OUT da 32 B senza ID. Lunghezze del descrittore HID e di configurazione ricavate con `sizeof`. Layout identico a quello già provato dal porting: **descrittori scritti da zero ma da confrontare con il porting sulla tastiera**.
- `core/usb_device.[ch]`: la macchina a stati di EP0, senza registri. Il driver le passa SETUP, pacchetti OUT e «pacchetto IN preso»; risponde con 6 hook (`usb_hook_ep0_in`, `…out_arm`, `…stall`, `…set_address`, `…configured`, `…ep_halt`) più due per l'applicazione (`usb_hook_led`, `usb_hook_isp_request`). Gestisce GET_DESCRIPTOR a pacchetti da 8 B (troncato a wLength, pacchetto vuoto finale solo se i dati sono multipli di 8 e più corti di quanto chiesto), SET_ADDRESS applicato dopo lo status stage (anche se un nuovo SETUP lo supera), SET/GET_CONFIGURATION (richiede un indirizzo), GET/SET_INTERFACE, GET_STATUS, SET/CLEAR_FEATURE (remote wakeup, halt di EP1/EP2 IN e EP2 OUT), richieste HID SET/GET_IDLE e SET/GET_PROTOCOL (si parte in report protocol), SET_REPORT dei LED (1 byte, o 2 con l'ID) e feature ISP (comando 0x75, eseguito solo dopo lo status stage e annullato se arriva un altro SETUP); tutto il resto va in STALL.
- Test: `test_usb_desc.c` (coerenza di configurazione, HID, endpoint, stringhe; **un piccolo parser dei report descriptor** controlla che i report abbiano le dimensioni attese: tastiera 8 B, LED 1 B, system 1 B, consumer 2 B, ISP 5 B, raw HID 32+32 B) e `test_usb_device.c` (enumerazione come la fa un host, ogni descrittore a ogni lunghezza richiesta, indirizzo dopo lo status, richieste non supportate in STALL, protocollo/idle, LED, ISP, remote wakeup e halt, interfacce). Il test include `usb_device.c` per provare direttamente `send()` sul pacchetto vuoto finale, che nessun descrittore reale esercita; verificato con una mutazione (regola del pacchetto vuoto tolta → il test fallisce).
- `Makefile`: `core/usb_desc.c` e `core/usb_device.c` vengono compilati con SDCC (`--Werror`) ma non ancora collegati, finché il driver del passo 2 non fornisce gli hook. Esito: flash 14767/44032, XDATA 2378/4096 (invariati).
- Simulatore non usato; nessuna prova sull'hardware.

### Fase 4, passo 2 — driver hardware e trasporto dei report
- `hal/sh68f90.h`: registri e bit del controller USB (USBCON/USBIF/USBIE/EPxCON/contatori), regolatore REGCON, buffer degli endpoint (0x1100–0x11FF, fuori dai 4 KB di XDATA), vettore USB.
- `hal/usb.[ch]`: implementa gli hook di `usb_device` sul chip (EP0 IN/OUT/stallo, indirizzo, configurazione che rimette gli endpoint puliti, halt di EP1/EP2) e l'interrupt USB: reset di bus/power-up (impulso su SWRST, reinizializzazione), SOF (fine sospensione), sospensione, SETUP (copia degli 8 byte; un IEP0IF ancora acceso vale come status stage precedente concluso), EP0 OUT/IN. L'ISR non chiama niente che chiami anche il `main`: parla con il `main` solo attraverso flag `volatile` (`event`, richiesta ISP, byte dei LED). Vettore a 0x003B verificato nell'hex. Accende il regolatore REGCON prima del modulo USB (nel porting lo faceva `ldo_init`). **Non provato sull'hardware**; sequenza e registri ripresi dal porting.
- `core/usb_reports.[ch]`: dai report ai pacchetti di EP1. `report_hook_send` costruisce subito il pacchetto dallo stato del report e lo mette in coda (8); il `main` lo consegna all'endpoint un pacchetto per volta quando l'host ha preso il precedente. Tastiera = ID 3 + 8 byte (in boot protocol senza ID), system = ID 1 + bit, consumer = ID 2 + uso a 16 bit. **In boot protocol consumer e system non vengono inviati** (un BIOS riceve solo la tastiera; SMK li mandava comunque con ID). Prima della configurazione non si invia nulla. Coda piena: il report più recente dello stesso tipo prende il posto dell'ultimo in coda (sono stati completi, quindi lo stato finale è giusto), altrimenti esce il pacchetto più vecchio; il conteggio è in `usb_reports_overflows()`. Bus reset o nuova configurazione svuotano la coda.
- `board/usb_app.[ch]`: byte dei LED dall'host (`usb_host_leds()`, per il caps lock della fase 6), richiesta ISP, collegamento di configurazione/protocollo alla coda e `usb_task()` del main loop (svuotamento della coda, salto al bootloader **dal main**, mai dall'ISR). `board/isp.[ch]`: `isp_jump` condiviso con il recupero Esc+1+2. `board/transport.c`: `report_hook_send` → coda USB. `board/main.c`: inizializza l'USB dopo il recupero e prima di abilitare gli interrupt; `usb_task()` nel giro.
- Non portato (per ora): `usb_wait_for_enumeration` del porting (attesa dell'enumerazione prima di accendere i LED; serve ai LED, fase 6); EP2 OUT (raw HID) non è armato: arriva al passo 3.
- Test `test_usb_reports.c` (6 test: formato dei pacchetti con e senza ID, consumer/system, un pacchetto per volta e rispetto dell'endpoint occupato, niente prima della configurazione, svuotamento, coda piena). Verifica: `make test` 11 file OK; `make fw size` → flash 21002/44032, XDATA 2560/4096 (la flash cresce di ~6 KB). Stack libero dopo l'avvio: 223 B; l'uso massimo in ISR + catena del main **non è misurato** (da fare in fase 7 con un riempimento dello stack sull'hardware). Simulatore non usato.

### Fase 4, passo 3 — raw HID
- `core/raw_hid.[ch]`: il canale di Vial e VIA, richiesta da 32 B su EP2 OUT e risposta da 32 B su EP2 IN. Una richiesta alla volta: l'interrupt copia il pacchetto e lo segna in attesa, e finché non c'è risposta EP2 OUT non viene riarmato, quindi il pacchetto successivo dell'host riceve NAK e riprova (niente coda, niente perdite, come nel porting). Il `main` esegue `raw_hid_hook_process` (la fase 5 lo trasforma in VIA/Vial; la risposta resta nello stesso buffer), aspetta che EP2 IN sia libero e poi riarma. Un pacchetto di dimensione diversa viene scartato e si riarma. `raw_hid_reset()` al cambio di configurazione/reset di bus.
- `hal/usb.c`: hook di EP2 (IN libero/invio, riarmo OUT), interrupt OEP2 abilitato; `board/usb_app.c`: all'evento di configurazione svuota la coda dei report e riavvia il canale raw HID; nell'attesa della fase 5 ogni richiesta riceve la risposta «non gestito» (`0xFF` nel primo byte), che VIA/Vial capiscono, invece di un'eco.
- Sospensione e configurazione sono già esposte (`usb_hw_suspended()`, `usb_device_configured()`) per sleep e remote wakeup della fase 6.
- Test `test_raw_hid.c` (5 test: richiesta e risposta, risposta che aspetta EP2 IN occupato, una sola esecuzione per richiesta, nessuna seconda richiesta in volo, pacchetto di altra dimensione, reset); verificato con una mutazione (la guardia «una volta sola» tolta → i test falliscono). Verifica: `make test` 12 file OK; `make fw size` → flash 21348/44032, XDATA 2594/4096. Simulatore non usato; nessuna prova sull'hardware.

### Fine fase 4
Enumerazione USB, report HID (6KRO, consumer, system), LED dall'host, canale raw HID, ingresso nel bootloader dal comando ISP: tutto scritto, compilato e con la logica testata sull'host; **nessuna parte USB è stata provata sull'hardware** (registri, tempi, riarmo degli endpoint, sospensione). Restano: Vial/VIA veri (fase 5) e LED/radio/sleep/impostazioni (fase 6).

### Fase 5, passo 1 — protocollo VIA / Vial (testato sull'host)
- `core/via.[ch]`: i comandi VIA sul buffer raw HID da 32 B, con la risposta che riscrive la richiesta nello stesso buffer (come `raw_hid_receive` di QMK): versione del protocollo (0x0009), valori tastiera (uptime, opzioni di layout; lo stato della matrice solo a tastiera sbloccata), get/set keycode (big-endian), reset della keymap, numero di layer, lettura/scrittura a blocchi (≤28 B) del buffer della keymap, macro (conteggio, dimensione, lettura; scrittura e azzeramento solo da sbloccata), salto al bootloader (da sbloccata; la risposta parte prima). Lunghezza diversa da 32 → ignorata; comando sconosciuto → `0xFF`.
- `core/vial.[ch]`: i comandi dietro il prefisso 0xFE: keyboard id (protocollo Vial 6 + UID a 8 byte), dimensione e pagine da 32 B della definizione compressa, encoder, sblocco (i tasti devono restare premuti 50 passi da 100 ms, un poll troppo anticipato o un tasto rilasciato azzera il conteggio; durante lo sblocco si risponde solo alle domande di sblocco), lock, QMK settings (solo il tapping term del profilo in uso), tap dance (conteggio, lettura, scrittura; nessun combo/key override). Dati della scheda (definizione, UID, tasti di sblocco) dichiarati `extern`, generati al passo 2; tempo da `via_hook_now()` (ms a 32 bit).
- **Bug del porting SMK corretto:** la query delle QMK settings leggeva il numero richiesto *dopo* aver riempito il buffer di 0xFF, quindi non elencava mai il tapping term; ora lo legge prima (il test della query lo copre).
- Le modifiche passano tutte da `keymap_*`, quindi lo store le salva da solo grazie al contatore di versione (nessuna chiamata «impostazioni cambiate» come in SMK). `via_init`/`vial_init` riportano lo stato a bloccato.
- Test `test_via.c` (15 test, richiesta/risposta esatte): versione e valori, lunghezza errata, keycode (anche fuori limite), buffer della keymap (confini, scritture dispari, blocchi troppo grandi), encoder, id/dimensione/pagine della definizione (ultima parziale, oltre la fine), flusso di sblocco con tempo simulato, tasti rilasciati e ritmo, filtro durante lo sblocco, macro/matrice/bootloader protetti, geometria delle macro, stato della matrice (bit per colonna, big-endian), QMK settings, tap dance. Mutazione provata (la protezione dello stato della matrice tolta → il test fallisce).
- `Makefile`: `core/via.c` e `core/vial.c` sono compilati con SDCC ma non ancora collegati (mancano i dati generati e l'hook del tempo: passo 2). Flash/XDATA invariati (21348/44032, 2594/4096). Simulatore non usato.

### Fase 5, passo 2 — definizione Vial e collegamento
- `board/vial.json`: la definizione della B68 (matrice 5×16, tasti disegnati, 20 tasti personalizzati) presa dal porting SMK (è un dato descrittivo, non codice; elencata in NOTICE.md). Unica modifica: il primo tasto personalizzato, che nel porting era l'overlay HUD, resta come segnaposto «HUD Toggle (unused)» perché gli altri tasti personalizzati mantengano i loro numeri (0x7E01…).
- `tools/vial_gen.py` (nuovo): minifica il JSON e lo comprime in LZMA (contenitore xz, quello che Vial si aspetta), scrive `build/vial_data.c` con definizione, UID `0x46ecd0565df84fa8` e tasti di sblocco Esc + Invio (celle 0,0 e 2,13): stessi valori del porting, così le impostazioni salvate in Vial restano valide. Il `Makefile` lo esegue al build.
- `tools/check_vial.py`, eseguito da `make test` (`check-vial`): le celle disegnate nel layout sono esattamente le 67 celle di `LAYOUT_B68` nella mappa predefinita (gli encoder, marcati con la `e`, sono esclusi), la matrice è 5×16, i tasti di sblocco esistono, i `B68_*` sono contigui da 0x7E00 e tanti quanti i `customKeycodes`, la definizione sopravvive a compressione e decompressione. Verificato che fallisca con un tasto di sblocco inesistente.
- `board/usb_app.c`: `raw_hid_hook_process` ora è `via_receive`; il salto al bootloader chiesto da Vial parte almeno 100 ms **dopo** la risposta (non bloccante, dal `main`); `board/main.c`: `via_init()`, contatore dell'uptime a 32 bit (`via_hook_now`) e `usb_task(now)`.
- `Makefile`: `.DELETE_ON_ERROR` (un link fallito lasciava un `.ihx` che `make` considerava aggiornato; me ne sono accorto leggendo una cifra di flash sbagliata e l'ho ricostruita da pulito), il vecchio filtro temporaneo su `via.c`/`vial.c` è tolto.
- Verifica: `make test` 13 file + `check-vial` OK; `make fw size` da pulito → flash 28733/44032, XDATA 2611/4096 (definizione compressa: 916 B). Simulatore non usato; nessuna prova con l'app Vial sull'hardware: la compatibilità del formato (xz, pagine da 32 B) è quella del porting, che funzionava.

### Fine fase 5
Vial/VIA completi per quanto serve al porting: tasti, encoder, macro, tap dance, tapping term, sblocco, matrice, bootloader. Restano fuori (come nel porting): combo, key override, altre QMK settings, HUD.

## Fase 6 — encoder, impostazioni, LED, radio, sleep (piano in `CHANGELOG`, passi sotto)

### Fase 6, passo 1 — encoder e impostazioni globali
- `core/encoder.[ch]`: decodifica in quadratura. Gli stati scorrono 0→1→3→2→0 in senso orario (A = P0.6 = bit 0, B = P0.7 = bit 1); uno scatto sono 4 variazioni; un salto di due stati è un campione perso e non conta; un rimbalzo avanti e indietro si annulla; la prima lettura è la posizione della manopola, non un movimento. `encoder_poll()` a ogni scansione, `encoder_next()` consegna gli scatti uno per uno. A differenza del porting non c'è un'ISR: campionamento nel `main`, quindi niente accessi atomici.
- `core/keyboard.c`: ogni scatto è un **tocco** (pressione e rilascio) del codice che i layer attivi danno per quella direzione, fino a 8 per giro; passa da `run_keycode`, quindi la manopola può muovere anche layer, macro e tasti luce. `board/encoder_hw.c` legge P0.6/P0.7.
- `core/settings.[ch]`: impostazioni della tastiera, non del profilo (profilo in uso, livello di sleep, slot Bluetooth). Un record nel settore 0xEC00 con magic `OS`, versione e CRC-16, riscritto 500 ms dopo una modifica, a tasti rilasciati, solo se qualcosa è cambiato; valori fuori limite rimessi nei limiti; record non valido → predefiniti. Usa gli hook di flash di `store.h` e quindi la protezione di `hal/flash.c` (solo 0xAC00–0xF000). Il marcatore a 0xEE00 non viene toccato.
- **Il profilo in uso ora si ricorda dopo lo spegnimento:** `store_init(profile)` parte da quello salvato; `store_hook_loaded` lo registra nelle impostazioni.
- Test: `test_encoder.c` (9 test: scatto completo nei due sensi, scatto incompleto, scatti consecutivi, scatto che non parte da riposo, rimbalzi, campione perso, inversione dentro lo scatto, prima lettura, encoder inesistente; mutazione del segno nella tabella → falliscono), `test_settings.c` (8 test: predefiniti, persistenza, niente scritture inutili, valori fuori limite, record danneggiato o estraneo, record con CRC giusto ma valori fuori limite, scrittura dopo la quiete a tasti rilasciati, giro dell'orologio), `test_store.c` (profilo iniziale scelto), `test_keyboard.c` (scatti dell'encoder → tocchi).
- Verifica: `make test` 15 file + `check-vial` OK; `make fw size` → flash 30028/44032, XDATA 2622/4096. Simulatore non usato; nessuna prova sull'hardware.

### Fase 6, passo 2 — retroilluminazione, parte pura
- `core/led_effect.[ch]`: gli effetti (radiale, orizzontale, verticale, fisso, diagonale, respiro, impulso; «off» è il numero successivo), la ruota dei colori (rosso → blu → verde → rosso) e la luminosità. **Differenza dal porting:** la geometria (distanza dal centro per l'effetto radiale, assi x e y) è calcolata all'avvio con una radice intera, non da un generatore C eseguito al build; il centro è la metà delle 16 colonne e delle 5 righe (stessi valori del porting: `cx2 = 15`, `cy2 = 4` in mezze celle). Costo: 101 byte di XDATA.
- `core/led.[ch]`: il framebuffer a livelli da 8 bit (16 colonne × 15 canali: riga × R, G, B) e il motore dei LED. L'interrupt (passo 3) chiede `led_next_column()` (a tempo costante; conta un frame a ogni giro di 16) e legge `led_level()`/`led_column_lit()`; la larghezza PWM (`LED_WIDTH`: 3,5 per livello, massimo 892 su un periodo di 1200) si calcola lì dal livello. **Differenza dal porting:** non si tiene una tabella di larghezze da 480 byte, solo i 240 byte di livelli; il resto avviene in ISR con due somme. Tutto ciò che costa tempo (effetti, ridisegno) sta in `led_task()` nel `main`, 8 LED per giro; ogni 9 frame la fase avanza della velocità; flash del tasto del profilo (60 frame, bianco), feedback radio/sleep sulla riga dei numeri a colore invertito (62 frame; accoppiamento: lampeggio per ~10 s), caps lock bianco, impostazioni di effetto/luminosità/velocità/colore dal profilo in uso (valori fuori limite riportati nei limiti) e gli otto tasti luce (`led_effect_next/prev`, `brightness_*`, `speed_*`, `color_*`).
- `board/led_app.c`: caps lock dal report dei LED dell'host, profilo da `store_profile()`. `board/main.c` inizializza il motore e chiama `led_task()`: i livelli si calcolano ma **non esce niente** finché il passo 3 non porta l'hardware.
- Test `test_led.c` (14 test: ruota ed effetti — radiale con angoli più lontani e centro tra le colonne 7 e 8, respiro uguale su tutta la tastiera, impulso, assi —, bianco fisso, spento (tranne il flash del profilo), tinta unita a colori, larghezze dentro il periodo, sequenza delle colonne, avanzamento ogni 9 frame, caps lock, flash del profilo e cambio profilo, tasto radio a colore invertito (2.4G, slot, passi dello sleep, slot inesistente), lampeggio dell'accoppiamento, tasti luce con saturazioni, impostazioni fuori limite, ridisegno a pezzi). Il test ha rivelato un limite: il contatore dei frame è a un byte, quindi `led_task()` deve girare almeno ogni 255 frame (~2 s), cosa che il `main` fa; mutazione (passo dell'animazione sbagliato) → il test fallisce.
- Verifica: `make test` 16 file + `check-vial` OK; `make fw size` → flash 33730/44032, XDATA 2995/4096 (restano ~10 KB di flash per LED hardware, radio e sleep: il margine è stretto). Simulatore non usato; nessuna prova sull'hardware.

### Fase 6, passo 3 — retroilluminazione sull'hardware e nuovo tick
**Cambia l'architettura di fase 2:** la scansione della matrice non gira più nel `main` ma nell'interrupt, perché le linee delle colonne fanno anche da pozzo dei LED e le due cose non possono sovrapporsi.
- `core/sched.[ch]` (puro, testato): la sequenza degli slot del Timer 2 — 4 slot LED da 400 µs, poi uno slot di scansione da 450 µs —, con le lunghezze note, così l'interrupt tiene anche il tempo in millisecondi senza un secondo timer. Un quadro di 16 colonne dura 8,2 ms (122 Hz), una scansione ogni 2,05 ms. (Il porting usava 420 µs, stesso schema; ho lasciato ~100 µs di margine sulla scansione, che dura ~345 µs: *stima*, non misurata.) Test: sequenza, `sched_upcoming` coerente con il passo successivo, periodi, i millisecondi contati seguono la somma degli slot (a meno di uno slot), mai due millisecondi di fila; mutazione provata.
- `hal/tick.[ch]` riscritto: **Timer 2 in auto-reload** con il valore del *prossimo* slot scritto in `RCAP2` durante l'interrupt in corso (la durata di uno slot è il valore ricaricato al suo inizio, quindi esatta a prescindere dalla latenza; SMK riavviava il timer a mano a ogni slot). L'ISR esegue il lavoro dello slot tramite due hook della scheda (`tick_hook_scan`, `tick_hook_led`), conta i millisecondi e lascia l'esito della scansione in un buffer di 16 byte; il `main` lo copia con l'interrupt fermo (`tick_scan_take`) e lo passa al debounce. `tick_pause/resume` fermano gli slot attorno alle scritture in flash.
- `hal/pwm.h`: i 18 canali PWM sono registri XDATA regolari a 0xFF80 (controllo +k, DUTY1L 0xFFA0, DUTY1H 0xFFB8, DUTY2L 0xFFD0, DUTY2H 0xFFE8, periodi 0xFF98/0xFF9C +banco): accesso per indice, niente 18 nomi.
- `board/led_hw.[ch]`: un sottoquadro = colonna precedente spenta, PWM parcheggiati, larghezze delle 15 canali caricate (`DUTY2 = 195 + n + larghezza`, `DUTY1 = 195 + n` fisso e sfalsato come lo stock, periodo 1200), strobo della colonna basso, PWM abilitati per ultimi. Mappa canale → PWM e valori di controllo (`0x89` il primo di ogni banco, `0x08` gli altri; PWM20 senza larghezza avvia il banco 2) come nel porting. Durante la scansione i PWM sono parcheggiati e poi riavviati. La riga `PWM00CON &= ~(1 << 5)` del porting è ripresa (sembra un flag PWM, senza effetto con 0x89): ipotesi, non verificata.
- `board/slots.c`, `board/matrix_hw.c` (`matrix_hw_cols_off`, `matrix_hw_col_on`), `board/led_app.c`: la luce si accende quando l'host ha configurato la tastiera, non in sospensione, e se dopo 500 ms dall'accensione non si è sentito nulla dal bus (versione non bloccante di `usb_wait_for_enumeration`; sul cavo un dispositivo alimentato dal bus deve stare sotto 100 mA finché non è configurato). **Le modalità wireless la accenderanno al passo 5.**
- Tasti luce collegati in `board/custom_keys.c` (effetto −/+, luminosità −/+, velocità −/+, colore −/+).
- `board/store_hw.c`: ogni cancellazione o scrittura di flash ferma il timer e i PWM (altrimenti una colonna accesa resterebbe tale per i ~5 ms di cancellazione e lampeggerebbe). Il tempo in millisecondi perde quel tempo (decine di ms a ogni salvataggio).
- Altre modifiche: debounce da 5 a 3 scansioni (~6 ms con scansioni da 2 ms); l'encoder si legge a **ogni giro del main** (molto più veloce di una scansione); `hal/flash.c` ripristina `EA` com'era invece di riaccenderlo (avrebbe abilitato gli interrupt a metà dell'avvio); `hal/usb.c` espone `usb_hw_setup_seen()`; il `Makefile` ora fa dipendere ogni oggetto da tutti gli header (una modifica a un header non lasciava oggetti vecchi, come mi era già successo con il link).
- Non verificato (tutto da misurare sull'hardware): durata reale dell'ISR di scansione e di LED (stime: sottoquadro ~110 µs, scansione ~390 µs con parcheggio dei PWM), flicker, corrente, interferenza con l'USB (stesso livello di priorità dell'ISR USB).
- Verifica: `make test` 17 file OK; `make fw size` da pulito → flash 35157/44032, XDATA 3021/4096 (restano 8,8 KB di flash per radio e sleep). Vettori Timer 2 (0x0003) e USB (0x003B) presenti nell'hex. Simulatore non usato; nessuna prova sull'hardware.

### Fase 6, passo 4 — radio BK3632, protocollo e scheduler (puro)
- `core/radio.[ch]`: riscrittura del driver del porting (stesso protocollo, verificato sulla tastiera dal porting; il nome dei simboli cambia, `b68radio_*` → `radio_*`). Frame `01 cmd a b c chk` con `chk = 0x55 − Σ`; report tastiera da 30 byte con la regola «un tasto va nella lista (fino a 5) **oppure** nella bitmap (6 tasti), mai in entrambe» (altrimenti l'host lo scrive due volte); consumer da 13 byte; nome Bluetooth da 32 byte (tipo 0 LE, tipo 1 classico; «OBLIO», tagliato a 24 caratteri). Scheduler: un frame solo con ATT alta e almeno 1 ms dopo il precedente; SEL basso per tutta la trasmissione e rilasciato con l'ultimo byte (o dopo 50 ms se la seriale si blocca); le risposte non si aspettano; le notifiche `03`/`08` della radio si confermano con `F0`/`F1` prima di ogni altra cosa; comando di modo (ripetuto ogni 300 ms fino a 8 volte finché non risponde; un «non ora» al modo conta come risposta); nomi Bluetooth una volta per accensione dopo la prima risposta al modo; report tastiera e consumer in due code da 4 (a coda piena il più recente prende il posto dell'ultimo) con il rimando dopo un rifiuto (10 ms) solo se lo stato non coincide con quello già accettato; richiesta di stato ogni 200 ms, salvata. `radio_sleep()` manda i due frame dello stock (`01 0C 08 07 00`, `01 0B 00`) con attese a ritardi, poi ferma la radio. Il parser scarta i byte che non iniziano con 02/03/08 e i frame con checksum sbagliato.
- Differenze dal porting: niente traccia né contatori di debug (compilati solo con un flag nel porting, ~1,5 KB di flash); tempo e seriale solo tramite 7 hook (`radio_hook_*`), per cui il modulo è puro; i tasti arrivano come stato (modificatori + 6 tasti) dal report, non da una funzione di tap.
- Test `test_radio.c` (17 test con una radio simulata che registra i frame e si può far tacere/rifiutare/rispondere): costruzione dei frame con checksum, partenza di un link (modo, poi rilascio di tastiera e consumer), ATT bassa, SEL per tutta la durata, risposte non attese, conferme `F0`/`F1` con priorità e frame con checksum errata ignorati, rifiuto con e senza rimando, consumer rifiutato, ripetizione del modo, rifiuto del modo, nomi una sola volta, stato ogni 200 ms, coda a 4, rumore prima di un frame, frame che non finisce mai, stop e sleep, inattività. Mutazione provata (conferma `F0` disabilitata → fallisce).
- Il `Makefile` compila `core/radio.c` con SDCC ma non lo collega ancora (i sette hook arrivano al passo 5; variabile `NOT_LINKED`). Flash/XDATA invariati (35157/44032, 3021/4096). Simulatore non usato.

### Fase 6, passo 5 — radio sull'hardware, selettore di modo, instradamento
- `core/link.[ch]` (puro, testato): il modo in cui la tastiera parla all'host — cavo, dongle 2.4G, Bluetooth —, deciso dal selettore sulla scheda e stabile per 50 ms prima di valere (un selettore che rimbalza non fa nulla). Passando a 2.4G/Bluetooth parte il comando di modo (slot Bluetooth dalle impostazioni) e il tasto 4 o 1–3 lampeggia; passando a USB la radio lascia andare i tasti che teneva (tastiera e consumer vuoti) e si ferma quando ha finito. Tasti Bluetooth 1–3: un tocco sceglie lo slot (e, in Bluetooth, lo riapplica), 3 s di pressione avviano l'accoppiamento (slot salvato nelle impostazioni, lampeggio del tasto); fuori dal Bluetooth ricordano solo lo slot. Dopo uno sleep `link_wake()` rimette il selettore a «da decidere».
- `board/radio_hw.[ch]`: EUART0 in modo 1 a 260870 baud (`SCON = 0x50` con `PCON.SSTAT`, `SBRT = 0x7FFB`, `SFINE = 12`), **priorità massima** (`IPH1`/`IPL1` bit ES0: senza, i byte escono con vuoti e i tasti restano premuti, come scoperto nel porting), anello di ricezione da 32 byte, trasmissione byte per byte in interrupt, SEL = P0.2 (basso durante il frame, poi ingresso), ATT = P4.7. Vettore dell'EUART verificato a 0x006B. Hook `radio_hook_*` come nel porting (`b68radio_hw.c`); il tempo è il clock del `main` (`board_now_ms()`).
- `board/transport.c`: in wireless i report della tastiera vanno alla radio come stato (modificatori + 6 tasti) e il consumer come uso; i tasti di sistema (power, sleep) non hanno una forma radio e si perdono; altrimenti coda USB. `board/link_app.c`: lettura del selettore (P0.4 basso = 2.4G, P0.3 basso = Bluetooth) e lampeggio sui LED. `board/custom_keys.c`: `B68_LK_1..3` (anche il rilascio). `board/led_app.c`: **le modalità wireless accendono la retroilluminazione** (alimentata dalla batteria).
- `hal/sh68f90.h`/`hal/gpio.h`: registri della seriale e macro di direzione. Makefile: la lista `NOT_LINKED` è di nuovo vuota.
- Test `test_link.c` (10 test con una radio che risponde ai comandi di modo): stabilità di 50 ms, selettore che rimbalza, USB = cavo, 2.4G, uscita dalla radio con rilascio dei tasti, tocco su un tasto Bluetooth, pressione lunga = accoppiamento (e rilascio successivo senza effetti), tasti fuori dal Bluetooth, rilascio di un altro tasto, riavvio dopo lo sleep.
- Verifica: `make test` 19 file OK; `make fw size` da pulito → flash 40143/44032, XDATA 3246/4096. **Restano 3,9 KB di flash**: lo sleep dovrà starci. Simulatore non usato; nessuna prova sull'hardware (seriale, priorità, SEL/ATT, selettore: tutto ripreso dal porting che funzionava in 2.4G e Bluetooth).

### Fase 6, passo 6 — sleep
- `core/sleep.[ch]` (puro, testato): tempo di inattività contato in secondi dall'orologio in ms del `main` (sopporta il giro dei 16 bit se chiamato almeno ogni 65 s), livelli 1, 5, 10, 30 minuti o mai (predefinito 10), uso (tasto, manopola) che azzera il conteggio, cambio di livello che lo riavvia (un livello più corto non lo trova già superato), richiesta immediata (Fn+Tab), e «non consentito» (cavo visto dal PC o modo USB): il conteggio resta a zero e parte dal momento in cui diventa possibile, una richiesta fatta nel frattempo si dimentica. 9 test; mutazione provata. **Differenza dal porting:** SMK contava frame LED (con un divisore per stare in 16 bit); qui si contano secondi veri.
- `hal/power.[ch]`: la sequenza di power-down dello stock/porting — USB spento, orologio e PLL fermi, sorgenti di risveglio armate (evento USB e INT4 con i valori `IENC = 0xF3`, `EXF0 = 0x40` dello stock, non documentati nel datasheet), regolatore spento, `SUSLO = 0x55` seguito subito da `PCON |= PD`; al risveglio orologio, regolatore (500 µs di assestamento) e flag USB. `hal/clock.c`: `clock_stop`/`clock_wake_restart`; `hal/usb.c`: `usb_hw_deinit`. Vettore INT4 a 0x000B verificato.
- `board/sleep_hw.c`: la sequenza completa nell'ordine dello stock: radio (`01 0C 08 07 00`, `01 0B 00`), timer e PWM fermi, seriale spenta, tutti i `PWMxxCON` a 1, pin parcheggiati (righe in ingresso con pull-up, colonne/LED/selettore/encoder in uscita a 0, P4.1 alto = linea di risveglio, P7.6 basso), INT4; al risveglio GPIO come lo stock, PWM, seriale, radio e selettore da riapplicare, USB rilasciato e riavviato. Solo con un modo wireless e **senza** cavo configurato dal PC. Tasti: `B68_SLP_DN/UP` (livello −/+, salvato nelle impostazioni, i tasti 1–5 lampeggiano il passo) e `B68_SLEEP` (subito). L'uso è registrato da `keyboard_hook_activity` (tasti e scatti della manopola).
- **Rischio dichiarato, non attenuabile senza hardware:** se un pin o un registro fosse sbagliato la tastiera non si sveglierebbe; si recupera staccando/riattaccando l'USB (e con Esc+1+2 si entra nel bootloader). Tutto il codice di questo passo è ripreso dal porting, dove era scritto ma non ancora provato dall'utente; da provare con la batteria, senza cavo, con un tester in serie per misurare la corrente.
- Verifica: `make test` 20 file OK; `make fw size` → flash 41194/44032, XDATA 3257/4096. Simulatore non usato; nessuna prova sull'hardware.

### Fine fase 6
Encoder, impostazioni globali, retroilluminazione con tick alternato, radio 2.4G/Bluetooth con selettore e slot, sleep. **Margine di flash: 2,8 KB sotto 0xAC00.**

## Fase 7 — parità con il porting e preparazione alla prima prova
Il piano (approvato dall'utente) è: 1 correzioni di sicurezza, 2 controllo degli interrupt, 3 diagnostica, 4 build a gradini, 5 HUD, 6 ucsim, 7 indicatore della batteria, 8 procedura di prova e documenti. Dal confronto con il porting SMK è emerso, tra l'altro, che l'**HUD** (usato dall'app dell'utente) era stato escluso per errore.

### Fase 7, passo 1 — correzioni di sicurezza
- **Watchdog (grave).** Il porting compila con `-DWATCHDOG_ENABLE=1` e ricarica il watchdog (`RSTSTAT = 0`) nel ciclo principale e dentro alle attese; Oblio non definiva il flag, quindi non lo ricaricava mai: se il watchdog fosse attivo nel chip (code option) la tastiera si sarebbe riavviata in continuazione. Ora `-DWATCHDOG_ENABLE=1` è nei flag del `Makefile`, `delay_us` ricarica il watchdog a ogni giro (la scrittura a `RSTSTAT` sta nei 19 cicli del corpo, tempi invariati; verificato nell'asm generato), l'init ricarica fra uno stadio e l'altro.
- **Il watchdog non deve essere tenuto vivo dall'interrupt.** La scansione della matrice (che gira nell'ISR del Timer 2) usa la nuova `delay_us_nokick`: con un `delay_us` che ricarica, l'ISR avrebbe mantenuto vivo il watchdog anche con il `main` bloccato e la tastiera non si sarebbe mai riavviata da sola. (Nel porting l'ISR lo ricaricava.)
- **Regolatore** (`REGCON`) acceso come prima cosa dell'avvio, prima del clock, come lo stock (`ldo_init`); prima si accendeva solo con l'USB.
- Verifica: `make test` 20 file OK; `make fw size` da pulito → flash 41250/44032, XDATA 3257/4096. (Il primo commit di questo passo non compilava il firmware: due etichette dell asm di `delay_us_nokick` coincidevano con quelle generate dal compilatore per `delay_ms`; corretto subito nel commit successivo. Da ora controllo che `make fw size` stampi la riga della flash prima di ogni commit.) Nessuna prova sull'hardware: se il watchdog è attivo nel chip lo si vedrà dalla causa di reset (diagnostica, passo 3).

### Fase 7, passo 2 — controllo degli interrupt (CI)
- `tools/check_interrupts.py` (nuovo, riscritto; il porting ha `utils/check_interrupts.py`): legge l'hex e la mappa del linker e controlla che (1) i vettori siano **esattamente** Timer 2 (`_tick_isr`), INT4 (`_power_int4_isr`), USB (`_usb_isr`) e EUART0 (`_radio_uart_isr`), che ogni slot libero sotto il più alto sia un `reti` (come li riempie SDCC) e che ogni salto punti al simbolo giusto; (2) l'**overlay statico di SDCC sia vuoto** (`l_OSEG = 0`): SDCC mette le variabili locali delle funzioni non rientranti in un'area condivisa senza considerare gli interrupt, e un'ISR potrebbe sovrascrivere variabili del `main` (nel porting aveva bloccato la tastiera); con `--stack-auto` le locali stanno nello stack e il controllo fa sì che resti così.
- `make check-interrupts` e `make check` (con `check-vial`); nuovo passo nel workflow CI dopo il controllo del budget. Verificato con due casi negativi costruiti a mano (un vettore sovrascritto in un hex copia; un overlay finto nella mappa): falliscono entrambi con messaggio chiaro. Sul firmware attuale: «interrupt vectors ok (4 handlers), no static overlay».
- Verifica: `make fw size` → flash 41250/44032 (invariata).

### Fase 7, passo 3 — diagnostica per la prima prova
- `core/diag.[ch]` (puro, testato): 13 contatori a 16 bit con un solo scrittore ciascuno (un'ISR **o** il main): causa dell'ultimo reset (`RSTSTAT` letto prima del primo kick del watchdog, che altrimenti ne cancella i flag), stack massimo usato (IRAM dello stack riempita con 0xA5 all'avvio e riletta dall'alto: l'errore possibile è per difetto di qualche byte, mai per eccesso), slot del timer in ritardo (il flag di overflow è già alto a fine ISR), scansioni perse prima del ritiro dal `main`, giro più lento del ciclo principale (ms), overflow della coda USB, cambi di matrice rinviati, rifiuti / frame non terminati / frame `03` e `08` / cambi di stato della radio, settori di flash cancellati.
- Comando VIA riservato **`0xF1`** (lo `0xF0` è dell'HUD, passo 5), risposto **solo a tastiera sbloccata**: tipo 0 = i contatori (little-endian dal byte 3), tipo 1 = i 6 byte dell'ultimo stato della radio e **i 22 byte dell'ultimo frame `08`**, per decodificare la batteria (passo 7). `tools/diag.py` (nuovo; Python, `/dev/hidraw`, nessuna dipendenza): trova l'interfaccia raw HID per VID:PID e usage page, guida lo sblocco (tenere Esc e Invio), stampa i contatori, `--radio` anche i dati della radio, `--watch N` ripete e mostra solo i cambiamenti.
- **Esito della ricerca per la batteria (serve al passo 7):** nel dump stock il comparatore di bassa tensione (`LPDCON` 0xB3, `LPDSEL` 0x89) compare **una sola volta, in una routine che li azzera** (`e4 f5 b3 f5 89 22` a 0xEDCE, accanto ad altre azzerature di periferiche): lo stock non lo usa per misurare la batteria. Resta la radio (stato `02 06`, frame `08`, comandi ancora ignoti `0D`, `0E`) come candidata; quindi niente scansione LPD nella diagnostica.
- La radio conserva ora per intero l'ultimo frame `08` (`radio_last_info()`, 22 byte di XDATA) e ne conta i tipi di frame.
- **Costo:** flash 42413/44032 (+1163 B, più del previsto: tutte le chiamate ai contatori e le copie dei buffer), XDATA 3305/4096. **Margine rimasto: 1,6 KB**: HUD e batteria devono starci; `--opt-code-size` non guadagna nulla (42399), quindi se serve spazio si ottimizzerà il codice (per esempio l'accesso a parola nello store) prima di togliere funzioni.
- Test: `test_diag.c` (contatori, saturazione, massimo, risposta tipo 0 e 1, tipo sconosciuto) e un caso in `test_via.c` (bloccata: `0xFF`; sbloccata: risponde). Verifica: `make test` 21 file OK; `make fw size` → riga della flash presente.

### Fase 7, passo 4 — build a gradini per la prima accensione
- `make stage N=1..4` (e `make stages` per 1–3): stesso codice, ma si **avvia** meno roba, così un guasto della prima prova si isola al modulo che l'ha portato. **1** = matrice, USB, Vial, HUD (quando arriva), profili, impostazioni, encoder; **2** = + retroilluminazione (uscite PWM e slot LED del timer); **3** = + radio, selettore di modo e tasti Bluetooth; **4** = + sleep (la build di rilascio, `make fw`). Ogni stadio finisce in `build/stageN/oblio.hex`, con budget e controllo degli interrupt (`make stage` li esegue).
- Come: `board/stage.h` (`STAGE_LED`, `STAGE_RADIO`, `STAGE_SLEEP`) e pochi `#if` in `board/main.c`, `board/slots.c` e `board/led_hw.c` (pausa/ripresa/sottoquadro dei LED diventano nulli sotto lo stadio 2: nessun registro PWM toccato, nemmeno dal salvataggio in flash). `link_wireless()` resta falso finché `link_init()` non gira, quindi sotto lo stadio 3 i report vanno sempre all'USB. I moduli non hanno `#if`; la scansione resta nell'ISR in ogni stadio (così la matrice si prova com'è nella versione finale).
- Il `Makefile` ha la directory di build parametrica (`B`) e `STAGE`; la CI compila gli stadi 1–3 e carica `oblio-stages` come artifact.
- Flash per stadio (il codice è lo stesso, cambia solo ciò che resta richiamabile): 1 → 42319, 2 → 42356, 3 → 42377, 4 → 42413 su 44032; XDATA 3305/4096 in tutti. `make test` 21 file OK.
- Corretto di passaggio: `HEADERS` era definito dopo il suo primo uso nel `Makefile` (le dipendenze dagli header del file generato di Vial erano vuote).

### Fase 7, passo 5 — HUD (e un recupero di flash)
- `core/hud.[ch]` (puro, testato): il protocollo 2 di `docs/hud-protocol-v2.md` per l'overlay dell'utente, comando `0xF0` sull'interfaccia raw HID (pacchetti da 32 B, eco di comando e sequenza, little-endian). `GET_INFO` (protocollo 2, 5 righe, 16 colonne, 8 layer, capacità bit 3 = tasto HUD_TOG, keycode `0x7E00`, 12 keycode per richiesta, versione della keymap, 2 byte per riga), `GET_STATE` (layer attivi a 32 bit, layer di default, prime 8 colonne delle 5 righe, mods, mods deboli = 0, mods e layer one-shot, flag Caps/Num dal report dei LED dell'host, contatore HUD_TOG, versione), `GET_KEYMAP` (keycode effettivi, ≤12; argomenti fuori limite, **anche un offset che “gira” a 16 bit**, → stato 2), `GET_MATRIX` (10 byte, 2 per riga, bit 0 = colonna 0), comando sconosciuto → stato 1. Instradato da `core/via.c` (`0xF0`); il tasto `0x7E00` incrementa il contatore e non va all'host (`board/custom_keys.c`); in `board/vial.json` la voce `0x7E00` torna «HUD Toggle» (il segnaposto «unused» era un errore mio). Nome USB «Oblio B68»: l'app dell'utente non filtra per nome. Test `test_hud.c` (8) con l'esempio reale del documento (`f0 01 00 05 …`, matrice con due tasti premuti), argomenti non validi, one-shot, contatore; un caso in `test_via.c` per l'instradamento. Il test ha scovato un mio errore: il controllo `offset + count` era a 16 bit e si lasciava ingannare da un offset 0xFFFF.
- **Spazio in flash.** L'HUD ha portato il firmware oltre 0xAC00 (44258 B su 44032). Recuperato in questo modo (misurato a ogni passo):
  - **puntatori `__xdata` al posto dei puntatori generici** nei percorsi dei pacchetti (VIA, Vial, HUD, diagnostica, raw HID): con `--model-large --stack-auto` un puntatore generico costa 3 byte e un accesso lento a ogni `data[i]`; con `XDATA uint8_t *` (macro in `core/platform.h`, vuota sull'host) due byte e accesso diretto: **−1481 B** in un colpo solo;
  - tabella statica dell'effetto radiale (80 B in ROM) al posto del calcolo all'avvio con la radice intera e le divisioni: **−580 B di flash e −101 B di XDATA** (il test ricalcola la tabella dalla formula e la confronta);
  - `matrix_row_bits()` (una riga alla volta) per la matrice di Vial e dell'HUD; scorrimento della keymap senza la divisione per 80 nel buffer di VIA; dimensione della definizione di Vial a 16 bit (niente moltiplicazione a 32 bit); `store.c` con un cursore che avanza per conteggio invece dei tre dispatcher a divisioni (−59 B, il guadagno vero è la leggibilità: lo store resta ~3 KB per via del codice di cancellazione/scrittura/CRC).
  - Non servono: `--opt-code-size` e `--max-allocs-per-node` (differenze di pochi byte).
- Risultato: flash **42103/44032** (margine 1929 B), XDATA 3215/4096. Stadi 1–3: 42009, 42046, 42067. `make test` 22 file OK; vettori e overlay OK.

### Fase 7, passo 6 — ucsim: il firmware vero nel simulatore del chip
- **Strumento.** `flake.nix` ora costruisce `ucsim-sh68f90` (uCsim di SDCC con l'SH68F90 come variante di CPU, `tools/ucsim/`: dal porting SMK, GPLv2, in NOTICE.md; ~3 min la prima volta). Il modello riproduce core, vettori (Timer 2 a 0x0003, INT4, USB a 0x003B, seriale), registri degli endpoint USB, programmazione della flash, watchdog e power-down; **non** una scheda: i pin li decide il test. Il suo Timer 2 è a cadenza virtuale fissa: si verifica l'**ordine** degli eventi, non le durate.
- **Modifiche al modello (marcate nel sorgente, NOTICE.md):** sorgente di interrupt per EP2 OUT (il canale raw HID non poteva essere simulato), log del pacchetto EP2 IN fino a 64 byte, e tolto il limite di 16 pacchetti del log di EP0 (tagliava i descrittori lunghi e ogni trasferimento successivo).
- `make sim` (`tests/sim/oblio_sim.py`, `test_sim.py`; non fa parte di `make test`; job `simulation` nella CI): **23 test sul firmware vero**: parte e arriva al ciclo principale con gli interrupt accesi; il watchdog non scatta in 3000 giri; gli slot del timer sono 4 LED poi 1 scansione (60 slot); lo **stack** dopo 400 giri con tutti gli interrupt: **91 byte su 223** (prima misura reale, soglia di regressione a 120); recupero Esc+1+2 → salto a 0xFF00 con A = 0x5A e B = 0xA5, e nessun salto senza accordo o con un'altra riga; **ogni descrittore** (dispositivo, configurazione, stringhe, HID, report) esce dal vero gestore dell'interrupt a pacchetti da 8 byte identico alle tabelle dei test host (compilate con gcc e confrontate); SET_ADDRESS (indirizzo applicato dopo lo status stage) e SET_CONFIGURATION; richiesta sconosciuta senza dati; **il canale raw HID** attraverso ISR e `main`: versione VIA `01 00 09`, id Vial `06 00 00 00 46 ec d0 56 5d f8 4f a8`, **HUD `f0 01 00 05 02 05 10 08 08 00 00 7e 0c 02 02`** (l'esempio del documento del protocollo), lettura della keymap, comando sconosciuto `0xFF`, diagnostica rifiutata da bloccata, due richieste di fila; **tasti → report su EP1** (riga tenuta bassa: `03 02 …` poi Z, solo con USB configurato); il comando ISP via USB salta al bootloader (e un comando diverso no).
- Limiti dichiarati: PWM, radio e sleep non sono provati qui (il modello non li riproduce con fedeltà); le durate (slot da 400/450 µs, millisecondi veri) restano da misurare sull'hardware; il selettore di modo si finge portando a 1 i bit della porta 0 (il modello non ha pull-up su quella porta).
- `CLAUDE.md` aggiornato: il simulatore ora è ammesso dove serve.

### Fase 7, passo 7 — indicatore della batteria (interfaccia completa, sorgente del dato ancora ignota)
- **Tasto:** Fn+Backspace, keycode `0x7E10` (era la traccia della radio, non portata); in Vial «Battery» (`BAT`), stesso numero degli altri tasti personalizzati. Il livello Fn predefinito cambia di una cella (cambia quindi l'impronta della mappa predefinita: come previsto dallo store, eventuali copie salvate con la vecchia impronta non si usano).
- **Visualizzazione** (`core/led.c`, `led_show_battery(percent)`): per ~3 s dopo il tocco (366 frame; un secondo tocco rinnova il tempo e rilegge il livello) i tasti **1 2 … 9 0** (riga 0, colonne 1–10) mostrano *n* tasti accesi con *n* = ceil(percentuale / 10), almeno 1: l'**1 è circa il 10 %, lo 0 è pieno**; **verdi** sopra il 20 %, **rossi** al 20 % o meno (soglie scelte dall'utente). Gli altri LED restano come sono, i tasti non accesi del numero conservano l'effetto. Senza lettura (`LED_BATTERY_UNKNOWN`, o un valore oltre 100) tutti e dieci lampeggiano **in bianco**: si vede che la funzione c'è ma il dato no. Funziona in tutti e tre i modi (USB, 2.4G, Bluetooth), perché dipende solo dai LED, che sono accesi in tutti e tre.
- **La sorgente del dato non c'è e non la invento.** `board/battery.c: battery_percent()` oggi dà «sconosciuto». Cosa si sa: lo stock **non** usa il comparatore di bassa tensione del chip (lo azzera soltanto, a 0xEDCE); il frame di stato `02 06` e il frame `08` della radio non sono decodificati; i comandi radio `0D` e `0E` sono ignoti. Per trovarla: `tools/diag.py --radio --watch 2` stampa gli ultimi byte di stato e **il frame `08` intero**; l'utente li guarda mentre la batteria si scarica e si carica (e nei tre modi) e si scrive la mappatura solo dopo aver visto i dati.
- **USB.** In modo USB la radio è spenta (come nello stock, secondo i miei appunti: la seriale veniva disabilitata), quindi lì non c'è nessun dato dalla radio. Per vedere se risponde comunque: nuovo `diag.py --monitor` (comando di diagnostica tipo 2) la mette «in ascolto» (`radio_monitor`: solo richieste di stato e conferme dei suoi frame, **nessun comando di modo, nessun report**), ed è spenta di default. È un esperimento che richiede l'hardware; se la radio non risponde senza modo, la batteria in USB dovrà avere un'altra sorgente (un segnale di «in carica» e una lettura dell'MCU) e sarà una decisione da prendere sui dati.
- Test: 4 nuovi in `test_led.c` (tutte le soglie 0/1/10/11/20/21/50/90/91/95/100 con colore e numero di tasti, fine dopo 3 s e rinnovo, lampeggio senza lettura, valore oltre 100), `radio_monitor` in `test_radio.c` e `test_link.c` (solo in USB), tipo 2 in `test_diag.c`; mutazione (soglia `<=` → `<`) → falliscono. `make test` 22 file; `make sim` 23 test OK (stack ora 104 B su 223).
- Flash **42549/44032** (margine 1483 B), XDATA 3219/4096. Stadi 1–3: 42455, 42492, 42513.

### Fase 7, passo 8 — documenti
- `docs/architecture.md` (nuovo, inglese): livelli (`core`/`hal`/`board`/`tools`), hook, flusso dei dati dai tasti ai report, perché il timer possiede le colonne (la sequenza degli slot), regole di concorrenza ISR/main, mappa della memoria, dove stanno le incognite, come si prova (host / linker / simulatore / hardware).
- `docs/first-boot.md` (nuovo, inglese): la procedura della prima accensione a gradini, con il rischio dichiarato (senza ICP, un firmware che cade prima di `recovery_check()` non si recupera con Esc+1+2), il ritorno al firmware SMK, il comando `sinowisp`, per ogni stadio cosa guardare e quali valori attendersi da `tools/diag.py`, e cosa rimandare indietro.
- `README.md`: era l'unico file con modifiche rimaste da una sessione precedente; verificato, aggiornato (HUD, indicatore batteria, target `make`, cartelle, link ai due documenti nuovi) e committato su indicazione implicita dell'utente di proseguire il lavoro.

### Fine fase 7
Parità con il porting verificata (tutto portato, HUD compreso; escluse solo traccia/sonda radio e console di debug), correzioni di sicurezza (watchdog), controlli di CI (budget, vettori, overlay, Vial, stadi, simulatore), diagnostica e documento di prova. **Prossimo passo: la prima prova sulla tastiera, a gradini (docs/first-boot.md).** Flash 42549/44032, XDATA 3219/4096.
