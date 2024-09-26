/**
 * ! USING ESP8266
 ** RC 522 RFID Reader Module Pins
 * - VCC: 3.3V
 * - GND: GND
 * - SDA/SS:  D8
 * - SCK: D5
 * - MOSI: D7
 * - MISO : D6
 * - RST: D3
 ** LiquidCrystal_I2C 16x2
 * - VCC: 5V
 * - GND: GND
 * - SDA: D2 with an address of 0x27
 * - SCL: D1
 ** Adafruit SSD1306
 * - VCC: 5V
 * - GND: GND
 * - SDA: D2 with an address of 0x3C
 * - SCL: D1
 ** RTC DS3231
 * - VCC: 5V
 * - GND: GND
 * - SDA: D2 with an address of 0x68
 * - SCL: D1
 ** SD Card Reader
 * - GND: GND
 * - CS: D4
 * - MOSI: D7
 * - MISO: D6
 * - SCK: D5
 * - SD: D8
 * - VCC: 3.3V
*/

#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <WiFi.h>
#include <LiquidCrystal_I2C.h>
#include <WebSockets.h>
#include <WebSocketsClient.h>
#include <Neotimer.h>
#include <Wire.h>
#include <faces.h>
#include <MFRC522.h>
#include <SPI.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>

#include "enums/AttendanceMode.h"
#include "dto.h"
#include "OfflineAttendanceController.h"

// Region : WiFi Credentials
#define WIFI_SSID "Nytri"
#define WIFI_PASSWORD "87643215"

// Region:  Define WebSocket Creds and Host
#define WS_HOST "192.168.109.66"
#define WS_PORT 8080
#define WS_URI "/ws/attendances"

// Region: RFID Scanner
#define RST_PIN         34          // Configurable, see typical pin layout above
#define SS_PIN          5         // Configurable, see typical pin layout above
MFRC522 rfid(SS_PIN, RST_PIN); // Create MFRC522 instance
MFRC522::MIFARE_Key key;

// Region: Buzzer
#define BUZZER_PIN 32

// Region: Displays
auto lcd = LiquidCrystal_I2C(0x27, 16, 2);
auto display = Adafruit_SSD1306(128, 32, &Wire, -1);

// Region: WebSocket Controller
WebSocketsClient apiWSClient;

// Region: Timers
Neotimer gtoTimer(250);
Neotimer gtoTimerReset(3000);

// Region OLED Face Variables
int faceMode = 1;
int frame = 0;
int frameStop = 2;
int frameCount = 0;

// Region: SD Controller
// constexpr uint8_t sdCardPin = D4;
// auto sdController = OfflineAttendanceController(sdCardPin);
//

// Region: SMS Pins
SoftwareSerial sim800l(26, 25);

// Region: Arduino JSON
JsonDocument rfidCard;

// Region: Settings
auto mode = IN;

void debugWiFiInfo() {
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.print("MAC address: ");
  Serial.println(WiFi.macAddress());

  Serial.print("Hostname: ");
  Serial.println(WiFiClass::getHostname());

  Serial.print("RSSI: ");
  Serial.println(WiFi.RSSI());

  Serial.print("BSSID: ");
  Serial.println(WiFi.BSSIDstr());

  Serial.print("Channel: ");
  Serial.println(WiFi.channel());
}

void beep(const int frequency, const int duration) {
  tone(BUZZER_PIN, frequency, duration);
}

CodeStatus stringToCodeStatus(const String &statusStr) {
  if (statusStr == "OK") return CodeStatus::OK;
  if (statusStr == "FAILED") return CodeStatus::FAILED;
  if (statusStr == "BAD_REQUEST") return CodeStatus::BAD_REQUEST;
  if (statusStr == "NOT_FOUND") return CodeStatus::NOT_FOUND;
  if (statusStr == "NULL") return CodeStatus::NULL_STATUS;
  if (statusStr == "EXISTS") return CodeStatus::EXISTS;
  return CodeStatus::FAILED; // Default case
}

