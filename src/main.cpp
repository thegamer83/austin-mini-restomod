#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <FastLED.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <TinyGPS++.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include <FS.h>
#include <SD.h>


// ==========================================
// BROCHES CAPTEURS & PÉRIPHÉRIQUES
// ==========================================
#define PIN_RPM       14  
#define PIN_OIL       26  // Contact pression d'huile
#define PIN_TEMP      34  
#define PIN_FUEL      36  
#define PIN_PAGE_BTN  12  
#define PIN_WIFI_BTN  25  
#define PIN_BACKLIGHT 27  
#define PIN_LED_DATA  13  

// Entrées physiques feux & clignotants
#define PIN_IND_L     17  // Clignotant Gauche
#define PIN_IND_R     33  // Clignotant Droit
#define PIN_LIGHTS    35  // Phares (Feux de croisement)
#define PIN_HIGH_BEAM 32  // Pleins phares (Feux de route)

// Le GPIO 33 est utilisé pour le CS de la carte SD
#define SD_CS         33  

#define PIN_SDA       21  
#define PIN_SCL       22  

#define PIN_GPS_RX    16  
#define PIN_GPS_TX    -1  

TFT_eSPI tft = TFT_eSPI();
Adafruit_MPU6050 mpu;
TinyGPSPlus gps;
HardwareSerial gpsSerial(2);
Preferences preferences;
WebServer server(80);

#define NUM_LEDS 24
CRGB leds[NUM_LEDS];
uint8_t luminositeLeds = 128;

int pageActuelle = 1;
int dernierePageAffichee = -1;
int seuilRpmMax = 7000;
int rpmMinShift = 2000;

// Seuils RPM pour les zones Vert, Rouge et Bleu
int rpmSeuilVert = 2500;
int rpmSeuilRouge = 4500;
int rpmSeuilBleu = 6000;

// Rôles des LEDs
uint8_t roleLeds[NUM_LEDS]; 
uint32_t couleurLeds[NUM_LEDS];

// États physiques feux / clignotants / huile
bool clignotantGauche = false;
bool clignotantDroit = false;
bool feuxActifs = false;
bool pleinsPharesActifs = false;
bool etatAlerteHuile = false;

// Paramètres Wi-Fi AP
bool wifiApActif = false;
const char* ssidAP = "Mini-Dashboard-AP";
const char* passwordAP = "minicooper";

// États des périphériques pour la page de réglages
bool carteSdOk = false;
bool mpuOk = false;
bool gpsOk = false;
unsigned long dernierOctetGpsRecu = 0;

// Variables capteurs dynamiques
volatile unsigned long derniereInterruptionRPM = 0;
volatile unsigned int rpmBrut = 0;
float tempEau = 85.0f;
float niveauEssence = 75.0f;

// Odométrie Flash
double kilometrageTotal = 124850.0;
double tripPartiel = 0.0;
unsigned long dernierTempsOdo = 0;
unsigned long dernierTempsLogSD = 0;

int rapportEngage = 2; 
int dernierRapportAffiche = -99; 
float gLat = 0.0f;
float gLong = 0.0f;
float offsetX = 0.0f;
float offsetY = 0.0f;
float vitesseGpsKmph = 0.0f;

int ancienCursorGx = 380;
int ancienCursorGy = 220;

// ==========================================
// INTERRUPTIONS COMPTE-TOURS
// ==========================================
void IRAM_ATTR ISR_CompteTours() {
  unsigned long tempsActuel = micros();
  unsigned long intervalle = tempsActuel - derniereInterruptionRPM;
  if (intervalle > 2000) {
    rpmBrut = 6000000 / intervalle;
    derniereInterruptionRPM = tempsActuel;
  }
}

// ==========================================
// GESTION CARTE SD & LOGS / BMP
// ==========================================
void initialiserCarteSD() {
  if (!SD.begin(SD_CS)) {
    Serial.println("Erreur : Carte SD introuvable ou échec d'initialisation !");
    carteSdOk = false;
  } else {
    Serial.println("Carte SD initialisée avec succès.");
    carteSdOk = true;
  }
}

uint16_t lire16(File &f) {
  uint16_t result;
  f.read((uint8_t*)&result, sizeof(result));
  return result;
}

uint32_t lire32(File &f) {
  uint32_t result;
  f.read((uint8_t*)&result, sizeof(result));
  return result;
}

