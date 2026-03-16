/*
 * PORTE CONNECTÉE - Contrôle depuis PARTOUT dans le monde
 * Arduino UNO R4 WiFi (COM6)
 *
 * Fonctionne via MQTT (broker gratuit HiveMQ)
 * L'Arduino écoute les commandes envoyées depuis la page web
 *
 * Bibliothèques nécessaires (à installer via le Gestionnaire) :
 *   - ArduinoMqttClient (par Arduino)
 *
 * 1. Modifier SSID et mot de passe WiFi ci-dessous
 * 2. Téléverser sur la carte
 * 3. Ouvrir telecommande.html dans un navigateur (n'importe où)
 */

#include <WiFiS3.h>
#include <ArduinoMqttClient.h>

// ===== À MODIFIER AVEC TON WIFI =====
char ssid[] = "iPhone du bg";
char pass[] = "skedddddd";
// =====================================

// MQTT - Broker gratuit (pas besoin de compte)
const char broker[]    = "broker.hivemq.com";
int        port        = 1883;
const char topicCmd[]  = "porte-vincent-2026/commande";   // topic pour recevoir ON/OFF
const char topicState[] = "porte-vincent-2026/etat";      // topic pour envoyer l'état

const int LED_PIN = 13;
bool ledState = false;

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

unsigned long lastAlive = 0;

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
  // ID unique pour éviter les conflits
  String clientId = "arduino-porte-" + String(random(10000));
  mqttClient.setId(clientId);

  while (!mqttClient.connect(broker, port)) {
    Serial.print(".");
    delay(2000);
  }
  Serial.println(" connecte !");

  // S'abonner au topic de commande
  mqttClient.subscribe(topicCmd);
  Serial.print("Ecoute sur : ");
  Serial.println(topicCmd);

  // Envoyer l'état actuel
  mqttClient.beginMessage(topicState);
  mqttClient.print(ledState ? "ON" : "OFF");
  mqttClient.endMessage();

  Serial.println("\n====================================");
  Serial.println("   PRET ! Ouvre telecommande.html");
  Serial.println("   depuis n'importe ou dans le monde");
  Serial.println("====================================\n");
}

void onMqttMessage(int messageSize) {
  String topic = mqttClient.messageTopic();
  String message = "";
  while (mqttClient.available()) {
    message += (char)mqttClient.read();
  }

  Serial.print("Recu : ");
  Serial.println(message);

  if (message == "ON") {
    ledState = true;
    digitalWrite(LED_PIN, HIGH);
    Serial.println(">> LED ALLUMEE (Porte ouverte)");
  }
  else if (message == "OFF") {
    ledState = false;
    digitalWrite(LED_PIN, LOW);
    Serial.println(">> LED ETEINTE (Porte fermee)");
  }

  // Confirmer l'état
  mqttClient.beginMessage(topicState);
  mqttClient.print(ledState ? "ON" : "OFF");
  mqttClient.endMessage();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== Porte Connectee - Mondial ===");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  connectWiFi();
  connectMQTT();

  mqttClient.onMessage(onMqttMessage);
}

void loop() {
  // Reconnecter si déconnecté
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }
  if (!mqttClient.connected()) {
    connectMQTT();
  }

  // Traiter les messages entrants
  mqttClient.poll();

  // Envoyer un signe de vie toutes les 30 secondes
  if (millis() - lastAlive > 30000) {
    lastAlive = millis();
    mqttClient.beginMessage(topicState);
    mqttClient.print(ledState ? "ON" : "OFF");
    mqttClient.endMessage();
  }
}
