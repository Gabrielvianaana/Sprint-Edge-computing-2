#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>

// ===== LCD I2C =====
LiquidCrystal_I2C lcd(0x27, 16, 2);  // se o teu LCD for 0x3F, trocar aqui

// ===== Botão (SCAN) =====
const int btnPin = 12;   // pino do botão
bool lastButtonState = HIGH;

// ===== WiFi (Wokwi) =====
const char* ssid     = "Wokwi-GUEST";
const char* password = "";

// ===== FIWARE / Orion =====
// >>> AJUSTE AQUI: IP/host que o Wokwi consegue alcançar (não use rede interna do Docker 172.18.x.x)
const char* BROKER_HOST   = "172.183.151.189";  // <-- troque pelo IP/host do seu Orion
const int   BROKER_PORT   = 1026;

const char* SERVICE       = "fantasy";     // Fiware-Service (tenant)
const char* SERVICE_PATH  = "/jogadoras";  // Fiware-ServicePath (tem que bater com a tua coleção)
const char* ENTITY_PREFIX = "Player_";     // id no Postman: Player_01
const char* ENTITY_TYPE   = "Jogadora";    // type exatamente como criado: Jogadora (com J maiúsculo)

void setup() {
  Serial.begin(115200);

  // LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Iniciando...");

  // Botao
  pinMode(btnPin, INPUT_PULLUP);

  // WiFi
  WiFi.begin(ssid, password);
  lcd.setCursor(0, 1);
  lcd.print("WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  lcd.clear();
  lcd.print("WiFi OK!");
  delay(800);
  lcd.clear();
  lcd.print("Pressione SCAN");
}

void loop() {
  bool buttonState = digitalRead(btnPin);

  // borda de descida = clique
  if (lastButtonState == HIGH && buttonState == LOW) {
    lcd.clear();
    lcd.print("Lendo cartao...");
    delay(150);

    // ===== UID simulado =====
    String uid = "01";
    Serial.println(String("[SCAN] UID: ") + uid);

    consultarFIWARE(uid);
  }

  lastButtonState = buttonState;
}

void consultarFIWARE(const String& uid) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[ERRO] WiFi desconectado");
    lcd.clear();
    lcd.print("Sem WiFi");
    return;
  }

  // GET /v2/entities/Player_01?type=Jogadora&options=keyValues
  String url = "http://" + String(BROKER_HOST) + ":" + String(BROKER_PORT) +
               "/v2/entities/" + String(ENTITY_PREFIX) + uid +
               "?type=" + String(ENTITY_TYPE) + "&options=keyValues";

  HTTPClient http;
  http.setTimeout(7000); // evita travar
  http.begin(url);
  http.addHeader("Accept", "application/json");
  http.addHeader("Fiware-Service", SERVICE);
  http.addHeader("Fiware-ServicePath", SERVICE_PATH);

  Serial.println(String("[GET] ") + url);
  int code = http.GET();

  if (code == 200) {
    String payload = http.getString();
    Serial.println("[200] Resposta Orion:");
    Serial.println(payload);

    // Com options=keyValues, o JSON vem achatado: {"id":"Player_01","type":"Jogadora","nome":"Marta",...}
    DynamicJsonDocument doc(1024);
    DeserializationError err = deserializeJson(doc, payload);

    lcd.clear();
    if (!err) {
      const char* nome = doc["nome"] | "(sem nome)";  // atributo certo eh 'nome'
      Serial.println(String("[OK] Jogadora: ") + nome);

      lcd.setCursor(0, 0); lcd.print("Jogadora:");
      lcd.setCursor(0, 1); lcd.print(nome);
    } else {
      Serial.println(String("[ERRO] JSON: ") + err.c_str());
      lcd.setCursor(0, 0); lcd.print("Erro JSON");
    }

  } else if (code == 404) {
    Serial.println("[404] Entity nao encontrada (id/type/service/path)");
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("UID n/cad:");
    lcd.setCursor(0, 1); lcd.print(uid);

  } else if (code == 401 || code == 403) {
    Serial.println("[Auth] Falta permissao/headers/token");
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Sem acesso");
    lcd.setCursor(0, 1); lcd.print("Ver headers");

  } else if (code < 0) {
    // -1 / timeout / DNS / rede
    Serial.println(String("[NET] Falha HTTP: ") + code);
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Timeout/Net");
    lcd.setCursor(0, 1); lcd.print("Ver IP/porta");

  } else {
    Serial.println(String("[ERRO] FIWARE: ") + code);
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Erro FIWARE");
    lcd.setCursor(0, 1); lcd.print("Cod:" + String(code));
  }

  http.end();
}