void afficherBmpSD(const char *filename, int x, int y) {
  File bmpFile = SD.open(filename, FILE_READ);
  if (!bmpFile) {
    Serial.println("Fichier BMP introuvable sur la carte SD.");
    return;
  }

  if (lire16(bmpFile) != 0x4D42) {
    Serial.println("Format BMP invalide.");
    bmpFile.close();
    return;
  }

  lire32(bmpFile); // Taille du fichier
  lire32(bmpFile); // Réservé
  uint32_t offset = lire32(bmpFile); // Adresse de début des données image
  lire32(bmpFile); // Taille de l'en-tête
  int32_t largeur = lire32(bmpFile);
  int32_t hauteur = lire32(bmpFile);

  if (lire16(bmpFile) != 1) {
    bmpFile.close();
    return;
  }

  uint16_t profondeurBits = lire16(bmpFile);
  if (profondeurBits != 24) {
    Serial.println("Seuls les fichiers BMP 24-bits sont pris en charge.");
    bmpFile.close();
    return;
  }

  lire32(bmpFile); // Compression
  lire32(bmpFile); // Taille image
  lire32(bmpFile); // Résolution horizontale
  lire32(bmpFile); // Résolution verticale
  lire32(bmpFile); // Couleurs palettes
  lire32(bmpFile); // Couleurs importantes

  tft.startWrite();
  bool flip = true;
  if (hauteur < 0) {
    hauteur = -hauteur;
    flip = false;
  }

  uint32_t lignePad = (4 - ((largeur * 3) % 4)) % 4;
  uint8_t sBuffer[3 * largeur];

  for (int row = 0; row < hauteur; row++) {
    int posY = y + (flip ? (hauteur - 1 - row) : row);
    if (posY >= tft.height()) break;

    bmpFile.seek(offset + (row * (largeur * 3 + lignePad)));
    bmpFile.read(sBuffer, sizeof(sBuffer));

    uint8_t *bptr = sBuffer;
    for (int col = 0; col < largeur; col++) {
      uint8_t b = *bptr++;
      uint8_t g = *bptr++;
      uint8_t r = *bptr++;
      tft.drawPixel(x + col, posY, tft.color565(r, g, b));
    }
  }
  tft.endWrite();
  bmpFile.close();
}

void enregistrerLogSD() {
  if (!carteSdOk) return;
  unsigned long tempsActuel = millis();
  if (tempsActuel - dernierTempsLogSD >= 1000) {
    dernierTempsLogSD = tempsActuel;
    
    File fichierLog = SD.open("/trip_log.csv", FILE_APPEND);
    if (fichierLog) {
      fichierLog.printf("%lu,%.1f,%u,%.1f,%.1f,%d,%.1f\n", 
                        millis(), vitesseGpsKmph, rpmBrut, tempEau, niveauEssence, etatAlerteHuile ? 1 : 0, kilometrageTotal);
      fichierLog.close();
    }
  }
}

// ==========================================
// GESTION MÉMOIRE FLASH (Préférences)
// ==========================================
void chargerDonneesFlash() {
  preferences.begin("odometre", true);
  kilometrageTotal = preferences.getDouble("total", 124850.0);
  tripPartiel = preferences.getDouble("trip", 0.0);
  preferences.end();

  preferences.begin("dashconf", true);
  luminositeLeds = preferences.getUChar("ledBright", 128);
  seuilRpmMax = preferences.getInt("rpmMax", 7000);
  rpmMinShift = preferences.getInt("rpmMin", 2000);
  rpmSeuilVert = preferences.getInt("rpmVert", 2500);
  rpmSeuilRouge = preferences.getInt("rpmRouge", 4500);
  rpmSeuilBleu = preferences.getInt("rpmBleu", 6000);
  preferences.end();

  preferences.begin("ledsconf", true);
  for (int i = 0; i < NUM_LEDS; i++) {
    char keyRole[10], keyColor[10];
    sprintf(keyRole, "r%d", i);
    sprintf(keyColor, "c%d", i);
    
    uint8_t defRole = 0;
    uint32_t defColor = 0x00FF00;
    if (i < 2) { defRole = 3; defColor = 0xFFA500; } 
    else if (i >= 2 && i < 8) { defRole = 0; defColor = 0x00FF00; } 
    else if (i >= 8 && i < 15) { defRole = 1; defColor = 0xFF0000; } 
    else if (i >= 15 && i < 22) { defRole = 2; defColor = 0x0000FF; } 
    else if (i == 22) { defRole = 5; defColor = 0xFF0000; } 
    else if (i == 23) { defRole = 7; defColor = 0x00FFFF; } 

    roleLeds[i] = preferences.getUChar(keyRole, defRole);
    couleurLeds[i] = preferences.getUInt(keyColor, defColor);
  }
  preferences.end();
}

void sauvegarderOdometre() {
  preferences.begin("odometre", false);
  preferences.putDouble("total", kilometrageTotal);
  preferences.putDouble("trip", tripPartiel);
  preferences.end();
}

void sauvegarderConfigGenerale() {
  preferences.begin("dashconf", false);
  preferences.putUChar("ledBright", luminositeLeds);
  preferences.putInt("rpmMax", seuilRpmMax);
  preferences.putInt("rpmMin", rpmMinShift);
  preferences.putInt("rpmVert", rpmSeuilVert);
  preferences.putInt("rpmRouge", rpmSeuilRouge);
  preferences.putInt("rpmBleu", rpmSeuilBleu);
  preferences.end();
}

