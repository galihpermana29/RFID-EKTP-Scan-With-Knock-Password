#include <AvrI2c_Greiman.h>
#include <LiquidCrystal_I2C_AvrI2C.h>

//Viral Science www.youtube.com/c/viralscience  www.viralsciencecreativity.com
//Secret Knock Pattern Door Lock
#include <SPI.h>
#include <MFRC522.h>
//#include <LiquidCrystal_I2C.h> // Library for LCD

// Wiring: SDA pin is connected to A4 and SCL pin to A5.
// Connect to LCD via I2C, default address 0x27 (A0-A2 not jumpered)
#define RST_PIN         9          // Configurable, see typical pin layout above
#define SS_PIN          10         // Configurable, see typical pin layout above

LiquidCrystal_I2C_AvrI2C lcd(0x27, 16, 2); // Change to (0x27,20,4) for 20x4 LCD.
MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance
bool trig = false;

// index 0 nik, 1 status vaksin, 2 admin
const String data[2][3] = {{"05 86 C6 3C AC 91 00", "1", "1"}, {"05 8C 48 A6 2E A1 00", "0", "0"}};

const int knockSensor = A0;
const int programSwitch = 6;
const int lockMotor = A2;
const int redLED = 4;
const int greenLED = 7;

const int threshold = 10;
const int rejectValue = 25;
const int averageRejectValue = 15;
const int knockFadeTime = 150;
const int lockTurnTime = 5000;

int maximumLimit = 3;
const int maximumKnocks = 20;
const int knockComplete = 1200;

int secretCode[maximumKnocks] = {50, 25, 25, 50, 100, 50, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int knockReadings[maximumKnocks];
int knockSensorValue = 0;
int programButtonPressed = false;

bool locked = true;

void setup() {
  pinMode(lockMotor, OUTPUT);
  pinMode(redLED, OUTPUT);
  pinMode(greenLED, OUTPUT);
  pinMode(programSwitch, INPUT_PULLUP);
  digitalWrite(lockMotor, HIGH);

  Serial.begin(9600);
  lcd.begin();
  lcd.backlight();
  Serial.println("Program start.");
  Serial.println(analogRead(knockSensor));
  digitalWrite(greenLED, HIGH);
  while (!Serial);    // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)
  SPI.begin();      // Init SPI bus
  mfrc522.PCD_Init();   // Init MFRC522
  delay(4);       // Optional delay. Some board do need more time after init to be ready, see Readme
  mfrc522.PCD_DumpVersionToSerial();  // Show details of PCD - MFRC522 Card Reader details
  Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));
  lcd.setCursor(0, 0); // column, row
  lcd.print(" Scan Your RFID ");
  lcd.setCursor(0, 1); // column, row
  lcd.print("   Door Locked   ");
}

void loop() {
  maximumLimit = 3;
  String content = "";
  locked = true;
  if ( mfrc522.PICC_IsNewCardPresent()) {
    if ( mfrc522.PICC_ReadCardSerial()) {
      trig = true;
      byte letter;
      for (byte i = 0; i < mfrc522.uid.size; i++) {
        content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
        content.concat(String(mfrc522.uid.uidByte[i], HEX));
      }
    }
    //delay percobaan
    delay(1000);
  }

  content.toUpperCase();
  String NIK = "";
  if (content != "") {
    Serial.println(content);
    for (int i = 0; i < 2; i++) {
      for (int j = 0; j < 3; j++) {
        if (content.substring(1) == data[i][0] && locked) {
          // cek if the user is vaccined
          if (data[i][1] == "1" && locked) {
            Serial.println("User Has Vaccined");
            lcd.clear();
            lcd.setCursor(4, 0);
            lcd.print("User Has");
            lcd.setCursor(4, 1);
            lcd.print("Vaccined");
            while (true && locked) {
              knockSensorValue = analogRead(knockSensor);
              if (digitalRead(programSwitch) == LOW) {
                programButtonPressed = true;
                digitalWrite(redLED, HIGH);
              } else {
                programButtonPressed = false;
                digitalWrite(redLED, LOW);
              }

              if (knockSensorValue >= threshold) {
                listenToSecretKnock();
              }
            }
          } else {
            Serial.println("User Hasn't Vaccined");
            lcd.clear();
            lcd.setCursor(3, 0);
            lcd.print("User Hasn't");
            lcd.setCursor(4, 1);
            lcd.print("Vaccined");
            delay(500);
            lcd.clear();
            lcd.setCursor(1, 0);
            lcd.print("Access Denied!");
          }
        }
      }
    }
  }
}


