#include <M5Unified.h>
#include <Preferences.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "web_page.h"
#include "logo_b64.h"

M5Canvas sprite(&M5.Display);
Preferences prefs;
AsyncWebServer server(80);

const int SCREEN_W = 240;
const int SCREEN_H = 135;
const int FACE_SIZE = 12;

// ---------- WI-FI ----------
const char* WIFI_SSID = "WRENCH_DEDSEC";
const char* WIFI_PASS = "dedsec2016";
const unsigned long WIFI_TIMEOUT_MS = 300000;  // 5 мин без запросов -> выкл
bool wifiActive = false;
unsigned long lastWebMs = 0;
bool aLongUsed = false;

enum Emotion {
  E_DEAD=0, E_DIZZY, E_WINK, E_GRIT, E_SASS, E_STAR,
  E_HAPPY, E_ANGRY, E_SAD, E_MEH, E_LOOPY, E_CALM,
  E_PAIN, E_CONFUSED, E_SLEEPY, E_CRY, E_ALERT,
  E_COUNT
};
enum Mode { M_AUTO=0, M_BUTTON, M_MIC, M_CAR, M_COUNT };

const char* MODE_NAMES[M_COUNT] = { "AUTO: timer", "BTN: manual", "MIC: music", "CAR: drive" };

const unsigned long AUTO_MIN_MS = 8000;
const unsigned long AUTO_MAX_MS = 15000;
const unsigned long MIC_MIN_MS  = 7000;
const unsigned long MIC_MAX_MS  = 12000;

const char* FACE_STR[E_COUNT] = {
  "X X",   "@ @",   "O o",   "# #",   "~ ^",
  "* *",   "^ ^",   "\\ /",  "/ \\",  "= =",
  "9 9",   "- -",   "> <",   "? ?",   "Z Z",
  "; ;",   "! !"
};

const char* BLINK_STYLES[] = { "- -", "~ ~" };
const char* blinkStyle = BLINK_STYLES[0];
unsigned long blinkDuration = 120;

int gazeX = 0;
enum LookState { LOOK_NONE, LOOK_LEFT, LOOK_RIGHT };
LookState lookState = LOOK_NONE;
unsigned long lookStart = 0;

// ---------- МИКРОФОН ----------
float micSens = 0.5;
int thLow = 40, thHigh = 200, scareEnterPct = 66, bright = 180;
const int SCARE_EXIT_PCT = 40;
const unsigned long SCARE_MS = 2000;
const Emotion CAT_QUIET[] = { E_SLEEPY, E_CALM, E_MEH, E_SAD };
const Emotion CAT_MED[]   = { E_HAPPY, E_SASS, E_STAR, E_WINK, E_LOOPY };
const Emotion CAT_LOUD[]  = { E_ALERT, E_ANGRY, E_PAIN, E_GRIT, E_DIZZY };

int16_t micBuf[256];
int volSmooth = 0;
int volPct = 0;
int micCategory = -1;
int catCandidate = -1;
unsigned long catCandidateSince = 0;
unsigned long lastMicSwitch = 0;
unsigned long micIntervalMs = 7000;

unsigned long scareUntil = 0;
unsigned long scareStart = 0;
bool scareArmed = true;

// ---------- IMU / CAR ----------
const bool  SWAP_AXES  = true;
const int   FWD_SIGN   = 1;
const int   LAT_SIGN   = 1;
const float VIB_TH     = 0.03;
const float IMPACT_TH  = 0.9;
const float BRAKE_TH   = 0.22;   // городское торможение ~0.2-0.4g
const float ACCEL_TH   = 0.15;   // городской разгон ~0.15-0.3g
const float GAZE_K     = 60.0;
const unsigned long STILL_MS = 90000;