void sauvegarderConfigLeds() {
  preferences.begin("ledsconf", false);
  for (int i = 0; i < NUM_LEDS; i++) {
    char keyRole[10], keyColor[10];
    sprintf(keyRole, "r%d", i);
    sprintf(keyColor, "c%d", i);
    preferences.putUChar(keyRole, roleLeds[i]);
    preferences.putUInt(keyColor, couleurLeds[i]);
  }
  preferences.end();
}

void mettreAJourOdometre() {
  unsigned long tempsActuel = millis();
  if (tempsActuel - dernierTempsOdo >= 1000) {
    if (vitesseGpsKmph > 2.0f) {
      double distanceParcourue = (vitesseGpsKmph / 3600.0f) * 1.0f;
      kilometrageTotal += distanceParcourue;
      tripPartiel += distanceParcourue;

      static double dernierKmSauvegarde = kilometrageTotal;
      if (kilometrageTotal - dernierKmSauvegarde >= 1.0) {
        sauvegarderOdometre();
        dernierKmSauvegarde = kilometrageTotal;
      }
    }
    dernierTempsOdo = tempsActuel;
  }
}

// ==========================================
// SERVEUR WEB DE CONFIGURATION & LOGS SD
// ==========================================
void handleDownloadLog() {
  if (SD.exists("/trip_log.csv")) {
    File logFile = SD.open("/trip_log.csv", FILE_READ);
    server.streamFile(logFile, "text/csv");
    logFile.close();
  } else {
    server.send(404, "text/plain", "Fichier de log introuvable sur la carte SD.");
  }
}

void handleRoot() {
  String html = "<html lang='fr'><head><meta charset='UTF-8'><title>Dashboard Austin Mini</title>";
  html += "<style>body{font-family:Arial;background:#222;color:#fff;text-align:center;padding:15px;}";
  html += ".card{background:#333;padding:15px;margin:10px auto;border-radius:8px;max-width:540px;text-align:left;}";
  html += "input[type=text], select{padding:4px;} label{display:inline-block;width:180px;}";
  html += ".led-row{background:#2a2a2a;padding:6px;margin:4px 0;border-radius:4px;display:flex;justify-content:space-between;align-items:center;}</style></head>";
  html += "<body><h1>Austin Mini - Dashboard</h1>";
  
  // Carte SD & Logs
  html += "<div class='card'><h3>Gestion Carte SD & Logs</h3>";
  if (carteSdOk) {
    html += "<p style='color:#2ecc71;'>Carte SD connectée et prête.</p>";
    html += "<a href='/download_log' style='background:#2980b9;color:#fff;padding:8px 15px;text-decoration:none;border-radius:4px;display:inline-block;'>Telecharger le fichier trip_log.csv</a>";
  } else {
    html += "<p style='color:#e74c3c;'>Carte SD non détectée !</p>";
  }
  html += "</div>";

  html += "<div class='card'><h3>Parametres Generaux, Odométre & Seuils RPM</h3>";
  html += "<form action='/save' method='GET'>";
  html += "<label>Kilometrage Total (km):</label><input type='text' name='odoTotal' value='" + String(kilometrageTotal, 1) + "'><br><br>";
  html += "<label>Intensite LEDs:</label><input type='text' name='ledBright' value='" + String(luminositeLeds) + "'><br><br>";
  html += "<label>RPM Min (Debut):</label><input type='text' name='rpmMin' value='" + String(rpmMinShift) + "'><br><br>";
  html += "<label>Seuil RPM Vert:</label><input type='text' name='rpmVert' value='" + String(rpmSeuilVert) + "'><br><br>";
  html += "<label>Seuil RPM Rouge:</label><input type='text' name='rpmRouge' value='" + String(rpmSeuilRouge) + "'><br><br>";
  html += "<label>Seuil RPM Bleu:</label><input type='text' name='rpmBleu' value='" + String(rpmSeuilBleu) + "'><br><br>";
  html += "<label>RPM Max (Seuil/Max):</label><input type='text' name='rpmMax' value='" + String(seuilRpmMax) + "'><br><br>";

  html += "<h3>Configuration Bandeau LEDs (24 LEDs)</h3>";
  for (int i = 0; i < NUM_LEDS; i++) {
    char hexColor[8];
    sprintf(hexColor, "#%06X", couleurLeds[i]);
    
    html += "<div class='led-row'><span>LED " + String(i) + "</span>";
    html += "<select name='r_" + String(i) + "'>";
    html += "<option value='0'" + String(roleLeds[i] == 0 ? " selected" : "") + ">Shift Vert</option>";
    html += "<option value='1'" + String(roleLeds[i] == 1 ? " selected" : "") + ">Shift Rouge</option>";
    html += "<option value='2'" + String(roleLeds[i] == 2 ? " selected" : "") + ">Shift Bleu</option>";
    html += "<option value='3'" + String(roleLeds[i] == 3 ? " selected" : "") + ">Cligno Gauche</option>";
    html += "<option value='4'" + String(roleLeds[i] == 4 ? " selected" : "") + ">Cligno Droit</option>";
    html += "<option value='5'" + String(roleLeds[i] == 5 ? " selected" : "") + ">Alerte Eau (Rouge)</option>";
    html += "<option value='6'" + String(roleLeds[i] == 6 ? " selected" : "") + ">Alerte Huile (Low)</option>";
    html += "<option value='7'" + String(roleLeds[i] == 7 ? " selected" : "") + ">WiFi Actif</option>";
    html += "<option value='8'" + String(roleLeds[i] == 8 ? " selected" : "") + ">Fixe / Feux</option>";
    html += "</select>";
    html += "<input type='color' name='c_" + String(i) + "' value='" + String(hexColor) + "'>";
    html += "</div>";
  }

  html += "<br><center><input type='submit' value='Enregistrer tout' style='padding:10px 25px;background:#c0392b;color:#fff;border:none;border-radius:4px;cursor:pointer;font-size:16px;'></center></form></div></body></html>";
  server.send(200, "text/html", html);
}

