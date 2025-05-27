#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>

SoftwareSerial mySerial(D1, D2);  // RX, TX
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

void setup() {
  Serial.begin(9600);
  while (!Serial);  // Wait for Serial Monitor
  delay(2000);

  finger.begin(57600);
  if (finger.verifyPassword()) {
    Serial.println("Fingerprint sensor found.");
  } else {
    Serial.println("Fingerprint sensor not found.");
    while (1) { delay(1); }
  }

  listFingerprints();
  listFingerprints();
  listFingerprints();
  listFingerprints();
}

void loop() {
  // Nothing to do in loop
}

void listFingerprints() {
  Serial.println("Stored fingerprints:");

  uint8_t count = 0;
  for (int id = 1; id < 128; id++) {
    uint8_t result = finger.loadModel(id);
    if (result == FINGERPRINT_OK) {
      Serial.print("ID "); Serial.print(id); Serial.println(" is stored.");
      count++;
    }
  }

  if (count == 0) {
    Serial.println("No fingerprints stored.");
  } else {
    Serial.print("Total stored fingerprints: ");
    Serial.println(count);
  }
}

