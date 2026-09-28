#undef IR_SEND_PIN
#include <IRremote.hpp>

// --- Pin Definitions ---
const int IR_RECEIVE_PIN = 2;
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

// State:
const char modeAProntoData[] PROGMEM = "0000 006D 0022 0000 0159 00A8 001A 0012 0018 0012 001A 0012 0018 0012 0018 0012 001A 0012 0018 0012 001A 0011 001A 003D 0018 003D 0018 003F 0016 003D 001A 0012 0018 003D 0018 003F 0018 003D 0018 0012 0018 0014 0018 0012 0018 0014 0016 0014 0018 0012 0018 0014 0016 0014 0018 003D 0018 003F 0016 003F 0018 003D 0018 003D 0018 003F 0016 003F 0018 003D 0018 06C3 ";
const char modeBProntoData[] PROGMEM = "0000 006D 0022 0000 015B 00A8 001A 0011 001A 0012 0018 0012 001A 0012 0018 0012 001A 0011 001A 0012 0018 0012 001A 003B 001A 003D 0018 003D 001A 003B 001A 0012 0018 003D 001A 003B 001A 003B 001A 003D 001A 0011 001A 0012 0018 0012 001A 0012 0018 0012 001A 0011 001A 0012 0018 0012 001A 003B 001A 003D 0018 003D 001A 003D 0018 003D 0018 003D 0018 003D 001A 06C3 ";

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

// Extract NEC Command directly from a standard Pronto String for fast matching
uint8_t extractCommandFromPronto(const char* prontoStr) {
  // Parses the 6th hex byte from the end of a standard NEC Pronto string
  // For standard NEC Pronto format, this extracts the payload byte
  uint32_t val = strtoul(&prontoStr[strlen(prontoStr) - 30], NULL, 16);
  return (uint8_t)(val & 0xFF);
}

// Compare decoded IR signal against a Pronto String
bool isProntoMatch(const char* prontoTarget) {
  // Reads incoming NEC command and compares against target Pronto string payload
  uint8_t targetCmd = extractCommandFromPronto(prontoTarget);
  return (IrReceiver.decodedIRData.command == targetCmd);
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

// --- State Classes ---
class TrafficYellowState;
class TrafficGreenState;

class TrafficRedState : public State {
private:
  unsigned long previousMillis = 0;
public:
  void enter(ControllerContext* ctx) override {
    sendProntoToPin(IR_SEND_PIN_2, offProntoData);
    sendProntoToPin(IR_SEND_PIN_3, offProntoData);

    sendProntoToPin(IR_SEND_PIN_1, onProntoData);
    sendProntoToPin(IR_SEND_PIN_1, redProntoData);
    Serial.println("Update RED");
    previousMillis = millis();
  }
  void update(ControllerContext* ctx, unsigned long currentMillis) override;
};

class TrafficYellowState : public State {
private:
  unsigned long previousMillis = 0;
  bool nextIsRed;
public:
  TrafficYellowState(bool toRed)
    : nextIsRed(toRed) {}
  void enter(ControllerContext* ctx) override {
    sendProntoToPin(IR_SEND_PIN_1, offProntoData);
    sendProntoToPin(IR_SEND_PIN_3, offProntoData);

    sendProntoToPin(IR_SEND_PIN_2, onProntoData);
    sendProntoToPin(IR_SEND_PIN_2, whiteProntoData);

    Serial.println("Update YELLOW");
    previousMillis = millis();
  }
  void update(ControllerContext* ctx, unsigned long currentMillis) override;
};

class TrafficGreenState : public State {
private:
  unsigned long previousMillis = 0;
public:
  void enter(ControllerContext* ctx) override {
    sendProntoToPin(IR_SEND_PIN_1, offProntoData);
    sendProntoToPin(IR_SEND_PIN_2, offProntoData);

    sendProntoToPin(IR_SEND_PIN_3, onProntoData);
    sendProntoToPin(IR_SEND_PIN_3, greenProntoData);
    Serial.println("Update GREEN");
    previousMillis = millis();
  }
  void update(ControllerContext* ctx, unsigned long currentMillis) override;
};

void TrafficRedState::update(ControllerContext* ctx, unsigned long currentMillis) {
  if (currentMillis - previousMillis >= 4000) {
    ctx->transitionTo(new TrafficYellowState(false));
  }
}

void TrafficYellowState::update(ControllerContext* ctx, unsigned long currentMillis) {
  if (currentMillis - previousMillis >= 1500) {
    if (nextIsRed) ctx->transitionTo(new TrafficRedState());
    else ctx->transitionTo(new TrafficGreenState());
  }
}

void TrafficGreenState::update(ControllerContext* ctx, unsigned long currentMillis) {
  if (currentMillis - previousMillis >= 4000) {
    ctx->transitionTo(new TrafficYellowState(true));
  }
}

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

// --- Setup & Main Loop ---
ControllerContext systemContext;

void setup() {
  Serial.begin(115200);

  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
  irsend.begin(0);

  sendProntoToAll(onProntoData);
  // Initialize into Traffic-Light Mode
  systemContext.transitionTo(new TrafficRedState());
  // Initialize into Party Mode
  // systemContext.transitionTo(new PartyFadeState());
  
}

void loop() {
  unsigned long currentMillis = millis();

  // 1. Process Receiver Signals & Compare directly against Pronto Strings
  if (IrReceiver.decode()) {
    if (!(IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT)) {
      //Serial.println("Got DATA");
      //IrReceiver.compensateAndPrintIRResultAsPronto(&Serial);
      // Compare incoming signal against Pronto string definitions
      //if (isProntoMatch(modeAProntoData)) {
      if (IrReceiver.decodedIRData.address == 0xEF00 && IrReceiver.decodedIRData.command == 0x0) {
        Serial.println(F("Matched PRONTO_CMD_MODE_A -> Switching to Traffic Light"));
        systemContext.transitionTo(new TrafficRedState());
//      } else if (isProntoMatch(modeBProntoData)) {
      } else if (IrReceiver.decodedIRData.address == 0xEF00 && IrReceiver.decodedIRData.command == 0x1) {
        Serial.println(F("Matched PRONTO_CMD_MODE_B -> Switching to Party Mode"));
        systemContext.transitionTo(new PartyFlashState());
      }
    }
    IrReceiver.resume();
  }

  // 2. State Machine Update
  systemContext.update(currentMillis);
}