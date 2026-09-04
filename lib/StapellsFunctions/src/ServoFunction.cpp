#include "stapells/functions/ServoFunction.h"
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
  if (hardwareReady_) return;
  Wire.begin(); pwm_.begin(); pwm_.setPWMFreq(42);
  pcf1Ready_ = pcf1_.begin(0x20, &Wire);
  pcf2Ready_ = pcf2_.begin(0x21, &Wire);
  if (pcf1Ready_) for (uint8_t pin = 0; pin < 8; ++pin) pcf1_.pinMode(pin, OUTPUT);
  if (pcf2Ready_) for (uint8_t pin = 0; pin < 8; ++pin) pcf2_.pinMode(pin, OUTPUT);
  hardwareReady_ = pcf1Ready_ && pcf2Ready_;
  Serial.printf("[servo] PCA9685 42Hz; required frogs 0x20=%s 0x21=%s\n",
                pcf1Ready_ ? "ready" : "missing", pcf2Ready_ ? "ready" : "missing");
  const String statusTopic = String("Control/") + boardId_ + "/status/functions/servo";
  mqtt_->publishTopic(statusTopic, hardwareReady_ ? "READY" : "FAULT_FROG_HARDWARE", true);
  if (!hardwareReady_) {
    Serial.println(F("[servo] Movement locked: both PCF8574 frog boards are required"));
    return;
  }
  for (auto& turnout : turnouts_) update(turnout);
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
  if (!hardwareReady_ || !owns(turnout) || turnout.channel < 0 || turnout.frog < 0 ||
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
  if (!hardwareReady_) return;
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
  Adafruit_PCF8574* pcf = frog < 8 ? &pcf1_ : &pcf2_;
  const bool ready = frog < 8 ? pcf1Ready_ : pcf2Ready_;
  if (ready) pcf->digitalWrite(frog % 8, thrown ? LOW : HIGH);
}
}  // namespace stapells
