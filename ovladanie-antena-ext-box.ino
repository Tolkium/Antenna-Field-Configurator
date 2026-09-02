#include "config.h"
#include "html_pages.h"
#include <ctype.h>

// --- Globálne premenné a stavy ---
char currentStates[8] = {'C','C','C','C','C','C','C','C'}; 
char targetStates[8]  = {'C','C','C','C','C','C','C','C'}; 
bool waitingForReply   = false;
unsigned long requestTimestamp = 0;
bool isStandbyActive = false;

float glowAngles[8] = {0,0,0,0,0,0,0,0};
int lastSentRed[8]  = {-1,-1,-1,-1,-1,-1,-1,-1}; // Pamäť pre červený kanál
int lastSentBlue[8] = {-1,-1,-1,-1,-1,-1,-1,-1}; // Pamäť pre modrý kanál
String currentMode  = "PHONE"; 
int clientID        = 1;       
bool pca9685Present = false; 

// --- Konfigurácia úložísk a sietí (NVS) ---
Preferences prefs;
String wifi_ssid     = "";
String wifi_password = "";
String currentPin    = "1234";
String staticIP = "0.0.0.0"; // Lokálna kópia IP adresy, aby sme nedopytovali sieťovú kartu
int maxBrightness = 1500; // 0–4095, odvodené z ledPercent
int ledPercent = 37;

// --- Driver Inštancie ---
// 1. PCA9685 inicializujeme s predvolenou adresou 0x40
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);
WebServer server(80);
// 2. BEZPEČNÝ SW_I2C konštruktor pre OLED (poradie: SCL, SDA) - NIKDY NEZAMRZNE CPU
//U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, I2C_SCL, I2C_SDA, /* reset=*/ U8X8_PIN_NONE); 
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

String displayIP    = "Connecting";
String displayRS485 = "C-INIT";

const int OFFSET_X = 28;
const int OFFSET_Y = 24;

String rs485Buffer = "";
String usbBuffer   = ""; // PRIDANÉ: Asynchrónny buffer pre USB konzolu

unsigned long lastRS485QueryMillis = 0;
unsigned long wifiReconnectAt = 0;
bool pendingTxWhenIdle = false;

float nodeTempMCU = 0.0;
float nodeTempRelay = 0.0;
String sensorStatus = "OK";
String nodeAnomaly = "OK";

void parseRS485Incoming();
void triggerRS485Transmission();

bool isAuthorized() {
  if (server.hasHeader("Cookie")) {
    String cookie = server.header("Cookie");
    if (cookie.indexOf("session=auth_valid") != -1) return true;
  }
  return false;
}

void invalidateLEDCache() {
  for (int i = 0; i < 8; i++) {
    lastSentRed[i] = -1;
    lastSentBlue[i] = -1;
  }
}

String statesToString() {
  String s = "";
  for (int i = 0; i < 8; i++) s += targetStates[i];
  return s;
}

void sanitizeTargetStates() {
  for (int i = 0; i < 8; i++) {
    targetStates[i] = sanitizeAntennaState(targetStates[i]);
  }
}

void persistAntennaStates() {
  prefs.begin("system_cfg", false);
  prefs.putString("ant_states", statesToString());
  prefs.end();
}

void persistStandbyFlag() {
  prefs.begin("system_cfg", false);
  prefs.putBool("sys_standby", isStandbyActive);
  prefs.end();
}

void persistForkMode() {
  prefs.begin("system_cfg", false);
  prefs.putString("fork_mode", currentMode);
  prefs.end();
}

void applyForkHardware(String mode) {
  currentMode = (mode == "RTTY") ? "RTTY" : "PHONE";
  digitalWrite(RELAY_FORK_PIN, (currentMode == "PHONE") ? HIGH : LOW);
}

void setForkHardware(String mode) {
  applyForkHardware(mode);
  persistForkMode();
}

