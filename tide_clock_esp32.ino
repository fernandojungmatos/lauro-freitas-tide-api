/*
  Relogio circular de mare - ESP32 + ILI9341 ou ST7789
  API: /api/v1/esp32 da API de Lauro de Freitas

  Bibliotecas pelo Library Manager:
    - Adafruit GFX Library
    - Adafruit ILI9341 (se usar ILI9341)
    - Adafruit ST7735 and ST7789 (se usar ST7789)
    - ArduinoJson 7

  Selecione somente um driver abaixo.
*/

#define DISPLAY_ILI9341 1
// #define DISPLAY_ST7789 1

#if defined(DISPLAY_ILI9341) && defined(DISPLAY_ST7789)
#error "Selecione apenas DISPLAY_ILI9341 ou DISPLAY_ST7789"
#endif
#if !defined(DISPLAY_ILI9341) && !defined(DISPLAY_ST7789)
#error "Selecione DISPLAY_ILI9341 ou DISPLAY_ST7789"
#endif

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <SPI.h>
#include <Adafruit_GFX.h>

#if defined(DISPLAY_ILI9341)
#include <Adafruit_ILI9341.h>
#else
#include <Adafruit_ST7789.h>
#endif

// -------------------- Wi-Fi e API --------------------
const char* WIFI_SSID = "SUA_REDE_WIFI";
const char* WIFI_PASSWORD = "SUA_SENHA_WIFI";

// IP do computador/Raspberry Pi onde a API esta rodando.
const char* API_URL = "http://192.168.1.100:8000/api/v1/esp32";

// Bahia: UTC-3. Nao usa horario de verao.
const long GMT_OFFSET_SECONDS = -3 * 3600;
const int DAYLIGHT_OFFSET_SECONDS = 0;

// Atualizacao recomendada: 15 minutos.
const unsigned long API_INTERVAL_MS = 15UL * 60UL * 1000UL;
const unsigned long SCREEN_INTERVAL_MS = 1000UL;

// -------------------- Pinagem ESP32 --------------------
// Barramento VSPI padrao do ESP32.
#define TFT_SCLK 18
#define TFT_MISO 19
#define TFT_MOSI 23
#define TFT_CS   5
#define TFT_DC   2
#define TFT_RST  4
#define TFT_BL   15  // coloque -1 se o backlight estiver sempre ligado

#if defined(DISPLAY_ST7789)
// Ajuste conforme seu modulo ST7789.
#define TFT_WIDTH  320
#define TFT_HEIGHT 240
#endif

#if defined(DISPLAY_ILI9341)
Adafruit_ILI9341 display(TFT_CS, TFT_DC, TFT_RST);
#else
Adafruit_ST7789 display(TFT_CS, TFT_DC, TFT_RST);
#endif

// -------------------- Cores RGB565 --------------------
#define COLOR_BG       0x0841  // azul-marinho
#define COLOR_PANEL    0x18E3
#define COLOR_TEXT     0xFFFF
#define COLOR_MUTED    0x9CF3
#define COLOR_CYAN     0x07FF
#define COLOR_YELLOW   0xFFE0
#define COLOR_RED      0xF800
#define COLOR_GREEN    0x07E0
#define COLOR_ORANGE   0xFD20
#define COLOR_BLUE     0x051F
#define COLOR_DARK     0x0000

// -------------------- Modelo local --------------------
struct TideEvent {
  bool valid = false;
  bool high = false;
  int minutes = 0;
  float height = 0.0f;
};

TideEvent tides[8];
int tideCount = 0;
String sourceDate = "";
String moonPhase = "";
String activity = "";
String sunrise = "--:--";
String sunset = "--:--";
int coefficientMin = -1;
int coefficientMax = -1;

bool apiReady = false;
String lastError = "Inicializando";
unsigned long lastApiUpdate = 0;
unsigned long lastScreenUpdate = 0;
int screen = 0;
int lastRenderedMinute = -1;
int lastRenderedScreen = -1;

// -------------------- Funcoes auxiliares --------------------
int parseClock(const String& value) {
  int colon = value.indexOf(':');
  if (colon < 1) return -1;
  int h = value.substring(0, colon).toInt();
  int m = value.substring(colon + 1).toInt();
  if (h < 0 || h > 23 || m < 0 || m > 59) return -1;
  return h * 60 + m;
}

