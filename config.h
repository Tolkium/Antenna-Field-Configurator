#ifndef CONFIG_H
#define CONFIG_H

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <U8g2lib.h>
#include <Preferences.h>

// Hardvérové piny (ESP32-C3)
#define I2C_SDA 5
#define I2C_SCL 6
#define RELAY_FORK_PIN 7
#define RS485_RX 20
#define RS485_TX 21

// Globálne stavy antény
extern char currentStates[8];
extern char targetStates[8];
extern bool waitingForReply;
extern unsigned long requestTimestamp;
const unsigned long TIMEOUT_MS = 800;
const int RS485_BUF_MAX = 80;
const int USB_BUF_MAX = 80;
const int FIXED_CLIENT_ID = 1;

inline bool isAntennaState(char c) {
  return c == 'A' || c == 'B' || c == 'C';
}

inline char sanitizeAntennaState(char c) {
  return isAntennaState(c) ? c : 'C';
}

// LED a Jas
extern float glowAngles[8];
extern int lastSentRed[8];
extern int lastSentBlue[8];
extern int maxBrightness;
extern int ledPercent;
extern bool pca9685Present;

inline int clampLedPercent(int percent) {
  return constrain(percent, 5, 100);
}

inline int ledPercentToPwm(int percent) {
  return (int)((clampLedPercent(percent) * 4095L + 50) / 100);
}

// Systémové premenné
extern String currentMode;
extern int clientID;
extern String wifi_ssid;
extern String wifi_password;
extern String currentPin;
extern String staticIP;
extern bool isStandbyActive;

extern float nodeTempMCU;      // T1: teplota elektroniky (Nano)
extern float nodeTempRelay;    // T2: teplota relé
extern String sensorStatus;    // OK / ERR SEN T1 / ERR SEN T2 / ERR SENS ALL
extern String nodeAnomaly;     // OK / OFFLINE / STANDBY / ERR SEN …


// Inštancie ovládačov
extern Adafruit_PWMServoDriver pwm;
extern WebServer server;
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
extern Preferences prefs;

#endif
