#include <Arduino.h>
#include <NimBLEDevice.h>
#include <ArduinoJson.h>

const int MAX_MESSAGE_LINE_LENGTH{2000};
int loops{5000};
bool deviceConnected;
String myJson;
StaticJsonDocument<MAX_MESSAGE_LINE_LENGTH> doc;
StaticJsonDocument<MAX_MESSAGE_LINE_LENGTH> rx;
String receiveLine{""};

NimBLECharacteristic *rxTxCharacteristic;
NimBLEServer *pServer;
NimBLEService *pService;
NimBLEAdvertising *pAdvertising;


#define SERVICE_UUID               "0000ffe0-0000-1000-8000-00805f9b34fb" // This is the same as the HM-10 module (on it, the CC2541 chip) uses by default for the GATT Service.
#define CHARACTERISTIC_RX_TX_UUID  "0000ffe1-0000-1000-8000-00805f9b34fb" // And the same as the HM-10 module for the Characteristic UUID.

void receivedComplete(String const& message) {
  deserializeJson(rx, message);

  const char* sensor = rx["name"];
  Serial.print("RX: name attribute value from received JSON: ");
  Serial.println(sensor);
}

class ServerCallbacks: public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
        deviceConnected = true;
        Serial.println("deviceConnected");
    };
    void onDisconnect(NimBLEServer* pServer) {
        deviceConnected = false;
        Serial.println("deviceDisonnected");
    };
};

class CharacteristicCallbacks: public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) {
        if(receiveLine.length() + pCharacteristic->getValue().size() > MAX_MESSAGE_LINE_LENGTH) {
          Serial.println("receiveLine gets too long, resetting it now, dropping message.");
          receiveLine = "";
        } else {
          receiveLine += pCharacteristic->getValue().c_str();
          Serial.print("receiveLine: ");
          Serial.println(receiveLine);
          
          // Is the last char an line break? Must be the end of message / JSON. (The JSON must not contain a line break of course)
          if(receiveLine.charAt(receiveLine.length()-1) == '\n') {
            Serial.println("LINE BREAK, received line seems to be complete.");
            receivedComplete(receiveLine);
            receiveLine = "";
          }
        }      
    };
};


void setup() {
  Serial.begin(115200);
  Serial.println("Starting NimBLE Server");
  NimBLEDevice::init("NimBLE");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  NimBLEDevice::setSecurityAuth(true, true, true);
  NimBLEDevice::setSecurityPasskey(123456); // The pin for pairing with this ESP - in contrast to most examples, I think you always should use at least this easy security feature...
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);

  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  pService = pServer->createService(SERVICE_UUID);
  rxTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_RX_TX_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::WRITE_AUTHEN | NIMBLE_PROPERTY::NOTIFY);
  rxTxCharacteristic->setCallbacks(new CharacteristicCallbacks());

  pService->start();

  pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();

  // Generate a static JSON here, storing it into "myJson" to send it later.
  doc["hello"] = "world";  
  JsonObject temps = doc.createNestedObject("temperatures");
  temps["inside"] = 22;
  temps["outside"] = 5;
  
  // Generate an attribute with an long array in it. Comment out to keep the test JSON compact.
  JsonArray data = doc.createNestedArray("looooooooongArray");
  for(int i=0; i < 200; i++) {
    data.add("exampleString" + String(i));  // .add only adds the string if it fits into the StaticJsonDocument 
  }

  serializeJson(doc, myJson);
  Serial.println("Generated JSON: \n" + myJson);
}

void loop() {

      // Send if connected and every ~5 seconds
      if(deviceConnected && loops++ > 5000) { 
        int splits = (myJson.length()/500);
        Serial.println("length of JSON: " + String(myJson.length()) + ". It will be splitted into " + String(splits+1) + " messages (max 500 chars each), followed by a line break to mark the end...");

        for(int i=0; i <= splits; i++) {
          String split = myJson.substring(i*500,i*500+500);
          rxTxCharacteristic->setValue(std::string(split.c_str()));
          rxTxCharacteristic->notify();
        }
        rxTxCharacteristic->setValue(std::string("\n"));
        rxTxCharacteristic->notify();
        Serial.println("Sent message and line ending!"); 

        loops = 0;
      }
      delay(1);
}