void handleSave() {
  if (server.hasArg("ledBright")) {
    if (server.hasArg("odoTotal")) {
      kilometrageTotal = server.arg("odoTotal").toFloat();
    }
    luminositeLeds = constrain(server.arg("ledBright").toInt(), 10, 255);
    rpmMinShift = server.arg("rpmMin").toInt();
    rpmSeuilVert = server.arg("rpmVert").toInt();
    rpmSeuilRouge = server.arg("rpmRouge").toInt();
    rpmSeuilBleu = server.arg("rpmBleu").toInt();
    seuilRpmMax = server.arg("rpmMax").toInt();
    
    sauvegarderOdometre();
    sauvegarderConfigGenerale();

    for (int i = 0; i < NUM_LEDS; i++) {
      String argRoleName = "r_" + String(i);
      String argColorName = "c_" + String(i);

      if (server.hasArg(argRoleName)) {
        roleLeds[i] = server.arg(argRoleName).toInt();
      }
      if (server.hasArg(argColorName)) {
        String hexStr = server.arg(argColorName);
        hexStr.replace("#", "");
        couleurLeds[i] = strtoul(hexStr.c_str(), NULL, 16);
      }
    }
    sauvegarderConfigLeds();

    server.send(200, "text/html", "<h2>Parametres et Odomètre enregistres avec succes !</h2><br><a href='/'>Retour au Dashboard</a>");
    return;
  }
  server.send(400, "text/html", "Erreur de parametre");
}

void demarrerWiFiAP() {
  WiFi.softAP(ssidAP, passwordAP);
  wifiApActif = true;
  server.on("/", handleRoot);
  server.on("/save", handleSave);
  server.on("/download_log", handleDownloadLog);
  server.begin();
}

void arreterWiFiAP() {
  WiFi.softAPdisconnect(true);
  wifiApActif = false;
}