String twoDigits(int value) {
  if (value < 10) return "0" + String(value);
  return String(value);
}

String formatMinutes(int minutes) {
  minutes = (minutes % 1440 + 1440) % 1440;
  return twoDigits(minutes / 60) + ":" + twoDigits(minutes % 60);
}

String formatHeight(float value) {
  return String(value, 1) + "m";
}

void sortTides() {
  for (int i = 0; i < tideCount - 1; i++) {
    for (int j = i + 1; j < tideCount; j++) {
      if (tides[j].minutes < tides[i].minutes) {
        TideEvent tmp = tides[i];
        tides[i] = tides[j];
        tides[j] = tmp;
      }
    }
  }
}

void clearTides() {
  tideCount = 0;
  for (int i = 0; i < 8; i++) tides[i] = TideEvent();
}

// Retorna o intervalo entre dois eventos que contem o horario atual.
// O proximo evento pode estar no dia seguinte, por isso usamos modulo 24h.
bool currentTideState(int nowMinutes, TideEvent& before, TideEvent& after,
                      float& currentHeight, bool& rising) {
  if (tideCount < 2) return false;

  int beforeIndex = -1;
  int afterIndex = -1;

  for (int i = 0; i < tideCount; i++) {
    int next = (i + 1) % tideCount;
    int start = tides[i].minutes;
    int end = tides[next].minutes;
    int now = nowMinutes;

    if (next == 0) end += 1440;
    if (now < start) now += 1440;

    if (now >= start && now <= end) {
      beforeIndex = i;
      afterIndex = next;
      break;
    }
  }

  if (beforeIndex < 0) {
    beforeIndex = tideCount - 1;
    afterIndex = 0;
  }

  before = tides[beforeIndex];
  after = tides[afterIndex];

  int start = before.minutes;
  int end = after.minutes;
  int now = nowMinutes;
  if (end <= start) end += 1440;
  if (now < start) now += 1440;

  float progress = (float)(now - start) / (float)(end - start);
  progress = constrain(progress, 0.0f, 1.0f);
  currentHeight = before.height + (after.height - before.height) * progress;
  rising = after.height > before.height;
  return true;
}

bool nextTide(int nowMinutes, TideEvent& nextEvent) {
  if (tideCount == 0) return false;
  int bestDelta = 2000;
  bool found = false;
  for (int i = 0; i < tideCount; i++) {
    int delta = tides[i].minutes - nowMinutes;
    if (delta <= 0) delta += 1440;
    if (delta < bestDelta) {
      bestDelta = delta;
      nextEvent = tides[i];
      found = true;
    }
  }
  return found;
}

// Converte minuto do dia para angulo em graus: 00:00 fica no topo.
float minuteAngle(int minutes) {
  return ((float)minutes / 1440.0f) * 360.0f - 90.0f;
}

int circleX(int cx, int radius, float degrees) {
  return cx + (int)(cos(degrees * DEG_TO_RAD) * radius);
}

int circleY(int cy, int radius, float degrees) {
  return cy + (int)(sin(degrees * DEG_TO_RAD) * radius);
}

void drawThickRadial(int cx, int cy, int r1, int r2, float angle, uint16_t color) {
  int x1 = circleX(cx, r1, angle);
  int y1 = circleY(cy, r1, angle);
  int x2 = circleX(cx, r2, angle);
  int y2 = circleY(cy, r2, angle);
  display.drawLine(x1, y1, x2, y2, color);
  display.drawLine(x1 + 1, y1, x2 + 1, y2, color);
  display.drawLine(x1, y1 + 1, x2, y2 + 1, color);
}

void drawTextCentered(const String& text, int x, int y, uint8_t size, uint16_t color) {
  int16_t x1, y1;
  uint16_t w, h;
  display.setTextSize(size);
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  display.setCursor(x - (int)w / 2, y);
  display.setTextColor(color);
  display.print(text);
}

