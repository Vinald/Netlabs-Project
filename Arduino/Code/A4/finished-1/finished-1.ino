#include <LiquidCrystal_I2C.h>
#include <Arduino.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ThingSpeak.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <BluetoothSerial.h>
#include <modelNew.h>
// #include <pipeline.h>
#include <vector> // Add this include for std::vector

// Constants for ThingSpeak
const char* ssid = "vinald";
const char* password = "14231423";
unsigned long myChannel = 2234562;
const char* apiKey = "H0JAID5SFXIIFCJX";
const int soilMoisturePin = A0; 
const int LDRPin = 34;
const int temperatureSensorPin = 17; 

// Constants for Firebase
#define WIFI_SSID "vinald"
#define WIFI_PASSWORD "14231423"
#define FIREBASE_HOST F("https://agriculture-a2856-default-rtdb.firebaseio.com/")
#define FIREBASE_AUTH F("YOUR_FIREBASE_AUTH_TOKEN")
OneWire oneWire(temperatureSensorPin);
DallasTemperature DS18B20(&oneWire);

LiquidCrystal_I2C lcd(0x27, 16, 2);
WiFiClient client;
FirebaseData firebaseData;

// Constants for pump control
const int moistureThresholdMin = 600;
const int moistureThresholdMax = 1000;
const int temperatureThresholdMin = 30;
const int temperatureThresholdMax = 50;

const int relayPin = 32;
bool pumpStatus = false;

BluetoothSerial SerialBT;

// Declare the loadRandomForestModel function
void loadRandomForestModel();

Eloquent::ML::Port::RandomForest rf_classifier;

//declaring variable for inputs
char* str_CropType;
float temperature;
float SoilMoisture;
float CropDays;

//label encoding crop type
int label_encoded_croptype(char* str_CropType) {
  if(str_CropType == "Wheat"){
    return 0;
  }else if(str_CropType == "Groundnuts"){
    return 1;
  }else if(str_CropType == "Garden Flowers"){
    return 2;
  }else if(str_CropType == "Maize"){
    return 3;
  }else if(str_CropType == "Paddy"){
    return 4;
  }else if(str_CropType == "Potato"){
    return 5;
  }else if(str_CropType == "Pulse"){
    return 6;
  }else if(str_CropType == "Sugarcane"){
    return 7;
  }else if(str_CropType == "Coffee"){
    return 8;
  }
  return -1;
}

//instatiating the scaler 
// pipeline myPipeline;

void setup() {
  Serial.begin(9600);
  pinMode(soilMoisturePin, INPUT);
  pinMode(LDRPin, INPUT);
  pinMode(temperatureSensorPin, INPUT);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);

  lcd.init();
  lcd.backlight();

  DS18B20.begin();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  ThingSpeak.begin(client);
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  SerialBT.begin("ESP32-Bluetooth");

  Serial.println("Bluetooth device ready for pairing.");

  // Load the RandomForest model
  // loadRandomForestModel();


  
}

void loop() {
  int soilMoistureValue = analogRead(soilMoisturePin);
  int mappedSoilMoistureValue = map(soilMoistureValue, 0, 1023, 0, 250);
  DS18B20.requestTemperatures();
  float temperatureC = DS18B20.getTempCByIndex(0);
  int ldrValue = analogRead(LDRPin);

  // Preprocessing of sensor data
  temperature = temperatureC;
  str_CropType = "Garden Flowers";
  int CropType = label_encoded_croptype(str_CropType);
  SoilMoisture = mappedSoilMoistureValue;  
  CropDays = 15;
  
  // Performing feature extraction
  float features_array[] = { static_cast<float>(CropType), CropDays, temperature, SoilMoisture };
  // if (!myPipeline.transform(features_array)){
  //   return;
  // }
  
  // Predict using the converted features
  // float prediction = rf_classifier.predict(myPipeline.transform(features_array));
  float prediction = rf_classifier.predict(features_array);

   // Display the predicted result
  Serial.print("Predicted Result: ");
  if (prediction == 0.0) {
    Serial.println("No Irrigation");
  } else if (prediction == 1.0) {
    Serial.println("Irrigation");
  } else {
    Serial.println("Unknown");
  }

  // Bluetooth command handling
  if (SerialBT.available()) {
    char command = SerialBT.read();
    if (command == '1') { // Turn on the pump
      pumpStatus = true;
      digitalWrite(relayPin, HIGH);
    } else if (command == '0') { // Turn off the pump
      pumpStatus = false;
      digitalWrite(relayPin, LOW);
    }
  }

  // Check pump conditions
  bool newPumpStatus = (temperatureC >= temperatureThresholdMin && temperatureC <= temperatureThresholdMax) ||
                      (mappedSoilMoistureValue >= moistureThresholdMin && mappedSoilMoistureValue <= moistureThresholdMax);

  if (newPumpStatus != pumpStatus) {
    pumpStatus = newPumpStatus;
    digitalWrite(relayPin, pumpStatus ? HIGH : LOW);
  }

  // Update ThingSpeak and Firebase
  ThingSpeak.setField(1, mappedSoilMoistureValue);
  ThingSpeak.setField(2, temperatureC);
  ThingSpeak.setField(3, ldrValue);
  ThingSpeak.setField(4, (int)temperatureSensorPin);
  ThingSpeak.setField(5, pumpStatus);
  int x = ThingSpeak.writeFields(myChannel, apiKey);
  if (x == 200) {
    Serial.println("ThingSpeak Updated");
  } else {
    Serial.println("ThingSpeak Problem. HTTP error code: " + String(x));
  }

   // Prepare input data for prediction
  // str_CropType = 8; // 'Coffee' corresponds to 8 (as you've mentioned in the comment)
  // float CropDays = 15.0; // Change this to the actual value
  // float Temperature = temperatureC;
  // float SoilMoisture = mappedSoilMoistureValue;

  unsigned long currentMillis = millis();
  String timestampStr = String(currentMillis / 1000);
  Firebase.setInt(firebaseData, "/sensor_readings/" + timestampStr + "/soil_moisture", mappedSoilMoistureValue);
  Firebase.setInt(firebaseData, "/sensor_readings/" + timestampStr + "/ldr", ldrValue);
  Firebase.setFloat(firebaseData, "/sensor_readings/" + timestampStr + "/temperature", temperatureC);
  Firebase.setString(firebaseData, "/sensor_readings/" + timestampStr + "/timestamp", timestampStr);
  Firebase.setBool(firebaseData, "/sensor_readings/" + timestampStr + "/pump_status", pumpStatus);

  // Display on LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Moisture: ");
  lcd.print(mappedSoilMoistureValue);
  lcd.setCursor(0, 1);
  lcd.print("Temp: ");
  lcd.print(temperatureC);
  lcd.print(" C ");
  lcd.setCursor(0, 2);
  lcd.print("LDR: ");
  lcd.print(ldrValue);

  delay(500);
}