String attendanceModeToString(const AttendanceMode mode) {
  switch (mode) {
    case 0:
      return "IN";
    case 1:
      return "OUT";
    case 2:
      return "EXCUSED";
    default:
      return "UNKNOWN";
  }
}

void lcdPrint(const String &chars) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(chars);
}

/**
 * @brief WebSocket event handler.
 *
 * This function is called whenever there is a WebSocket event.
 *
 * @param webSocketType Type of the event.
 * @param payload Payload of the event.
 * @param length Length of the payload.
 */
void webSocketEvent(const WStype_t webSocketType, const uint8_t *payload, size_t length) {
  switch (webSocketType) {
    case WStype_ERROR: {
      Serial.println("WebSocket Error");
      beep(2500, 3000);
      break;
    }
    case WStype_DISCONNECTED: {
      Serial.println("WebSocket Client Disconnected");
      beep(2500, 2000);
      break;
    }
    case WStype_CONNECTED: {
      Serial.println("WebSocket Client Connected");
      beep(2400, 1000);
      break;
    }
    case WStype_TEXT: {
      Serial.printf("Message: %p\n", payload);
      // Result
      JsonDocument attendanceMessage;

      // Deserialize JSON
      const DeserializationError error = deserializeJson(attendanceMessage, payload, length);
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return;
      }

      // Check if successful then beep and show lcd
      const String status = attendanceMessage["status"];
      if (stringToCodeStatus(status) == CodeStatus::NULL_STATUS) {
        lcdPrint("Invalid ID");
        Serial.println("Invalid ID");
        faceMode = 3;
        beep(1500, 250);
      } else {
        if (stringToCodeStatus(status) == CodeStatus::OK) {
          lcdPrint(attendanceMessage["message"]);
          Serial.println(attendanceMessage["message"].as<String>());
          faceMode = 1;
          beep(2400, 150);
        } else if (stringToCodeStatus(status) == CodeStatus::NOT_FOUND) {
          lcdPrint("Not Found");
          Serial.println("Not Found");
          faceMode = 3;
          beep(1800, 250);
        } else if (stringToCodeStatus(status) == CodeStatus::EXISTS) {
          lcdPrint("Already Checked in");
          Serial.println("Already Checked in");
          beep(2200, 250);
        }
        gtoTimerReset.start();
        break;
      }
    }
  }
}

void updateSerial() {
  delay(500);
  while (Serial.available()) {
    sim800l.write(Serial.read()); //Forward what Serial received to Software Serial Port
  }
  while (sim800l.available()) {
    Serial.write(sim800l.read()); //Forward what Software Serial received to Serial Port
  }
}

void displayFace() {
  if (frameCount == frameStop && faceMode != 0) {
    faceMode = 0;
    frameCount = 0;
  }

  if (frame >= allArray_LEN[faceMode]) {
    frame = 0;
    if (faceMode != 0) {
      frameCount += 1;
    }
  }

  display.clearDisplay();
  const unsigned char **faceArray;
  int faceArraySize;

  switch (faceMode) {
    case 0:
      faceArray = idle_face_allArray;
      break;
    case 1:
      faceArray = welcome_face_allArray;
      break;
    case 2:
      faceArray = late_face_allArray;
      break;
    case 3:
      faceArray = invalid_face_allArray;
      break;
    default:
      faceArray = idle_face_allArray;
      break;
  }
  faceArraySize = allArray_LEN[faceMode];

  if (frame < faceArraySize) {
    display.drawBitmap(0, 0, faceArray[frame], 128, 32, WHITE);
  }

  display.display();
  frame = (frame + 1) % faceArraySize;
}

void getSIMInfo() {
  sim800l.println("AT"); //Once the handshake test is successful, it will back to OK
  updateSerial();
  sim800l.println("AT+CSQ"); //Signal quality test, value range is 0-31 , 31 is the best
  updateSerial();
  sim800l.println("AT+CCID"); //Read SIM information to confirm whether the SIM is plugged
  updateSerial();
  sim800l.println("AT+CREG?"); //Check whether it has registered in the network
  updateSerial();
}

