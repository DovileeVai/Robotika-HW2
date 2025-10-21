#include <LiquidCrystal.h>
#include <EEPROM.h>   

// LED'ai
#define LED1_PIN 3
#define LED2_PIN 4
#define LED3_PIN 5
#define LED4_PIN 6
#define LED5_PIN 7

// Mygtukas
#define BUTTON_PIN 2

// Buzzeris
#define BUZZER_PIN 8

const int leds[5] = {LED1_PIN, LED2_PIN, LED3_PIN, LED4_PIN, LED5_PIN};

// RS=12, E=11, D4=A0, D5=A1, D6=A2, D7=A3
LiquidCrystal lcd(12, 11, A0, A1, A2, A3);

// ---------- Mygtuko interrupt'as + debouncing -----
volatile byte buttonReleased = false; // Ivyko mygtuko krastas

const unsigned long DEBOUNCE_MS = 20;  // debounce laikas 20 ms
bool btnStable = HIGH;     // paskutine stabili mygtuko busena
bool btnRawPrev = HIGH;    // ankstesne busena pokycio aptikimui
unsigned long btnLastChange = 0;  // kada paskutini karta pasikeite


// ---------------- EEPROM ----------------
struct Stats {
  byte     magic;   // 0xA5 magic baitas
  byte     version;  // 0x01 versijos numeris
  uint16_t bestMs;   // geriausias laikas (ms)
  uint16_t attempts; // atliktu bandymu sk
};

// nustatomos magic ir version reiksmes
const byte MAGIC = 0xA5, VERSION = 0x01;
const int EEPROM_ADDR = 0;  // pradzios adresas
Stats st;                   


// ------------- Timer1 ---------------------
volatile bool every10ms = false, every1s = false;

// didina counteri c kas milisekunde ir kas 1 s pakelia veliava
ISR(TIMER1_COMPA_vect) {
  static uint16_t c = 0;
  c++;
  if (c >= 1000) {
    every1s = true;
    c = 0;  // kas 1 s
  }
}


// --------------- State machine (busenu masina) ----------
enum Phase { PRADZIA, LEMPUTES, ATSITIKTINIS, GO_LAUKIMAS, RODYMAS, JUMP, RESET };
Phase phase = PRADZIA;  // pradine busena

int           ledIndex = 0;       
unsigned long deadline = 0;
unsigned long tGo = 0;            // GO momentas
uint16_t      attempts = 0;
uint16_t      best = 0;

int           showPage = 0;       // kuris puslapis matomas dabar (0,1,...)
unsigned long showCycleMs = 1500; // kas kiek ms keisti puslapi
unsigned long lastResultMs = 0;   // paskutinio bandymo rezultatas (ms)

// ----------------- RESET'as --------------------
const unsigned long RESET_HOLD_MS = 2000; // laikas, kiek laikyti mygtuka, kad nusiresetintu
const unsigned long RESET_SHOW_MS = 1500; // kiek rodyti "RESET DONE"

// jump start veliava, kad vienkartiniai veiksmai vyktu tik karta
bool jump = false;

void setup() {
  for (int i = 0; i < 5; i++){
    pinMode(leds[i], OUTPUT);
    digitalWrite(leds[i], LOW);
  }

  pinMode(BUTTON_PIN, INPUT_PULLUP); // paspaustas - LOW|nepaspaustas - HIGH
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonReleasedInterrupt, CHANGE);
  
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  // EEPROM
  statsLoad();
  best = st.bestMs;
  attempts = st.attempts;

  // Timer1
  timer1Start();  // paleidziam laikmati

  lcd.begin(16, 2);
  startMsg();  // pradinis ekranas
}

