#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>

// RFID pin configuration
#define RST_PIN D3  // RST to D3 (GPIO0)
#define SS_PIN D4   // SDA to D4 (GPIO2)
MFRC522 rfid(SS_PIN, RST_PIN);

// Fingerprint sensor configuration
#define FINGERPRINT_RX D1  // Connect to TX on fingerprint sensor
#define FINGERPRINT_TX D2  // Connect to RX on fingerprint sensor
SoftwareSerial fingerprintSerial(FINGERPRINT_RX, FINGERPRINT_TX);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&fingerprintSerial);

// WiFi credentials
const char* ssid = "Galaxy S9+3b92";
const char* password = "00000000";

// Web server
ESP8266WebServer server(80);

// Server URL and API key
const char* serverURL = "http://82.112.241.179:8000/attendances";
const char* apiKey = "b3f65ecc04820ba6cdd0a3d0ac9a4f57162c6c9a06efac5ceca41e5f6e3ae956";

// Operation mode
bool fingerprintMode = true;
bool attendanceMode = false;
bool enrollmentInProgress = false;
int enrollmentID = -1;

// Attendance records buffer
struct AttendanceRecord {
  String timestamp;
  String rfidTag;
  int fingerprintID;
  String status;
};

AttendanceRecord recentRecords[10];
int recordIndex = 0;

// CORS headers function
void setCORSHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept, Authorization");
  server.sendHeader("Access-Control-Max-Age", "3600");
}

// Handle preflight requests
void handlePreflight() {
  setCORSHeaders();
  server.send(200, "text/plain", "");
}

void setup() {
  Serial.begin(9600);
  while (!Serial);
  delay(2000);
  
  // Initialize WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("MAC address: ");
  Serial.println(WiFi.macAddress());
  
  // Start RFID
  SPI.begin();
  rfid.PCD_Init();
  Serial.println("RFID reader initialized");
  
  // Start Fingerprint sensor
  fingerprintSerial.begin(57600);
  delay(500);
  finger.begin(57600);
  delay(1000);
  
  Serial.println("Checking fingerprint sensor...");
  for (int i = 0; i < 5; i++) {
    if (finger.verifyPassword()) {
      Serial.println("Fingerprint sensor connected!");
      break;
    } else {
      Serial.println("Fingerprint sensor not found! Retrying... " + String(i+1));
      delay(1000);
    }
  }
  
  if (!finger.verifyPassword()) {
    Serial.println("Continuing without fingerprint sensor. Switching to RFID-only mode.");
    fingerprintMode = false;
  } else {
    finger.getParameters();
    Serial.print("Status: 0x"); Serial.println(finger.status_reg, HEX);
    Serial.print("System ID: 0x"); Serial.println(finger.system_id, HEX);
    Serial.print("Capacity: "); Serial.println(finger.capacity);
    Serial.print("Security level: "); Serial.println(finger.security_level);
  }
  
  // Setup web server routes
  setupWebServer();
  
  // Start web server
  server.begin();
  Serial.println("Web server started");
  
  Serial.println("\n=== FINGERPRINT & RFID ATTENDANCE SYSTEM ===");
  Serial.println("Web interface available at: http://" + WiFi.localIP().toString());
}

