# Changelog — Oblio

## Piano (fasi)
0. Bootstrap (repo, flake, CI, docs) — **fatto**
1. CI: build del firmware, budget flash/XDATA, hex come artifact, Release al tag
2. HAL minimo e matrice 5×16
3. Logica pura con test sull'host (layer, tapping, report, macro, profili)
4. USB e raw HID (dopo l'arrivo del programmatore ICP)
5. Vial/VIA
6. Radio, LED, profili, sleep
7. Parità con il porting SMK e passaggio

Il controllo di recupero Esc+1+2 va all'inizio del `main`, prima di USB e radio.

## 2026-11-01
- Repo creato. Documenti hardware, radio e LED portati dal porting SMK (`docs/`).
