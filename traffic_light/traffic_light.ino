#undef IR_SEND_PIN
#include <IRremote.hpp>

// --- Switch Pin Definitions & Types ---
// Connected to the outer terminals of an ON-OFF-ON 3-state toggle switch (Center terminal to GND)
const int SWITCH_PIN_LEFT = 11;
const int SWITCH_PIN_RIGHT = 12;

enum SwitchPosition {
  SWITCH_POS_UNKNOWN = -1,
  SWITCH_POS_LEFT = 0,
  SWITCH_POS_CENTER = 1,
  SWITCH_POS_RIGHT = 2
};

SwitchPosition readSwitchPosition();
//       3-STATE TOGGLE SWITCH (Bottom View)
//        _______________________________
//
//       |       |       |       |       |
//       | Terminal 1    Terminal 2      Terminal 3
//       | (Left)        (Center/COM)    (Right)
//
//       |___|___|_______|___|___|_______|___|___|
//           |               |               |
//           |               |               |
//           v               v               v
//     [Digital Pin 2]    [ GND ]      [Digital Pin 3]
//
//
// --- IR Send Pin Definitions ---
const int IR_SEND_PIN_1 = 3;
const int IR_SEND_PIN_2 = 5;
const int IR_SEND_PIN_3 = 9;

#define NUMBER_OF_REPEATS 3U

const char offProntoData[] PROGMEM = "0000 006D 0022 0000 015B 00A8 001A 0011 001A 0012 001A 0011 001A 0012 0018 0012 001A 0011 001A 0012 001A 0011 001A 003B 001A 003D 001A 003B 001A 003B 001A 0012 001A 003B 001A 003D 0018 003D 0018 0012 001A 003B 001A 0012 001A 0012 0018 0012 0018 0012 001A 0012 0018 0012 001A 003B 001A 0012 0018 003D 0018 003D 001A 003B 001A 003D 0018 003D 001A 003D 0018 06C3 ";
const char onProntoData[] PROGMEM = "0000 006D 0022 0000 015B 00A8 0018 0012 0018 0012 001A 0011 001A 0012 001A 0012 0018 0012 0018 0012 001A 0011 001A 003D 0018 003D 001A 003B 001A 003B 001A 0012 0018 003D 001A 003B 001A 003B 001A 003D 0018 003D 0018 0012 001A 0012 0018 0012 001A 0011 001A 0012 0018 0012 001A 0012 0018 0012 0018 003D 001A 003B 001A 003D 0018 003D 0018 003D 001A 003D 0018 06C3 ";

// Brightness:
const char incBrightnessProntoData[] PROGMEM = "0000 006D 0022 0000 0159 00AA 0018 0012 0018 0014 0016 0014 0018 0012 001A 0012 0016 0014 0018 0014 0018 0012 0018 003F 0016 003F 0018 003D 0016 003F 0018 0012 0018 003F 0018 003D 0018 003F 0016 0014 0016 0014 0018 0012 0018 0014 0016 0014 0018 0014 0016 0014 0016 0016 0016 003F 0016 003F 0016 003F 0018 003D 0018 003F 0016 003F 0016 003F 0018 003D 0018 06C3 ";
const char decBrightnessProntoData[] PROGMEM = "0000 006D 0022 0000 015B 00A7 001A 0012 0018 0012 001A 0011 001A 0012 001A 0012 0018 0012 0018 0012 001A 0011 001A 003D 0018 003D 001A 003B 001A 003B 001A 0012 0018 003D 001A 003B 001A 003D 0018 003B 001A 0012 001A 0011 001A 0012 0018 0012 001A 0011 001A 0012 0018 0012 001A 0012 0018 003D 0018 003D 001A 003B 001A 003D 0018 003D 0018 003D 001A 003D 0018 06C3 ";

