#include <Arduino.h>
#include <VL53L1X.h>
#include <Wire.h>

namespace {

constexpr uint32_t kSerialBaudRate = 115200;
constexpr uint32_t kStatusIntervalMs = 1000;
constexpr uint32_t kSensorReadIntervalMs = 100;
constexpr uint32_t kSensorBootDelayMs = 50;

constexpr int kI2cSdaPin = 21;
constexpr int kI2cSclPin = 22;

// Annahme fuer den Testaufbau:
// Beide VL53L1X haben je einen eigenen XSHUT-Pin, damit sie nach dem
// Einschalten unterschiedliche I2C-Adressen bekommen koennen.
struct TofConfig {
  const char *name;
  int xshutPin;
  uint8_t i2cAddress;
};

constexpr TofConfig kTofConfigs[] = {
    {"tof_1", 16, 0x30},
    {"tof_2", 17, 0x31},
};

struct ButtonConfig {
  const char *name;
  int pin;
};

constexpr ButtonConfig kButtonConfigs[] = {
    {"btn_1", 32},
    {"btn_2", 33},
    {"btn_3", 25},
    {"btn_4", 26},
};

struct TofState {
  VL53L1X sensor;
  bool initialized = false;
  bool timeout = false;
  uint16_t distanceMm = 0;
};

TofState g_tofStates[sizeof(kTofConfigs) / sizeof(kTofConfigs[0])];
bool g_buttonStates[sizeof(kButtonConfigs) / sizeof(kButtonConfigs[0])];
bool g_streamEnabled = true;
String g_serialBuffer;
uint32_t g_lastStatusAtMs = 0;
uint32_t g_lastSensorReadAtMs = 0;

void sendLine(const String &line) {
  Serial.println(line);
}

String boolToString(bool value) {
  return value ? "1" : "0";
}

String hexAddress(uint8_t address) {
  char buffer[6];
  snprintf(buffer, sizeof(buffer), "0x%02X", address);
  return String(buffer);
}

bool readButtonPressed(size_t index) {
  return digitalRead(kButtonConfigs[index].pin) == LOW;
}

void sendHelp() {
  sendLine("INFO;commands=PING,STATUS,STREAM ON,STREAM OFF,REINIT,I2C_SCAN,HELP");
}

void sendStatus(const char *reason) {
  String line = String("STATUS;reason=") + reason + ";uptime_ms=" + String(millis());

  for (size_t i = 0; i < (sizeof(kTofConfigs) / sizeof(kTofConfigs[0])); ++i) {
    line += ";";
    line += kTofConfigs[i].name;
    line += "_ok=";
    line += boolToString(g_tofStates[i].initialized);
    line += ";";
    line += kTofConfigs[i].name;
    line += "_mm=";
    line += String(g_tofStates[i].distanceMm);
    line += ";";
    line += kTofConfigs[i].name;
    line += "_timeout=";
    line += boolToString(g_tofStates[i].timeout);
  }

  for (size_t i = 0; i < (sizeof(kButtonConfigs) / sizeof(kButtonConfigs[0])); ++i) {
    line += ";";
    line += kButtonConfigs[i].name;
    line += "=";
    line += boolToString(g_buttonStates[i]);
  }

  sendLine(line);
}

void scanI2cBus() {
  String line = "I2C_SCAN";
  bool foundAny = false;

  for (uint8_t address = 1; address < 127; ++address) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      line += ";found=" + hexAddress(address);
      foundAny = true;
    }
  }

  if (!foundAny) {
    line += ";found=none";
  }

  sendLine(line);
}

void initButtons() {
  for (size_t i = 0; i < (sizeof(kButtonConfigs) / sizeof(kButtonConfigs[0])); ++i) {
    pinMode(kButtonConfigs[i].pin, INPUT_PULLUP);
    g_buttonStates[i] = readButtonPressed(i);
  }
}

void shutdownAllTof() {
  for (size_t i = 0; i < (sizeof(kTofConfigs) / sizeof(kTofConfigs[0])); ++i) {
    pinMode(kTofConfigs[i].xshutPin, OUTPUT);
    digitalWrite(kTofConfigs[i].xshutPin, LOW);
  }
  delay(20);
}