void setup() {
  // Set up serial connection
  Serial.begin(115200);
  sim800l.begin(9600);

  // Wire.begin();
  SPI.begin();
  rfid.PCD_Init();
  rfid.PCD_DumpVersionToSerial(); // Show details of PCD - MFRC522 Card Reader details
  // Set up key
  for (unsigned char &i: key.keyByte) i = 0xFF;

  // Set up LCD
  lcd.init();
  lcd.backlight();

  // Set up OLED
  display.begin();
  display.clearDisplay();
  display.display();

  // Set up Buzzer
  pinMode(BUZZER_PIN, OUTPUT);

  // If there is no serial, don't start the program. This is for DEBUGGING
  while (!Serial) {
  }

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  lcd.print("Connecting...");
  while (WiFiClass::status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    lcd.print(".");
  }

  Serial.println("WiFi Connected!");
  lcd.clear();
  lcd.print("Connected!");
  Serial.println("LCD Initialized");
  debugWiFiInfo();
  apiWSClient.begin(WS_HOST, WS_PORT, WS_URI);
  apiWSClient.onEvent(webSocketEvent);
  delay(1000);
}

void updateFace() {
  if (gtoTimer.repeat()) {
    displayFace();
  }
}

String hexToString(const String &hex) {
  String result = "";
  for (int i = 0; i < hex.length(); i += 2) {
    const char c = static_cast<char>(strtol(hex.substring(i, i + 2).c_str(), nullptr, 16));
    result += c;
  }
  return result;
}

String readRFIDHash() {
  // Dump debug info about the card; PICC_HaltA() is automatically called
  String rfidHash = "";
  byte buffer[18];
  byte size = sizeof(buffer);

  for (byte block = 4; block <= 5; block++) {
    // Authenticate
    MFRC522::StatusCode status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(rfid.uid));
    if (status != MFRC522::STATUS_OK) {
      Serial.print(F("Authentication failed: "));
      return MFRC522::GetStatusCodeName(status);
    }

    status = rfid.MIFARE_Read(block, buffer, &size);
    if (status == MFRC522::STATUS_OK) {
      for (byte i = 0; i < 16; i++) {
        rfidHash += hexToString(String(buffer[i], HEX));
      }
    } else {
      Serial.print(F("Error reading block "));
      Serial.println(block);
      break;
    }

    memset(buffer, 0, sizeof(buffer));
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  return rfidHash;
}

void loop() {
  getSIMInfo();

  if (gtoTimerReset.done()) {
    faceMode = 0;
    lcdPrint("Ready!");
  }
  updateFace();
  if (WiFi.isConnected()) {
    apiWSClient.loop();
  }

  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if (!rfid.PICC_IsNewCardPresent()) {
    return;
  }

  if (!rfid.PICC_ReadCardSerial()) {
    return;
  }
  // Serial.println(readRFIDHash());
  const String hash = readRFIDHash();
  if (hash != "") {
    Serial.println(hash);

    rfidCard["hashedLrn"] = hash;
    rfidCard["mode"] = attendanceModeToString(mode);
    Serial.println(rfidCard["hashedLrn"].as<String>());

    // Region: Production
    if (apiWSClient.isConnected()) {
      char output[256];
      serializeJson(rfidCard, output);
      Serial.println(output);
      apiWSClient.sendTXT(output);
      Serial.println("Sent RFID data to WebSocket Server");
    } else {
      Serial.println("Not connected to WebSocket Server");
      Student student;
      Attendance attendance;
      //
      //   // Read the student data from students.csv
      //   // sdController.readStudentData("students.csv", student);
      //
      //   // Set attendance data for the student
      //   // TODO: Use RTC module to read date and time.
      //   attendance.student_id = student.student_id;
      //   attendance.date = "2024-09-22"; // Example date
      //   attendance.time_in = "08:00:00";
      //   attendance.time_out = "16:00:00";
      //   attendance.status = "Present"; // Example status
      //   Serial.println(attendance.status);
      //
      //   // Write the attendance data to attendances.csv
      //   // sdController.writeAttendanceData("attendances.csv", attendance);
      // }
    }
  }
}