void enterStandbyMute() {
  isStandbyActive = true;
  waitingForReply = false;
  nodeAnomaly = "STANDBY";
  displayRS485 = "STANDBY";
  invalidateLEDCache();
  persistStandbyFlag();
}

void exitStandbyMute(bool transmit) {
  isStandbyActive = false;
  invalidateLEDCache();
  persistStandbyFlag();
  if (!transmit) return;
  if (!waitingForReply) {
    triggerRS485Transmission();
  } else {
    pendingTxWhenIdle = true;
  }
}

void redirectHome(const char* extra = nullptr) {
  String loc = "/";
  if (extra && extra[0]) loc += extra;
  server.sendHeader("Location", loc);
  server.send(303);
}

void redirectBusy() {
  redirectHome("?busy=1");
}

// Operátor mení anténu → stand-by (utlmenie chýb/RS485/LED) sa ruší
void cancelStandbyForOperatorChange() {
  if (!isStandbyActive) return;
  isStandbyActive = false;
  invalidateLEDCache();
  persistStandbyFlag();
}

void triggerRS485Transmission() {
  sanitizeTargetStates();
  clientID = FIXED_CLIENT_ID;

  String payload = "#" + String(clientID) + " ";
  for (int i = 0; i < 8; i++) payload += targetStates[i];

  Serial1.println(payload);
  Serial.println("[RS485] TX: " + payload);

  displayRS485 = "TX:" + String(clientID) + "-PEND";
  waitingForReply = true;
  requestTimestamp = millis();
  lastRS485QueryMillis = millis();
}

void handleRoot() {
  sendHTML(isAuthorized());
}
void handleLogin() {
  if (server.hasArg("webpin") && server.arg("webpin") == currentPin) {
    server.sendHeader("Set-Cookie", "session=auth_valid; Path=/");
    server.sendHeader("Location", "/"); server.send(303);
  } else {
    server.send(401, "text/html; charset=utf-8", "<h3>Nespravny PIN</h3><a href='/'>Skusit znova</a>");
  }
}

void handleSet() {
  if (!isAuthorized()) { server.sendHeader("Location", "/"); server.send(303); return; }

  if (waitingForReply) {
    redirectBusy();
    return;
  }

  for (int i = 0; i < 8; i++) {
    String argName = "l" + String(i);
    if (server.hasArg(argName)) {
      targetStates[i] = sanitizeAntennaState(server.arg(argName).charAt(0));
    }
  }

  if (server.hasArg("fork_mode")) setForkHardware(server.arg("fork_mode"));

  cancelStandbyForOperatorChange();
  persistAntennaStates();
  Serial.println("[WEB] Nova konfiguracia ulozena do pamate: " + statesToString());

  triggerRS485Transmission();
  sendHTML(true);
}

void handleUpdateConfig() {
  if (!isAuthorized()) { server.sendHeader("Location", "/"); server.send(303); return; }
  bool resetWifiNeeded = false;
  bool pinErr = false;
  prefs.begin("system_cfg", false);
  if (server.hasArg("newpin")) {
    if (server.arg("newpin").length() == 4) {
      currentPin = server.arg("newpin");
      prefs.putString("sys_pin", currentPin);
    } else {
      pinErr = true;
    }
  }
  if (server.hasArg("led_bright")) {
    ledPercent = clampLedPercent(server.arg("led_bright").toInt());
    maxBrightness = ledPercentToPwm(ledPercent);
    prefs.putInt("led_pct", ledPercent);
    prefs.putInt("led_bright", maxBrightness);
    Serial.print("[WEB] Nastaveny novy maximalny jas: ");
    Serial.print(ledPercent);
    Serial.println("%");
  }
  if (server.hasArg("w_ssid") && server.arg("w_ssid").length() > 0 && server.arg("w_ssid") != wifi_ssid) {
    wifi_ssid = server.arg("w_ssid"); prefs.putString("w_ssid", wifi_ssid); resetWifiNeeded = true;
  }
  if (server.hasArg("w_pass") && server.arg("w_pass").length() > 0) {
    wifi_password = server.arg("w_pass"); prefs.putString("w_pass", wifi_password); resetWifiNeeded = true;
  }
  prefs.end();
  redirectHome(pinErr ? "?pinerr=1" : nullptr);
  if (resetWifiNeeded) wifiReconnectAt = millis() + 500;
}