float baseFwd = 0, baseLat = 0, carGaze = 0;
float fwdSmooth = 0, latSmooth = 0;
bool imuInited = false;
bool carAsleep = false;
unsigned long accelUntil = 0;
unsigned long lastMotionMs = 0;
unsigned long brakeUntil = 0;
unsigned long carScareUntil = 0;
unsigned long carScareStart = 0;
unsigned long carScareCooldown = 0;

// ---------- КАЛИБРОВКА IMU «НА НОЛЬ» ----------
bool calibActive = false;
bool hasCalib = false;
unsigned long calibStart = 0;
float calibSumF = 0, calibSumL = 0;
int calibN = 0;
const unsigned long CALIB_MS = 1500;   // 1.5 сек стоим и собираем «ноль»

// ---------- СОСТОЯНИЕ ----------
Mode mode = M_AUTO;
Emotion emotion = E_HAPPY;
bool flipped = false;
bool needRedraw = true;
unsigned long lastSwitchMs = 0;
unsigned long autoIntervalMs = 8000;

bool blinking = false;
unsigned long blinkStartMs = 0;
unsigned long nextBlinkMs = 2500;
bool bLongUsed = false;

// ---------- ОТРИСОВКА ----------
void drawFaceStr(const char* s, uint16_t bg) {
  sprite.fillSprite(bg);
  sprite.setFont(&fonts::Font0);
  sprite.setTextSize(FACE_SIZE);
  sprite.setTextColor(TFT_WHITE);
  int w = sprite.textWidth(s);
  int h = 8 * FACE_SIZE;
  sprite.drawString(s, (SCREEN_W - w) / 2 + gazeX, (SCREEN_H - h) / 2);
}

void drawEyes(Emotion e, bool closed, uint16_t bg) {
  if (closed) drawFaceStr(blinkStyle, bg);
  else        drawFaceStr(FACE_STR[e], bg);
}

void glitchFx(int frames) {
  const char* GL = "#@%&$*+=:;/\\|~";
  sprite.setFont(&fonts::Font0);
  sprite.setTextSize(2);
  sprite.setTextColor(TFT_WHITE);
  for (int f = 0; f < frames; f++) {
    sprite.fillSprite(TFT_BLACK);
    for (int i = 0; i < 36; i++) {
      char buf[2] = { GL[random(14)], 0 };
      sprite.drawString(buf, random(SCREEN_W - 16), random(SCREEN_H - 16));
    }
    sprite.pushSprite(0, 0);
    delay(25);
  }
}

void bootScreen() {
  sprite.setFont(&fonts::Font0);
  sprite.setTextColor(TFT_WHITE);
  sprite.setTextSize(2);
  sprite.fillSprite(TFT_BLACK);
  sprite.drawString("DEDSEC_OS v2.0", 20, 12);
  sprite.drawString("loading faces...", 20, 36);
  sprite.pushSprite(0, 0);
  delay(700);
  char line[18];
  for (int i = 0; i <= 14; i++) {
    line[0] = '[';
    for (int j = 0; j < 14; j++) line[1 + j] = (j < i) ? '#' : '-';
    line[15] = ']'; line[16] = 0;
    sprite.fillRect(0, 66, SCREEN_W, 20, TFT_BLACK);
    sprite.drawString(line, 20, 68);
    sprite.pushSprite(0, 0);
    delay(110);
  }
  delay(350);
  glitchFx(5);
}

void drawVolumeBar() {
  int w = constrain(volSmooth / 4, 0, SCREEN_W);
  sprite.drawFastHLine(0, SCREEN_H - 3, SCREEN_W, TFT_DARKGREY);
  sprite.drawFastHLine(0, SCREEN_H - 3, w, TFT_WHITE);
}

void showToast(const char* txt) {
  drawEyes(emotion, false, TFT_BLACK);
  sprite.setTextSize(2);
  sprite.fillRect(0, 0, SCREEN_W, 20, TFT_BLACK);
  sprite.drawString(txt, 8, 2);
  sprite.pushSprite(0, 0);
  delay(900);
  needRedraw = true;
}

