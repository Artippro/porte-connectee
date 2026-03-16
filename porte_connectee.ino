/*
 * PORTE CONNECTÉE - Contrôle depuis PARTOUT dans le monde
 * Arduino UNO R4 WiFi (COM6)
 *
 * Fonctionnalités :
 *   - Contrôle admin (ON/OFF)
 *   - Codes jetables : un ami entre le code, porte s'ouvre UNE SEULE FOIS
 *
 * Bibliothèques nécessaires :
 *   - ArduinoMqttClient (par Arduino)
 */

#include <WiFiS3.h>
#include <ArduinoMqttClient.h>

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

  // S'abonner aux topics
  mqttClient.subscribe(topicCmd);
  mqttClient.subscribe(topicAddCode);
  mqttClient.subscribe(topicUseCode);

  Serial.println("Ecoute sur :");
  Serial.println(topicCmd);
  Serial.println(topicAddCode);
  Serial.println(topicUseCode);

  // Envoyer l'état actuel
  mqttClient.beginMessage(topicState);
  mqttClient.print(ledState ? "ON" : "OFF");
  mqttClient.endMessage();

  Serial.println("\n====================================");
  Serial.println("   PRET ! Codes jetables actifs");
  Serial.println("====================================\n");
}

// Ajouter un code jetable
void addCode(String code) {
  if (codeCount >= MAX_CODES) {
    Serial.println("!! Max codes atteint, suppression du plus ancien");
    // Décaler tout
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

// Vérifier et utiliser un code (retourne true si valide)
bool useCode(String code) {
  for (int i = 0; i < codeCount; i++) {
    if (validCodes[i] == code) {
      Serial.print("Code VALIDE utilise : ");
      Serial.println(code);
      // Supprimer le code (décaler)
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
  ledState = true;
  digitalWrite(LED_PIN, HIGH);
  Serial.println(">> PORTE OUVERTE (code jetable) - fermeture auto dans 5s");

  // Envoyer l'état
  mqttClient.beginMessage(topicState);
  mqttClient.print("ON");
  mqttClient.endMessage();

  // Programmer fermeture auto dans 5 secondes
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

  // --- Commande admin ON/OFF ---
  if (topic == String(topicCmd)) {
    if (message == "ON") {
      ledState = true;
      autoCloseTime = 0; // pas de fermeture auto en mode admin
      digitalWrite(LED_PIN, HIGH);
      Serial.println(">> LED ALLUMEE (admin)");
    }
    else if (message == "OFF") {
      ledState = false;
      autoCloseTime = 0;
      digitalWrite(LED_PIN, LOW);
      Serial.println(">> LED ETEINTE (admin)");
    }
    mqttClient.beginMessage(topicState);
    mqttClient.print(ledState ? "ON" : "OFF");
    mqttClient.endMessage();
  }

  // --- Admin ajoute un code jetable ---
  else if (topic == String(topicAddCode)) {
    addCode(message);
  }

  // --- Quelqu'un utilise un code ---
  else if (topic == String(topicUseCode)) {
    if (useCode(message)) {
      // Code valide → ouvrir la porte temporairement
      mqttClient.beginMessage(topicCodeResult);
      mqttClient.print("OK");
      mqttClient.endMessage();
      // Notifier tous les admins quel code a été utilisé
      mqttClient.beginMessage(topicCodeUsed);
      mqttClient.print(message);
      mqttClient.endMessage();
      openDoorTemporary();
    } else {
      // Code invalide
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

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  connectWiFi();
  connectMQTT();

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
    ledState = false;
    digitalWrite(LED_PIN, LOW);
    Serial.println(">> FERMETURE AUTO (5s ecoulees)");
    mqttClient.beginMessage(topicState);
    mqttClient.print("OFF");
    mqttClient.endMessage();
  }

  // Signe de vie toutes les 30 secondes
  if (millis() - lastAlive > 30000) {
    lastAlive = millis();
    mqttClient.beginMessage(topicState);
    mqttClient.print(ledState ? "ON" : "OFF");
    mqttClient.endMessage();
  }
}