void handleProfile() {
  if (!isAuthorized()) { redirectHome(); return; }
  if (waitingForReply) {
    redirectBusy();
    return;
  }
  if (!server.hasArg("type")) { redirectHome(); return; }

  String type = server.arg("type");
  if (type == "clear_all") {
    for (int i = 0; i < 8; i++) targetStates[i] = 'C';
  } else if (type == "beam_left") {
    for (int i = 4; i <= 7; i++) targetStates[i] = 'A';
    for (int i = 0; i <= 3; i++) targetStates[i] = 'B';
  } else if (type == "beam_right") {
    for (int i = 4; i <= 7; i++) targetStates[i] = 'B';
    for (int i = 0; i <= 3; i++) targetStates[i] = 'A';
  } else {
    redirectHome();
    return;
  }

  cancelStandbyForOperatorChange();
  persistAntennaStates();
  triggerRS485Transmission();
  redirectHome();
}
void processUSBConsole() {
  // Spracujeme maximálne 1 znak za jeden prebeh loopu (žiadny while)
  // Tým chránime native USB CDC zbernicu pred zacyklením a lagovaním
  if (Serial.available() > 0) {
    char c = Serial.read();
    
    // Ak narazíme na akýkoľvek ukončovací znak riadku (Enter z PC)
    if (c == '\n' || c == '\r') {
      usbBuffer.trim(); // Bezpečne odstráni prípadné medzery a \r/\n z okrajov textu
      
      // Spracujeme príkaz IBA vtedy, ak reálne obsahuje textové znaky
      if (usbBuffer.length() > 0) {
        
        if (usbBuffer.equalsIgnoreCase("HELP")) {
          Serial.println("\nPrikazy: STATUS, SETMODE [P/R], SETVAL [0-7] [A/B/C], SEND");
          Serial.println("SETMODE zrusi stand-by. SETVAL ulozi do NVS; SEND posle na stoziar a zrusi stand-by.");
        }
        else if (usbBuffer.equalsIgnoreCase("STATUS")) {
          Serial.println("\n--- [STATUS ODOZVA] ---");
          Serial.print("IP: "); Serial.println(staticIP);
          Serial.printf("PCA9685: %s | Rele: %s | ID: #%d | Standby: %s\n",
            pca9685Present ? "ANO" : "NIE", currentMode.c_str(), clientID,
            isStandbyActive ? "ANO" : "NIE");
          Serial.print("Target:  "); for(int i=0;i<8;i++) Serial.print(targetStates[i]); Serial.println();
          Serial.print("Current: "); for(int i=0;i<8;i++) Serial.print(currentStates[i]); Serial.println();
        }
        else if (usbBuffer.startsWith("SETID ")) {
          Serial.println("ID uzla je pevne #1 (Nano). Zmena nie je mozna.");
        }
        else if (usbBuffer.startsWith("SETMODE ")) {
          char m = usbBuffer.substring(8).charAt(0);
          if (m == 'P' || m == 'p') {
            cancelStandbyForOperatorChange();
            setForkHardware("PHONE");
            Serial.println("Prepnute na PHONE");
          } else if (m == 'R' || m == 'r') {
            cancelStandbyForOperatorChange();
            setForkHardware("RTTY");
            Serial.println("Prepnute na RTTY");
          } else {
            Serial.println("[!] SETMODE P alebo R");
          }
        }
        else if (usbBuffer.startsWith("SETVAL ")) {
          String rest = usbBuffer.substring(7);
          rest.trim();
          int sp = rest.indexOf(' ');
          String idxTok = (sp > 0) ? rest.substring(0, sp) : "";
          String valTok = (sp > 0) ? rest.substring(sp + 1) : "";
          idxTok.trim();
          valTok.trim();
          int idx = (idxTok.length() == 1) ? idxTok.toInt() : -1;
          char val = (valTok.length() > 0) ? (char)toupper((unsigned char)valTok.charAt(0)) : 0;
          if (idx >= 0 && idx <= 7 && isAntennaState(val)) {
            targetStates[idx] = val;
            persistAntennaStates();
            Serial.printf("Clen %d -> %c (ulozeny, posli SEND)\n", idx + 1, val);
          } else {
            Serial.println("[!] SETVAL [0-7] [A/B/C]");
          }
        }
        else if (usbBuffer.equalsIgnoreCase("SEND")) {
          if (!waitingForReply) {
            cancelStandbyForOperatorChange();
            triggerRS485Transmission();
          } else {
            Serial.println("[!] System uz caka na odpoved zbernice...");
          }
        }
        
        Serial.flush(); // Vynútené vytlačenie textu do PC
      }
      
      // Vyčistíme textový buffer pre zbieranie ďalšieho príkazu
      usbBuffer = ""; 
      
      // KĽÚČOVÁ POISTKA PROTI KONFLIKTU PRERUŠENÍ:
      // Ak po sieti preletel parazitný bordel alebo zostal v bufferi nečitateľný riadiaci znak,
      // bleskovo ho „zožerieme“ a vyčistíme hardvérový register sériového portu,
      // čím uvoľníme cestu pre ďalšie čisté písmená.
      while (Serial.available() > 0 && Serial.peek() < 32) {
        Serial.read(); 
      }
      
    } else {
      // Do textového reťazca v RAM ukladáme výhradne iba čitateľné znaky (ASCII 32 až 126)
      if (c >= 32 && c < 127 && usbBuffer.length() < USB_BUF_MAX) {
        usbBuffer += c;
      }
    }
  }
}

}
void processLEDGlows() {
  unsigned long currentMillis = millis();
  static unsigned long lastGlowUpdate = 0;

  if (isStandbyActive) {
    if (pca9685Present) {
      bool needOff = false;
      for (int i = 0; i < 8; i++) {
        if (lastSentRed[i] != 0 || lastSentBlue[i] != 0) needOff = true;
      }
      if (needOff) {
        for (int i = 0; i < 16; i++) pwm.setPWM(i, 0, 0);
        for (int i = 0; i < 8; i++) {
          lastSentRed[i] = 0;
          lastSentBlue[i] = 0;
        }
      }
    }
    return;
  }

  if (currentMillis - lastGlowUpdate < 50) return;
  lastGlowUpdate = currentMillis;

  for (int i = 0; i < 8; i++) {
    int targetRed = 0;
    int targetBlue = 0;

    if (currentStates[i] != targetStates[i]) {
      glowAngles[i] += 0.15;
      if (glowAngles[i] > 2 * PI) glowAngles[i] -= 2 * PI;

      int pulseIntensity = (sin(glowAngles[i]) + 1.0) * (maxBrightness / 2.0);
      targetRed = pulseIntensity;
      targetBlue = pulseIntensity;
    }
    else {
      switch (currentStates[i]) {
        case 'A':
          targetRed = maxBrightness;
          targetBlue = 0;
          break;
        case 'B':
          targetRed = 0;
          targetBlue = maxBrightness;
          break;
        case 'C':
        default:
          targetRed = 0;
          targetBlue = 0;
          break;
      }
    }

    if (targetRed != lastSentRed[i] || targetBlue != lastSentBlue[i]) {
      if (pca9685Present) {
        pwm.setPWM(i, 0, targetBlue);
        pwm.setPWM(i + 8, 0, targetRed);
      }
      lastSentRed[i] = targetRed;
      lastSentBlue[i] = targetBlue;
    }
  }
}
void parseRS485Incoming() {
  while (Serial1.available() > 0) {
    char c = Serial1.read();

    if (c == '\n' || c == '\r') {
      rs485Buffer.trim();

      if (rs485Buffer.length() > 0) {
        String expectedPrefix = "#" + String(FIXED_CLIENT_ID) + "ACK:";

        if (rs485Buffer.startsWith(expectedPrefix)) {
          int payloadStartIdx = expectedPrefix.length();

          if (rs485Buffer.length() >= payloadStartIdx + 8) {
            for (int i = 0; i < 8; i++) {
              char s = rs485Buffer.charAt(payloadStartIdx + i);
              if (isAntennaState(s)) currentStates[i] = s;
            }

            int t1Idx = rs485Buffer.indexOf(",T1:");
            if (t1Idx != -1) {
              int t2Idx = rs485Buffer.indexOf(",T2:", t1Idx);
              String t1Str = (t2Idx != -1) ? rs485Buffer.substring(t1Idx + 4, t2Idx) : rs485Buffer.substring(t1Idx + 4);
              nodeTempMCU = t1Str.toFloat();
            }

            int t2Idx = rs485Buffer.indexOf(",T2:");
            if (t2Idx != -1) {
              nodeTempRelay = rs485Buffer.substring(t2Idx + 4).toFloat();
            }

            if (nodeTempMCU > 900.0 && nodeTempRelay > 900.0) {
              sensorStatus = "ERR SENS ALL";
            } else if (nodeTempMCU > 900.0) {
              sensorStatus = "ERR SEN T1";
            } else if (nodeTempRelay > 900.0) {
              sensorStatus = "ERR SEN T2";
            } else {
              sensorStatus = "OK";
            }

            waitingForReply = false;
            if (isStandbyActive) {
              nodeAnomaly = "STANDBY";
              displayRS485 = "STANDBY";
            } else {
              nodeAnomaly = sensorStatus;
              displayRS485 = "T1:" + String(nodeTempMCU > 900.0 ? "--" : String(nodeTempMCU, 0)) + " T2:" + String(nodeTempRelay > 900.0 ? "--" : String(nodeTempRelay, 0));
            }
            Serial.printf("[RS485] Prijate ACK -> T1(MCU): %.1f C, T2(Rele): %.1f C, Stav: %s\n", nodeTempMCU, nodeTempRelay, sensorStatus.c_str());
          }
        } else {
          Serial.println("[RS485] Prijaty neznamy paket: " + rs485Buffer);
        }
      }
      rs485Buffer = "";
    } else if (c >= 32) {
      if (rs485Buffer.length() < RS485_BUF_MAX) rs485Buffer += c;
    }
  }

  if (waitingForReply) {
    if (millis() - requestTimestamp > TIMEOUT_MS) {
      waitingForReply = false;
      if (!isStandbyActive) {
        displayRS485 = "ID" + String(FIXED_CLIENT_ID) + ": TIMEOUT";
        nodeAnomaly = "OFFLINE";
      }
    }
  }
}

