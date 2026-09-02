#include <SoftwareSerial.h>

const int MY_NODE_ID = 1;

// RS485 Sériová linka (D2 = RX/RO, D3 = TX/DI) - automatické riadenie smeru
SoftwareSerial RS485Serial(2, 3); 

// Analógové piny pre dva NTC 100k termistory
#define PIN_NTC_MCU A6   // T1: Teplota elektroniky Arduina
#define PIN_NTC_RELAY A7 // T2: Teplota relé zostavy

// Hardvérové priradenie 16 relé pinov (Riešenie A)
const int pinsGroup1[] = {8, 5, A3, A4, A0, 4, 9, 12 };          // On/Off (C stav)
const int pinsGroup2[] = {7, 6, A2, A5, A1, 13, 10, 11 };      // Smerovanie (A / B stav)

char currentStates[] = {'C', 'C', 'C', 'C', 'C', 'C', 'C', 'C'};

// Globálne diagnostické premenné pre teploty
float tempMCU = 0.0;
float tempRelay = 0.0;

void setup() {
  Serial.begin(115200);
  Serial.println("[START] Arduino Nano Uzol s duálnym meraním teplôt pripravený.");

  RS485Serial.begin(9600);

  // Inicializácia 16 relé
  for (int i = 0; i < 8; i++) {
    pinMode(pinsGroup1[i], OUTPUT);
    pinMode(pinsGroup2[i], OUTPUT);
    digitalWrite(pinsGroup1[i], HIGH); // Low-level trigger, predvolene rozopnuté
    digitalWrite(pinsGroup2[i], HIGH);
  }
  
  applyRelayStates();
}

void loop() {
  // Priebežne prepočítavame obe teploty
  readDualTeperatures();

  if (RS485Serial.available() > 0) {
    String msg = RS485Serial.readStringUntil('\n');
    msg.trim();
    if (msg.length() > 0) {
      parseIncomingCommand(msg);
    }
  }
}

// Pomocná funkcia na prepočet ADC hodnoty na stupne Celzia (100k NTC + 100k Pull-up)
float calculateCelsius(int adcVal) {
  // Bezpečnostná kontrola skratu alebo odpojenia drôtu (Error stav)
  if (adcVal <= 15 || adcVal >= 1008) {
    return 999.0; 
  }

  // Prepočet ADC hodnoty na odpor termistora
  float resistance = 100000.0 * (1023.0 / (float)adcVal - 1.0);
  
  // Steinhart-Hartova rovnica pre 100k NTC (Beta koeficient = 3950, Ro = 100k pri 25°C)
  float steinhart;
  steinhart = resistance / 100000.0;     // (R/Ro)
  steinhart = log(steinhart);             // ln(R/Ro)
  steinhart /= 3950.0;                    // 1/B * ln(R/Ro)
  steinhart += 1.0 / (25.0 + 273.15);     // + (1/To)
  steinhart = 1.0 / steinhart;            // Prevrátená hodnota pre Kelviny
  return steinhart - 273.15;              // Prepočet na Celzie
}

// Vyčítanie oboch analógových kanálov
void readDualTeperatures() {
  tempMCU = calculateCelsius(analogRead(PIN_NTC_MCU));
  tempRelay = calculateCelsius(analogRead(PIN_NTC_RELAY));
}

void parseIncomingCommand(String cmd) {
  if (!cmd.startsWith("#")) return;

  int spaceIndex = cmd.indexOf(' ');
  if (spaceIndex == -1) return;

  int targetID = cmd.substring(1, spaceIndex).toInt();
  if (targetID != MY_NODE_ID) return;

  String payload = cmd.substring(spaceIndex + 1);
  payload.trim();

  if (payload.length() != 8) return;

  for (int i = 0; i < 8; i++) {
    char state = payload.charAt(i);
    if (state == 'A' || state == 'B' || state == 'C') {
      currentStates[i] = state;
    }
  }

  applyRelayStates();
  sendConfirmation(); 
}

void applyRelayStates() {
  for (int i = 0; i < 8; i++) {
    char state = currentStates[i];
    if (state == 'C') {
      digitalWrite(pinsGroup1[i], LOW); 
      digitalWrite(pinsGroup2[i], LOW);
    } 
    else if (state == 'A') {
      digitalWrite(pinsGroup2[i], LOW); // Vetva A
      digitalWrite(pinsGroup1[i], HIGH);  // Zopnúť RF
    } 
    else if (state == 'B') {
      digitalWrite(pinsGroup2[i], HIGH);  // Vetva B
      digitalWrite(pinsGroup1[i], HIGH);  // Zopnúť RF
    }
  }
}

void sendConfirmation() {
  // Vytvorenie novej odpovede vo formáte: #1ACK:CCACBBCC,T1:24.5,T2:35.2
  String response = "#" + String(MY_NODE_ID) + "ACK:";
  for (int i = 0; i < 8; i++) {
    response += currentStates[i];
  }
  
  // Pridanie oboch teplôt za čiarku
  response += ",T1:" + String(tempMCU, 1);
  response += ",T2:" + String(tempRelay, 1);
  
  RS485Serial.println(response);
  RS485Serial.flush();

  Serial.print("[TX ACK DUÁLNA TELEMETRIA]: ");
  Serial.println(response);
}