// ---------- WI-FI (ASYNC) ----------
void wifiOn() {
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_OFF);
  delay(100);
  WiFi.mode(WIFI_AP);
  delay(200);
  WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
  WiFi.setSleep(true);
  WiFi.setTxPower(WIFI_POWER_15dBm);
  delay(100);
  WiFi.softAP(WIFI_SSID, WIFI_PASS, 1, 0, 4);
  delay(500);

  server.begin();
  wifiActive = true;
  lastWebMs = millis();
  showToast("WI-FI ON");
  Serial.println("WIFI ON, IP: 192.168.4.1");
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
}

void wifiOff() {
  server.end();
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);
  wifiActive = false;
  showToast("WI-FI OFF");
}

String faceJson(const char* s) {
  String r;
  for (const char* p = s; *p; p++) { if (*p == '\\') r += "\\\\"; else r += *p; }
  return r;
}

// ---------- МИКРОФОН ----------
void pickFromCategory(int cat) {
  const Emotion* arr; int n;
  switch (cat) {
    case 0:  arr = CAT_QUIET; n = sizeof(CAT_QUIET)/sizeof(CAT_QUIET[0]); break;
    case 1:  arr = CAT_MED;   n = sizeof(CAT_MED)/sizeof(CAT_MED[0]);     break;
    default: arr = CAT_LOUD;  n = sizeof(CAT_LOUD)/sizeof(CAT_LOUD[0]);   break;
  }
  Emotion next;
  do { next = arr[random(n)]; } while (n > 1 && next == emotion);
  emotion = next;
}

void updateVolume(unsigned long now) {
  if (!M5.Mic.record(micBuf, 256, 16000)) return;
  float sum = 0;
  for (int i = 0; i < 256; i++) sum += (float)micBuf[i] * micBuf[i];
  int rms = sqrt(sum / 256) * micSens;
  volSmooth = volSmooth * 0.7 + rms * 0.3;
  volPct = constrain((volSmooth * 100) / 960, 0, 100);

  if (scareArmed && volPct >= scareEnterPct) {
    scareArmed = false;
    scareStart = now;
    scareUntil = now + SCARE_MS;
    glitchFx(2);
  } else if (!scareArmed && volPct <= SCARE_EXIT_PCT) {
    scareArmed = true;
  }
}

int detectCat(int prev) {
  if (prev == 0) return (volSmooth > thLow) ? 1 : 0;
  if (prev == 1) {
    if (volSmooth > thHigh) return 2;
    if (volSmooth < thLow * 7 / 10) return 0;
    return 1;
  }
  if (prev == 2) return (volSmooth < thHigh * 7 / 10) ? 1 : 2;
  if (volSmooth < thLow) return 0;
  if (volSmooth < thHigh) return 1;
  return 2;
}

void micCategoryLogic(unsigned long now) {
  int cat = detectCat(micCategory);
  if (cat != micCategory) {
    if (cat != catCandidate) { catCandidate = cat; catCandidateSince = now; }
    else if (now - catCandidateSince > 600) {
      micCategory = cat;
      glitchFx(2);
      pickFromCategory(cat);
      lastMicSwitch = now;
      micIntervalMs = random(MIC_MIN_MS, MIC_MAX_MS);
    }
  } else if (now - lastMicSwitch >= micIntervalMs) {
    lastMicSwitch = now;
    micIntervalMs = random(MIC_MIN_MS, MIC_MAX_MS);
    pickFromCategory(cat);
  }
}