// ==========================================
// DESSIN DES FONDS DE PAGES (Statique)
// ==========================================
void dessinerFondPage1() {
  uint16_t couleurMagnoliaFond = 0xFF9E; 
  uint16_t couleurCadranInt = 0xF7BE; 

  tft.fillScreen(couleurMagnoliaFond); 

  int cx = 240;
  int cy = 160;
  int rayonCompteTour = 145;

  tft.fillCircle(cx, cy, rayonCompteTour, couleurCadranInt);
  tft.drawCircle(cx, cy, rayonCompteTour, TFT_DARKGREY);
  tft.drawCircle(cx, cy, rayonCompteTour - 2, TFT_DARKGREY);

  int xFinAiguille = cx - 80;
  int yFinAiguille = cy - 70;
  tft.drawLine(cx, cy, xFinAiguille, yFinAiguille, TFT_BLACK);
  tft.drawLine(cx + 1, cy, xFinAiguille + 1, yFinAiguille, TFT_BLACK); 
  tft.drawLine(cx - 1, cy, xFinAiguille - 1, yFinAiguille, TFT_BLACK); 

  for (int i = 0; i <= 9; i++) {
    float angle = map(i, 0, 9, -210, 30) * 0.0174533; 
    int x1 = cx + (rayonCompteTour - 4) * cos(angle);
    int y1 = cy + (rayonCompteTour - 4) * sin(angle);
    int x2 = cx + (rayonCompteTour - 18) * cos(angle);
    int y2 = cy + (rayonCompteTour - 18) * sin(angle);
    tft.drawLine(x1, y1, x2, y2, TFT_BLACK);
    
    int xt = cx + (rayonCompteTour - 30) * cos(angle) - 6;
    int yt = cy + (rayonCompteTour - 30) * sin(angle) - 8;
    tft.setTextColor(TFT_BLACK, couleurCadranInt);
    tft.setTextSize(2);
    tft.setCursor(xt, yt);
    tft.print(i);
  }

  tft.setTextSize(1);
  tft.setCursor(cx - 28, cy + 30);
  tft.print("rpm x 1000");

  int odoLargeur = 130;
  int odoHauteur = 42;
  int odoX = cx - (odoLargeur / 2);
  int odoY = cy + 44;
  tft.fillRect(odoX, odoY, odoLargeur, odoHauteur, TFT_BLACK);
  tft.drawRect(odoX, odoY, odoLargeur, odoHauteur, TFT_DARKGREY);
  tft.drawFastHLine(odoX, odoY + 21, odoLargeur, TFT_DARKGREY);

  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(odoX + 4, odoY + 6);
  tft.print("TOT");
  tft.setCursor(odoX + 4, odoY + 27);
  tft.print("TRP");

  tft.fillCircle(cx, cy, 10, TFT_BLACK);  

  int rayonPetitCadran = 28;
  tft.fillCircle(45, 45, rayonPetitCadran, TFT_WHITE);
  tft.drawCircle(45, 45, rayonPetitCadran, TFT_DARKGREY);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(33, 26); tft.print("Eau");

  tft.fillCircle(435, 45, rayonPetitCadran, TFT_WHITE);
  tft.drawCircle(435, 45, rayonPetitCadran, TFT_DARKGREY);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(419, 26); tft.print("Huile");

  tft.fillCircle(45, 275, rayonPetitCadran, TFT_WHITE);
  tft.drawCircle(45, 275, rayonPetitCadran, TFT_DARKGREY);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(25, 256); tft.print("Km/h");

  tft.fillCircle(435, 275, rayonPetitCadran, TFT_WHITE);
  tft.drawCircle(435, 275, rayonPetitCadran, TFT_DARKGREY);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(412, 256); tft.print("Essence");
}

void dessinerFondPage2() {
  tft.fillScreen(0x18E3); 
  dernierRapportAffiche = -99;
  
  int xDepart = 20;
  int yBarre = 10;
  int largeurTotale = 440;
  int hauteurBarre = 36; 

  for (int i = 0; i < largeurTotale; i++) {
    uint16_t couleurPixel;
    if (i < largeurTotale / 2) {
      int r = map(i, 0, largeurTotale / 2, 0, 31);
      int g = 63;
      int b = 0;
      couleurPixel = (r << 11) | (g << 5) | b;
    } else {
      int r = 31;
      int g = map(i, largeurTotale / 2, largeurTotale, 63, 0);
      int b = 0;
      couleurPixel = (r << 11) | (g << 5) | b;
    }
    tft.drawFastVLine(xDepart + i, yBarre, hauteurBarre, couleurPixel);
  }
  tft.drawRect(xDepart - 1, yBarre - 1, largeurTotale + 2, hauteurBarre + 2, TFT_DARKGREY);

  int radarX = 380;
  int radarY = 220;
  int radarR = 48;
  tft.fillCircle(radarX, radarY, radarR, 0x1082); 
  tft.drawCircle(radarX, radarY, radarR, TFT_WHITE);
  tft.drawCircle(radarX, radarY, radarR / 2, TFT_DARKGREY);
  tft.drawFastHLine(radarX - radarR, radarY, radarR * 2, TFT_DARKGREY);
  tft.drawFastVLine(radarX, radarY - radarR, radarR * 2, TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE, 0x18E3);
  tft.setTextSize(1);
  tft.setCursor(radarX - 18, radarY - radarR - 12);
  tft.print("G-METER");
}