// Base colors:
const char whiteProntoData[] PROGMEM = "0000 006D 0022 0000 015B 00A8 0018 0012 0018 0014 0018 0012 0018 0012 0018 0014 0018 0012 0018 0014 0016 0014 0018 003D 001A 003B 0018 003F 0018 003D 0018 0014 0016 003F 0018 003D 0018 003D 0018 003F 0016 003F 0018 003D 0018 0014 0018 0012 0018 0012 0018 0014 0016 0014 0018 0014 0018 0012 0018 0012 0018 003F 0016 003F 0018 003D 0018 003F 0016 003F 0018 06C3 ";
const char redProntoData[] PROGMEM   = "0000 006D 0022 0000 015B 00A8 001A 0011 001A 0012 0018 0012 001A 0011 001A 0012 0018 0012 001A 0012 0018 0012 001A 003B 001A 003B 001A 003B 001A 003D 0018 0012 001A 003B 001A 003D 0018 003D 001A 0011 001A 0012 0018 003D 001A 0011 001A 0012 0018 0012 001A 0011 001A 0012 0018 003D 001A 003B 001A 0012 0018 003D 001A 003B 001A 003D 0018 003D 0018 003D 0018 06C3 ";
const char greenProntoData[] PROGMEM = "0000 006D 0022 0000 015B 00A8 001A 0012 0018 0012 001A 0011 001A 0011 001A 0012 001A 0011 001A 0012 0018 0012 001A 003B 001A 003D 0018 003D 001A 003B 001A 0012 0018 003D 001A 003D 0018 003D 0018 003D 001A 0011 001A 003D 0018 0012 001A 0011 001A 0012 0018 0012 001A 0011 001A 0012 001A 003B 001A 0011 001A 003D 0018 003D 001A 003D 0018 003D 0018 003D 001A 06C3 ";
const char blueProntoData[] PROGMEM  = "0000 006D 0022 0000 015B 00A8 001A 0012 0018 0012 001A 0011 001A 0012 0018 0012 001A 0011 001A 0012 0018 0012 001A 003B 001A 003D 0018 003D 001A 003B 001A 0012 0018 003D 001A 003B 001A 003B 001A 0012 0018 003D 001A 003B 001A 0012 0018 0012 001A 0011 001A 0012 0018 0012 001A 003B 001A 0012 0018 0014 0018 003B 001A 003D 0018 003D 001A 003D 0018 003D 0018 06C3 ";
// Other colors:
const char orange1ProntoData[] PROGMEM = "0000 006D 0022 0000 0159 00A8 001A 0011 001A 0012 001A 0011 001A 0012 0018 0012 001A 0011 001A 0012 0018 0012 001A 003B 001A 003B 001A 003D 0018 003D 001A 0011 001A 003D 0018 003D 001A 003B 001A 0011 001A 0012 0018 0012 001A 003D 0018 0012 0018 0012 001A 0011 001A 0012 001A 003B 001A 003B 001A 003D 0018 0012 001A 003B 001A 003B 001A 003D 0018 003D 0018 06C3 ";
const char orange2ProntoData[] PROGMEM = "0000 006D 0022 0000 015B 00A8 0018 0012 001A 0011 001A 0012 0018 0012 001A 0012 0018 0012 001A 0011 001A 0012 0018 003D 001A 003B 001A 003B 001A 003B 001A 0012 001A 003B 001A 003B 001A 003D 0018 0012 001A 0012 0018 003D 0018 003D 001A 0011 001A 0012 0018 0012 001A 0012 0018 003B 001A 003D 0018 0012 001A 0011 001A 003D 0018 003D 001A 003B 001A 003B 001A 06C3 ";
const char yellow1ProntoData[] PROGMEM = "0000 006D 0022 0000 015B 00A7 001A 0012 0018 0012 001A 0011 001A 0012 0018 0012 001A 0011 001A 0012 001A 0011 001A 003D 0018 003D 0018 003D 001A 003B 001A 0011 001A 003D 0018 003D 001A 003B 001A 0012 0018 0012 001A 0011 001A 0012 0018 003D 001A 0011 001A 0012 0018 0014 0018 003B 001A 003D 0018 003D 001A 003D 0018 0012 0018 003D 001A 003B 001A 003D 0018 06C3 ";
const char yellow2ProntoData[] PROGMEM = "0000 006D 0022 0000 015D 00A7 001A 0012 001A 0012 0018 0011 001A 0012 001A 0012 0018 0012 001A 0011 001A 0012 0018 003D 001A 003D 0018 003B 001A 003D 001A 0011 001A 003D 0018 003D 0018 003D 001A 0011 001A 0012 001A 003D 0018 0011 001A 003D 001A 0011 001A 0012 0018 0012 001A 003B 001A 003D 0018 0012 001A 003B 001A 0012 0018 003D 001A 003D 0018 003B 001A 06C3 ";
const char green2ProntoData[] PROGMEM = "0000 006D 0022 0000 015B 00A7 001A 0012 0018 0012 001A 0011 001A 0012 001A 0011 001A 0012 0018 0012 001A 0011 001A 003D 0018 003D 0018 003D 001A 003B 001A 0012 0018 003D 001A 003B 001A 003B 001A 003D 0018 0012 001A 0011 001A 003B 001A 0012 001A 0011 001A 0012 0018 0012 001A 0011 001A 003D 0018 003D 0018 0012 001A 003B 001A 003D 0018 003D 0018 003D 001A 06C3 ";
const char blue1ProntoData[] PROGMEM = "0000 006D 0022 0000 015B 00A8 0018 0012 001A 0011 001A 0012 0018 0012 001A 0011 001A 0012 0018 0012 001A 0011 001A 003D 0018 003D 001A 003D 0018 003B 001A 0012 001A 003B 001A 003B 001A 003D 0018 003D 0018 0012 001A 003B 001A 003D 0018 0012 001A 0011 001A 0012 0018 0012 001A 0011 001A 003D 0018 0012 001A 0011 001A 003D 0018 003D 0018 003D 001A 003B 001A 06C3 ";
const char blue2ProntoData[] PROGMEM = "0000 006D 0022 0000 015B 00A7 001A 0012 001A 0011 001A 0012 0018 0012 001A 0011 001A 0012 0018 0012 001A 0011 001A 003D 0018 003D 001A 003B 001A 003D 0018 0012 001A 003B 001A 003D 0018 003B 001A 003D 001A 0011 001A 0012 0018 0012 001A 003B 001A 0012 0018 0012 001A 0012 0018 0012 001A 003D 0018 003B 001A 003D 0018 0012 001A 003D 0018 003D 0018 003D 001A 06C3 ";
const char blue3ProntoData[] PROGMEM = "0000 006D 0022 0000 015D 00A7 001A 0012 0018 0012 001A 0011 001A 0012 0018 0012 001A 0011 001A 0012 001A 0011 001A 003B 001A 003D 0018 003D 001A 003B 001A 0011 001A 003D 0018 003D 001A 003B 001A 003D 0018 0012 001A 003B 001A 0012 0018 003D 0018 0012 001A 0012 0018 0012 001A 0011 001A 003D 0018 0012 001A 003D 0018 0012 0018 003D 001A 003D 0018 003B 001A 06C3 ";
const char blue4ProntoData[] PROGMEM = "0000 006D 0022 0000 015B 00A8 001A 0011 001A 0012 001A 0011 001A 0012 0018 0012 001A 0011 001A 0012 0018 0012 001A 003B 001A 003B 001A 003D 001A 003B 001A 0011 001A 003D 0018 003D 001A 003B 001A 0011 001A 003D 001A 0011 001A 003B 001A 0012 001A 0011 001A 0012 0018 0012 001A 003D 0018 0012 0018 003D 001A 0012 0018 003B 001A 003D 0018 003D 001A 003D 0018 06C3 ";
const char violett1ProntoData[] PROGMEM = "0000 006D 0022 0000 015B 00A8 001A 0011 001A 0012 001A 0011 001A 0012 0018 0012 001A 0011 001A 0012 0018 0012 001A 003B 001A 003D 0018 003D 001A 003B 001A 0012 0018 003D 001A 003B 001A 003B 001A 0012 0018 003D 001A 003D 0018 003B 001A 0012 001A 0012 0018 0012 0018 0012 001A 003D 0018 0012 0018 0012 001A 0011 001A 003D 0018 003D 001A 003B 001A 003D 0018 06C3 ";
const char violett2ProntoData[] PROGMEM = "0000 006D 0022 0000 015B 00A7 001A 0012 0018 0012 001A 0011 001A 0012 001A 0011 001A 0011 001A 0012 001A 0011 001A 003D 0018 003D 0018 003D 001A 003B 001A 0011 001A 003D 0018 003D 001A 003B 001A 0012 0018 003D 001A 0012 0018 0012 0018 003D 001A 0012 0018 0012 001A 0011 001A 003D 0018 0012 001A 003B 001A 003B 001A 0012 0018 003D 001A 003B 001A 003D 0018 06C3 ";
const char violett3ProntoData[] PROGMEM = "0000 006D 0022 0000 0159 00A8 001A 0011 001A 0012 0018 0012 001A 0011 001A 0012 001A 0011 001A 0012 0018 0012 001A 003B 001A 003B 001A 003B 001A 003D 0018 0012 001A 003B 001A 003B 001A 003D 0018 0012 001A 003B 001A 003D 0018 0012 001A 003B 001A 0012 0018 0012 001A 0011 001A 003D 0018 0012 0018 0012 001A 003B 001A 0012 0018 003D 001A 003B 001A 003D 0018 06C3 ";