// ---------- IMU: МАШИНА ЧУВСТВУЕТ + КАЛИБРОВКА ----------
void updateImu(unsigned long now) {
  float ax, ay, az;
  if (!M5.Imu.getAccel(&ax, &ay, &az)) return;
  float rx = ax, ry = ay;
  if (SWAP_AXES) { rx = ay; ry = ax; }
  float fwd = ry * FWD_SIGN;
  float lat = rx * LAT_SIGN;

  // первый кадр: если ручной калибровки не было — запоминаем текущий наклон
  if (!imuInited) {
    if (!hasCalib) { baseFwd = fwd; baseLat = lat; }
    imuInited = true;
  }

  // быстрое сглаживание от шума датчика
  fwdSmooth += (fwd - fwdSmooth) * 0.3;
  latSmooth += (lat - latSmooth) * 0.3;

  // КАЛИБРОВКА: 1.5 сек стоим, усредняем и запоминаем как «ноль»
  if (calibActive) {
    calibSumF += fwdSmooth;
    calibSumL += latSmooth;
    calibN++;
    if (now - calibStart >= CALIB_MS && calibN > 0) {
      baseFwd = calibSumF / calibN;
      baseLat = calibSumL / calibN;
      calibActive = false;
      hasCalib = true;
      prefs.putUChar("calib", 1);
      prefs.putFloat("bf", baseFwd);
      prefs.putFloat("bl", baseLat);
      showToast("CALIB OK");
    }
    return;   // на время калибровки реакции приостановлены
  }

  // медленный базлайн: доедает только дрейф, но не разгон
  baseFwd += (fwdSmooth - baseFwd) * 0.004;
  baseLat += (latSmooth - baseLat) * 0.004;

  float dF = fwdSmooth - baseFwd;
  float dev = fabsf(dF) + fabsf(latSmooth - baseLat);

  if (dev > VIB_TH) lastMotionMs = now;
  if (dev > IMPACT_TH && now > carScareCooldown) {
    carScareStart = now;
    carScareUntil = now + SCARE_MS;
    carScareCooldown = carScareUntil + 1000;
    glitchFx(2);
  }

  carAsleep = (now - lastMotionMs > STILL_MS);

  // тормоз / разгон с удержанием лица на 0.8 сек
  if (dF < -BRAKE_TH) brakeUntil = now + 800;
  if (dF > ACCEL_TH)  accelUntil = now + 800;

  // взгляд в поворот
  if (!carAsleep) {
    float target = constrain(latSmooth * GAZE_K, -18.0, 18.0);
    carGaze += (target - carGaze) * 0.2;
    gazeX = (int)carGaze;
  } else { carGaze = 0; gazeX = 0; }
}

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("=== BOOT STARTED ===");

  M5.begin();
  M5.Display.fillScreen(TFT_BLACK);

  prefs.begin("wrench", false);
  uint8_t m = prefs.getUChar("mode", 255);
  mode = (m < M_COUNT) ? (Mode)m : M_AUTO;
  flipped = prefs.getBool("flip", false);
  micSens = prefs.getFloat("ms", 0.5);
  thLow = prefs.getInt("tl", 40);
  thHigh = prefs.getInt("th", 200);
  scareEnterPct = prefs.getInt("se", 66);
  bright = prefs.getInt("br", 180);

  // сохранённая калибровка IMU
  hasCalib = prefs.getUChar("calib", 0) == 1;
  baseFwd = prefs.getFloat("bf", 0);
  baseLat = prefs.getFloat("bl", 0);

  M5.Display.setRotation(flipped ? 3 : 1);
  M5.Display.setBrightness(bright);

  sprite.createSprite(SCREEN_W, SCREEN_H);
  randomSeed(esp_random());

  M5.Imu.begin();
  bootScreen();
  M5.Mic.begin();

  // ---------- АСИНХРОННЫЕ МАРШРУТЫ ----------
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    lastWebMs = millis();
    String html = String(INDEX_HTML);
    size_t logoLen = strlen(LOGO_B64);
    if (logoLen > 0 && logoLen < 100000) html.replace("__LOGO_B64__", LOGO_B64);
    request->send(200, "text/html", html);
  });

  server.on("/state", HTTP_GET, [](AsyncWebServerRequest *request){
    lastWebMs = millis();
    String j = "{\"mode\":" + String((int)mode);
    j += ",\"face\":\"" + faceJson(FACE_STR[emotion]) + "\"";
    j += ",\"vol\":" + String(volPct);
    j += ",\"bat\":" + String((int)M5.Power.getBatteryLevel());
    j += ",\"ms\":" + String(micSens, 2);
    j += ",\"tl\":" + String(thLow);
    j += ",\"th\":" + String(thHigh);
    j += ",\"se\":" + String(scareEnterPct);
    j += ",\"br\":" + String(bright);
    j += "}";
    request->send(200, "application/json", j);
  });

  server.on("/cmd", HTTP_GET, [](AsyncWebServerRequest *request){
    lastWebMs = millis();
    if (request->hasParam("e")) { emotion = (Emotion)constrain(request->getParam("e")->value().toInt(), 0, E_COUNT - 1); needRedraw = true; }
    if (request->hasParam("m")) { mode = (Mode)constrain(request->getParam("m")->value().toInt(), 0, M_COUNT - 1); prefs.putUChar("mode", (uint8_t)mode); scareUntil = 0; carScareUntil = 0; }
    if (request->hasParam("flip")) { flipped = !flipped; M5.Display.setRotation(flipped ? 3 : 1); prefs.putBool("flip", flipped); needRedraw = true; }
    if (request->hasParam("scare")) { scareStart = millis(); scareUntil = scareStart + SCARE_MS; }
    if (request->hasParam("calib")) {
      calibActive = true;
      calibStart = millis();
      calibSumF = 0; calibSumL = 0; calibN = 0;
    }

    bool save = false;
    if (request->hasParam("ms")) { micSens = request->getParam("ms")->value().toFloat(); save = true; }
    if (request->hasParam("tl")) { thLow = request->getParam("tl")->value().toInt(); save = true; }
    if (request->hasParam("th")) { thHigh = request->getParam("th")->value().toInt(); save = true; }
    if (request->hasParam("se")) { scareEnterPct = request->getParam("se")->value().toInt(); save = true; }
    if (request->hasParam("br")) { bright = request->getParam("br")->value().toInt(); M5.Display.setBrightness(bright); save = true; }

    if (save) {
      prefs.putFloat("ms", micSens); prefs.putInt("tl", thLow); prefs.putInt("th", thHigh);
      prefs.putInt("se", scareEnterPct); prefs.putInt("br", bright);
    }
    request->send(200, "text/plain", "ok");
  });

  server.onNotFound([](AsyncWebServerRequest *request){
    request->redirect("/");
  });

  Serial.println("SETUP COMPLETE!");
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());

  lastSwitchMs = millis();
  lastMotionMs = millis();
  autoIntervalMs = random(AUTO_MIN_MS, AUTO_MAX_MS);
  micIntervalMs = random(MIC_MIN_MS, MIC_MAX_MS);
  nextBlinkMs = millis() + random(2000, 5000);
}

