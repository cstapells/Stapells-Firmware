#include "StapellsCore.h"

namespace stapells {

void Core::begin() {
  platform_ = detectPlatform();

  // The health-strip GPIO comes from the selected hardware profile/config.
  health_.begin();
  health_.bootWalk();

  setState(NodeState::ConnectingWifi);
  Serial.printf("\nStapells Base %s on %s (%s)\n",
                STAPELLS_FIRMWARE_VERSION,
                platform_.name.c_str(),
                platform_.boardId.c_str());
}

void Core::loop() {
  health_.loop();

  // Service order will remain centralized here as concrete Core services land:
  // recovery -> network -> MQTT -> OTA -> configuration -> registered modules.
  platformYield();
}

void Core::activity() {
  health_.pulse(HealthLed::Activity, HealthColour::Orange);
}

void Core::setState(NodeState state) {
  state_ = state;

  switch (state_) {
    case NodeState::Booting:
      health_.set(HealthLed::System, HealthColour::Yellow);
      break;
    case NodeState::ConnectingWifi:
      health_.set(HealthLed::System, HealthColour::Green);
      health_.set(HealthLed::Wifi, HealthColour::Yellow);
      health_.set(HealthLed::Mqtt, HealthColour::Off);
      break;
    case NodeState::ConnectingMqtt:
      health_.set(HealthLed::Wifi, HealthColour::Blue);
      health_.set(HealthLed::Mqtt, HealthColour::Yellow);
      break;
    case NodeState::Unconfigured:
      health_.set(HealthLed::System, HealthColour::Yellow);
      break;
    case NodeState::Operational:
      health_.set(HealthLed::System, HealthColour::Green);
      health_.set(HealthLed::Wifi, HealthColour::Blue);
      health_.set(HealthLed::Mqtt, HealthColour::Purple);
      break;
    case NodeState::Updating:
    case NodeState::Recovering:
      health_.set(HealthLed::System, HealthColour::Yellow);
      health_.pulse(HealthLed::Activity, HealthColour::Orange);
      break;
    case NodeState::Fault:
      health_.set(HealthLed::System, HealthColour::Red);
      break;
  }
}

}  // namespace stapells