// Modes:
const char flashProntoData[] PROGMEM = "0000 006D 0022 0000 015B 00A7 001A 0012 0018 0012 001A 0011 001A 0012 001A 0011 001A 0012 0018 0012 001A 0011 001A 003B 001A 003D 0018 003D 001A 003B 001A 0012 0018 003D 0018 003D 001A 003B 001A 003B 001A 003D 0018 0012 001A 003B 001A 0012 0018 0012 001A 0011 001A 0012 0018 0012 001A 0012 0018 003D 0018 0012 001A 003D 0018 003B 001A 003D 0018 003D 0018 06C3 ";
const char strobeProntoData[] PROGMEM = "0000 006D 0022 0000 015B 00A8 001A 0011 001A 0012 0018 0012 001A 0011 001A 0012 0018 0012 001A 0011 001A 0012 0018 003D 001A 003B 001A 003B 001A 003D 0018 0012 001A 003B 001A 003B 001A 003D 0018 003D 001A 003B 001A 003B 001A 003D 0018 0014 0018 0011 001A 0012 001A 0012 0018 0012 0018 0012 001A 0011 001A 0012 001A 003B 001A 003D 0018 003D 0018 003D 001A 06C3 ";
const char fadeProntoData[] PROGMEM = "0000 006D 0022 0000 015B 00A8 001A 0011 001A 0012 0018 0012 001A 0011 001A 0012 0018 0012 001A 0011 001A 0012 0018 003D 001A 003B 001A 003B 001A 003D 0018 0012 001A 003B 001A 003B 001A 003D 0018 003D 001A 003D 0018 0011 001A 0012 001A 003D 0018 0011 001A 0012 001A 0011 001A 0012 0018 0012 001A 003B 001A 003D 0018 0012 001A 003D 0018 003B 001A 003D 0018 06C3 ";
const char smoothProntoData[] PROGMEM = "0000 006D 0022 0000 015B 00A7 001A 0012 0018 0012 001A 0011 001A 0012 001A 0011 001A 0012 0018 0012 001A 0011 001A 003D 0018 003D 001A 003B 001A 003B 001A 0012 001A 003B 001A 003B 001A 003D 0018 003D 001A 003B 001A 003B 001A 0012 001A 003B 001A 0012 0018 0012 001A 0011 001A 0012 0018 0012 001A 0012 0018 003D 001A 0012 0018 003B 001A 003D 0018 003D 001A 06C3 ";

