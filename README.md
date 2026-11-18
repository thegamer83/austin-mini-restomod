# 🏎️ Restomod Digital Dashboard - Austin Mini (ESP32)

Tableau de bord numérique moderne de type "Fast Road / GT" développé pour une Austin Mini classique[cite: 6]. Le système repose sur un microcontrôleur **ESP32 (30 broches)** et combine un affichage graphique SPI, un bandeau de LEDs adressables WS2812B pour le shift-light et les alertes, un lecteur de carte SD, une centrale inertielle MPU-6050, un module GPS, ainsi qu'un serveur Web embarqué via Wi-Fi pour la configuration dynamique et l'export des données de trajet[cite: 5, 6].

---

## 📸 Aperçu du Projet

| Vue d'ensemble du Dashboard | Interface Web & Paramétrage |
| :---: | :---: |
| ![Dashboard en action](image/20261118_181519.jpg) | ![Interface Web](/image/Screenshot_20261010_135628_Chrome.jpg) |
| *Affichage TFT vintage / course*[cite: 6] | *Configuration web (Seuils RPM & Rôles LEDs)*[cite: 6] |

---

## ✨ Fonctionnalités Principales

- **Affichage TFT (3 Pages de Navigation) :**[cite: 6]
  1. *Page 1 (Cadrans Smiths Vintage) :* Grand compte-tours central (0–8000 tr/min) intégrant la vitesse GPS numérique, l'odomètre total, le trip partiel et les voyants (clignotants, feux, pleins phares), entouré de 4 cadrans auxiliaires (jauge d'essence, rapport engagé, température d'eau, pression d'huile)[cite: 5].
  2. *Page 2 (Tableau de Bord Course) :* Barre de régime symétrique progressive en dégradé avec affichage numérique central, indicateur de rapport engagé géant, jauge d'essence verticale, radar G-Force dynamique (Latéral / Accélération), vitesse GPS, odomètre/trip et températures/alertes moteur[cite: 5].
  3. *Page 3 (Réglages & Système) :* Statut du point d'accès Wi-Fi (et adresse IP), kilométrage total, trip partiel, et contrôle en temps réel de l'état opérationnel des périphériques (**Carte SD**, **MPU6050** et **GPS**)[cite: 5, 6].
- **Mode Démonstration intégré (`MODE_DEMO`) :**
  * Simulation autonome d'un cycle complet de conduite de 36 secondes en boucle (accélération 0–120 km/h, passage des rapports, variations de régime, température d'eau avec test d'alerte > 100 °C, clignotants, phares et forces G) sans nécessiter de capteurs branchés ni altérer l'odomètre sauvegardé en Flash[cite: 5].
- **Gestion Carte SD & Datalogging :**[cite: 6]
  * Affichage d'un logo de démarrage personnalisé (`/logo.bmp` au format 24-bits) au boot[cite: 5, 6].
  * Enregistrement automatique toutes les secondes des données de trajet (timestamp, vitesse GPS, régime moteur, température d'eau, niveau d'essence, alerte huile et odomètre) dans `/trip_log.csv`[cite: 5, 6].
- **Bandeau LED WS2812B (24 LEDs configurables) :**[cite: 6]
  * *Shift-Light progressif :* Seuils personnalisables pour le **Vert**, le **Rouge** et le **Bleu**, avec clignotement rapide global en cas de dépassement du RPM Max[cite: 5, 6].
  * *Rôles multiples affectables LED par LED :* Shift Vert/Rouge/Bleu, clignotants (gauche/droit), alertes eau (> 100 °C) / huile, statut Wi-Fi actif, ou éclairage fixe/feux[cite: 5, 6].
- **Serveur Web Embarqué (Mode AP) :**[cite: 6]
  * Connexion directe en Wi-Fi pour administrer le système, ajuster le kilométrage total, régler la luminosité, les seuils RPM, l'assignation et la couleur RGB de chacune des 24 LEDs, et **télécharger directement le fichier de logs CSV (`/trip_log.csv`)** de la carte SD[cite: 5, 6].
- **Persistance en Mémoire Flash :**[cite: 6]
  * Sauvegarde automatique du kilométrage total, du trip partiel (remise à zéro par appui long de 2 secondes sur le bouton Page) et de la configuration complète via la bibliothèque `Preferences`[cite: 5, 6].

---

## 📌 Schéma de Câblage Global

![Schéma de câblage final](image/Screenshot_2026-10-11_093627.png)

---

## 🔌 Tableaux des Assignations de Broches (Pinout)

