#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>


// ——————— Your Sensor Code (unchanged) ———————
const int oneWireBus = 4;
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

// ——————— Wi-Fi & MQTT over mTLS ———————

// WiFi credentials
const char* ssid     = "your_wifi_name";
const char* password = "your_wifi_pass";

// MQTT Broker (mTLS port)
const char* mqtt_server = "hostname_machine";
const int   mqtt_port   = 8883;
const char* mqtt_topic  = "sensor/temperature";



// ** Embed your PEM files here as C strings **
// 1) CA certificate (your CA that signed the broker’s cert)
static const char ca_cert[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
******************
******************
******************
-----END CERTIFICATE-----
)EOF";

// 2) Client certificate (signed by that CA)
static const char client_cert[] PROGMEM = R"KEY(
-----BEGIN CERTIFICATE-----
******************
******************
******************
-----END CERTIFICATE-----
)KEY";

// 3) Client private key
static const char client_key[] PROGMEM = R"KEY(
-----BEGIN PRIVATE KEY-----
******************
******************
******************
-----END PRIVATE KEY-----
)KEY";

// Use a secure client instead of plain WiFiClient
WiFiClientSecure espClient;
PubSubClient      client(espClient);



void setup_wifi() {
  Serial.printf("\nConnecting to %s … ", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nWiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT (mTLS) connection… ");
    // Use a unique client ID that matches your cert CN if you set ACLs by CN
    if (client.connect("esp32-secure")) {
      Serial.println("connected!");
    } else {
      Serial.printf("failed, rc=%d; retry in 5s\n", client.state());
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(10);

  // 1) Connect Wi-Fi
  setup_wifi();

  


  // 2) Configure espClient for mTLS
  espClient.setCACert(ca_cert);
  espClient.setCertificate(client_cert);
  espClient.setPrivateKey(client_key);


  // 3) Point PubSubClient to your TLS broker
  client.setServer(mqtt_server, mqtt_port);

  // 4) Init your sensor
  sensors.begin();
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  static unsigned long lastMsg = 0;
  unsigned long now = millis();
  if (now - lastMsg > 5000) {
    lastMsg = now;

    sensors.requestTemperatures();
    float temperatureC = sensors.getTempCByIndex(0);

    // Build JSON payload
    char payload[64];
    int len = snprintf(payload, sizeof(payload),
                       "{\"tempC\":%.2f}", temperatureC);

    // Publish securely over mTLS
    Serial.printf("Publishing to %s: %s\n", mqtt_topic, payload);
    if (!client.publish(mqtt_topic, payload, len)) {
      Serial.println("Publish failed");
    }
  }
}
