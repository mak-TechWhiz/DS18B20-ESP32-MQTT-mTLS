# DS18B20-ESP32-MQTT-mTLS
This is one of my university projects. I have configured the communication between the temperature sensor DS18B20 and Mosquitto MQTT Broker to use mTLS.

# DS18B20-ESP32-MQTT-mTLS

![Build Status](https://img.shields.io/badge/build-passing-brightgreen)
![License](https://img.shields.io/badge/license-NUST-blue)

## Overview

An end-to-end example that reads temperature from a DS18B20 sensor on an ESP32 and publishes it securely to a Mosquitto MQTT broker using mutual TLS (mTLS).  
This guide covers:

- Generating your own Certificate Authority (CA)
- Signing server & client certificates with OpenSSL
- Configuring `mosquitto.conf` for mTLS
- Preparing the ESP32 firmware

---

## Prerequisites

1. **Hardware**
   - ESP32 development board
   - DS18B20 one-wire temperature sensor + 4.7 kΩ pull-up resistor
   - Breadboard & jumper wires
2. **Software**
   - Arduino IDE 
   - OpenSSL (v1.1+)
   - Mosquitto MQTT broker (v2.x)

---

## 1. Certificate Authority & Certificates

All certificate operations happen from your local machine using OpenSSL.

### 1.1 Create Your CA

```bash
# 1. Generate CA private key
openssl genrsa -out ca.key 4096

# 2. Generate self-signed CA certificate
openssl req -x509 -new -nodes -key ca.key \
  -sha256 -days 3650 \
  -subj "/C=US/ST=State/L=City/O=Org/CN=MyMQTT-CA" \
  -out ca.crt
```
This step i.e. 1.2 is very critical as it will save you from hours of hassle.

### 1.2 Generate Server Certificate

```bash
# 1. Generate server private key
openssl genrsa -out server.key 2048

# 2. Create server CSR (Common Name = broker hostname)
openssl req -new -key server.key \
  -subj "/C=US/ST=State/L=City/O=Org/CN=your.broker.address" \
  -out server.csr

# 3. Sign server CSR with your CA
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key \
  -CAcreateserial -out server.crt -days 365 -sha256
```
IMPORTANT: Put the CN (Common Name) as your server's (On which the Mosquitto Broker is running) hostname.
For Example: If you are using Raspberry Pi, then check it's hostname and put that in CN field. 
Refer to my other repo for how to configure hostname.

### 1.3 Generate Client Certificate (for ESP32)

```bash
# 1. Generate client private key
openssl genrsa -out client.key 2048

# 2. Create client CSR (set CN to e.g. esp32-client)
openssl req -new -key client.key \
  -subj "/C=US/ST=State/L=City/O=Org/CN=esp32-client" \
  -out client.csr

# 3. Sign client CSR
openssl x509 -req -in client.csr -CA ca.crt -CAkey ca.key \
  -CAcreateserial -out client.crt -days 365 -sha256
```

> **Tip:** Keep `ca.key`, `server.key`, and `client.key` secure! Only `ca.crt`, `server.crt`, and `client.crt` are distributed.

---

## 2. Configure Mosquitto Broker

Edit your `mosquitto.conf` (typically in `/etc/mosquitto/mosquitto.conf`):

```conf
# Listener & protocol
listener 8883

# CA and server certificates
cafile /path/to/ca.crt
certfile /path/to/server.crt
keyfile  /path/to/server.key

# Require client to present a valid cert
require_certificate true
use_identity_as_username true
allow_anonymous false
```

Reload or restart Mosquitto:

```bash
sudo systemctl restart mosquitto
```
If you are using docker then:
```bash
sudo docker your_broker_container_name restart
```

Use `mosquitto_sub`/`mosquitto_pub` to test:

From your machine on the same network do:

```bash
mosquitto_sub -h your_broker_hostname -p 8883 \
  --cafile ca.crt --cert client.crt --key client.key \
  -t sensors/temperature -v
```

---

## 3. ESP32 Firmware Setup

### 3.1 Suggested Folder Layout
```
DS18B20-ESP32-MQTT-mTLS/
├── src/
│   └── main.ino       # your Arduino sketch
├── data/
│   └── ca.crt         # copy CA cert here
│   └── client.crt     # copy client cert
│   └── client.key     # copy client key (git-ignored)
└── README.md
```

### 3.2 `main.ino`

Edit `main.ino`:
```cpp

#define WIFI_SSID    "YOUR_SSID"
#define WIFI_PASS    "YOUR_PASS"
#define MQTT_HOST    "your.broker.address"
#define MQTT_PORT    8883
#define MQTT_TOPIC   "sensors/temperature"

#endif
```

### 3.3 Sketch Highlights

- Include certificate files via SPIFFS or embed in code using `WiFiClientSecure`.
- Initialize OneWire & DallasTemperature to read DS18B20.
- Use `PubSubClient` over `WiFiClientSecure`.
- Connect to broker with:
  ```cpp
  secureClient.setCACert(ca_crt);
  secureClient.setCertificate(client_crt);
  secureClient.setPrivateKey(client_key);
  mqttClient.setClient(secureClient);
  mqttClient.connect("esp32-client");
  ```

Refer to [Temperature_MQTT_mTLS.ino](Temperature_MQTT_mTLS.ino) for full code.

---

## 4. Usage

1. Generate & copy certs: `ca.crt`, `client.crt`, `client.key` → `data/`
2. Rename & edit `config/mqtt_config.h`
3. Build & flash firmware
4. Monitor Serial at `115200` for connection and sensor logs
5. Subscribe on your PC:
   ```bash
   mosquitto_sub -h your.broker.address -p 8883 \
     --cafile data/ca.crt --cert data/client.crt --key data/client.key \
     -t sensors/temperature -v
   ```

---

## Troubleshooting

- **TLS handshake errors**: verify CNs match and file paths  
- **Connection timeout**: ensure broker is listening on 8883 and firewall is open.  

---

## License

This project is licensed under the NUST License. See the [LICENSE](LICENSE) file for details.

