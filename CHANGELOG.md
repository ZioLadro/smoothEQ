# Changelog

## [0.1.0] - 2026-11-18
Prima versione.
- Soppressione dinamica delle risonanze su STFT (Hann, sovrapposizione 75 %): ogni bin viene confrontato con il suo intorno logaritmico e abbassato dell'eccesso, al massimo di `Depth`.
- Rilevatore con `Depth`, `Sharpness`, `Selectivity`, `Attack`, `Release`, modo `Soft` / `Hard` e tre risoluzioni (21, 43 o 85 ms di latenza a 48 kHz, stessa risoluzione in Hz fino a 192 kHz).
- Curva di sensibilità a sei nodi (low shelf, quattro campane, high shelf) e range `Low Cut` / `High Cut`, modificabili trascinando sul display.
- Uscita con `Link`, `Mix` allineato alla latenza, `Output`, modi `Stereo`, `Mid`, `Side`, `Mid/Side` e ascolto `Delta`.
- Display con spettro d'ingresso e d'uscita, riduzione e curva; latenza e riduzione di picco nell'header.
- 15 preset di fabbrica e preset utente.
- Interfaccia nella grafica della famiglia (SONIC GRAIN, DR. SEGG-UENCER, MULTITAILS), su fondo viola e verde acqua. Finestra 1280 × 800 ridimensionabile dal 60 al 200 %.
- VST3 per Windows, macOS e Linux, AU per macOS, AAX opzionale. Test headless, pluginval strictness 8.