void loop() {
 
  if (every1s) {
    every1s = false;
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));  // heartbeat
  } 

  // nuskaitom dabartini laika nuo paleidimo
  unsigned long now = millis();

  // stebejimui visa laika, ar nera norima reset'inti
  resetWatch(now);

  if (phase == RESET) {
    static bool didInit = false;
    if (!didInit) {
      doReset();
      didInit = true;
    }
    if (millis() >= deadline && !buttonIsDown()) {
      startMsg();
      didInit = false;
      phase = PRADZIA;
    }
    return;
  }

  switch (phase) {
    case PRADZIA:
      if (buttonPressedEdge()){
        ledIndex = 0;
        deadline = now + 500; // kas 500 ms nauja lempute
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Lights...");
        phase = LEMPUTES;
      }
      break;
    
    case LEMPUTES:
      // JUMP START per lepmputes
      if (buttonPressedEdge()) {
        jump = false;
        phase = JUMP;
        break;
      }
      // Laikas ijungti kita LED'a?
      if (now >= deadline) {
        digitalWrite(leds[ledIndex], HIGH);
        shortBeep();
        ledIndex++;
        if (ledIndex >= 5) {
          deadline = now + (unsigned long)random(1000, 4000);
          phase = ATSITIKTINIS;
        } else {
          deadline = now + 500;
        }
      }
      break;
    
    case ATSITIKTINIS:
      // JUMP START per atsitiktini laukima
      if (buttonPressedEdge()) {
        jump = false;
        phase = JUMP;
        break;
      }
      if (now >= deadline) {
        // GO: visos lemputes isjungiamos
        for (int i = 0; i < 5; i++) {
          digitalWrite(leds[i], LOW);
        }
        longBeep();
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("GO! Press!");
        tGo = now;
        phase = GO_LAUKIMAS;
      }
      break;

    case GO_LAUKIMAS:
      // laukiame vienkartinio paspaudimo
      if (buttonPressedEdge()) {
        unsigned long result = millis() - tGo;
        lastResultMs = result;
        attempts++;
        if (best == 0 || result < best) {
          best = (uint16_t)result; 
        }
        statsSaveIfChanged(best, attempts);
        
        showPage = 0;
        drawShowPage(showPage, best, attempts, lastResultMs);
        deadline = millis() + showCycleMs; // rodom apie 1.5 s
        phase = RODYMAS;
      }
      break;

    case RODYMAS:
      // jei paspausta – grįžtam į pradžią
      if (buttonPressedEdge()) {
        startMsg();
        phase = PRADZIA;
        break;
      }

      // kas showCycleMs perjungiame puslapį
      if (millis() >= deadline) {
        showPage++;
        drawShowPage(showPage, best, attempts, lastResultMs);
        deadline = millis() + showCycleMs;
      }
      break;

    case JUMP:
      if (!jump) {
        for (int i = 0; i < 5; i++) {
          digitalWrite(leds[i], LOW);
        }
        longBeep();
        attempts++;
        statsSaveIfChanged(best, attempts);

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("JUMP START!");
        lcd.setCursor(0, 1);
        lcd.print("Wait and retry");
        deadline = millis() + 1500;
        jump = true;
      }
      if (millis() >= deadline) {
        // griztam i pradzia
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Best:");
        if (best == 0) {
          lcd.print("--");
        } else {
          lcd.print(best);
        }
        lcd.print(" Att:");
        lcd.print(attempts);
        lcd.setCursor(0, 1);
        lcd.print("Press to start");
        phase = PRADZIA;
      }
      break;
  }
}

//------------------ Buzzerio garsas---------
void shortBeep () {
  tone(BUZZER_PIN, 2500, 100);
}

void longBeep () {
  tone(BUZZER_PIN, 400, 400);
}

// ----------------- EEPROM---------------
void statsLoad() {
  EEPROM.get(EEPROM_ADDR, st);
  bool bad = (st.magic != MAGIC || st.version != VERSION);
  // jei duomenys nesutampa su nurodytu formatu, irasomi defaultiniai duomenys
  if (bad) {
    st.magic = MAGIC;
    st.version = VERSION;
    st.bestMs = 0;
    st.attempts = 0;
    EEPROM.put(EEPROM_ADDR, st);
  }
}

