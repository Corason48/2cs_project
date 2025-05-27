#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>

// RFID pin configuration
#define RST_PIN D3  // RST to D3 (GPIO0)
#define SS_PIN D4   // SDA to D4 (GPIO2)
MFRC522 rfid(SS_PIN, RST_PIN);

// Fingerprint sensor configuration - Try different pins
#define FINGERPRINT_RX D1  // Connect to TX on fingerprint sensor
#define FINGERPRINT_TX D2  // Connect to RX on fingerprint sensor
SoftwareSerial fingerprintSerial(FINGERPRINT_RX, FINGERPRINT_TX);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&fingerprintSerial);

// WiFi credentials
const char* ssid = "Galaxy S9+3b92";
const char* password = "00000000";

// Server URL and API key (using HTTP, not HTTPS)
const char* serverURL = "http://82.112.241.179:8000/attendances";
const char* apiKey = "b3f65ecc04820ba6cdd0a3d0ac9a4f57162c6c9a06efac5ceca41e5f6e3ae956";

// Operation mode
bool fingerprintMode = true;  // Set to false if you want to use only RFID mode initially

void setup() {
  Serial.begin(9600);
  
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
  fingerprintSerial.begin(57600);  // Start SoftwareSerial first
  delay(500);
  finger.begin(57600);  // JM-101 typically uses 57600 baud
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
  
  // Set default operating mode if sensor not found
  if (!finger.verifyPassword()) {
    Serial.println("Continuing without fingerprint sensor. Switching to RFID-only mode.");
    fingerprintMode = false;
  }
  
  // Set fingerprint parameters
  if (fingerprintMode) {
    finger.getParameters();
    Serial.print("Status: 0x"); Serial.println(finger.status_reg, HEX);
    Serial.print("System ID: 0x"); Serial.println(finger.system_id, HEX);
    Serial.print("Capacity: "); Serial.println(finger.capacity);
    Serial.print("Security level: "); Serial.println(finger.security_level);
  }
  
  Serial.println("System ready!");
  Serial.println("Press 'F' to toggle Fingerprint mode (currently " + String(fingerprintMode ? "ON" : "OFF") + ")");
  Serial.println("Ready to scan...");
}

void loop() {
  // Check if user wants to toggle fingerprint mode
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'F' || c == 'f') {
      fingerprintMode = !fingerprintMode;
      Serial.println("Fingerprint mode " + String(fingerprintMode ? "enabled" : "disabled"));
    }
    // Add command to test the fingerprint sensor
    else if (c == 'T' || c == 't') {
      Serial.println("Testing fingerprint sensor connection...");
      if (finger.verifyPassword()) {
        Serial.println("Fingerprint sensor is connected and responding!");
      } else {
        Serial.println("Fingerprint sensor not responding. Check wiring!");
      }
    }
    // Add command to enroll fingerprint
    else if (c == 'E' || c == 'e') {
      enrollFingerprint();
    }
  }
  
  // Check for RFID cards
  String uid = "";
  bool cardDetected = false;
  
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    cardDetected = true;
    // Read UID
    for (byte i = 0; i < rfid.uid.size; i++) {
      if (rfid.uid.uidByte[i] < 0x10) uid += "0";
      uid += String(rfid.uid.uidByte[i], HEX);
    }
    uid.toUpperCase();
    Serial.println("RFID Card detected - UID: " + uid);
    
    int fingerprintID = -1;
    
    // If fingerprint mode is on, wait for fingerprint scan
    if (fingerprintMode) {
      Serial.println("Please place finger on sensor...");
      
      // Wait for fingerprint with a reasonable timeout
      unsigned long startTime = millis();
      boolean timeout = false;
      boolean fingerFound = false;
      
      while (!fingerFound && !timeout) {
        fingerprintID = getFingerprintID();
        if (fingerprintID >= 0) {
          fingerFound = true;
          Serial.println("Fingerprint matched! ID #" + String(fingerprintID));
        }
        
        // Check for timeout (10 seconds)
        if (millis() - startTime > 10000) {
          timeout = true;
          Serial.println("Fingerprint scan timeout");
        }
        
        delay(100);  // Small delay to prevent tight loop
      }
      
      // Send data based on detection results
      if (WiFi.status() == WL_CONNECTED) {
        sendAttendanceData(uid, fingerprintID);
      } else {
        Serial.println("WiFi disconnected");
      }
    } else {
      // RFID only mode
      if (WiFi.status() == WL_CONNECTED) {
        sendAttendanceData(uid, -1);
      } else {
        Serial.println("WiFi disconnected");
      }
    }
    
    delay(1000);
    rfid.PICC_HaltA();
  }
  
  // If set to fingerprint-only mode, we could add a standalone fingerprint check here
  // For now we're assuming RFID is required and fingerprint is optional
}