IRsend irsend;

// --- IR Output Helpers ---
void sendProntoToPin(int pin, const char* prontoString) {
  irsend.setSendPin(pin);
  irsend.sendPronto_P(prontoString, NUMBER_OF_REPEATS);
}

void sendProntoToAll(const char* prontoString) {
  int pins[] = { IR_SEND_PIN_1, IR_SEND_PIN_2, IR_SEND_PIN_3 };
  for (int i = 0; i < 3; i++) {
    irsend.setSendPin(pins[i]);
    irsend.sendPronto_P(onProntoData, NUMBER_OF_REPEATS);
    irsend.sendPronto_P(prontoString, NUMBER_OF_REPEATS);
  }
}

// --- Forward Declarations & Context ---
class ControllerContext;

class State {
public:
  virtual ~State() {}
  virtual void enter(ControllerContext* ctx) {}
  virtual void update(ControllerContext* ctx, unsigned long currentMillis) = 0;
};

class ControllerContext {
private:
  State* currentState;

public:
  ControllerContext()
    : currentState(nullptr) {}

  void transitionTo(State* newState) {
    if (currentState != nullptr) {
      delete currentState;
    }
    currentState = newState;
    if (currentState != nullptr) {
      currentState->enter(this);
    }
  }

  void update(unsigned long currentMillis) {
    if (currentState != nullptr) {
      currentState->update(this, currentMillis);
    }
  }
};

// --- Traffic Light Duration Constants (ms) ---
const unsigned long TRAFFIC_MAX_DURATION = 15000;
const unsigned long TRAFFIC_RED_MIN_DURATION = 4000;
const unsigned long TRAFFIC_YELLOW_MIN_DURATION = 1500;
const unsigned long TRAFFIC_GREEN_MIN_DURATION = 4000;

// --- Traffic Light State Classes ---
class TrafficYellowState;
class TrafficGreenState;