void setupWebServer() {
  // Handle all OPTIONS requests (CORS preflight)
  server.onNotFound([]() {
    if (server.method() == HTTP_OPTIONS) {
      handlePreflight();
    } else {
      setCORSHeaders();
      server.send(404, "text/plain", "Not Found");
    }
  });
  
  // Explicit OPTIONS handlers for each endpoint
  server.on("/status", HTTP_OPTIONS, handlePreflight);
  server.on("/fingerprints", HTTP_OPTIONS, handlePreflight);
  server.on("/enroll", HTTP_OPTIONS, handlePreflight);
  server.on("/delete", HTTP_OPTIONS, handlePreflight);
  server.on("/attendance", HTTP_OPTIONS, handlePreflight);
  server.on("/attendance-records", HTTP_OPTIONS, handlePreflight);
  
  // Status endpoint
  server.on("/status", HTTP_GET, []() {
    setCORSHeaders();
    DynamicJsonDocument doc(512);
    doc["status"] = "connected";
    doc["fingerprint_sensor"] = fingerprintMode;
    doc["attendance_mode"] = attendanceMode;
    doc["ip"] = WiFi.localIP().toString();
    doc["free_heap"] = ESP.getFreeHeap();
    doc["uptime"] = millis();
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
  });
  
  // List fingerprints
  server.on("/fingerprints", HTTP_GET, []() {
    setCORSHeaders();
    handleListFingerprints();
  });
  
  // Enroll fingerprint
  server.on("/enroll", HTTP_POST, []() {
    setCORSHeaders();
    handleEnrollFingerprint();
  });
  
  // Delete fingerprint
  server.on("/delete", HTTP_POST, []() {
    setCORSHeaders();
    handleDeleteFingerprint();
  });
  
  // Toggle attendance mode
  server.on("/attendance", HTTP_POST, []() {
    setCORSHeaders();
    handleAttendanceMode();
  });
  
  // Get attendance records
  server.on("/attendance-records", HTTP_GET, []() {
    setCORSHeaders();
    handleGetAttendanceRecords();
  });
  
  // Root endpoint for testing
  server.on("/", HTTP_GET, []() {
    setCORSHeaders();
    String html = "<html><body>";
    html += "<h1>ESP8266 Attendance System</h1>";
    html += "<p>Status: Running</p>";
    html += "<p>IP: " + WiFi.localIP().toString() + "</p>";
    html += "<p>Fingerprint Mode: " + String(fingerprintMode ? "ON" : "OFF") + "</p>";
    html += "<p>Attendance Mode: " + String(attendanceMode ? "ON" : "OFF") + "</p>";
    html += "</body></html>";
    server.send(200, "text/html", html);
  });
}

void loop() {
  server.handleClient();
  
  if (attendanceMode) {
    handleAttendanceScanning();
  }
  
  if (enrollmentInProgress) {
    handleEnrollmentProcess();
  }
  
  delay(50); // Reduced delay for better responsiveness
}

void handleListFingerprints() {
  if (!finger.verifyPassword()) {
    server.send(500, "application/json", "{\"error\":\"Fingerprint sensor not connected\"}");
    return;
  }
  
  DynamicJsonDocument doc(2048);
  JsonArray fingerprints = doc.createNestedArray("fingerprints");
  
  int count = 0;
  for (int id = 1; id <= 127; id++) {
    uint8_t result = finger.loadModel(id);
    if (result == FINGERPRINT_OK) {
      JsonObject fp = fingerprints.createNestedObject();
      fp["id"] = id;
      fp["status"] = "stored";
      count++;
    }
  }
  
  doc["total"] = count;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
  
  Serial.println("Fingerprints list sent: " + String(count) + " fingerprints found");
}

void handleEnrollFingerprint() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data received\"}");
    return;
  }
  
  DynamicJsonDocument doc(200);
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  int id = doc["id"];
  
  if (id < 1 || id > 127) {
    server.send(400, "application/json", "{\"error\":\"Invalid ID. Must be between 1 and 127\"}");
    return;
  }
  
  if (!finger.verifyPassword()) {
    server.send(500, "application/json", "{\"error\":\"Fingerprint sensor not connected\"}");
    return;
  }
  
  // Check if ID already exists
  uint8_t result = finger.loadModel(id);
  if (result == FINGERPRINT_OK) {
    server.send(400, "application/json", "{\"error\":\"ID already in use\"}");
    return;
  }
  
  enrollmentInProgress = true;
  enrollmentID = id;
  
  Serial.println("Starting enrollment for ID: " + String(id));
  Serial.println("Please place finger on sensor...");
  
  server.send(200, "application/json", "{\"message\":\"Enrollment started. Please place finger on sensor.\",\"id\":" + String(id) + "}");
}

void handleDeleteFingerprint() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data received\"}");
    return;
  }
  
  DynamicJsonDocument doc(200);
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  int id = doc["id"];
  
  if (!finger.verifyPassword()) {
    server.send(500, "application/json", "{\"error\":\"Fingerprint sensor not connected\"}");
    return;
  }
  
  uint8_t p = finger.loadModel(id);
  if (p != FINGERPRINT_OK) {
    server.send(404, "application/json", "{\"error\":\"Fingerprint not found at ID " + String(id) + "\"}");
    return;
  }
  
  p = finger.deleteModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("Fingerprint ID " + String(id) + " deleted via web interface");
    server.send(200, "application/json", "{\"message\":\"Fingerprint deleted successfully\",\"id\":" + String(id) + "}");
  } else {
    server.send(500, "application/json", "{\"error\":\"Failed to delete fingerprint\",\"code\":" + String(p) + "}");
  }
}