// Get fingerprint ID (returns -1 if no match)
int getFingerprintID() {
  Serial.println("Getting fingerprint image...");
  int p = finger.getImage();
  
  if (p != FINGERPRINT_OK) {
    // Only print error if it's not "no finger detected"
    if (p != FINGERPRINT_NOFINGER) {
      Serial.print("Error getting image: ");
      switch (p) {
        case FINGERPRINT_PACKETRECIEVEERR: Serial.println("Communication error"); break;
        case FINGERPRINT_IMAGEFAIL: Serial.println("Imaging error"); break;
        default: Serial.println("Unknown error"); break;
      }
    }
    return -1;
  }
  
  // Image taken, now convert
  Serial.println("Image taken, converting...");
  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) {
    Serial.print("Image conversion error: ");
    switch (p) {
      case FINGERPRINT_IMAGEMESS: Serial.println("Image too messy"); break;
      case FINGERPRINT_PACKETRECIEVEERR: Serial.println("Communication error"); break;
      case FINGERPRINT_FEATUREFAIL: Serial.println("Couldn't find features"); break;
      case FINGERPRINT_INVALIDIMAGE: Serial.println("Invalid image"); break;
      default: Serial.println("Unknown error"); break;
    }
    return -1;
  }
  
  // Search for fingerprint
  Serial.println("Searching for matching fingerprint...");
  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK) {
    Serial.print("Search error: ");
    switch (p) {
      case FINGERPRINT_PACKETRECIEVEERR: Serial.println("Communication error"); break;
      case FINGERPRINT_NOTFOUND: Serial.println("No match found"); break;
      default: Serial.println("Unknown error"); break;
    }
    return -1;
  }
  
  // Found a match!
  Serial.println("Found fingerprint match!");
  Serial.print("ID #"); Serial.println(finger.fingerID);
  return finger.fingerID;
}

// Function to enroll new fingerprints (can be called from setup or via serial command)
uint8_t enrollFingerprint() {
  int p = -1;
  Serial.println("Ready to enroll a fingerprint!");
  Serial.println("Please type in the ID # (1-127) you want to save this finger as...");
  
  int id = 0;
  while (id == 0) {
    while (!Serial.available());
    id = Serial.parseInt();
    if (id < 1 || id > 127) {
      Serial.println("ID must be between 1-127");
      id = 0;
    }
  }
  
  Serial.println("Enrolling ID #" + String(id));
  
  // First capture
  Serial.println("Place your finger on the sensor...");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
        Serial.print(".");
        delay(100);
        break;
      default:
        Serial.println("Error in fingerprint capture");
        return p;
    }
  }
  
  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) {
    Serial.println("Image conversion failed");
    return p;
  }
  
  Serial.println("Remove finger");
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }
  
  // Second capture
  Serial.println("Place same finger again...");
  p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
        Serial.print(".");
        delay(100);
        break;
      default:
        Serial.println("Error in fingerprint capture");
        return p;
    }
  }
  
  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    Serial.println("Image conversion failed");
    return p;
  }
  
  // Create model
  p = finger.createModel();
  if (p != FINGERPRINT_OK) {
    Serial.println("Failed to create fingerprint model");
    return p;
  }
  
  // Store model
  p = finger.storeModel(id);
  if (p != FINGERPRINT_OK) {
    Serial.println("Failed to store fingerprint model");
    return p;
  }
  
  Serial.println("Success! Fingerprint enrolled!");
  return FINGERPRINT_OK;
}

void testServerConnectivity() {
  // Use a simple GET request to test server connectivity
  WiFiClient client;
  HTTPClient http;
  
  Serial.println("Testing server connectivity...");
  
  // Strip the path from the URL to test basic connectivity
  String testURL = "http://82.112.241.179:8000/";
  
  http.begin(client, testURL);
  int httpCode = http.GET();
  
  if (httpCode > 0) {
    Serial.print("Server responded with code: ");
    Serial.println(httpCode);
  } else {
    Serial.print("Server connection test failed, error: ");
    Serial.println(http.errorToString(httpCode).c_str());
  }
  
  http.end();
}

void sendAttendanceData(String uid, int fingerprintID) {
  // Use regular WiFiClient for HTTP
  WiFiClient client;
  HTTPClient http;
  
  Serial.print("Connecting to: ");
  Serial.println(serverURL);
  
  http.begin(client, serverURL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", apiKey);
  
  String payload;
  if (fingerprintID >= 0) {
    // Send both RFID and fingerprint data
    payload = "{\"rfidTag\":\"" + uid + "\",\"fingerprintID\":\"" + String(fingerprintID) + "\",\"apiKey\":\"" + String(apiKey) + "\"}";
  } else {
    // Send only RFID data
    payload = "{\"rfidTag\":\"" + uid + "\",\"apiKey\":\"" + String(apiKey) + "\"}";
  }
  
  Serial.print("Sending payload: ");
  Serial.println(payload);
  
  // Set timeout to give more time for the connection
  http.setTimeout(10000); // 10 seconds timeout
  
  int httpCode = http.POST(payload);
  
  // Handle response based on status code
  if (httpCode > 0) {
    if (httpCode >= 200 && httpCode < 300) {
      Serial.println("Access Registered");
    } else if (httpCode >= 400) {
      Serial.println("Access Denied");
    } else {
      Serial.print("HTTP Response Code: ");
      Serial.println(httpCode);
    }
    
    // For debugging, print the server response
    String response = http.getString();
    Serial.println("Server response: " + response);
  } else {
    Serial.println(httpCode);
    Serial.print("HTTP request failed, error: ");
    Serial.println(http.errorToString(httpCode).c_str());
  }
  
  http.end();
}