void drawHeader(const String& title) {
  display.fillRect(0, 0, 320, 28, COLOR_PANEL);
  display.setTextColor(COLOR_CYAN);
  display.setTextSize(1);
  display.setCursor(6, 8);
  display.print(title);

  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 10)) {
    String clock = twoDigits(timeinfo.tm_hour) + ":" + twoDigits(timeinfo.tm_min);
    display.setTextColor(COLOR_YELLOW);
    display.setCursor(270, 8);
    display.print(clock);
  }
}

void drawTideClock() {
  display.fillScreen(COLOR_BG);
  drawHeader("LAURO DE FREITAS");

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10)) {
    display.setTextColor(COLOR_TEXT);
    display.setTextSize(2);
    display.setCursor(35, 100);
    display.print("Sem horario NTP");
    return;
  }

  int nowMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  // Layout compacto para evitar que textos e ponteiro ultrapassem a tela.
  const int cx = 82;
  const int cy = 124;
  const int radius = 61;

  // Anel principal e marcas de hora.
  display.drawCircle(cx, cy, radius, COLOR_MUTED);
  display.drawCircle(cx, cy, radius - 1, COLOR_MUTED);
  display.drawCircle(cx, cy, radius - 18, COLOR_PANEL);

  for (int hour = 0; hour < 24; hour++) {
    float angle = minuteAngle(hour * 60);
    int outer = (hour % 6 == 0) ? radius : radius - 5;
    uint16_t markColor = (hour % 6 == 0) ? COLOR_CYAN : COLOR_MUTED;
    drawThickRadial(cx, cy, outer - 3, radius + 1, angle, markColor);
  }

  // Identificacao dos eventos: azul = preia-mar, amarelo = baixa-mar.
  for (int i = 0; i < tideCount; i++) {
    float angle = minuteAngle(tides[i].minutes);
    uint16_t eventColor = tides[i].high ? COLOR_CYAN : COLOR_YELLOW;
    drawThickRadial(cx, cy, radius - 12, radius + 5, angle, eventColor);
  }

  // Ponteiro para o horario atual.
  float nowAngle = minuteAngle(nowMinutes);
  int handX = circleX(cx, radius - 27, nowAngle);
  int handY = circleY(cy, radius - 27, nowAngle);
  display.drawLine(cx, cy, handX, handY, COLOR_RED);
  display.drawLine(cx + 1, cy, handX + 1, handY, COLOR_RED);
  display.fillCircle(cx, cy, 6, COLOR_RED);
  display.drawCircle(cx, cy, 8, COLOR_TEXT);

  drawTextCentered("00", cx, cy - radius - 20, 1, COLOR_TEXT);
  drawTextCentered("06", cx - radius - 12, cy - 3, 1, COLOR_TEXT);
  drawTextCentered("12", cx, cy + radius + 6, 1, COLOR_TEXT);
  drawTextCentered("18", cx + radius + 12, cy - 3, 1, COLOR_TEXT);

  TideEvent before, after;
  float currentHeight = 0.0f;
  bool rising = false;
  bool stateOk = currentTideState(nowMinutes, before, after, currentHeight, rising);

  TideEvent next;
  bool nextOk = nextTide(nowMinutes, next);

  // Painel lateral de leitura.
  display.setTextSize(1);
  display.setTextColor(rising ? COLOR_CYAN : COLOR_YELLOW);
  display.setCursor(170, 50);
  display.print(rising ? "SUBINDO" : "DESCENDO");

  display.setTextColor(COLOR_TEXT);
  display.setCursor(170, 73);
  display.print("Nivel");
  display.setTextSize(2);
  display.setCursor(170, 86);
  display.print(stateOk ? String(currentHeight, 1) + "m" : "--");

  display.setTextSize(1);
  display.setTextColor(COLOR_MUTED);
  display.setCursor(170, 116);
  display.print("Proximo evento");
  display.setTextColor(nextOk && next.high ? COLOR_CYAN : COLOR_YELLOW);
  display.setTextSize(2);
  display.setCursor(170, 129);
  display.print(nextOk ? formatMinutes(next.minutes) : "--:--");
  display.setTextSize(1);
  display.setCursor(170, 153);
  display.print(nextOk ? (next.high ? "PREIA-MAR" : "BAIXA-MAR") : "sem dados");
  display.setCursor(170, 169);
  display.print(nextOk ? formatHeight(next.height) : "--");

  // Evento anterior e seguinte no rodape.
  display.setTextColor(COLOR_MUTED);
  display.setCursor(4, 205);
  display.print("Anterior: ");
  display.setTextColor(before.valid ? (before.high ? COLOR_CYAN : COLOR_YELLOW) : COLOR_TEXT);
  display.print(stateOk ? formatMinutes(before.minutes) : "--:--");
  display.setTextColor(COLOR_MUTED);
  display.setCursor(4, 220);
  display.print("Seguinte: ");
  display.setTextColor(after.valid ? (after.high ? COLOR_CYAN : COLOR_YELLOW) : COLOR_TEXT);
  display.print(stateOk ? formatMinutes(after.minutes) : "--:--");

  display.setTextColor(COLOR_MUTED);
  display.setCursor(148, 220);
  display.print("C:");
  display.setTextColor(COLOR_YELLOW);
  if (coefficientMin >= 0) {
    display.print(coefficientMin);
    display.print("-");
    display.print(coefficientMax);
  } else {
    display.print("--");
  }
}

