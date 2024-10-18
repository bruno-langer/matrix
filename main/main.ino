#include <FastLED.h>
#include <WiFi.h>
#include <WebServer.h>

#define DATA_PIN 26
#define NUM_LEDS 144

#define MODE_OFF 0
#define MODE_COLOR 1
#define MODE_LIGHT 2
#define MODE_NET_SOLID 3
#define MODE_NET_DRAW 4

#define SWITCH_PIN1 34  // Pino digital 1 da chave
#define SWITCH_PIN2 35  // Pino digital 2 da chave

const int pinoCLK = 3;  // PINO DIGITAL (CLK)
const int pinoDT = 4;   // PINO DIGITAL (DT)
const int pinoSW = 5;   // PINO DIGITAL (SW)

// WiFi configuration
const char* ssid = "SEU_SSID";
const char* password = "SUA_SENHA";

WebServer server(80);

CRGB leds[NUM_LEDS];
int mode = MODE_OFF;

int encoderCount = 0;
int ultPosicao;
int leituraCLK;
bool dir;

int hue = 0;                    // Variável para armazenar o valor de HUE no modo COLOR
int lightTemperatureIndex = 4;  // Começa com um valor médio no array de temperatura


// Array de temperaturas de cor
const CRGB temperatureColors[] = {
  Candle, Tungsten40W, Tungsten100W, Halogen,
  CarbonArc, HighNoonSun, DirectSunlight, OvercastSky, ClearBlueSky
};

void setup() {
  Serial.begin(115200);
  FastLED.addLeds<WS2812B, DATA_PIN>(leds, NUM_LEDS);

  // Conectar ao Wi-Fi
  connectToWiFi();

  // Configurar os pinos da chave de 3 posições e encoder
  pinMode(SWITCH_PIN1, INPUT);
  pinMode(SWITCH_PIN2, INPUT);
  pinMode(pinoCLK, INPUT);
  pinMode(pinoDT, INPUT);
  pinMode(pinoSW, INPUT_PULLUP);

  // Inicializar a leitura do encoder
  ultPosicao = digitalRead(pinoCLK);
}

void loop() {
  // Verificar o modo de operação com base nos switches
  readSwitchMode();

  // Verificar o botão do encoder para alternar modos
  if (buttonRead()) {
    mode = (mode + 1) % 3;  // Alterna entre os 3 modos
  }

  // Ler a posição do encoder e ajustar cor ou temperatura
  encoderRead();

  // Executar ação conforme o modo
  handleModes();

  // Processar requisições HTTP
  server.handleClient();
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

   server.on("/setColors", HTTP_POST, []() {
    if (server.hasArg("plain")) {
      String colorString = server.arg("plain");  // Lê a carga útil
      int ledCount = 0;

      // Divide a string em cores individuais usando a vírgula como delimitador
      int startIndex = 0;
      int commaIndex;
      while ((commaIndex = colorString.indexOf(',', startIndex)) != -1 && ledCount < NUM_LEDS) {
        String hexColor = colorString.substring(startIndex, commaIndex);
        setLedColor(ledCount, hexColor);
        ledCount++;
        startIndex = commaIndex + 1;  // Atualiza o índice de início para a próxima cor
      }

      // Para o último LED (caso não tenha vírgula no final)
      if (startIndex < colorString.length() && ledCount < NUM_LEDS) {
        String hexColor = colorString.substring(startIndex);
        setLedColor(ledCount, hexColor);
      }

      FastLED.show();  // Atualiza os LEDs
      server.send(200, "text/plain", "Cores definidas com sucesso.");
    } else {
      server.send(400, "text/plain", "Nenhuma carga útil fornecida.");
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

void setLedColor(int ledIndex, String hexColor) {
  if (hexColor.length() == 7 && hexColor[0] == '#') {                                   // Verifica se o formato é válido
    long number = strtol(&hexColor[1], NULL, 16);                                       // Converte a string HEX para um número
    leds[ledIndex] = CRGB((number >> 16) & 0xFF, (number >> 8) & 0xFF, number & 0xFF);  // Define a cor do LED
  }
}


// Função para gerenciar modos de operação
void handleModes() {
  switch (mode) {
    case MODE_OFF:
      FastLED.showColor(CRGB::Black);
      delay(100);
      break;

    case MODE_COLOR:
      FastLED.showColor(CHSV(hue, 255, 255));  // Controla o HUE, com saturação e brilho fixos
      delay(100);
      break;

    case MODE_LIGHT:
      fill_solid(leds, NUM_LEDS, temperatureColors[lightTemperatureIndex]);
      FastLED.show();
      delay(100);
      break;
  }
}

// Função para ler o botão do encoder com debounce
bool buttonRead() {
  static unsigned long lastPress = 0;
  if (digitalRead(pinoSW) == LOW) {
    if (millis() - lastPress > 200) {  // Debounce
      lastPress = millis();
      return true;
    }
  }
  return false;
}

// Função para ler o encoder rotativo
void encoderRead() {
  leituraCLK = digitalRead(pinoCLK);
  if (leituraCLK != ultPosicao && leituraCLK == LOW) {
    if (digitalRead(pinoDT) != leituraCLK) {
      adjustColor(1);  // Girou no sentido horário
    } else {
      adjustColor(-1);  // Girou no sentido anti-horário
    }
  }
  ultPosicao = leituraCLK;
}

// Função para ajustar cor ou temperatura com base no modo
void adjustColor(int direction) {
  if (mode == MODE_COLOR) {
    hue = (hue + direction * 5) % 256;  // Ajusta o valor de HUE, variando entre 0 e 255
    FastLED.showColor(CHSV(hue, 255, 255));
  } else if (mode == MODE_LIGHT) {
    lightTemperatureIndex = constrain(lightTemperatureIndex + direction, 0, 8);  // Ajusta o índice de temperatura
    fill_solid(leds, NUM_LEDS, temperatureColors[lightTemperatureIndex]);
    FastLED.show();
  }
}
