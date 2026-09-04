#include "stapells/functions/ServoFunction.h"
#include <ArduinoJson.h>
#include <Wire.h>
#include "stapells/MqttService.h"

namespace stapells {
namespace {
constexpr char kRoot[] = "track/turnouts/";
constexpr uint16_t kStepMs = 2;
bool validPulse(long value) { return value >= 80 && value <= 800; }
}

void ServoFunction::begin(MqttService& mqtt, const String& boardId) {
  mqtt_ = &mqtt; boardId_ = boardId; boardId_.toUpperCase();
  for (auto& id : ownedIds_) id = -1;
}

void ServoFunction::onConnected() {
  mqtt_->subscribe("Control/#");
  mqtt_->subscribe("track/turnouts/#");
}

void ServoFunction::enableHardware() {
  if (!hardwareStarted_) {
    Wire.begin(); pwm_.begin(); pwm_.setPWMFreq(42);
    hardwareStarted_ = true;
  }
  bool allReady = frogBoardCount_ > 0;
  for (uint8_t board = 0; board < frogBoardCount_; ++board) {
    pcfReady_[board] = pcf_[board].begin(frogAddresses_[board], &Wire);
    if (pcfReady_[board]) {
      for (uint8_t pin = 0; pin < 8; ++pin) pcf_[board].pinMode(pin, OUTPUT);
    } else {
      allReady = false;
    }
    Serial.printf("[servo] Frog board %u at 0x%02X: %s\n", board + 1,
                  frogAddresses_[board], pcfReady_[board] ? "ready" : "missing");
  }
  for (uint8_t board = frogBoardCount_; board < 2; ++board) pcfReady_[board] = false;
  const String statusTopic = String("Control/") + boardId_ + "/status/functions/servo";
  mqtt_->publishTopic(statusTopic, allReady ? "READY" : "FAULT_FROG_HARDWARE", true);
  for (auto& turnout : turnouts_) update(turnout);
}

void ServoFunction::readModules(const String& payload) {
  JsonDocument document;
  if (deserializeJson(document, payload) || !document.is<JsonArray>()) {
    Serial.println(F("[servo] Invalid module configuration; keeping one PCF8574 at 0x20"));
    return;
  }
  uint8_t count = 0;
  for (JsonObject module : document.as<JsonArray>()) {
    if (count >= 2 || String(module["type"] | "") != "PCF8574") continue;
    String address = module["address"] | "0x20";
    char* end = nullptr;
    const long parsed = strtol(address.c_str(), &end, 0);
    if (end == address.c_str() || *end != '\0' || parsed < 0x20 || parsed > 0x27) continue;
    frogAddresses_[count++] = static_cast<uint8_t>(parsed);
  }
  frogBoardCount_ = count;
  Serial.printf("[servo] Configured frog boards: %u\n", frogBoardCount_);
  if (enabled_) enableHardware();
}

ServoFunction::Turnout* ServoFunction::get(int id) {
  for (auto& turnout : turnouts_) if (turnout.id == id) return &turnout;
  for (auto& turnout : turnouts_) if (turnout.id < 0) { turnout.id = id; return &turnout; }
  return nullptr;
}

bool ServoFunction::listOwns(int id) const {
  for (uint8_t i = 0; i < ownedCount_; ++i) if (ownedIds_[i] == id) return true;
  return false;
}

bool ServoFunction::owns(const Turnout& turnout) const {
  if (!enabled_) return false;
  return turnout.hasBoard ? turnout.boardMatches : listOwns(turnout.id);
}

void ServoFunction::readList(const String& payload) {
  ownedCount_ = 0;
  int start = 0;
  while (start < static_cast<int>(payload.length()) && ownedCount_ < kChannelCapacity) {
    int comma = payload.indexOf(',', start);
    if (comma < 0) comma = payload.length();
    String item = payload.substring(start, comma); item.trim();
    if (!item.isEmpty()) ownedIds_[ownedCount_++] = item.toInt();
    start = comma + 1;
  }
  for (auto& turnout : turnouts_) update(turnout);
}

void ServoFunction::onMessage(const String& topic, const String& payload) {
  const String control = String("Control/") + boardId_ + '_';
  if (topic == control + "Type") {
    String value = payload; value.trim(); value.toUpperCase();
    if (value == "SERVO") { enabled_ = true; enableHardware(); }
    return;
  }
  if (topic == control + "Functions") {
    if (payload.indexOf("TURNOUT_SERVO") >= 0) { enabled_ = true; enableHardware(); }
    return;
  }
  if (topic == control + "Modules") { readModules(payload); return; }
  if (topic == control + "Servos") { readList(payload); return; }
  if (!topic.startsWith(kRoot)) return;

  const String rest = topic.substring(sizeof(kRoot) - 1);
  const int slash = rest.indexOf('/');
  const String idText = slash < 0 ? rest : rest.substring(0, slash);
  if (idText.isEmpty()) return;
  for (size_t i = 0; i < idText.length(); ++i) if (!isDigit(idText[i])) return;
  Turnout* turnout = get(idText.toInt());
  if (!turnout) return;
  const String field = slash < 0 ? "state" : rest.substring(slash + 1);

  if (field == "min" || field == "max") {
    const long pulse = payload.toInt();
    if (!validPulse(pulse)) {
      Serial.printf("[servo] Unsafe endpoint %ld ignored for turnout %d\n", pulse, turnout->id);
      return;
    }
    // Preserve the legacy meaning: min=CLOSED, max=THROWN. Never reorder them.
    if (field == "min") { turnout->closed = pulse; turnout->hasClosed = true; }
    else { turnout->thrown = pulse; turnout->hasThrown = true; }
  } else if (field == "channel") {
    const int value = payload.toInt(); turnout->channel = value >= 0 && value < 16 ? value : -1;
  } else if (field == "frog") {
    const int value = payload.toInt(); turnout->frog = value >= 0 && value < 16 ? value : -1;
  } else if (field == "board") {
    String value = payload; value.trim(); value.toUpperCase();
    turnout->hasBoard = !value.isEmpty(); turnout->boardMatches = value == boardId_;
  } else if (field == "state") {
    String value = payload; value.trim(); value.toUpperCase();
    if (value != "CLOSED" && value != "THROWN") return;
    turnout->isThrown = value == "THROWN"; turnout->hasState = true;
  }
  update(*turnout);
}

void ServoFunction::update(Turnout& turnout) {
  const int frogBoard = turnout.frog < 0 ? -1 : turnout.frog / 8;
  if (!hardwareStarted_ || !owns(turnout) || turnout.channel < 0 ||
      frogBoard < 0 || frogBoard >= frogBoardCount_ || !pcfReady_[frogBoard] ||
      !turnout.hasClosed || !turnout.hasThrown || !turnout.hasState) return;
  turnout.target = turnout.isThrown ? turnout.thrown : turnout.closed;
  if (!turnout.started) {
    // Physical position is unknowable after boot. Apply only the complete,
    // validated retained target; do not sweep from a guessed endpoint.
    turnout.current = turnout.target;
    pwm_.setPWM(turnout.channel, 0, turnout.current);
    turnout.started = true; finish(turnout); return;
  }
  turnout.moving = turnout.current != turnout.target;
  turnout.nextStep = millis();
}

void ServoFunction::loop(uint32_t now) {
  if (!hardwareStarted_) return;
  for (auto& turnout : turnouts_) {
    if (!turnout.moving || static_cast<int32_t>(now - turnout.nextStep) < 0) continue;
    turnout.nextStep = now + kStepMs;
    turnout.current += turnout.current < turnout.target ? 1 : -1;
    pwm_.setPWM(turnout.channel, 0, turnout.current);
    if (turnout.current == turnout.target) finish(turnout);
  }
}

void ServoFunction::finish(Turnout& turnout) {
  turnout.moving = false; setFrog(turnout.frog, turnout.isThrown);
  Serial.printf("[servo] Turnout %d channel %d %s at %u\n", turnout.id,
                turnout.channel, turnout.isThrown ? "THROWN" : "CLOSED", turnout.current);
}

void ServoFunction::setFrog(int frog, bool thrown) {
  if (frog < 0 || frog >= 16) return;
  const uint8_t board = frog / 8;
  if (board < frogBoardCount_ && pcfReady_[board]) {
    pcf_[board].digitalWrite(frog % 8, thrown ? LOW : HIGH);
  }
}
}  // namespace stapells
