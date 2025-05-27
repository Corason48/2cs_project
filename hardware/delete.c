#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>

SoftwareSerial mySerial(D1, D2); // RX, TX
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

void setup() {
  Serial.begin(9600);
  while (!Serial);  // Wait for serial monitor
  delay(100);
  
  finger.begin(57600);
  if (finger.verifyPassword()) {
    Serial.println("Fingerprint sensor detected!");
  } else {
    Serial.println("Fingerprint sensor not found :(");
    while (1) { delay(1); }
  }
}

void loop() {
  Serial.println("\nEnter fingerprint ID to delete (1 - 127):");
  while (!Serial.available());
  int id = Serial.parseInt();
  while (Serial.available()) Serial.read(); // Clear serial buffer
  
  if (id < 1 || id > 127) {
    Serial.println("Invalid ID. Must be between 1 and 127.");
    return;
  }
  
  deleteFingerprint(id);
  delay(2000); // Small delay before next input
}

void deleteFingerprint(int id) {
  // First check if fingerprint exists at this ID
  Serial.print("Checking if fingerprint exists at ID #");
  Serial.println(id);
  
  uint8_t p = finger.loadModel(id);
  if (p != FINGERPRINT_OK) {
    Serial.print("No fingerprint found at ID #");
    Serial.println(id);
    return;
  }
  
  Serial.print("Fingerprint found at ID #");
  Serial.print(id);
  Serial.println(". Deleting...");
  
  // Now delete the fingerprint
  p = finger.deleteModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("Fingerprint deleted successfully!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error.");
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Could not delete - bad location.");
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash.");
  } else {
    Serial.print("Unknown error: "); 
    Serial.println(p);
  }
}
