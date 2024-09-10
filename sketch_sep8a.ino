#include <BluetoothSerial.h>
#include <IRremote.h>
#include <DHT.h>
#include <ESP32Servo.h>
#include <Wire.h>

BluetoothSerial SerialBT;

int lampu[] = {14, 25, 26, 27};
int tombol[] = {32, 33, 34, 35};
int lampuState[] = {0, 0, 0, 0};

int ledPin = 2;

int IR_PIN = 4;
IRrecv irrecv(IR_PIN);
decode_results results;

#define DHTPIN 15
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

Servo servoMotor;
const int trigPin = 5;
const int echoPin = 18;
int servoPin = 23;
long duration;
int distance;
int thresholdDistance = 20;
bool bendaTerdeteksi = false;

void kontrolLampu(int index, int status) {
  digitalWrite(lampu[index], status);
  lampuState[index] = status;

  if (status == LOW) {
    Serial.print("Lampu ");
    Serial.print(index);
    Serial.println(": ON (Relay aktif)");
    digitalWrite(ledPin, HIGH);
  } else {
    Serial.print("Lampu ");
    Serial.print(index);
    Serial.println(": OFF (Relay mati)");
    digitalWrite(ledPin, LOW);
  }

  SerialBT.print("LAMP ");
  SerialBT.print(index);
  SerialBT.println(status == LOW ? ":on" : ":off");
}

void setup() {
  Serial.begin(115200);
  Serial.println("Inisialisasi...");

  if (!SerialBT.begin("ESP32_SmartHome_4CH")) {
    Serial.println("Bluetooth gagal diinisialisasi");
  } else {
    Serial.println("Bluetooth siap dan mendengarkan");
  }

  for (int i = 0; i < 4; i++) {
    pinMode(lampu[i], OUTPUT);
    pinMode(tombol[i], INPUT_PULLUP);
  }
  
  pinMode(ledPin, OUTPUT);

  dht.begin();
  irrecv.enableIRIn();
  Serial.println("System Siap dengan 4 channel.");

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  servoMotor.attach(servoPin);
  servoMotor.write(0);
}

void kontrolRemote() {
  if (irrecv.decode(&results)) {
    Serial.print("Kode IR Diterima: 0x");
    Serial.println(results.value, HEX);

    if (results.value == 0xFFA25D) {
      kontrolLampu(0, !lampuState[0]);
    } else if (results.value == 0xFF629D) {
      kontrolLampu(1, !lampuState[1]);
    } else if (results.value == 0xFFE21D) {
      kontrolLampu(2, !lampuState[2]);
    } else if (results.value == 0xFF22DD) {
      kontrolLampu(3, !lampuState[3]);
    } else if (results.value == 0xFF02FD) {
      for (int i = 0; i < 4; i++) {
        kontrolLampu(i, HIGH);
      }
    } else if (results.value != 0xFFFFFFFF) {
      Serial.println("Kode IR tidak dikenal.");
    }

    irrecv.resume();
  }
}

void kontrolServo() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH);
  distance = duration * 0.034 / 2;

  if (distance > 0 && distance < thresholdDistance) {
    servoMotor.write(90);
    Serial.println("Benda terdeteksi! Servo bergerak ke 90 derajat.");
  } else {
    servoMotor.write(0);
  }

  delay(500);
}

void prosesPerintah(String command) {
    command.trim();
    Serial.print("Menerima perintah: ");
    Serial.println(command);
    
    if (command == "GET_TEMP") {
        float suhu = dht.readTemperature();
        float kelembaban = dht.readHumidity();

        Serial.println("Memproses perintah GET_TEMP...");

        if (!isnan(suhu) && !isnan(kelembaban)) {
            Serial.println("Data suhu dan kelembaban berhasil dibaca.");
            Serial.print("TEMP:");
            Serial.print(suhu, 1);
            Serial.print(" HUM:");
            Serial.println(kelembaban, 1);
            SerialBT.print("TEMP:");
            SerialBT.print(suhu, 1);
            SerialBT.print(" HUM:");
            SerialBT.println(kelembaban, 1);
        } else {
            Serial.println("Gagal membaca data dari sensor DHT!");
        }
    }
    else if (command.startsWith("LAMP_ON_")) {
        int index = command.substring(8).toInt();
        if (index >= 0 && index < 4) {
            Serial.print("Memproses perintah LAMP_ON untuk lampu ");
            Serial.println(index);
            kontrolLampu(index, LOW);
        }
    }
    else if (command.startsWith("LAMP_OFF_")) {
        int index = command.substring(9).toInt();
        if (index >= 0 && index < 4) {
            Serial.print("Memproses perintah LAMP_OFF untuk lampu ");
            Serial.println(index);
            kontrolLampu(index, HIGH);
        }
    }
}

void kontrolBluetooth() {
  if (SerialBT.available()) {
    String command = SerialBT.readString();
    prosesPerintah(command);
  }
}

void kontrolSerial() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    Serial.print("Command dari Serial Monitor: ");
    Serial.println(command);
    prosesPerintah(command);
  }
}

void kontrolTombol() {
  static unsigned long lastDebounceTime[4] = {0, 0, 0, 0};
  static int lastButtonState[4] = {HIGH, HIGH, HIGH, HIGH};
  int buttonState;

  for (int i = 0; i < 4; i++) {
    buttonState = digitalRead(tombol[i]);
    
    if (buttonState != lastButtonState[i]) {
      lastDebounceTime[i] = millis();
    }
    
    if ((millis() - lastDebounceTime[i]) > 50) {  
      if (buttonState == LOW) {
        kontrolLampu(i, !lampuState[i]);
        while (digitalRead(tombol[i]) == LOW);  
      }
    }

    lastButtonState[i] = buttonState;
  }
}


void loop() {
  kontrolRemote();
  kontrolBluetooth();
  kontrolSerial();
  kontrolTombol();
  kontrolServo();

  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 2000) {
    float suhu = dht.readTemperature();
    float kelembaban = dht.readHumidity();

    lastUpdate = millis();
  }
}