void dessinerFondPage3() {
  tft.fillScreen(TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextSize(3);
  tft.setCursor(20, 15);
  tft.println("PAGE 3 : REGLAGES");
  
  tft.setTextSize(2);
  tft.setCursor(20, 65);
  tft.print("WiFi AP: ");
  
  tft.setCursor(20, 110);
  tft.printf("Odo: %.1f km", kilometrageTotal);
  
  tft.setCursor(20, 155);
  tft.printf("Trip: %.1f km", tripPartiel);
  
  // État Carte SD
  tft.setCursor(20, 205);
  tft.print("Carte SD : ");

  // État MPU6050 (replacé proprement sur la page réglages)
  tft.setCursor(20, 245);
  tft.print("MPU6050 : ");

  // État GPS
  tft.setCursor(20, 285);
  tft.print("GPS : ");
}

// ==========================================
// MISE À JOUR DES VALEURS (Dynamique)
// ==========================================
void actualiserPage1() {
  uint16_t couleurCadranInt = 0xF7BE;

  int cx = 240;
  int cy = 160;
  int boiteW = 50;
  int boiteH = 46;
  int boiteX = cx - (boiteW / 2);
  int boiteY = cy - 88;

  tft.fillRect(boiteX, boiteY, boiteW, boiteH, TFT_BLACK);
  tft.drawRect(boiteX, boiteY, boiteW, boiteH, TFT_RED);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.setTextSize(4);
  if (rapportEngage == 0) {
    tft.setCursor(boiteX + 12, boiteY + 9);
    tft.print("N");
  } else {
    tft.setCursor(boiteX + 14, boiteY + 9);
    tft.print(rapportEngage);
  }

  int odoLargeur = 130;
  int odoX = cx - (odoLargeur / 2);
  int odoY = cy + 44;

  tft.fillRect(odoX + 28, odoY + 3, odoLargeur - 30, 16, TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(odoX + 30, odoY + 4);
  tft.printf("%.0f km", kilometrageTotal);

  tft.fillRect(odoX + 28, odoY + 23, odoLargeur - 30, 16, TFT_BLACK);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(odoX + 30, odoY + 24);
  tft.printf("%.1f km", tripPartiel);

  tft.fillCircle(cx, cy, 10, TFT_BLACK);  

  tft.fillRect(20, 40, 50, 20, couleurCadranInt);
  tft.setTextColor(TFT_BLACK, couleurCadranInt);
  tft.setTextSize(2);
  tft.setCursor(27, 42);
  tft.printf("%.0f", tempEau);

  tft.fillRect(410, 40, 50, 20, TFT_WHITE);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(etatAlerteHuile ? 412 : 421, 42);
  tft.print(etatAlerteHuile ? "LOW" : "OK");

  tft.fillRect(20, 270, 50, 20, TFT_WHITE);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(27, 272);
  tft.printf("%.0f", vitesseGpsKmph);

  tft.fillRect(410, 270, 50, 20, TFT_WHITE);
  tft.setTextColor(TFT_WHITE, TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(413, 272);
  tft.printf("%.0f%%", niveauEssence);
}

void actualiserPage2() {
  int xDepart = 20;
  int yBarre = 10;
  int largeurTotale = 440;
  int hauteurBarre = 36; 

  int niveauActuelPixels = map(rpmBrut, 0, seuilRpmMax, 0, largeurTotale);
  niveauActuelPixels = constrain(niveauActuelPixels, 0, largeurTotale);
  
  tft.fillRect(xDepart + niveauActuelPixels, yBarre, largeurTotale - niveauActuelPixels, hauteurBarre, 0x18E3);

  tft.fillRect(160, 48, 160, 25, 0x18E3);
  tft.setTextColor(TFT_WHITE, 0x18E3);
  tft.setTextSize(2);
  tft.setCursor(190, 52);
  tft.printf("%u RPM", rpmBrut);

  if (rapportEngage != dernierRapportAffiche) {
    dernierRapportAffiche = rapportEngage;

    int zX = 130;
    int zY = 85;
    int zW = 180;
    int zH = 175;

    tft.fillRect(zX, zY, zW, zH, 0x18E3); 

    int ep = 18; 

    if (rapportEngage == 0) {
      tft.fillRect(zX + 15, zY + 15, ep, zH - 30, TFT_WHITE); 
      tft.fillRect(zX + zW - 15 - ep, zY + 15, ep, zH - 30, TFT_WHITE); 
      for(int d = 0; d < 100; d++) {
        int px = zX + 15 + ep + (d * (zW - 30 - 2*ep)) / 100;
        int py = zY + 15 + (d * (zH - 30)) / 100;
        tft.fillRect(px, py, ep - 2, ep, TFT_WHITE);
      }
    } 
    else if (rapportEngage == 1) {
      tft.fillRect(zX + (zW / 2) - (ep / 2), zY + 15, ep, zH - 30, TFT_WHITE);
    }
    else if (rapportEngage == 2) {
      tft.fillRect(zX + 15, zY + 15, zW - 30, ep, TFT_WHITE);                  
      tft.fillRect(zX + zW - 15 - ep, zY + 15, ep, (zH / 2), TFT_WHITE);       
      tft.fillRect(zX + 15, zY + (zH / 2) - (ep / 2), zW - 30, ep, TFT_WHITE); 
      tft.fillRect(zX + 15, zY + (zH / 2), ep, (zH / 2) - 15, TFT_WHITE);      
      tft.fillRect(zX + 15, zY + zH - 15 - ep, zW - 30, ep, TFT_WHITE);        
    }
    else if (rapportEngage == 3) {
      tft.fillRect(zX + 15, zY + 15, zW - 30, ep, TFT_WHITE);                  
      tft.fillRect(zX + zW - 15 - ep, zY + 15, ep, zH - 30, TFT_WHITE);        
      tft.fillRect(zX + 15, zY + (zH / 2) - (ep / 2), zW - 30, ep, TFT_WHITE); 
      tft.fillRect(zX + 15, zY + zH - 15 - ep, zW - 30, ep, TFT_WHITE);        
    }
    else if (rapportEngage == 4) {
      tft.fillRect(zX + 15, zY + 15, ep, (zH / 2), TFT_WHITE);                 
      tft.fillRect(zX + zW - 15 - ep, zY + 15, ep, zH - 30, TFT_WHITE);        
      tft.fillRect(zX + 15, zY + (zH / 2) - (ep / 2), zW - 30, ep, TFT_WHITE); 
    }
  }

  tft.fillRect(160, 265, 160, 30, 0x18E3);
  tft.setTextColor(TFT_YELLOW, 0x18E3);
  tft.setTextSize(3);
  tft.setCursor(175, 268);
  tft.printf("%.0f km/h", vitesseGpsKmph);

  int radarX = 380;
  int radarY = 220;
  int radarR = 42;

  tft.fillCircle(ancienCursorGx, ancienCursorGy, 5, 0x1082);

  int offsetX_pixel = constrain((int)(gLat * 35.0f), -radarR, radarR);
  int offsetY_pixel = constrain((int)(gLong * 35.0f), -radarR, radarR);

  int nouveauGx = radarX + offsetX_pixel;
  int nouveauGy = radarY - offsetY_pixel;

  tft.fillCircle(nouveauGx, nouveauGy, 5, TFT_RED);

  ancienCursorGx = nouveauGx;
  ancienCursorGy = nouveauGy;
}

void actualiserPage3() {
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextSize(2);
  
  tft.setCursor(125, 65);
  tft.printf("%s      ", wifiApActif ? "ON (192.168.4.1)" : "OFF");
  
  tft.setCursor(70, 110);
  tft.printf("%.1f km        ", kilometrageTotal);
  
  tft.setCursor(80, 155);
  tft.printf("%.1f km        ", tripPartiel);
  
  // Rafraîchissement État Carte SD
  tft.setCursor(140, 205);
  if (carteSdOk) {
    tft.setTextColor(TFT_GREEN, TFT_NAVY);
    tft.print("Prete   ");
  } else {
    tft.setTextColor(TFT_RED, TFT_NAVY);
    tft.print("Absente ");
  }

  // Rafraîchissement État MPU6050
  tft.setCursor(140, 245);
  if (mpuOk) {
    tft.setTextColor(TFT_GREEN, TFT_NAVY);
    tft.print("OK (Operationnel) ");
  } else {
    tft.setTextColor(TFT_RED, TFT_NAVY);
    tft.print("Erreur / Absent   ");
  }

  // Rafraîchissement État GPS
  tft.setCursor(90, 285);
  if (gpsOk) {
    tft.setTextColor(TFT_GREEN, TFT_NAVY);
    tft.print("Actif / Fix     ");
  } else {
    tft.setTextColor(TFT_YELLOW, TFT_NAVY);
    tft.print("Recherche signal ");
  }
}

// ==========================================
// GESTION LEDS
// ==========================================
void gererBandeauLEDs() {
  FastLED.clear();
  FastLED.setBrightness(luminositeLeds);

  bool seuilMaxAtteint = (rpmBrut >= seuilRpmMax);
  bool clignotementRapide = (millis() / 150) % 2 == 0;

  for (int i = 0; i < NUM_LEDS; i++) {
    uint8_t role = roleLeds[i];
    uint32_t col = couleurLeds[i];

    if (role == 7 && wifiApActif) {
      leds[i] = col;
    }
    else if (role == 5 && tempEau > 100.0f) {
      if (clignotementRapide) leds[i] = col;
    }
    else if (role == 6 && etatAlerteHuile) {
      if (clignotementRapide) leds[i] = col;
    }
    else if (role == 3 && clignotantGauche) {
      if ((millis() / 250) % 2 == 0) leds[i] = col;
    }
    else if (role == 4 && clignotantDroit) {
      if ((millis() / 250) % 2 == 0) leds[i] = col;
    }
    else if (role == 8 && (feuxActifs || pleinsPharesActifs)) {
      leds[i] = col;
    }
    else if (role == 0 || role == 1 || role == 2) {
      if (rpmBrut >= rpmMinShift) {
        bool allumerVert = (role == 0 && rpmBrut >= rpmSeuilVert);
        bool allumerRouge = (role == 1 && rpmBrut >= rpmSeuilRouge);
        bool allumerBleu = (role == 2 && rpmBrut >= rpmSeuilBleu);

        if (allumerVert || allumerRouge || allumerBleu) {
          if (seuilMaxAtteint) {
            if (clignotementRapide) {
              leds[i] = col;
            }
          } else {
            leds[i] = col;
          }
        }
      }
    }
  }
  FastLED.show();
}

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(PIN_PAGE_BTN, INPUT_PULLUP);
  pinMode(PIN_WIFI_BTN, INPUT_PULLUP);
  pinMode(PIN_OIL, INPUT_PULLUP);
  pinMode(PIN_RPM, INPUT_PULLUP);
  pinMode(PIN_IND_L, INPUT_PULLUP);
  pinMode(PIN_IND_R, INPUT_PULLUP);
  pinMode(PIN_LIGHTS, INPUT_PULLUP);
  pinMode(PIN_HIGH_BEAM, INPUT_PULLUP);

  pinMode(PIN_BACKLIGHT, OUTPUT);
  analogWrite(PIN_BACKLIGHT, 255);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  // Initialisation de la carte SD et affichage du logo de démarrage
  initialiserCarteSD();
  if (carteSdOk) {
    afficherBmpSD("/logo.bmp", 0, 0);
    delay(2000); 
  }

  attachInterrupt(digitalPinToInterrupt(PIN_RPM), ISR_CompteTours, FALLING);

  gpsSerial.begin(9600, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);

  Wire.begin(PIN_SDA, PIN_SCL);
  if (mpu.begin()) {
    mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
    mpuOk = true;
  } else {
    mpuOk = false;
  }

  chargerDonneesFlash();

  FastLED.addLeds<WS2812B, PIN_LED_DATA, GRB>(leds, NUM_LEDS);

  dessinerFondPage1();
  dernierePageAffichee = 1;
}

// ==========================================
// LOOP
// ==========================================
void loop() {
  if (wifiApActif) {
    server.handleClient();
  }

  while (gpsSerial.available() > 0) {
    char c = gpsSerial.read();
    gps.encode(c);
    dernierOctetGpsRecu = millis();
  }

  if (millis() - dernierOctetGpsRecu < 3000) {
    gpsOk = true;
  } else {
    gpsOk = false;
  }

  if (gps.speed.isUpdated()) {
    vitesseGpsKmph = gps.speed.kmph();
  }

  mettreAJourOdometre();
  enregistrerLogSD();

  etatAlerteHuile    = (digitalRead(PIN_OIL) == LOW);
  clignotantGauche   = (digitalRead(PIN_IND_L) == LOW);
  clignotantDroit    = (digitalRead(PIN_IND_R) == LOW);
  feuxActifs         = (digitalRead(PIN_LIGHTS) == LOW);
  pleinsPharesActifs = (digitalRead(PIN_HIGH_BEAM) == LOW);
  
  int rawTemp = analogRead(PIN_TEMP);
  float tempInstantane = (rawTemp / 4095.0f) * 120.0f; 
  tempEau = (tempEau * 0.9f) + (tempInstantane * 0.1f);

  int rawFuel = analogRead(PIN_FUEL);
  float fuelInstantane = (rawFuel / 4095.0f) * 100.0f;
  niveauEssence = (niveauEssence * 0.9f) + (fuelInstantane * 0.1f);

  if (micros() - derniereInterruptionRPM > 1000000) {
    rpmBrut = 0;
  }

  sensors_event_t a, g, tempMpu;
  if (mpuOk && mpu.getEvent(&a, &g, &tempMpu)) {
    gLat = (a.acceleration.x - offsetX) / 9.80665f;
    gLong = (a.acceleration.y - offsetY) / 9.80665f;
  }

  static unsigned long tempsAppuiBtn = 0;
  static bool boutonEnCours = false;
  bool etatBtn = digitalRead(PIN_PAGE_BTN);

  if (etatBtn == LOW && !boutonEnCours) {
    boutonEnCours = true;
    tempsAppuiBtn = millis();
  } 
  else if (etatBtn == HIGH && boutonEnCours) {
    unsigned long dureeAppui = millis() - tempsAppuiBtn;
    boutonEnCours = false;

    if (dureeAppui >= 2000) {
      tripPartiel = 0.0;
      sauvegarderOdometre();
      if (pageActuelle == 3) {
        dessinerFondPage3();
      }
    } else if (dureeAppui > 40) {
      pageActuelle++;
      if (pageActuelle > 3) pageActuelle = 1;
    }
  }

  static bool etatPrecedentWifiBtn = HIGH;
  bool etatWifiBtn = digitalRead(PIN_WIFI_BTN);
  if (etatWifiBtn == LOW && etatPrecedentWifiBtn == HIGH) {
    delay(40);
    if (!wifiApActif) {
      demarrerWiFiAP();
    } else {
      arreterWiFiAP();
    }
  }
  etatPrecedentWifiBtn = etatWifiBtn;

  if (pageActuelle != dernierePageAffichee) {
    switch (pageActuelle) {
      case 1: dessinerFondPage1(); break;
      case 2: dessinerFondPage2(); break;
      case 3: dessinerFondPage3(); break;
    }
    dernierePageAffichee = pageActuelle;
  }

  gererBandeauLEDs();

  static unsigned long dernierRafraichissement = 0;
  if (millis() - dernierRafraichissement > 100) {
    switch (pageActuelle) {
      case 1: actualiserPage1(); break;
      case 2: actualiserPage2(); break;
      case 3: actualiserPage3(); break;
    }
    dernierRafraichissement = millis();
  }
}