void initTofSensors() {
  shutdownAllTof();

  for (size_t i = 0; i < (sizeof(kTofConfigs) / sizeof(kTofConfigs[0])); ++i) {
    g_tofStates[i].initialized = false;
    g_tofStates[i].timeout = false;
    g_tofStates[i].distanceMm = 0;

    pinMode(kTofConfigs[i].xshutPin, OUTPUT);
    digitalWrite(kTofConfigs[i].xshutPin, HIGH);
    delay(kSensorBootDelayMs);

    g_tofStates[i].sensor.setTimeout(100);
    if (!g_tofStates[i].sensor.init()) {
      sendLine(String("SENSOR_INIT;name=") + kTofConfigs[i].name + ";ok=0");
      continue;
    }

    g_tofStates[i].sensor.setAddress(kTofConfigs[i].i2cAddress);
    g_tofStates[i].sensor.setDistanceMode(VL53L1X::Long);
    g_tofStates[i].sensor.setMeasurementTimingBudget(50000);
    g_tofStates[i].sensor.startContinuous(50);
    g_tofStates[i].initialized = true;

    sendLine(String("SENSOR_INIT;name=") + kTofConfigs[i].name + ";ok=1;addr=" +
             hexAddress(kTofConfigs[i].i2cAddress));
  }
}

void updateButtons() {
  for (size_t i = 0; i < (sizeof(kButtonConfigs) / sizeof(kButtonConfigs[0])); ++i) {
    const bool pressed = readButtonPressed(i);
    if (pressed == g_buttonStates[i]) {
      continue;
    }

    g_buttonStates[i] = pressed;
    sendLine(String("EVENT;type=button;name=") + kButtonConfigs[i].name + ";pressed=" +
             boolToString(pressed));
  }
}

void updateTofSensors(uint32_t nowMs) {
  if ((nowMs - g_lastSensorReadAtMs) < kSensorReadIntervalMs) {
    return;
  }

  g_lastSensorReadAtMs = nowMs;

  for (size_t i = 0; i < (sizeof(kTofConfigs) / sizeof(kTofConfigs[0])); ++i) {
    if (!g_tofStates[i].initialized) {
      continue;
    }

    const uint16_t distance = g_tofStates[i].sensor.read();
    const bool timeout = g_tofStates[i].sensor.timeoutOccurred();

    if (timeout != g_tofStates[i].timeout) {
      sendLine(String("EVENT;type=tof_timeout;name=") + kTofConfigs[i].name + ";active=" +
               boolToString(timeout));
    }

    g_tofStates[i].timeout = timeout;

    if (!timeout) {
      g_tofStates[i].distanceMm = distance;
    }
  }
}

void handleCommand(String line) {
  line.trim();
  line.toUpperCase();

  if (line.length() == 0) {
    return;
  }

  if (line == "PING") {
    sendLine(String("PONG;uptime_ms=") + String(millis()));
    return;
  }

  if (line == "STATUS") {
    sendStatus("manual");
    return;
  }

  if (line == "STREAM ON") {
    g_streamEnabled = true;
    sendLine("OK;stream=1");
    return;
  }

  if (line == "STREAM OFF") {
    g_streamEnabled = false;
    sendLine("OK;stream=0");
    return;
  }

  if (line == "REINIT") {
    initTofSensors();
    initButtons();
    sendStatus("reinit");
    return;
  }

  if (line == "I2C_SCAN") {
    scanI2cBus();
    return;
  }

  if (line == "HELP") {
    sendHelp();
    return;
  }

  sendLine(String("ERR;unknown_command=") + line);
}

void pollSerial() {
  while (Serial.available() > 0) {
    const char next = static_cast<char>(Serial.read());

    if (next == '\r' || next == '\n') {
      if (g_serialBuffer.length() > 0) {
        handleCommand(g_serialBuffer);
        g_serialBuffer = "";
      }
      continue;
    }

    if (g_serialBuffer.length() < 120) {
      g_serialBuffer += next;
    } else {
      g_serialBuffer = "";
      sendLine("ERR;input_too_long=1");
    }
  }
}

}  // namespace

void setup() {
  Serial.begin(kSerialBaudRate);
  delay(500);

  g_serialBuffer.reserve(128);

  Wire.begin(kI2cSdaPin, kI2cSclPin);
  initButtons();
  initTofSensors();

  sendLine("READY;fw=esp32_component_test");
  sendHelp();
  sendStatus("boot");
}

void loop() {
  const uint32_t nowMs = millis();

  pollSerial();
  updateButtons();
  updateTofSensors(nowMs);

  if (g_streamEnabled && (nowMs - g_lastStatusAtMs) >= kStatusIntervalMs) {
    g_lastStatusAtMs = nowMs;
    sendStatus("stream");
  }
}
