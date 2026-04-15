# InsideNow — Manuale d'uso

## Cos'è InsideNow

InsideNow è un sistema di luci wireless composto da due tipi di dispositivi:

- **Gateway** — riceve il segnale luci dallo show controller (via cavo DMX) e lo trasmette via radio
- **Nodo** — riceve il segnale radio e accende la striscia LED con i colori comandati

Ogni nodo è autonomo: ha la sua batteria e si gestisce da solo.

---

## Il Nodo

### Primo avvio

1. Premi il **tasto** per accendere il nodo
2. I LED lampeggiano **verde 3 volte** → il nodo è avviato correttamente
3. Per un secondo si accende il **primo LED** con un colore dal rosso al verde: indica la carica della batteria
   - Rosso = batteria scarica
   - Arancione = mezza carica
   - Verde = batteria carica

### Ricezione del segnale

Non appena il gateway trasmette, i LED del nodo si accendono con i colori commandati dallo show controller. Il nodo funziona in modo completamente automatico.

### Se il segnale si interrompe

| Cosa vedi | Cosa significa |
|---|---|
| LED lampeggia **arancione** | Segnale radio perso da più di 2 secondi |
| LED lampeggia **rosso 5 volte** poi si spegne | Il nodo sta andando in standby |

Il nodo va in standby automaticamente dopo **5 minuti** senza ricevere segnale.

### Spegnimento manuale

Tieni premuto il tasto per **3 secondi**: i LED lampeggiano rosso 5 volte e il nodo si spegne.

### Riaccensione

Premi il tasto: il nodo si risveglia e la sequenza di avvio ricomincia (lampeggio verde, indicatore batteria).

---

## Ricarica del Nodo

Collega il nodo all'alimentatore tramite il connettore di ricarica.

- Il **primo LED** inizia a fare un effetto di **respiro** (si accende e spegne lentamente)
- Il colore indica la carica attuale: rosso = scarico, verde = carico
- Mentre è in carica il nodo **rimane sempre acceso** e continua a ricevere il segnale normalmente — non va mai in standby

Scollega l'alimentatore quando hai finito: il nodo torna a funzionare a batteria.

---

## Riepilogo segnali luminosi

| Segnale | Significato |
|---|---|
| 3 lampeggi **verdi** all'accensione | Avvio avvenuto correttamente |
| 1° LED colorato fisso per 2 secondi | Livello batteria (rosso = scarico, verde = carico) |
| LED accesi con colori | Funzionamento normale, segnale ricevuto |
| LED **arancione** lampeggiante | Segnale radio assente, in attesa... |
| 5 lampeggi **rossi** | Spegnimento in corso |
| 1° LED che respira (rosso/verde) | In carica tramite cavo |

---

## Il Gateway

Il gateway non ha batteria né tasto. Si alimenta sempre da rete e funziona in automatico:

1. Collegalo all'alimentatore
2. Collegalo allo show controller con il cavo DMX
3. Inizia subito a trasmettere: non serve fare nulla

Un piccolo LED sul gateway lampeggia ad ogni segnale DMX ricevuto — se non lampeggia, controlla il cavo DMX.

---

## Risoluzione problemi rapida

**I nodi non si accendono con i colori giusti**
Controlla che il gateway sia acceso e che il LED sul gateway lampeggi.

**Il nodo fa blink arancione anche se il gateway è attivo**
Il nodo è fuori portata o c'è un ostacolo. Avvicinalo al gateway.

**Il nodo si è spento da solo**
Batteria scarica o 5 minuti senza segnale: ricaricalo o riaccendilo con il tasto.

**Il nodo non risponde al tasto**
Batteria completamente esaurita: collegalo alla ricarica per alcuni minuti, poi ripremi il tasto.