void updateOLEDDisplay() {
static unsigned long lastScreenRefresh = 0;
if (millis() - lastScreenRefresh < 250) return;
lastScreenRefresh = millis();
displayIP = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "WIFI_CONN...";
u8g2.clearBuffer();
u8g2.setFont(u8g2_font_5x7_tf);
u8g2.setCursor(OFFSET_X + 0, OFFSET_Y + 7); u8g2.print("IP:" + displayIP);
u8g2.setCursor(OFFSET_X + 0, OFFSET_Y + 17); u8g2.print("FRK:" + currentMode + " #" + String(clientID));
u8g2.setCursor(OFFSET_X + 0, OFFSET_Y + 27); u8g2.print(displayRS485);
u8g2.setCursor(OFFSET_X + 0, OFFSET_Y + 37); u8g2.print("PIN:" + currentPin);
u8g2.sendBuffer();
}
void setup() {
  pinMode(RELAY_FORK_PIN, OUTPUT);
  digitalWrite(RELAY_FORK_PIN, HIGH);
  
  Serial.begin(115200);
  Serial.setTimeout(5);
  delay(500);
  Serial.println("\n[START] Štartujem v neblokujúcom asynchrónnom režime...");

  Serial1.begin(9600, SERIAL_8N1, RS485_RX, RS485_TX);
  Serial1.setRxBufferSize(256);

  // --- KĽÚČOVÁ OPRAVA: Najskôr konfigurácia hardvérovej zbernice ---
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setTimeOut(10); 

  // Teraz inicializujeme displej cez spoločný hardvér
  u8g2.begin();
  Serial.println("[OLED] Displej inicializovaný cez HW I2C.");

  // Overenie prítomnosti PCA9685
  Wire.beginTransmission(0x40); 
  if (Wire.endTransmission() == 0) {
    pwm.begin();
    pwm.setPWMFreq(1000);     // Nastavenie frekvencie
    pwm.setOutputMode(true);  // Zapnutie Totem-Pole (Push-Pull) režimu
    
    pca9685Present = true;
    Serial.println("[PCA9685] Modul nájdený a pripojený v Totem-Pole.");
  } else {
    pca9685Present = false;
    Serial.println("[ PCA9685] Modul NENÁJDENÝ! Skontrolujte napájanie.");
  }

  clientID = FIXED_CLIENT_ID;

  bool pinClamped = false;
  prefs.begin("system_cfg", true);
  if (prefs.isKey("sys_pin")) currentPin = prefs.getString("sys_pin");
  if (currentPin.length() > 4) {
    currentPin = currentPin.substring(0, 4);
    pinClamped = true;
  }
  if (currentPin.length() == 0) currentPin = "1234";
  if (prefs.isKey("w_ssid")) wifi_ssid = prefs.getString("w_ssid");
  if (prefs.isKey("w_pass")) wifi_password = prefs.getString("w_pass");
  if (prefs.isKey("led_pct")) {
    ledPercent = clampLedPercent(prefs.getInt("led_pct", 37));
    maxBrightness = ledPercentToPwm(ledPercent);
  } else {
    maxBrightness = constrain(prefs.getInt("led_bright", 1500), 0, 4095);
    ledPercent = clampLedPercent((int)((maxBrightness * 100L + 2047) / 4095));
    maxBrightness = ledPercentToPwm(ledPercent);
  }
  String savedStates = prefs.getString("ant_states", "CCCCCCCC");
  isStandbyActive = prefs.getBool("sys_standby", false);
  String savedFork = prefs.getString("fork_mode", "PHONE");
  prefs.end();
  if (pinClamped) {
    prefs.begin("system_cfg", false);
    prefs.putString("sys_pin", currentPin);
    prefs.end();
  }

  applyForkHardware(savedFork);

  for (int i = 0; i < 8; i++) {
    targetStates[i] = sanitizeAntennaState(savedStates.charAt(i));
    currentStates[i] = 'C';
  }
  Serial.print("[START] Nacitana ziadana konfiguracia z pamate: ");
  Serial.println(statesToString());

  lastRS485QueryMillis = millis();
  if (isStandbyActive) {
    nodeAnomaly = "STANDBY";
    displayRS485 = "STANDBY";
    waitingForReply = false;
    invalidateLEDCache();
    Serial.println("[START] Stand-by: RS485 a LED utlmene, rele na stoziari sa nemenia.");
  } else {
    Serial.println("[START] Realny stav caka na overenie z Nano...");
    triggerRS485Transmission();
  }


  // --- ASYNCHRÓNNY REŽIM PRE WIFI (Vypnutie úsporného módu) ---
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true); 
  
  if (wifi_ssid.length() > 0) {
    WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
    Serial.println("[WiFi] Pokus o pripojenie k: " + wifi_ssid);
  } else {
    Serial.println("[WiFi] SSID prazdne. Offline rezim.");
  }
  
  // Vypnutie Wi-Fi spánku - rádio už nebude náhodne uspávať procesorové jadro!
  WiFi.setSleep(false); 

  const char* headerkeys[] = {"Cookie"};
  server.collectHeaders(headerkeys, 1);
  server.on("/", handleRoot);
  server.on("/login", HTTP_POST, handleLogin);
  server.on("/set", handleSet);
  server.on("/updateConfig", HTTP_POST, handleUpdateConfig);
  // Nový ultra-rýchly endpoint pre JavaScript na pozadí