void drawInfoScreen() {
  display.fillScreen(COLOR_BG);
  drawHeader("INFORMACOES DO DIA");

  display.setTextSize(1);
  display.setTextColor(COLOR_MUTED);
  display.setCursor(8, 45);
  display.print("SOL");
  display.setTextSize(2);
  display.setTextColor(COLOR_YELLOW);
  display.setCursor(8, 60);
  display.print(sunrise);
  display.print("  ");
  display.print(sunset);

  display.setTextSize(1);
  display.setTextColor(COLOR_MUTED);
  display.setCursor(8, 105);
  display.print("LUA");
  display.setTextColor(COLOR_TEXT);
  display.setTextSize(2);
  display.setCursor(8, 120);
  display.print(moonPhase.length() ? moonPhase : "--");

  display.setTextSize(1);
  display.setTextColor(COLOR_MUTED);
  display.setCursor(8, 165);
  display.print("ATIVIDADE SOLUNAR");
  display.setTextColor(COLOR_CYAN);
  display.setTextSize(2);
  display.setCursor(8, 180);
  display.print(activity.length() ? activity : "--");

  display.setTextSize(1);
  display.setTextColor(COLOR_MUTED);
  display.setCursor(8, 215);
  display.print("API: ");
  display.setTextColor(apiReady ? COLOR_GREEN : COLOR_RED);
  display.print(apiReady ? "OK" : lastError.substring(0, 20));
}

void drawErrorScreen() {
  display.fillScreen(COLOR_BG);
  drawHeader("STATUS DA CONEXAO");
  display.setTextColor(COLOR_RED);
  display.setTextSize(2);
  display.setCursor(12, 60);
  display.print("API indisponivel");
  display.setTextColor(COLOR_TEXT);
  display.setTextSize(1);
  display.setCursor(12, 100);
  display.print(lastError.substring(0, 38));
  display.setCursor(12, 125);
  display.print("Mantendo ultimo dado valido");
  display.setCursor(12, 160);
  display.print("Verifique:");
  display.setCursor(12, 180);
  display.print("1. IP da API");
  display.setCursor(12, 195);
  display.print("2. Wi-Fi e firewall");
  display.setCursor(12, 210);
  display.print("3. Servidor na porta 8000");
}

void renderIfNeeded(bool force = false) {
  struct tm info;
  int currentMinute = -1;
  if (getLocalTime(&info, 10)) currentMinute = info.tm_hour * 60 + info.tm_min;

  // Evita limpar o frame inteiro a cada segundo, reduzindo flicker no TFT.
  if (force || currentMinute != lastRenderedMinute || screen != lastRenderedScreen) {
    if (!apiReady && tideCount == 0) drawErrorScreen();
    else if (screen == 0) drawTideClock();
    else drawInfoScreen();
    lastRenderedMinute = currentMinute;
    lastRenderedScreen = screen;
  }
}

void renderScreen() {
  renderIfNeeded(true);
}

