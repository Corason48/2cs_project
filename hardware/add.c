#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>

// Set up software serial to communicate with the fingerprint sensor
SoftwareSerial mySerial(D1, D2); // RX, TX
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

void setup() {
  Serial.begin(9600);
  while (!Serial);  // Wait for serial
  delay(100);

  finger.begin(57600);
  if (finger.verifyPassword()) {
    Serial.println("Fingerprint sensor detected!");
  } else {
    Serial.println("Did not find fingerprint sensor :(");
    while (1) { delay(1); }
  }
}

void loop() {
  Serial.println("\nType an ID (1 - 127) to enroll this fingerprint:");
  while (!Serial.available());
  int id = Serial.parseInt();
  while (Serial.available()) Serial.read(); // Clear input

  if (id < 1 || id > 127) {
    Serial.println("Invalid ID. Must be between 1 and 127.");
    return;
  }

  enrollFingerprint(id);
}

void enrollFingerprint(int id) {
  int p;
  
  // Check if the ID slot is already occupied
  uint8_t result = finger.loadModel(id);
  if (result == FINGERPRINT_OK) {
    Serial.print("ID #");
    Serial.print(id);
    Serial.println(" is already in use. Please choose a different ID.");
    return;
  }

  Serial.print("Enrolling ID #"); Serial.println(id);
  delay(1000);

  // Step 1: Get image
  Serial.println("Place finger on sensor...");
  while ((p = finger.getImage()) != FINGERPRINT_OK);

  // Step 2: Convert image to characteristics and store in buffer 1
  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) {
    Serial.println("Error converting image.");
    return;
  }

  // Check if this fingerprint already exists in the database
  Serial.println("Checking for duplicate fingerprints...");
  p = finger.fingerSearch(1); // Search using buffer 1 which contains our current fingerprint
  
  if (p == FINGERPRINT_OK) {
    Serial.print("This fingerprint already exists at ID #");
    Serial.print(finger.fingerID);
    Serial.print(" with confidence: ");
    Serial.println(finger.confidence);
    Serial.println("Cannot enroll duplicate fingerprint.");
    return;
  } else if (p == FINGERPRINT_NOTFOUND) {
    Serial.println("No duplicate found. Continuing enrollment...");
  } else {
    Serial.print("Error during fingerprint search: ");
    Serial.println(p);
    return;
  }

  // Step 3: Remove and place again
  Serial.println("Remove finger...");
  delay(2000);
  while (finger.getImage() != FINGERPRINT_NOFINGER);
  Serial.println("Place same finger again...");

  while ((p = finger.getImage()) != FINGERPRINT_OK);
  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    Serial.println("Error converting second image.");
    return;
  }

  // Step 4: Create model and store it
  p = finger.createModel();
  if (p != FINGERPRINT_OK) {
    Serial.println("Fingerprint did not match.");
    return;
  }

  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("Successfully stored fingerprint!");
  } else {
    Serial.println("Error storing fingerprint.");
  }
}