> ⚠️ **Note importante sur le câblage :** Les assignations ci-dessous correspondent exactement au code `main.cpp` actuel (notamment l'inversion entre `SD_CS` sur **GPIO 2** et `PIN_IND_R` sur **GPIO 33**)[cite: 5].

### 🔌 Connecteur GX16 (10 Broches - Écran, LEDs & SD)
| Broche GX16 (10 pins) | Destination / Rôle | Broche ESP32 / Bus |
| :---: | :--- | :--- |
| **Pin 1** | VCC / Alimentation Écran | **5V** ou **3.3V**[cite: 6] |
| **Pin 2** | GND Masse Écran & LEDs | **GND**[cite: 6] |
| **Pin 3** | SPI MOSI (`TFT_MOSI`) | **GPIO 23** (`23`)[cite: 5, 6] |
| **Pin 4** | SPI SCK (`TFT_SCLK`) | **GPIO 18** (`18`)[cite: 5, 6] |
| **Pin 5** | TFT Chip Select (`TFT_CS`) | **GPIO 15** (`15`)[cite: 6] |
| **Pin 6** | TFT Data/Command (`TFT_DC`) | **GPIO 4** (`4`)[cite: 6] |
| **Pin 7** | TFT Reset (`TFT_RST`) | **GPIO 5** (`5`)[cite: 5, 6] |
| **Pin 8** | Rétroéclairage (`TFT_BL` / `PIN_BACKLIGHT`) | **GPIO 27** (`27`)[cite: 5, 6] |
| **Pin 9** | Données Bandeau LEDs (`PIN_LED_DATA`) | **GPIO 13** (`13`)[cite: 5, 6] |
| **Pin 10** | SD Chip Select (`SD_CS`) | **GPIO 2** (`2`)[cite: 5] |

### 🖥️ Affichage TFT & Carte SD (Bus SPI Partagé)
| Broche Périphérique | Rôle / Description | Broche ESP32 |
| :--- | :--- | :--- |
| **MOSI (SDA)** | Données SPI (Master Out - Partagé)[cite: 6] | **GPIO 23**[cite: 5, 6] |
| **MISO** | Données SPI (Master In - Partagé)[cite: 6] | **GPIO 19**[cite: 5, 6] |
| **SCK (SCL)** | Horloge SPI (Clock - Partagé)[cite: 6] | **GPIO 18**[cite: 5, 6] |
| **SD_CS** | Chip Select de la Carte SD (`SD_CS`)[cite: 5, 6] | **GPIO 2**[cite: 5] |
| **BL (Backlight)** | Rétroéclairage écran (`PIN_BACKLIGHT`)[cite: 6] | **GPIO 27**[cite: 5, 6] |

### ⚙️ Entrées Numériques, Commandes & Sécurité (TOR)
| Composant / Signal | Rôle / Description | Broche ESP32 |
| :--- | :--- | :--- |
| **Bouton Page** | Changement de page (appui court) / Reset Trip (appui long ≥ 2s) (`PIN_PAGE_BTN`)[cite: 5, 6] | **GPIO 12**[cite: 5, 6] |
| **Bouton Wi-Fi** | Bascule du point d'accès AP (`PIN_WIFI_BTN`)[cite: 6] | **GPIO 25**[cite: 5, 6] |
| **Signal RPM** | Compte-tours moteur (`PIN_RPM` - Interruption)[cite: 6] | **GPIO 14**[cite: 5, 6] |
| **Pression d'huile** | Alerte défaut pression d'huile (`PIN_OIL`)[cite: 6] | **GPIO 26**[cite: 5, 6] |
| **Clignotant gauche** | Entrée clignotant gauche (`PIN_IND_L`)[cite: 6] | **GPIO 17**[cite: 5, 6] |
| **Clignotant droit** | Entrée clignotant droit (`PIN_IND_R`)[cite: 6] | **GPIO 33**[cite: 5] |
| **Phares / Feux** | Feux de croisement (`PIN_LIGHTS`)[cite: 6] | **GPIO 35**[cite: 5, 6] |
| **Pleins Phares** | Feux de route (`PIN_HIGH_BEAM`)[cite: 6] | **GPIO 32**[cite: 5, 6] |

### 📊 Entrées Analogiques (ADC)
| Capteur | Rôle / Description | Broche ESP32 |
| :--- | :--- | :--- |
| **Sonde Température** | Température d'eau moteur (`PIN_TEMP`)[cite: 6] | **GPIO 34**[cite: 5, 6] |
| **Jauge Essence** | Niveau de carburant (`PIN_FUEL`)[cite: 6] | **GPIO 36** (VP)[cite: 5, 6] |

### 🛠️ Périphériques & Bus de Communication
| Périphérique | Rôle / Description | Broches ESP32 |
| :--- | :--- | :--- |
| **Bus I2C** | Centrale inertielle MPU-6050 (`PIN_SDA` / `PIN_SCL`)[cite: 6] | **GPIO 21** (SDA) / **GPIO 22** (SCL)[cite: 5, 6] |
| **Bus UART2** | Module GPS (`PIN_GPS_RX` / `PIN_GPS_TX`)[cite: 6] | **GPIO 16** (RX) / TX non connecté (`-1`)[cite: 5, 6] |
| **Bandeau LED** | Données du bandeau WS2812B (`PIN_LED_DATA`)[cite: 6] | **GPIO 13**[cite: 5, 6] |

---

## 🌐 Connexion Wi-Fi, Configuration & Récupération des Logs

Pour interagir avec le tableau de bord[cite: 6] :
1. Appuyer sur le bouton Wi-Fi (`GPIO 25`) pour démarrer le point d'accès[cite: 6].
2. Se connecter au réseau Wi-Fi : `Mini-Dashboard-AP` (Mot de passe : `minicooper`)[cite: 6].
3. Ouvrir un navigateur web à l'adresse : `http://192.168.4.1`[cite: 6].
4. Depuis la page web, vous pouvez ajuster l'odomètre total, configurer les paramètres généraux et les 24 LEDs, ou cliquer sur le lien pour **télécharger vos logs de trajet au format `.csv`**[cite: 5, 6].

---

## 🛠️ Matériel Utilisé
- Microcontrôleur ESP32 (30 broches)[cite: 6]
- Écran TFT SPI (contrôleur type ST7796S)[cite: 6]
- Centrale inertielle MPU-6050[cite: 6]
- Lecteur de carte SD (carte formatée en FAT32)[cite: 6]
- Module GPS UART[cite: 6]
- Bandeau LED WS2812B (24 LEDs)[cite: 6]
