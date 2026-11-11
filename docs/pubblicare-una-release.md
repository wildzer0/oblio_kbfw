# Pubblicare una nuova versione del firmware (e renderla disponibile nell'utility web)

Questa guida serve ogni volta che modifichi il firmware e vuoi che la nuova versione compaia **nel menu delle release** della pagina
<https://wildzer0.github.io/oblio_kbfw/>, pronta da installare con un click.

## In due righe

1. Alza il numero in `VERSION`, aggiorna `CHANGELOG.md`, controlla che tutto sia verde, fai commit e push su `main`.
2. Metti il tag `vX.Y.Z` (uguale a `VERSION`) e fai il push del tag. Il resto lo fa la CI: pochi minuti dopo la versione è nel menu della pagina.

## Che cosa succede dietro le quinte

```
 git push origin vX.Y.Z   (il tag)
        │
        ▼
 ci.yml sul tag:  test ─► simulation ─► release
        │                                  │
        │  controlla che il numero dentro   ├─ pubblica la Release di GitHub con oblio.hex
        │  l'immagine = VERSION = il tag    └─ avvia pages.yml sul ramo main
        ▼                                                 │
   (se non coincide: la CI si ferma)                      ▼
                                       pages.yml: tools/make_site.py
                                       ─ copia web/ e il .hex di OGNI release pubblicata
                                       ─ scrive releases.json
                                       ─ pubblica su GitHub Pages
```

Perché la pagina non scarica il file direttamente dalla Release di GitHub: il browser non può (il download passa da un server senza CORS).
Per questo il sito ha una **sua copia** di ogni `oblio.hex`, ricostruita ogni volta dalle Release, che restano la fonte. Nel repository non ci sono file binari.

Perché `pages.yml` parte da `main` e non dal tag: l'ambiente `github-pages` accetta pubblicazioni solo dal ramo `main`; un tag viene rifiutato.
Il job `release` lo avvia con `gh workflow run pages.yml --ref main`.

## Procedura passo per passo

### 1. Scegli il numero di versione

Il formato è `major.minor.patch` nel file `VERSION` (solo numeri, tre parti, niente «v» e niente suffissi).

| Cosa hai cambiato | Cosa alzare | Esempio |
| --- | --- | --- |
| Una correzione, un'ottimizzazione, un piccolo effetto | **patch** | 0.9.0 → 0.9.1 |
| Una funzione nuova o un comportamento che l'utente nota | **minor** | 0.9.1 → 0.10.0 |
| Un cambiamento che richiede di rifare le impostazioni o rompe la compatibilità | **major** | 0.10.0 → 1.0.0 |

La pagina confronta i numeri (non i commit): «aggiornamento» se il nuovo è più alto, «più vecchio» se è più basso. Perciò **il numero deve crescere** a ogni release.
Il numero deve essere diverso da quello di una release già pubblicata: una stessa versione non si pubblica due volte (vedi «Se qualcosa va storto»).

### 2. Aggiorna il numero e il registro delle modifiche

```
echo 0.9.1 > VERSION
```

Poi in `CHANGELOG.md` aggiungi in cima una sezione con data e titolo e le modifiche (stile delle voci già presenti). Se hai cambiato un comportamento, aggiorna anche
`docs/design.md` e, se serve, `docs/session-state.md`; se hai aggiunto o cambiato funzioni pubbliche, `make api` rigenera `docs/api.md`.

### 3. Controlla che tutto sia verde, in locale

```
nix develop --command make test web-test
nix develop --command make fw size check
```

- `make test` e `make web-test`: i test sul PC e l'aggiornamento completo dalla pagina (con una finta tastiera).
- `make fw size`: la build e il budget di flash/XDATA (non deve superare i limiti).
- `make check`: la definizione Vial, i vettori di interrupt e **la versione nell'immagine** (`check-version`: compare una volta sola e coincide con `VERSION`).
- Se hai toccato `docs/api.md`: `make check-api`.
- Se vuoi la build senza registro batteria o di profilo: `make BATTERY_LOG=0 fw size`, `make LOOP_PROFILE=1 BATTERY_LOG=0 fw size` (la CI controlla la prima).

Un avviso utile: `build/oblio.hex` deve essere fatto da un albero **pulito** (tutto committato). Lo script scrive nel firmware se c'erano modifiche non committate
(`dirty`) e la pagina lo mostra come «+modifiche». Una release deve avere «0.9.1 (abc1234)» senza quel segno: committa prima, poi ricostruisci.

### 4. Commit e push su `main`

```
git add -A
git commit -m "Descrizione breve (0.9.1)"
git push
```

Aspetta che la CI di quel push sia verde (Actions → CI, oppure `gh run list --limit 3`). Non mettere il tag su un commit con la CI rossa: la Release pubblicherebbe un firmware non verificato.

### 5. Metti il tag e fallo arrivare su GitHub

Il tag deve essere **`v` + il contenuto di `VERSION`**:

```
git tag -a v0.9.1 -m "Oblio 0.9.1"
git push origin v0.9.1
```

(Sul Windows di PowerShell i comandi sono gli stessi. Nelle sessioni di Claude Code, mettere un tag è un'azione che il sistema di sicurezza può chiedere di confermare: si può
lanciare con il prefisso `!` nel prompt, oppure aggiungere un permesso.)

### 6. Guarda la CI e controlla il risultato

```
gh run list --limit 3                 # l'esecuzione del tag: test, simulation, release devono essere verdi
gh run list --workflow pages.yml --limit 1     # poi questa: deve essere verde
gh release view v0.9.1                # la Release, con oblio.hex
curl -s https://wildzer0.github.io/oblio_kbfw/releases.json    # deve elencare 0.9.1 per prima
```

Poi apri la pagina (ricaricala con Ctrl+F5 se il browser la tiene in cache): la versione nuova è nel menu «Release pubblicate». Con la tastiera collegata, la pagina mostra
«Versione installata: 0.9.0» e «Aggiornamento: da 0.9.0 a 0.9.1».

## Che cosa controlla la CI per te

- Che `build/oblio.hex` contenga la struttura di versione **una sola volta** (`tools/check_version.py`) e che dica lo stesso numero di `VERSION`.
- Sul tag: che il tag sia uguale a `v` + versione dell'immagine. Se scrivi `VERSION` 0.9.1 ma tagghi `v0.9.2`, la CI si ferma e non pubblica nulla.
- Che i test del PC, il simulatore, il budget di flash, i vettori di interrupt e i test dell'utility web passino.
- Quando costruisce il sito, che ogni Release contenga un'immagine con la versione del proprio tag (altrimenti quella Release viene lasciata fuori dal menu, con un avviso nel registro).

## Se qualcosa va storto

| Sintomo | Causa probabile | Cosa fare |
| --- | --- | --- |
| La CI sul tag fallisce al passo «The version is in the image…» | `VERSION` e tag non coincidono, o il numero non è nell'immagine | Correggi `VERSION`, committa, **cancella e rifai il tag** (sotto) |
| `release` verde ma il sito non mostra la versione nuova | `pages.yml` non è partito o è fallito | `gh run list --workflow pages.yml`; per rilanciarlo: `gh workflow run pages.yml --ref main` |
| `pages.yml` fallisce con «not allowed to deploy to github-pages» | La regola dell'ambiente accetta solo `main` e il workflow è partito da un tag | Va avviato su `main` (è il motivo del `--ref main` sopra) |
| `pages.yml` fallisce con un 404 sul Pages | Pages non è attivo con sorgente «GitHub Actions» | Settings → Pages → Source: **GitHub Actions** (una volta sola) |
| La pagina non mostra una Release che esiste | L'immagine di quella Release dice un'altra versione del tag | Guarda il registro di `make_site.py` in `pages.yml` («leaving out vX.Y.Z») |
| Hai pubblicato per errore una versione sbagliata | — | Vedi «Annullare o rifare un tag» |

### Annullare o rifare un tag

Solo se la versione è sbagliata **e nessuno l'ha ancora installata** (altrimenti pubblica una versione nuova con il numero successivo: è la via sicura).

```
gh release delete v0.9.1 --yes          # toglie la Release (se era stata creata)
git push origin :refs/tags/v0.9.1       # toglie il tag dal remoto
git tag -d v0.9.1                       # e da locale
# correggi, committa, poi rifai il passo 5
gh workflow run pages.yml --ref main    # il sito si ricostruisce senza la versione tolta
```

## Impostazioni fatte una volta sola (se rifai il repository o lo sposti)

- **Settings → Pages → Source = GitHub Actions**.
- **Settings → Environments → github-pages**: deve accettare il ramo `main` (è l'impostazione predefinita). I tag **non** servono.
- **Settings → Actions → General → Workflow permissions**: i permessi dei workflow sono dichiarati nei file `.github/workflows/*.yml` (`contents: write` e `actions: write` per il job `release`).
- Il repository deve essere pubblico (Pages gratuito).

## Dove sta il codice di tutto questo

| Cosa | File |
| --- | --- |
| Il numero di versione | `VERSION` |
| La struttura nel firmware | `core/version.[ch]` (il `Makefile` passa il numero e il commit) |
| Il controllo numero ↔ immagine ↔ tag | `tools/check_version.py`, `make check-version` |
| Il flusso test → release → sito | `.github/workflows/ci.yml` (job `test`, `simulation`, `release`) |
| La pubblicazione del sito | `.github/workflows/pages.yml`, `tools/make_site.py` |
| La pagina | `web/` (`index.html`, `js/app.js`, …; ogni file spiega il perché in testa) |
| Provare il sito in locale | `python3 tools/make_site.py /tmp/site --local build/oblio.hex v0.9.1 && cd /tmp/site && python3 -m http.server 8000`, poi `http://localhost:8000/` |