class TrafficRedState : public State {
private:
  unsigned long previousMillis = 0;
  unsigned long duration = 0;
public:
  void enter(ControllerContext* ctx) override {
    sendProntoToPin(IR_SEND_PIN_2, offProntoData);
    sendProntoToPin(IR_SEND_PIN_3, offProntoData);

    sendProntoToPin(IR_SEND_PIN_1, onProntoData);
    sendProntoToPin(IR_SEND_PIN_1, redProntoData);

    duration = random(TRAFFIC_RED_MIN_DURATION, TRAFFIC_MAX_DURATION + 1);
    Serial.print("Update RED (duration: ");
    Serial.print(duration);
    Serial.println(" ms)");

    previousMillis = millis();
  }
  void update(ControllerContext* ctx, unsigned long currentMillis) override;
};

class TrafficYellowState : public State {
private:
  unsigned long previousMillis = 0;
  unsigned long duration = 0;
  bool nextIsRed;
public:
  TrafficYellowState(bool toRed)
    : nextIsRed(toRed) {}
  void enter(ControllerContext* ctx) override {
    sendProntoToPin(IR_SEND_PIN_1, offProntoData);
    sendProntoToPin(IR_SEND_PIN_3, offProntoData);

    sendProntoToPin(IR_SEND_PIN_2, onProntoData);
    sendProntoToPin(IR_SEND_PIN_2, whiteProntoData);

    duration = random(TRAFFIC_YELLOW_MIN_DURATION, TRAFFIC_MAX_DURATION + 1);
    Serial.print("Update YELLOW (duration: ");
    Serial.print(duration);
    Serial.println(" ms)");

    previousMillis = millis();
  }
  void update(ControllerContext* ctx, unsigned long currentMillis) override;
};

class TrafficGreenState : public State {
private:
  unsigned long previousMillis = 0;
  unsigned long duration = 0;
public:
  void enter(ControllerContext* ctx) override {
    sendProntoToPin(IR_SEND_PIN_1, offProntoData);
    sendProntoToPin(IR_SEND_PIN_2, offProntoData);

    sendProntoToPin(IR_SEND_PIN_3, onProntoData);
    sendProntoToPin(IR_SEND_PIN_3, greenProntoData);

    duration = random(TRAFFIC_GREEN_MIN_DURATION, TRAFFIC_MAX_DURATION + 1);
    Serial.print("Update GREEN (duration: ");
    Serial.print(duration);
    Serial.println(" ms)");

    previousMillis = millis();
  }
  void update(ControllerContext* ctx, unsigned long currentMillis) override;
};

void TrafficRedState::update(ControllerContext* ctx, unsigned long currentMillis) {
  if (currentMillis - previousMillis >= duration) {
    ctx->transitionTo(new TrafficYellowState(false));
  }
}

void TrafficYellowState::update(ControllerContext* ctx, unsigned long currentMillis) {
  if (currentMillis - previousMillis >= duration) {
    if (nextIsRed) ctx->transitionTo(new TrafficRedState());
    else ctx->transitionTo(new TrafficGreenState());
  }
}

void TrafficGreenState::update(ControllerContext* ctx, unsigned long currentMillis) {
  if (currentMillis - previousMillis >= duration) {
    ctx->transitionTo(new TrafficYellowState(true));
  }
}

// --- Party State Classes ---
class PartyStrobeState;
class PartyFadeState;
class PartySmoothState;

class PartyFlashState : public State {
private:
  unsigned long previousMillis = 0;
public:
  void enter(ControllerContext* ctx) override {
    sendProntoToAll(flashProntoData);
    Serial.println("Party FLASH");
    previousMillis = millis();
  }
  void update(ControllerContext* ctx, unsigned long currentMillis) override;
};

class PartyStrobeState : public State {
private:
  unsigned long previousMillis = 0;
public:
  void enter(ControllerContext* ctx) override {
    sendProntoToAll(strobeProntoData);
    Serial.println("Party STROBE");
    previousMillis = millis();
  }
  void update(ControllerContext* ctx, unsigned long currentMillis) override;
};

class PartyFadeState : public State {
private:
  unsigned long previousMillis = 0;
public:
  void enter(ControllerContext* ctx) override {
    sendProntoToAll(fadeProntoData);
    Serial.println("Party FADE");
    previousMillis = millis();
  }
  void update(ControllerContext* ctx, unsigned long currentMillis) override;
};