void handleAttendanceMode() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data received\"}");
    return;
  }
  
  DynamicJsonDocument doc(200);
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  bool mode = doc["mode"];
  attendanceMode = mode;
  
  if (attendanceMode) {
    Serial.println("=== ATTENDANCE MODE ACTIVATED VIA WEB ===");
    Serial.println("Ready to scan RFID cards...");
    if (fingerprintMode) {
      Serial.println("Fingerprint verification is ON");
    }
  } else {
    Serial.println("=== ATTENDANCE MODE DEACTIVATED VIA WEB ===");
  }
  
  server.send(200, "application/json", "{\"message\":\"Attendance mode " + String(attendanceMode ? "activated" : "deactivated") + "\",\"mode\":" + String(attendanceMode ? "true" : "false") + "}");
}

void handleGetAttendanceRecords() {
  DynamicJsonDocument doc(2048);
  JsonArray records = doc.createNestedArray("records");
  
  int recordCount = 0;
  for (int i = 0; i < 10; i++) {
    int idx = (recordIndex - 1 - i + 10) % 10;
    if (recentRecords[idx].timestamp.length() > 0) {
      JsonObject record = records.createNestedObject();
      record["timestamp"] = recentRecords[idx].timestamp;
      record["rfidTag"] = recentRecords[idx].rfidTag;
      if (recentRecords[idx].fingerprintID >= 0) {
        record["fingerprintID"] = recentRecords[idx].fingerprintID;
      }
      record["status"] = recentRecords[idx].status;
      recordCount++;
    }
  }
  
  doc["total"] = recordCount;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleEnrollmentProcess() {
  static unsigned long lastStepTime = 0;
  
  if (millis() - lastStepTime > 1000) { // Check every second
    uint8_t result = enrollFingerprintStep(enrollmentID);
    if (result == FINGERPRINT_OK) {
      Serial.println("Enrollment completed successfully for ID: " + String(enrollmentID));
      enrollmentInProgress = false;
    } else if (result == 255) {
      Serial.println("Enrollment failed or cancelled for ID: " + String(enrollmentID));
      enrollmentInProgress = false;
    }
    lastStepTime = millis();
  }
}

void handleAttendanceScanning() {
  static unsigned long lastScanTime = 0;
  
  // Prevent too frequent scanning
  if (millis() - lastScanTime < 1000) {
    return;
  }
  
  String uid = "";
  
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    lastScanTime = millis();
    
    // Read UID
    for (byte i = 0; i < rfid.uid.size; i++) {
      if (rfid.uid.uidByte[i] < 0x10) uid += "0";
      uid += String(rfid.uid.uidByte[i], HEX);
    }
    uid.toUpperCase();
    Serial.println("RFID Card detected - UID: " + uid);
    
    int fingerprintID = -1;
    String status = "success";
    
    // If fingerprint mode is on, wait for fingerprint scan
    if (fingerprintMode) {
      Serial.println("Please place finger on sensor...");
      
      unsigned long startTime = millis();
      boolean timeout = false;
      boolean fingerFound = false;
      
      while (!fingerFound && !timeout) {
        fingerprintID = getFingerprintID();
        if (fingerprintID >= 0) {
          fingerFound = true;
          Serial.println("Fingerprint matched! ID #" + String(fingerprintID));
        }
        
        if (millis() - startTime > 10000) {
          timeout = true;
          Serial.println("Fingerprint scan timeout");
          status = "timeout";
        }
        
        // Handle web requests during fingerprint waiting
        server.handleClient();
        delay(100);
      }
    }
    
    // Store attendance record
    recentRecords[recordIndex].timestamp = String(millis());
    recentRecords[recordIndex].rfidTag = uid;
    recentRecords[recordIndex].fingerprintID = fingerprintID;
    recentRecords[recordIndex].status = status;
    recordIndex = (recordIndex + 1) % 10;
    
    // Send attendance data to external server
    if (WiFi.status() == WL_CONNECTED) {
      sendAttendanceData(uid, fingerprintID);
    } else {
      Serial.println("WiFi disconnected");
    }
    
    rfid.PICC_HaltA();
    Serial.println("Ready for next scan...");
  }
}