server.on("/api/status", HTTP_GET, []() {
  server.send(200, "application/json; charset=utf-8", jsonHubStatus());
});

server.on("/profile", HTTP_POST, handleProfile);

server.on("/toggleStandby", HTTP_GET, []() {
  if (isStandbyActive) {
    exitStandbyMute(true);
  } else {
    enterStandbyMute();
  }
  server.sendHeader("Location", "/");
  server.send(303);
});
  server.begin();

  Serial.println("[SYSTEM] Inicializácia dokončená. Všetko beží asynchrónne.");
  Serial.flush();
}

void loop() {
  // 1. Web server beží asynchrónne, ale FreeRTOS mu nedovolí zablokovať jadro
  static unsigned long lastWebCheck = 0;
  if (millis() - lastWebCheck > 20) {
    lastWebCheck = millis();
    server.handleClient();
    yield(); // KĽÚČOVÝ PRÍKAZ: Hneď po spracovaní webu vráť kontrolu FreeRTOS plánovaču!
  }

  // 2. USB KONZOLA - teraz s prioritným FreeRTOS oknom
  if (Serial.available() > 0) {
    yield(); // Poistka: Daj USB CDC ovládaču mikrosekundu na stabilizáciu dát pred čítaním
    processUSBConsole(); 
  }
  
  // 3. Ostatné zbernice bežia na pozadí
  parseRS485Incoming();
  processLEDGlows();

  if (wifiReconnectAt != 0 && (long)(millis() - wifiReconnectAt) >= 0) {
    wifiReconnectAt = 0;
    staticIP = "WIFI_CONN...";
    WiFi.disconnect();
    WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
    Serial.println("[WiFi] Pripajam k: " + wifi_ssid);
  }

  if (pendingTxWhenIdle && !waitingForReply && !isStandbyActive) {
    pendingTxWhenIdle = false;
    triggerRS485Transmission();
  } 

  // 4. Asynchrónna kontrola IP
  static unsigned long lastIPCheck = 0;
  if (millis() - lastIPCheck > 5000) {
    lastIPCheck = millis();
    if (WiFi.status() == WL_CONNECTED) {
      if (staticIP == "WIFI_CONN..." || staticIP == "0.0.0.0") {
        staticIP = WiFi.localIP().toString();
        Serial.println("\n[WiFi] Pridelená IP: " + staticIP);
      }
    } else {
      staticIP = "WIFI_CONN...";
    }
  }

  updateOLEDDisplay(); 

  unsigned long currentLoopMillis = millis();

  bool hasSyncMismatch = false;
  for (int i = 0; i < 8; i++) {
    if (currentStates[i] != targetStates[i]) hasSyncMismatch = true;
  }

  unsigned long requiredInterval = (nodeAnomaly == "OFFLINE" || hasSyncMismatch) ? 5000 : 30000;

  if (!isStandbyActive && currentLoopMillis - lastRS485QueryMillis > requiredInterval) {
    lastRS485QueryMillis = currentLoopMillis;

    if (!waitingForReply) {
      if (hasSyncMismatch) {
        Serial.println("[SYSTEM] Nesulad stavov! Posielam opravny dopyt...");
      } else if (nodeAnomaly == "OFFLINE") {
        Serial.println("[SYSTEM] Linka je OFFLINE. Skusam Auto-Recovery...");
      } else {
        Serial.println("[SYSTEM] 30s Heartbeat: kontrolujem stoziar...");
      }
      triggerRS485Transmission();
    }
  }
  
  // 5. Hardvérový oddych FreeRTOS (Umožní USBCDC radiču spracovať stratené prerušenia)
  delay(1); 
}
