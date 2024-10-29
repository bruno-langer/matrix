#include <FastLED.h>
#include <WiFi.h>
#include <WebServer.h>
#include "secrets.h"

#define DATA_PIN 26
#define NUM_LEDS 5

#define MODE_OFF 0
#define MODE_COLOR 1
#define MODE_LIGHT 2
#define MODE_NET_SOLID 3
#define MODE_NET_DRAW 4

#define SWITCH_PIN1 34  // Pino digital 1 da chave
#define SWITCH_PIN2 35  // Pino digital 2 da chave

// Pinos do encoder
#define ENCODER_CLK 4
#define ENCODER_DT 5

// WiFi configuration
const char* ssid = SECRET_SSID;
const char* password = SECRET_PASS;

WebServer server(80);

CRGB leds[NUM_LEDS];
int mode = MODE_OFF;

volatile int encoderCount = 0;
int lastEncoderCount = 0;       // Guarda o último valor do encoder
int hue = 0;                    // Variável para armazenar o valor de HUE no modo COLOR
int lightTemperatureIndex = 4;  // Começa com um valor médio no array de temperatura

unsigned long lastInterruptTime = 0;     // Para debounce
const unsigned long debounceDelay = 20;  // Delay em milissegundos para debounce

// Array de temperaturas de cor
const CRGB temperatureColors[] = {
  Candle, Tungsten40W, Tungsten100W, Halogen,
  CarbonArc, HighNoonSun, DirectSunlight, OvercastSky, ClearBlueSky
};

// Variável para controlar a direção do encoder
bool dir;

void IRAM_ATTR encoderISR() {
  unsigned long currentTime = millis();

  // Verifica se o tempo atual é maior que o último tempo de interrupção + o delay de debounce
  if (currentTime - lastInterruptTime > debounceDelay) {
    // Atualiza o último tempo de interrupção
    lastInterruptTime = currentTime;

    // Direção do encoder
    int dtState = digitalRead(ENCODER_DT);
    if (dtState != digitalRead(ENCODER_CLK)) {
      encoderCount++;
      dir = true;
    } else {
      encoderCount--;
      dir = false;  // Sentido anti-horário
    }
  }
}

void setup() {
  Serial.begin(115200);

  // Configuração dos pinos do encoder
  pinMode(ENCODER_CLK, INPUT);
  pinMode(ENCODER_DT, INPUT);

  // Configuração dos pinos da chave de 3 posições
  pinMode(SWITCH_PIN1, INPUT);
  pinMode(SWITCH_PIN2, INPUT);

  // Configura interrupções para o pino CLK
  attachInterrupt(digitalPinToInterrupt(ENCODER_CLK), encoderISR, CHANGE);

  FastLED.addLeds<WS2812B, DATA_PIN,GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(128);

  // Conectar ao Wi-Fi
   connectToWiFi();
   setupServer();
}

void loop() {
  // Verifica modo de operação com base nos switches
  readSwitchMode();

  // Ajusta cor ou temperatura de luz dependendo do modo e direção do encoder
  if (encoderCount != lastEncoderCount) {
    adjustColor(dir ? 1 : -1);  // Incrementa ou decrementa conforme a direção
    lastEncoderCount = encoderCount;
  }

  // Executa a ação conforme o modo
  handleModes();

  // Processar requisições HTTP
  server.handleClient();

  delay(100);
  debugPrint();
}

// Função para conectar ao Wi-Fi
void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Conectando ao Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi conectado!");
}

// Função para configurar as rotas do servidor HTTP
void setupServer() {
  // Rota para a página principal
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", "<h1>Controle de LED</h1><p>Use /color para ajustar a cor e /light para a temperatura.</p>");
  });

  // Rota para controlar a cor (HUE)
  server.on("/color", HTTP_GET, []() {
    if (server.hasArg("hue")) {
      hue = server.arg("hue").toInt();
      server.send(200, "text/plain", "Cor ajustada para HUE: " + String(hue));
    } else {
      server.send(400, "text/plain", "Parâmetro 'hue' não fornecido.");
    }
  });

  // Rota para ajustar a temperatura da luz
  server.on("/light", HTTP_GET, []() {
    if (server.hasArg("temp")) {
      lightTemperatureIndex = constrain(server.arg("temp").toInt(), 0, 8);
      server.send(200, "text/plain", "Temperatura ajustada para índice: " + String(lightTemperatureIndex));
    } else {
      server.send(400, "text/plain", "Parâmetro 'temp' não fornecido.");
    }
  });

  server.begin();
}

// Função para ler o estado dos switches e definir o modo
void readSwitchMode() {
  bool s1 = digitalRead(SWITCH_PIN1);
  bool s2 = digitalRead(SWITCH_PIN2);

  if (s1) {
    mode = MODE_COLOR;
  } else if (s2) {
    mode = MODE_LIGHT;
  } else {
    mode = MODE_OFF;
  }
}

void debugPrint() {
  Serial.print("Modo: ");
  Serial.print(mode);

  Serial.print(" | HUE ");
  Serial.print(hue);
  Serial.print(" | TEMP INDEX ");
  Serial.print(lightTemperatureIndex);

  Serial.print(" | Encoder Count: ");
  Serial.print(encoderCount);

  Serial.print(" | Direção Encoder: ");
  Serial.print(dir ? "Horario" : "Anti-horario");

  Serial.print(" | Wi-Fi: ");
  Serial.print(WiFi.status() == WL_CONNECTED ? "Conectado" : "Desconectado");

  Serial.print(" | IP: ");
  Serial.print(WiFi.localIP());


  Serial.println();
}

// Função para gerenciar modos de operação
void handleModes() {
  switch (mode) {
    case MODE_OFF:
      FastLED.showColor(CRGB::Black);
      break;

    case MODE_COLOR:
      FastLED.showColor(CHSV(hue, 255, 255));
      break;

    case MODE_LIGHT:
      fill_solid(leds, NUM_LEDS, temperatureColors[lightTemperatureIndex]);
      FastLED.show();
      break;
  }
}

// Função para ajustar cor ou temperatura com base no modo e direção do encoder
void adjustColor(int direction) {
  if (mode == MODE_COLOR) {
    hue = (hue + (direction * 10)) % 256;
    if (hue < 0) {
      hue += 256;
    }
    FastLED.showColor(CHSV(hue, 255, 255));
  } else if (mode == MODE_LIGHT) {
    lightTemperatureIndex = constrain(lightTemperatureIndex + direction, 0, 8);
    fill_solid(leds, NUM_LEDS, temperatureColors[lightTemperatureIndex]);
    FastLED.show();
  }
}
