# 🏎️ Restomod Digital Dashboard - Austin Mini (ESP32)

Tableau de bord numérique moderne de type "Fast Road / GT" développé pour une Austin Mini classique. Le système repose sur un microcontrôleur **ESP32 (30 broches)** et combine un affichage graphique SPI, un bandeau de LEDs adressables WS2812B pour le shift-light et les alertes, un lecteur de carte SD, une centrale inertielle MPU-6050, un module GPS, ainsi qu'un serveur Web embarqué via Wi-Fi pour la configuration dynamique et l'export des données de vol/trajet.

---

## 📸 Aperçu du Projet

| Vue d'ensemble du Dashboard | Interface Web & Paramétrage |
| :---: | :---: |
| ![Dashboard en action](/image/20261010_134729.jpg) | ![Interface Web](/image/Screenshot_20261010_135628_Chrome.jpg) |
| *Affichage TFT vintage / course* | *Configuration web (Seuils RPM & Rôles LEDs)* |

---

## ✨ Fonctionnalités Principales

- **Affichage TFT (3 Pages de Navigation) :**
  1. *Page 1 (Magnolia Vintage & Compte-tours) :* Vitesse GPS, Compte-tours analogique/numérique, rapport engagé (calculé), jauge d'eau, pression d'huile, jauge d'essence, et odomètre total.
  2. *Page 2 (Race Digital & G-Force) :* Barre de régime progressive en dégradé, régime numérique, rapport engagé géant, vitesse GPS et radar G-Force dynamique.
  3. *Page 3 (Réglages & Système) :* Statut du point d'accès Wi-Fi, kilométrage total, trip partiel, et contrôle en temps réel de l'état opérationnel des périphériques (**Carte SD**, **MPU6050** et **GPS**).
- **Gestion Carte SD & Datalogging :**
  * Affichage d'une image de démarrage personnalisée (`/logo.bmp`) au boot.
  * Enregistrement automatique et périodique des données de trajet (temps, vitesse, régime moteur, températures, alertes et odométrie) dans un fichier de logs horodaté (`/trip_log.csv`).
- **Bandeau LED WS2812B (24 LEDs configurables) :**
  * *Shift-Light progressif :* Seuils personnalisables pour le **Vert**, le **Rouge** et le **Bleu**, avec clignotement global en cas de dépassement du RPM Max.
  * *Rôles multiples affectables LED par LED :* Clignotants (gauche/droit), alertes eau/huile, statut Wi-Fi, ou éclairage fixe/feux.
- **Serveur Web Embarqué (Mode AP) :**
  * Connexion directe en Wi-Fi pour administrer le système, régler la luminosité, les seuils RPM, l'assignation des LEDs, et **télécharger directement le fichier de logs CSV (`/trip_log.csv`)** de la carte SD.
- **Persistance en Mémoire Flash :**
  * Sauvegarde automatique du kilométrage total et du trip partiel via la bibliothèque `Preferences`.

---

## 🔌 Schéma de Câblage & Affectation des Broches (ESP32 30 broches)

### 🗺️ Représentation Broches ESP32 (30 Broches)
```text
                                  +-------------------+
                         3V3  --- | 1             38  | --- GND
                         EN   --- | 2             37  | --- GPIO 23 (MOSI / SPI)
                   GPIO 36    --- | 3 (VP)        36  | --- GPIO 22 (SCL / I2C MPU6050)
                   GPIO 39    --- | 4 (VN)        35  | --- GPIO 21 (SDA / I2C MPU6050)
                   GPIO 34    --- | 5             34  | --- GPIO 33 (SD_CS & Clignotant Droit)*
                   GPIO 35    --- | 6             33  | --- GPIO 19 (MISO / SPI)
                   GPIO 32    --- | 7             32  | --- GPIO 18 (SCK / SPI)
                   GPIO 33    --- | 8             31  | --- GND
                   GPIO 25    --- | 9             30  | --- GPIO 5  (Libre / Autre CS)
                   GPIO 26    --- | 10            29  | --- GPIO 17 (UART2 RX / GPS)
                   GPIO 14    --- | 11            28  | --- GPIO 16 (UART2 TX / GPS)
                   GPIO 12    --- | 12            27  | --- 4 (GND)
                        GND   --- | 13            26  | --- 0 (GND)
                   GPIO 13    --- | 14            25  | --- GPIO 13 (LED Data - WS2812B)
                   GPIO 9     --- | 15            24  | --- GPIO 2  (Libre)
                   GPIO 10    --- | 16            23  | --- GPIO 0  (Boot)
                   GPIO 11    --- | 17            22  | --- GPIO 4  (Libre)
                         5V   --- | 18            21  | --- 5V  (Alim Régulée DC-DC)
                                  +-------------------+