bool parseApiPayload(const String& payload) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    lastError = "JSON: " + String(error.c_str());
    return false;
  }

  clearTides();
  JsonArray tideArray = doc["tides"].as<JsonArray>();
  for (JsonObject item : tideArray) {
    if (tideCount >= 8) break;
    String timeValue = item["time"] | "";
    int minutes = parseClock(timeValue);
    if (minutes < 0) continue;

    tides[tideCount].valid = true;
    tides[tideCount].high = (String(item["type"] | "") == "high");
    tides[tideCount].minutes = minutes;
    tides[tideCount].height = item["height_m"] | 0.0f;
    tideCount++;
  }
  sortTides();

  JsonObject sun = doc["sun"].as<JsonObject>();
  sunrise = sun["sunrise"] | "--:--";
  sunset = sun["sunset"] | "--:--";

  JsonObject moon = doc["moon"].as<JsonObject>();
  moonPhase = moon["moon_phase"] | "--";
  activity = doc["activity"] | "--";

  JsonObject coefficient = doc["coefficient"].as<JsonObject>();
  coefficientMin = coefficient["min"] | -1;
  coefficientMax = coefficient["max"] | -1;

  sourceDate = doc["date"] | "";
  return tideCount > 0;
}

bool fetchApi() {
  if (WiFi.status() != WL_CONNECTED) {
    lastError = "Wi-Fi desconectado";
    apiReady = false;
    return false;
  }

  HTTPClient http;
  WiFiClient plainClient;
  WiFiClientSecure secureClient;
  http.setConnectTimeout(5000);
  http.setTimeout(8000);
  if (String(API_URL).startsWith("https://")) {
    // Funciona para testes; em produção, prefira validar o certificado raiz.
    secureClient.setInsecure();
    http.begin(secureClient, API_URL);
  } else {
    http.begin(plainClient, API_URL);
  }
  int code = http.GET();

  if (code != HTTP_CODE_OK) {
    lastError = "HTTP " + String(code);
    http.end();
    apiReady = false;
    return false;
  }

  String payload = http.getString();
  http.end();

  bool ok = parseApiPayload(payload);
  apiReady = ok;
  if (!ok && lastError == "Inicializando") lastError = "Sem eventos de mare";
  if (ok) lastError = "OK";
  return ok;
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  display.fillScreen(COLOR_BG);
  display.setTextColor(COLOR_CYAN);
  display.setTextSize(2);
  display.setCursor(20, 95);
  display.print("Conectando Wi-Fi");

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    display.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Wi-Fi OK. IP do ESP32: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Falha ao conectar no Wi-Fi");
    lastError = "Falha no Wi-Fi";
  }
}

void setupDisplay() {
  SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);

#if defined(DISPLAY_ILI9341)
  display.begin();
#else
  display.init(TFT_WIDTH, TFT_HEIGHT);
#endif

  display.setRotation(1); // landscape 320x240
  display.fillScreen(COLOR_BG);

  if (TFT_BL >= 0) {
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  setupDisplay();
  connectWiFi();

  configTime(GMT_OFFSET_SECONDS, DAYLIGHT_OFFSET_SECONDS,
             "pool.ntp.org", "a.st1.ntp.br", "time.nist.gov");

  // Aguarda rapidamente o NTP; o desenho continua mesmo sem sincronizar.
  struct tm timeinfo;
  getLocalTime(&timeinfo, 5000);

  fetchApi();
  lastApiUpdate = millis();
  renderScreen();
}

void loop() {
  unsigned long now = millis();

  if (now - lastApiUpdate >= API_INTERVAL_MS || !apiReady) {
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.disconnect();
      WiFi.reconnect();
    }
    fetchApi();
    lastApiUpdate = now;
    renderScreen();
  }

  if (now - lastScreenUpdate >= SCREEN_INTERVAL_MS) {
    lastScreenUpdate = now;

    // Alterna entre relogio e informacoes a cada 10 segundos.
    static int secondsOnScreen = 0;
    secondsOnScreen++;
    if (secondsOnScreen >= 10) {
      secondsOnScreen = 0;
      screen = (screen + 1) % 2;
    }

    renderIfNeeded();
  }

  delay(20);
}