// ---------- LOOP ----------
void loop() {
  M5.update();
  unsigned long now = millis();

  // Wi-Fi таймаут (Async сам обрабатывает запросы)
  if (wifiActive) {
    if (now - lastWebMs > WIFI_TIMEOUT_MS) wifiOff();
  }

  // A: короткое = режим, длинное 800мс = WI-FI
  if (M5.BtnA.wasPressed()) aLongUsed = false;
  if (M5.BtnA.pressedFor(800) && !aLongUsed) {
    aLongUsed = true;
    if (wifiActive) wifiOff(); else wifiOn();
  }
  if (M5.BtnA.wasReleased() && !aLongUsed) {
    mode = (Mode)((mode + 1) % M_COUNT);
    prefs.putUChar("mode", (uint8_t)mode);
    scareUntil = 0;
    carScareUntil = 0;
    showToast(MODE_NAMES[mode]);
  }

  // B: короткое = эмоция, длинное = переворот
  if (M5.BtnB.wasPressed()) bLongUsed = false;
  if (M5.BtnB.pressedFor(800) && !bLongUsed) {
    bLongUsed = true;
    flipped = !flipped;
    M5.Display.setRotation(flipped ? 3 : 1);
    prefs.putBool("flip", flipped);
    glitchFx(3);
    needRedraw = true;
  }
  if (M5.BtnB.wasReleased() && !bLongUsed) {
    glitchFx(2);
    emotion = (Emotion)((emotion + 1) % E_COUNT);
    needRedraw = true;
  }

  if (mode == M_MIC) updateVolume(now);
  bool anyScare = (now < scareUntil) || (now < carScareUntil);
  if (anyScare) needRedraw = true;

  if (mode == M_AUTO && !anyScare && now - lastSwitchMs >= autoIntervalMs) {
    lastSwitchMs = now;
    autoIntervalMs = random(AUTO_MIN_MS, AUTO_MAX_MS);
    Emotion next;
    do { next = (Emotion)random(E_COUNT); } while (next == emotion);
    glitchFx(3);
    emotion = next;
    needRedraw = true;
  }

  if (mode == M_MIC && !anyScare) {
    micCategoryLogic(now);
    needRedraw = true;
  }

  if (mode == M_CAR) {
    updateImu(now);
    needRedraw = true;
  }

  bool eyesClosed = false;
  bool carSleeping = (mode == M_CAR && carAsleep);
  if (!anyScare && !carSleeping) {
    bool canBlink = (emotion != E_DEAD && emotion != E_SLEEPY && emotion != E_CALM);
    bool allowLook = (mode != M_CAR);

    if (allowLook && lookState == LOOK_NONE && canBlink && !blinking && now >= nextBlinkMs) {
      if (random(100) < 50) {
        lookState = LOOK_LEFT; lookStart = now; gazeX = -12; needRedraw = true;
      } else {
        blinking = true; blinkStartMs = now;
        blinkDuration = random(80, 170);
        blinkStyle = BLINK_STYLES[random(2)];
      }
    }
    if (allowLook && lookState == LOOK_LEFT && now - lookStart > 200) {
      lookState = LOOK_RIGHT; gazeX = 12; needRedraw = true;
    } else if (allowLook && lookState == LOOK_RIGHT && now - lookStart > 400) {
      lookState = LOOK_NONE; gazeX = 0;
      blinking = true; blinkStartMs = now;
      blinkDuration = random(80, 170);
      blinkStyle = BLINK_STYLES[random(2)];
      needRedraw = true;
    }

    if (blinking) {
      if (now - blinkStartMs < blinkDuration) eyesClosed = true;
      else {
        blinking = false;
        nextBlinkMs = (random(100) < 20) ? now + 180 : now + random(1500, 6000);
      }
      needRedraw = true;
    }
  }

  if (needRedraw) {
    Emotion shown = emotion;
    uint16_t bg = TFT_BLACK;
    bool closed = eyesClosed;
    if (anyScare) {
      bg = TFT_RED;
      unsigned long st = (now < scareUntil) ? scareStart : carScareStart;
      shown = (now - st < 600) ? E_ALERT : E_ANGRY;
      closed = false;
    } else if (mode == M_CAR) {
      if (carAsleep)             shown = E_SLEEPY;
      else if (now < brakeUntil) shown = E_ALERT;
      else if (now < accelUntil) shown = E_PAIN;
    }
    drawEyes(shown, closed, bg);
    if (mode == M_MIC) drawVolumeBar();
    if (calibActive) {
      sprite.setTextSize(2);
      sprite.fillRect(0, 0, SCREEN_W, 20, TFT_BLACK);
      sprite.drawString("CALIB...", 8, 2);
    }
    sprite.pushSprite(0, 0);
    needRedraw = false;
  }

  delay(10);
}