// Irasom tik, jei kas nors pasikeite
void statsSaveIfChanged(uint16_t newBest, uint16_t newAttempts) {
  bool need = false;
  if (st.bestMs != newBest) {
    st.bestMs = newBest;
    need = true;
  }
  if (st.attempts != newAttempts) {
    st.attempts = newAttempts;
    need = true;
  }
  if (need) {
    EEPROM.put(EEPROM_ADDR, st);
  }
}

// -------------Mygtukas ---------------
void buttonReleasedInterrupt() {
  buttonReleased = true;
}

bool readButtonDebounced() {
  bool raw = digitalRead(BUTTON_PIN);

  if (raw != btnRawPrev || buttonReleased) {
    btnRawPrev = raw;
    btnLastChange = millis();
  }

  if ((millis() - btnLastChange) > DEBOUNCE_MS) {
    btnStable = raw;
  }

  buttonReleased = false;
  return btnStable;
}

bool buttonPressedEdge() {
  static bool prev = HIGH;
  bool now = readButtonDebounced();
  bool edge = (prev == HIGH && now == LOW);
  prev = now;
  return edge;
}

inline bool buttonIsDown () {
  readButtonDebounced();
  return btnStable == LOW;
}

// ----------Timer-------------
void timer1Start() {
  cli();                          // Isjungia interruptus
  TCCR1A=0;                       
  TCCR1B=0;                       
  OCR1A = 249;                    // 16MHz(16 000 000)/64/1000 - 1 (250-1=249)
  TCCR1B |= (1<<WGM12);           // CTC, kai pasiekia 249, grizta i nuli
  TCCR1B |= (1<<CS11)|(1<<CS10);  // prescaler 64
  TIMSK1 |= (1<<OCIE1A);          // enable
  sei();                          // Ijungia interruptus
}

// ------------------ Rodymas LCD ------------
void drawShowPage (uint8_t page, uint16_t best, uint16_t attempts, unsigned long lastMs) {
  lcd.clear();
  switch (page % 2) {
    case 0:
      lcd.setCursor(0, 0);
      lcd.print("Time:");
      lcd.print(lastMs);
      lcd.print("ms");
      lcd.setCursor(0, 1);
      lcd.print("Best:");
      if (best == 0) {
        lcd.print("--");
      } else {
        lcd.print(best);
      }
      lcd.print(" Att:");
      lcd.print(attempts);
      break;

    case 1:
      lcd.setCursor(0, 0);
      lcd.print("Ready for next?");
      lcd.setCursor(0, 1);
      lcd.print("Press the button");
      break;
  }
}

void startMsg() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Best:");
  if (best==0) {
    lcd.print("--"); 
  } else {
    lcd.print(best);
  }
  lcd.print(" Att:"); lcd.print(attempts);
  lcd.setCursor(0, 1); lcd.print("Press to start");
}
  
// ---------------- Resetinimas-------------------
void enterReset(unsigned long now) {
  // nustatom, kiek rodysim RESET ekrana; pati RESET busena padarys nulinima ir piesima
  deadline = now + RESET_SHOW_MS;
  phase = RESET;
}

void resetWatch (unsigned long now) {
  // jei bet kur laikomas mygtukas >= RESET_HOLD_MS -> eneterReset()
  static bool counting = false;
  static unsigned long t0 = 0;

  if (buttonIsDown()) {
    if (!counting) {
      counting = true;
      t0 = now;
    }
    if ((now - t0) >= RESET_HOLD_MS && phase != RESET) {
      counting = false;
      enterReset(now);
    }
  } else {
    counting = false;
  }
}

void doReset() { 
  best = 0; 
  attempts = 0;
  statsSaveIfChanged(best, attempts); 

  for (int i = 0; i < 5; i++) { 
    digitalWrite(leds[i], LOW); 
    } 
  lcd.clear(); 
  lcd.setCursor(0, 0); 
  lcd.print("Stats RESET"); 
  lcd.setCursor(0, 1); 
  lcd.print("Best:--ms Att: 0"); 
}