// Simplified enrollment function for web interface
uint8_t enrollFingerprintStep(int id) {
  // This is a simplified version - in a real implementation,
  // you'd want to implement a proper state machine for enrollment
  return enrollFingerprint(id);
}

uint8_t enrollFingerprint(int id) {
  if (!finger.verifyPassword()) {
    return 255;
  }
  
  Serial.print("Enrolling ID #"); 
  Serial.println(id);
  
  int p = -1;
  
  // First image capture
  Serial.println("Place finger on sensor...");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
        delay(100);
        break;
      default:
        Serial.println("Error capturing fingerprint");
        return p;
    }
    // Handle web requests during enrollment
    server.handleClient();
  }
  
  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) {
    Serial.println("Error converting image.");
    return p;
  }
  
  // Check for duplicates
  Serial.println("Checking for duplicate fingerprints...");
  p = finger.fingerSearch(1);
  
  if (p == FINGERPRINT_OK) {
    Serial.print("This fingerprint already exists at ID #");
    Serial.print(finger.fingerID);
    Serial.println(" Cannot enroll duplicate fingerprint.");
    return 255;
  } else if (p == FINGERPRINT_NOTFOUND) {
    Serial.println("No duplicate found. Continuing enrollment...");
  } else {
    Serial.print("Error during fingerprint search: ");
    Serial.println(p);
    return p;
  }
  
  // Second image capture
  Serial.println("Remove finger...");
  delay(2000);
  while (finger.getImage() != FINGERPRINT_NOFINGER) {
    server.handleClient(); // Handle web requests
    delay(100);
  }
  
  Serial.println("Place same finger again...");
  p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
        delay(100);
        break;
      default:
        Serial.println("Error capturing fingerprint");
        return p;
    }
    // Handle web requests during enrollment
    server.handleClient();
  }
  
  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    Serial.println("Error converting second image.");
    return p;
  }
  
  // Create and store model
  p = finger.createModel();
  if (p != FINGERPRINT_OK) {
    Serial.println("Fingerprints did not match.");
    return p;
  }
  
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("Successfully stored fingerprint!");
    return FINGERPRINT_OK;
  } else {
    Serial.println("Error storing fingerprint.");
    return p;
  }
}

int getFingerprintID() {
  int p = finger.getImage();
  
  if (p != FINGERPRINT_OK) {
    return -1;
  }
  
  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) {
    return -1;
  }
  
  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK) {
    return -1;
  }
  
  return finger.fingerID;
}

void sendAttendanceData(String uid, int fingerprintID) {
  WiFiClient client;
  HTTPClient http;
  
  Serial.print("Connecting to: ");
  Serial.println(serverURL);
  
  http.begin(client, serverURL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", apiKey);
  
  String payload;
  if (fingerprintID >= 0) {
    payload = "{\"rfidTag\":\"" + uid + "\",\"fingerprintID\":\"" + String(fingerprintID) + "\",\"apiKey\":\"" + String(apiKey) + "\"}";
  } else {
    payload = "{\"rfidTag\":\"" + uid + "\",\"apiKey\":\"" + String(apiKey) + "\"}";
  }
  
  Serial.print("Sending payload: ");
  Serial.println(payload);
  
  http.setTimeout(10000);
  int httpCode = http.POST(payload);
  
  if (httpCode > 0) {
    if (httpCode >= 200 && httpCode < 300) {
      Serial.println("✓ Access Registered");
    } else if (httpCode >= 400) {
      Serial.println("✗ Access Denied");
    } else {
      Serial.print("HTTP Response Code: ");
      Serial.println(httpCode);
    }
    
    String response = http.getString();
    Serial.println("Server response: " + response);
  } else {
    Serial.print("HTTP request failed, error: ");
    Serial.println(http.errorToString(httpCode).c_str());
  }
  
  http.end();
}