class PartySmoothState : public State {
private:
  unsigned long previousMillis = 0;
public:
  void enter(ControllerContext* ctx) override {
    sendProntoToAll(smoothProntoData);
    Serial.println("Party SMOOTH");
    previousMillis = millis();
  }
  void update(ControllerContext* ctx, unsigned long currentMillis) override;
};

void PartyFlashState::update(ControllerContext* ctx, unsigned long currentMillis) {
  if (currentMillis - previousMillis >= 4000) ctx->transitionTo(new PartyStrobeState());
}
void PartyStrobeState::update(ControllerContext* ctx, unsigned long currentMillis) {
  if (currentMillis - previousMillis >= 6000) ctx->transitionTo(new PartyFadeState());
}
void PartyFadeState::update(ControllerContext* ctx, unsigned long currentMillis) {
  if (currentMillis - previousMillis >= 10000) ctx->transitionTo(new PartySmoothState());
}
void PartySmoothState::update(ControllerContext* ctx, unsigned long currentMillis) {
  if (currentMillis - previousMillis >= 12000) ctx->transitionTo(new PartyFlashState());
}

// --- Concert State Classes ---
class ConcertSmoothState : public State {
public:
  void enter(ControllerContext* ctx) override {
    sendProntoToAll(smoothProntoData);
    Serial.println("Concert SMOOTH");
  }
  void update(ControllerContext* ctx, unsigned long currentMillis) override {
    // Remains in Concert Smooth mode until switch changes
  }
};

// --- Switch Reading & Debounce ---

SwitchPosition readSwitchPosition() {
  int leftState = digitalRead(SWITCH_PIN_LEFT);
  int rightState = digitalRead(SWITCH_PIN_RIGHT);

  if (leftState == LOW) {
    return SWITCH_POS_LEFT;
  } else if (rightState == LOW) {
    return SWITCH_POS_RIGHT;
  } else {
    return SWITCH_POS_CENTER;
  }
}

SwitchPosition currentSwitchPos = SWITCH_POS_UNKNOWN;
SwitchPosition lastReading = SWITCH_POS_UNKNOWN;
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_DELAY = 50; // ms

// --- Setup & Main Loop ---
ControllerContext systemContext;

void setup() {
  // Enable internal pull-up resistors for both input pins
  pinMode(SWITCH_PIN_LEFT, INPUT_PULLUP);
  pinMode(SWITCH_PIN_RIGHT, INPUT_PULLUP);

  Serial.begin(115200);
  randomSeed(analogRead(A0));

  irsend.begin(0);

  sendProntoToAll(onProntoData);

  // Initialize into mode based on initial switch position
  currentSwitchPos = readSwitchPosition();
  lastReading = currentSwitchPos;
  lastDebounceTime = millis();

  if (currentSwitchPos == SWITCH_POS_LEFT) {
    Serial.println(F("Initial Switch -> LEFT: Traffic Light Mode"));
    systemContext.transitionTo(new TrafficRedState());
  } else if (currentSwitchPos == SWITCH_POS_RIGHT) {
    Serial.println(F("Initial Switch -> RIGHT: Party Mode"));
    systemContext.transitionTo(new PartyFlashState());
  } else {
    Serial.println(F("Initial Switch -> CENTER: Concert Smooth Mode"));
    systemContext.transitionTo(new ConcertSmoothState());
  }
}

void loop() {
  unsigned long currentMillis = millis();

  // 1. Debounced Switch Reading & State Transition
  SwitchPosition reading = readSwitchPosition();

  if (reading != lastReading) {
    lastDebounceTime = currentMillis;
    lastReading = reading;
  }

  if ((currentMillis - lastDebounceTime) >= DEBOUNCE_DELAY) {
    if (reading != currentSwitchPos) {
      currentSwitchPos = reading;

      if (currentSwitchPos == SWITCH_POS_LEFT) {
        Serial.println(F("Switch flipped to LEFT -> Traffic Light Mode"));
        systemContext.transitionTo(new TrafficRedState());
      } else if (currentSwitchPos == SWITCH_POS_RIGHT) {
        Serial.println(F("Switch flipped to RIGHT -> Party Mode"));
        systemContext.transitionTo(new PartyFlashState());
      } else {
        Serial.println(F("Switch in CENTER -> Concert Smooth Mode"));
        systemContext.transitionTo(new ConcertSmoothState());
      }
    }
  }

  // 2. State Machine Update
  systemContext.update(currentMillis);
}