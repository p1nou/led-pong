# led-pong

Un jeu de Pong à une dimension sur un ruban de LEDs, piloté par un ESP32.

---

## Présentation

Deux joueurs s'affrontent sur un ruban de 45 LEDs WS2812B. Une balle blanche rebondit d'un bout à l'autre. Chaque joueur doit renvoyer la balle en appuyant sur son bouton au bon moment. La balle s'accélère à chaque échange. Le joueur dont la zone de défense se réduit à zéro a perdu.

```
[ROUGE 🔴] LED 0 ←————————— balle blanche ————————————→ LED 44 [VERT 🟢]
```

---

## Matériel

| Composant | Détails |
|---|---|
| Microcontrôleur | ESP32 (type `esp32dev`) |
| Ruban LED | WS2812B, **45 LEDs**, signal sur broche **5** |
| Bouton joueur Rouge | Poussoir relié entre la broche **19** et GND |
| Bouton joueur Vert | Poussoir relié entre la broche **18** et GND |

> Les broches des boutons sont configurées en `INPUT_PULLUP` : le bouton doit relier la broche à la masse (actif à l'état bas).

### Câblage simplifié

```
ESP32
├── GPIO 5  ──► Data WS2812B
├── GPIO 18 ──► Bouton Vert (autre borne → GND)
└── GPIO 19 ──► Bouton Rouge (autre borne → GND)
```

---

## Règles du jeu

### Zones et score

- À l'initialisation, chaque joueur possède une **zone de défense de 3 LEDs** à son extrémité du ruban.
- Les LEDs de la zone Rouge s'allument en rouge, celles de la zone Verte en vert.

### Renvoyer la balle

- Appuyez sur votre bouton **quand la balle est dans votre zone** pour la renvoyer.
- La balle accélère à chaque renvoi réussi (vitesse initiale : 120 ms/LED, minimum : 25 ms/LED).

### Marquer un point

Un point est accordé à l'attaquant si :
- La balle atteint le **mur du fond** (extrémité du ruban) sans être renvoyée. Le défenseur doit alors appuyer sur son bouton pour relancer la balle depuis son mur (dans la seconde qui suit, sinon sa zone continue de se réduire).
- Un joueur appuie sur son bouton **hors de sa zone** ou au **mauvais moment**.

Quand un point est marqué :
- La zone de l'**attaquant grandit** d'une LED.
- La zone du **défenseur rétrécit** d'une LED.

### Victoire

La partie se termine quand :
- La zone d'un joueur tombe à **0 LED** → l'autre joueur gagne.
- Les deux zones remplissent tout le ruban → le joueur avec la plus grande zone gagne (égalité = résultat aléatoire).

Une animation de clignotement dans la couleur du gagnant est jouée, puis le jeu redémarre automatiquement.

### Pause et réinitialisation

- Relâcher les deux boutons simultanément met le jeu en **pause**.
- Une pause de **10 secondes** réinitialise complètement la partie.

---

## Installation

### Prérequis

- [PlatformIO](https://platformio.org/) (CLI ou extension VSCode)

### Compilation et flash

```bash
# Compiler et flasher l'ESP32
pio run --target upload

# Ouvrir le moniteur série (115200 baud)
pio device monitor
```

La dépendance [`makuna/NeoPixelBus`](https://github.com/Makuna/NeoPixelBus) est téléchargée automatiquement par PlatformIO.

---

## Paramètres principaux

| Constante | Valeur par défaut | Description |
|---|---|---|
| `NUM_LEDS` | 45 | Nombre de LEDs du ruban |
| `START_ZONE` | 3 | Taille initiale de chaque zone |
| `START_STEP_MS` | 120 ms | Intervalle de déplacement initial |
| `MIN_STEP_MS` | 25 ms | Intervalle minimum (vitesse max) |
| `SPEEDUP_MS` | 6 ms | Accélération par renvoi |
| `INACTIVITY_MS` | 1 000 ms | Délai de pénalité au mur du fond |
| `PAUSE_RESET_MS` | 10 000 ms | Durée de pause avant réinitialisation |
| `BRIGHTNESS` | 128 | Luminosité (0–255) |