void listenToSecretKnock() {
  Serial.println("knock starting");
  lcd.clear();
  lcd.setCursor(2, 0);
  lcd.print("Start Knock");
  int i = 0;
  for (i = 0; i < maximumKnocks; i++) {
    knockReadings[i] = 0;
  }

  int currentKnockNumber = 0;
  int startTime = millis();
  int now = millis();

  digitalWrite(greenLED, LOW);
  if (programButtonPressed == true) {
    digitalWrite(redLED, LOW);
  }
  delay(knockFadeTime);
  digitalWrite(greenLED, HIGH);
  if (programButtonPressed == true) {
    digitalWrite(redLED, HIGH);
  }
  do {
    knockSensorValue = analogRead(knockSensor);
    if (knockSensorValue >= threshold) {
      Serial.println("knock.");
      Serial.println(knockSensorValue);
      Serial.println(threshold);
      now = millis();
      knockReadings[currentKnockNumber] = now - startTime;
      currentKnockNumber ++;
      startTime = now;
      digitalWrite(greenLED, LOW);
      if (programButtonPressed == true) {
        digitalWrite(redLED, LOW);
      }
      delay(knockFadeTime);
      digitalWrite(greenLED, HIGH);
      if (programButtonPressed == true) {
        digitalWrite(redLED, HIGH);
      }
    }

    now = millis();

  } while ((now - startTime < knockComplete) && (currentKnockNumber < maximumKnocks));

  if (programButtonPressed == false) {
    if (validateKnock() == true) {
      triggerDoorUnlock();
    } else {
      maximumLimit--;
      if (maximumLimit == 0) {
        locked = false;
        Serial.println("maximum access! Re-scan your ktp");        delay(2000);
        lcd.setCursor(0, 0);
        lcd.print("Maximum Access!");
        lcd.setCursor(0, 1);
        lcd.print("Rescan your KTP");
        delay(2000);
      }
      Serial.println("Secret knock failed.");
      lcd.clear();
      lcd.setCursor(2, 0);
      lcd.print("Secret Knock");
      lcd.setCursor(5, 1);
      lcd.print("Failed!");
      digitalWrite(greenLED, LOW);
      for (i = 0; i < 4; i++) {
        digitalWrite(redLED, HIGH);
        delay(100);
        digitalWrite(redLED, LOW);
        delay(100);
      }
      digitalWrite(greenLED, HIGH);
    }
  } else {
    validateKnock();
    Serial.println("New lock stored.");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("New Lock Stored!");
    digitalWrite(redLED, LOW);
    digitalWrite(greenLED, HIGH);
    for (i = 0; i < 3; i++) {
      delay(100);
      digitalWrite(redLED, HIGH);
      digitalWrite(greenLED, LOW);
      delay(100);
      digitalWrite(redLED, LOW);
      digitalWrite(greenLED, HIGH);
    }
  }
}


void triggerDoorUnlock() {
  Serial.println("Door unlocked!");
  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print("Door Unlocked!");
  locked = false;
  int i = 0;

  digitalWrite(lockMotor, LOW);
  digitalWrite(greenLED, HIGH);

  delay (lockTurnTime);
  lcd.setCursor(0, 0); // column, row
  lcd.print(" Scan Your RFID ");
  lcd.setCursor(0, 1); // column, row
  lcd.print("   Door Locked   ");
  digitalWrite(lockMotor, HIGH);

  for (i = 0; i < 5; i++) {
    digitalWrite(greenLED, LOW);
    delay(100);
    digitalWrite(greenLED, HIGH);
    delay(100);
  }

}

boolean validateKnock() {
  int i = 0;

  int currentKnockCount = 0;
  int secretKnockCount = 0;
  int maxKnockInterval = 0;

  for (i = 0; i < maximumKnocks; i++) {
    if (knockReadings[i] > 0) {
      currentKnockCount++;
    }
    if (secretCode[i] > 0) {
      secretKnockCount++;
    }

    if (knockReadings[i] > maxKnockInterval) {
      maxKnockInterval = knockReadings[i];
    }
  }

  if (programButtonPressed == true) {
    for (i = 0; i < maximumKnocks; i++) {
      secretCode[i] = map(knockReadings[i], 0, maxKnockInterval, 0, 100);
    }
    digitalWrite(greenLED, LOW);
    digitalWrite(redLED, LOW);
    delay(1000);
    digitalWrite(greenLED, HIGH);
    digitalWrite(redLED, HIGH);
    delay(50);
    for (i = 0; i < maximumKnocks ; i++) {
      digitalWrite(greenLED, LOW);
      digitalWrite(redLED, LOW);
      if (secretCode[i] > 0) {
        delay( map(secretCode[i], 0, 100, 0, maxKnockInterval));
        digitalWrite(greenLED, HIGH);
        digitalWrite(redLED, HIGH);
      }
    }
    return false;
  }

  if (currentKnockCount != secretKnockCount) {
    return false;
  }
  int totaltimeDifferences = 0;
  int timeDiff = 0;
  for (i = 0; i < maximumKnocks; i++) {
    knockReadings[i] = map(knockReadings[i], 0, maxKnockInterval, 0, 100);
    timeDiff = abs(knockReadings[i] - secretCode[i]);
    if (timeDiff > rejectValue) {
      return false;
    }
    totaltimeDifferences += timeDiff;
  }
  if (totaltimeDifferences / secretKnockCount > averageRejectValue) {
    return false;
  }

  return true;

}
