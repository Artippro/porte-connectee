/*
 * PORTE CONNECTÉE - Contrôle depuis PARTOUT dans le monde
 * Arduino UNO R4 WiFi (COM6) + Servo Moteur 360°
 *
 * Fonctionnalités :
 * - Contrôle admin (ON/OFF)
 * - Codes jetables : un ami entre le code, porte s'ouvre UNE SEULE FOIS
 * - Ouverture mécanique par Servomoteur 360
 *
 * Bibliothèques nécessaires :
 * - ArduinoMqttClient (par Arduino)
 * - Servo (Standard)
 */
#include <WiFiS3.h>
#include <ArduinoMqttClient.h>
#include "Arduino_LED_Matrix.h"
#include <Servo.h> // === AJOUT SERVO ===
ArduinoLEDMatrix matrix;
// === AJOUT SERVO : Configuration ===
Servo monServo;
const int SERVO_PIN = 9;
const int ARRET_SERVO = 90; // Valeur pour arrêter le servo 360 (à ajuster si besoin, ex: 89 ou 91)
const int TEMPS_ROTATION = 800; // Temps de rotation en millisecondes pour ouvrir/fermer la serrure (A AJUSTER)
// Cadenas FERME (depuis cadenas.mpj)
const uint32_t lockClosed[] = { 0x0000FF19, 0xF11F11F1, 0x9F0FF000 };
// Cadenas OUVERT (depuis cadenas.mpj)
const uint32_t lockOpen[] = { 0x0003FF61, 0xF41F41F6, 0x1F39F000 };
// ===== À MODIFIER AVEC TON WIFI =====
char ssid[] = "iPhone du bg";
char pass[] = "skedddddd";
// =====================================
// MQTT - Broker gratuit
const char broker[]         = "broker.hivemq.com";
int        port             = 1883;
const char topicCmd[]       = "porte-vincent-2026/commande";
const char topicState[]     = "porte-vincent-2026/etat";
const char topicAddCode[]   = "porte-vincent-2026/addcode";    // admin ajoute un code
const char topicUseCode[]   = "porte-vincent-2026/usecode";    // ami utilise un code
const char topicCodeResult[] = "porte-vincent-2026/coderesult"; // résultat (ok/fail)
const char topicCodeUsed[]   = "porte-vincent-2026/codeused";   // notifie quel code a été utilisé
const int LED_PIN = 13;
bool ledState = false;
// ===== CODES JETABLES =====
const int MAX_CODES = 20;
String validCodes[MAX_CODES];
int codeCount = 0;
WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);
unsigned long lastAlive = 0;
unsigned long autoCloseTime = 0;  // pour fermer auto après ouverture par code
// === AJOUT SERVO : Fonctions de mouvement ===
// Ces fonctions gèrent le mouvement physique.
// On vérifie l'état (ledState) pour ne pas forcer le moteur si la porte est déjà ouverte/fermée.
void actionOuvrirMecanisme() {
  if (!ledState) { // Si c'était fermé, on ouvre
    Serial.println("-> Actionnement moteur : OUVERTURE");
    monServo.write(180); // Vitesse modérée dans un sens
    delay(TEMPS_ROTATION);
    monServo.write(ARRET_SERVO); // Stop
  }
}
void actionFermerMecanisme() {
  if (ledState) { // Si c'était ouvert, on ferme
    Serial.println("-> Actionnement moteur : FERMETURE");
    monServo.write(0); // Vitesse modérée dans l'autre sens
    delay(TEMPS_ROTATION);
    monServo.write(ARRET_SERVO); // Stop
  }
}
// ===========================================
void connectWiFi() {
  Serial.print("Connexion WiFi a ");
  Serial.print(ssid);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nWiFi connecte !");
  Serial.print("IP locale : ");
  Serial.println(WiFi.localIP());
}
void connectMQTT() {
  Serial.print("Connexion au broker MQTT...");
  String clientId = "arduino-porte-" + String(random(10000));
  mqttClient.setId(clientId);
  while (!mqttClient.connect(broker, port)) {
    Serial.print(".");
    delay(2000);
  }
  Serial.println(" connecte !");
  mqttClient.subscribe(topicCmd);
  mqttClient.subscribe(topicAddCode);
  mqttClient.subscribe(topicUseCode);
  Serial.println("Ecoute sur :");
  Serial.println(topicCmd);
  Serial.println(topicAddCode);
  Serial.println(topicUseCode);
  mqttClient.beginMessage(topicState);
  mqttClient.print(ledState ? "ON" : "OFF");
  mqttClient.endMessage();
  Serial.println("\n====================================");
  Serial.println("   PRET ! Codes jetables actifs");
  Serial.println("====================================\n");
}
void addCode(String code) {
  if (codeCount >= MAX_CODES) {
    Serial.println("!! Max codes atteint, suppression du plus ancien");
    for (int i = 0; i < MAX_CODES - 1; i++) {
      validCodes[i] = validCodes[i + 1];
    }
    codeCount = MAX_CODES - 1;
  }
  validCodes[codeCount] = code;
  codeCount++;
  Serial.print("Code ajoute : ");
  Serial.print(code);
  Serial.print(" (total: ");
  Serial.print(codeCount);
  Serial.println(")");
}
bool useCode(String code) {
  for (int i = 0; i < codeCount; i++) {
    if (validCodes[i] == code) {
      Serial.print("Code VALIDE utilise : ");
      Serial.println(code);
      for (int j = i; j < codeCount - 1; j++) {
        validCodes[j] = validCodes[j + 1];
      }
      codeCount--;
      return true;
    }
  }
  Serial.print("Code INVALIDE : ");
  Serial.println(code);
  return false;
}
void openDoorTemporary() {
  actionOuvrirMecanisme(); // === AJOUT SERVO ===

  ledState = true;
  digitalWrite(LED_PIN, HIGH);
  matrix.loadFrame(lockOpen);
  Serial.println(">> PORTE OUVERTE (code jetable) - Cadenas OUVERT - fermeture auto 5s");
  mqttClient.beginMessage(topicState);
  mqttClient.print("ON");
  mqttClient.endMessage();
  autoCloseTime = millis() + 5000;
}
void onMqttMessage(int messageSize) {
  String topic = mqttClient.messageTopic();
  String message = "";
  while (mqttClient.available()) {
    message += (char)mqttClient.read();
  }
  Serial.print("[");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(message);
  if (topic == String(topicCmd)) {
    if (message == "ON") {
      actionOuvrirMecanisme(); // === AJOUT SERVO ===
      ledState = true;
      autoCloseTime = 0;
      digitalWrite(LED_PIN, HIGH);
      matrix.loadFrame(lockOpen);
      Serial.println(">> LED ALLUMEE (admin) - Cadenas OUVERT");
    }
    else if (message == "OFF") {
      actionFermerMecanisme(); // === AJOUT SERVO ===
      ledState = false;
      autoCloseTime = 0;
      digitalWrite(LED_PIN, LOW);
      matrix.loadFrame(lockClosed);
      Serial.println(">> LED ETEINTE (admin) - Cadenas FERME");
    }
    mqttClient.beginMessage(topicState);
    mqttClient.print(ledState ? "ON" : "OFF");
    mqttClient.endMessage();
  }
  else if (topic == String(topicAddCode)) {
    addCode(message);
  }
  else if (topic == String(topicUseCode)) {
    if (useCode(message)) {
      mqttClient.beginMessage(topicCodeResult);
      mqttClient.print("OK");
      mqttClient.endMessage();

      mqttClient.beginMessage(topicCodeUsed);
      mqttClient.print(message);
      mqttClient.endMessage();

      openDoorTemporary();
    } else {
      mqttClient.beginMessage(topicCodeResult);
      mqttClient.print("FAIL");
      mqttClient.endMessage();
    }
  }
}
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== Porte Connectee - Codes Jetables ===");
  // === AJOUT SERVO : Initialisation ===
  monServo.attach(SERVO_PIN);
  monServo.write(ARRET_SERVO); // S'assurer qu'il ne bouge pas au démarrage
  // ====================================
  matrix.begin();
  matrix.loadFrame(lockClosed);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  connectWiFi();
  connectMQTT();
  matrix.loadFrame(lockClosed);
  mqttClient.onMessage(onMqttMessage);
}
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }
  if (!mqttClient.connected()) {
    connectMQTT();
  }
  mqttClient.poll();
  // Fermeture automatique après code jetable
  if (autoCloseTime > 0 && millis() >= autoCloseTime) {
    autoCloseTime = 0;
    actionFermerMecanisme(); // === AJOUT SERVO ===

    ledState = false;
    digitalWrite(LED_PIN, LOW);
    matrix.loadFrame(lockClosed);
    Serial.println(">> FERMETURE AUTO (5s ecoulees) - Cadenas FERME");

    mqttClient.beginMessage(topicState);
    mqttClient.print("OFF");
    mqttClient.endMessage();
  }
  if (millis() - lastAlive > 30000) {
    lastAlive = millis();
    mqttClient.beginMessage(topicState);
    mqttClient.print(ledState ? "ON" : "OFF");
    mqttClient.endMessage();
  }
}
