#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Adafruit_ADS1X15.h>
#include "internet.h"
#include "certificados.h"
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Bounce2.h>
#include "imagem.h"
#include "mini_game.h"
#include "carrinho.h"

#include <Adafruit_MCP23X17.h>
Adafruit_MCP23X17 mcp;

// ====================== CONFIGURAÇÃO DOS MOTORES ======================
#define pinM0Dir 18
#define pinM0Esq 3
#define pinM1Dir 10
#define pinM1Esq 46
#define pinM2Dir 13
#define pinM2Esq 14
#define pinM3Dir 12
#define pinM3Esq 11


#define frequenciaPWM 20000
#define resolucaoPWM 8

const uint8_t pinMotor[4][2] = {
    {pinM0Esq, pinM0Dir},
    {pinM1Esq, pinM1Dir},
    {pinM2Esq, pinM2Dir},
    {pinM3Esq, pinM3Dir}
};

const uint8_t chMotor[4][2] = {
    {0, 1},
    {2, 3},
    {4, 5},
    {6, 7}
};

// ====================== PINOS LEDS (MCP23X17) ======================
#define SETA_DIR       10
#define SETA_ESQ       13
#define FAROL_DIR      12
#define FAROL_ESQ      11
#define FREIO_DIR      15
#define FREIO_ESQ      14

// ====================== VARIÁVEIS LED ======================
bool estado_farol = 0;
bool estado_seta_dir = 0;
bool estado_seta_esq = 0;
bool estado_freio = 0;

unsigned long tempoSetaDir = 0;
unsigned long tempoSetaEsq = 0;

bool estadoSetaDir = false;
bool estadoSetaEsq = false;

// ====================== CONFIGURAÇÃO MQTT ======================
const int mqtt_port = 8883;
const char *mqtt_id = "CarrinhoESP32";
const char *mqtt_SUB = "projeto/carrinho";
WiFiClientSecure espClient;
PubSubClient mqtt(espClient);

uint8_t velocidade(uint8_t);
void mover(int velEsq, int velDir);
void frente(uint8_t vel = 40, int curva = 0);
void tras(uint8_t vel = 40);
void direita_lateral();
void esquerda_lateral();
void parar();
void Callback(char *topic, byte *payload, unsigned int length);
void conectaMQTT();

int curvaAtual = 0;

//!----------------------- encoder ---------------------------------------
#define ENCODER_A 38
#define ENCODER_B 37
#define ENCODER_BTN 19


TFT_eSPI tft = TFT_eSPI(); 
Bounce botao;
bool ultimoA = HIGH;
unsigned long tempoPressionado = 0;
bool segurando = false;

int contadorBonus = 0;
unsigned long tempoUltimoClique = 0;

enum Estado {
  TELA_LOGO,
  MENU_PRINCIPAL,
  SUBMENU_QR,
  SUBMENU_CONFIG,
  JOGANDO_BONUS
};

Estado estadoAtual = TELA_LOGO;

int opcaoPrincipal = 0;
const char* opcoesPrincipais[] = {"QR Code", "Configuracoes"};
int totalPrincipais = sizeof(opcoesPrincipais) / sizeof(opcoesPrincipais[0]);

int opcaoQR = 0;
const char* opcoesQR[] = {"QR Site", "QR App", "QR Dashboard"};
int totalQR = sizeof(opcoesQR) / sizeof(opcoesQR[0]);

int opcaoConfig = 0;
const char* opcoesConfig[] = {"Bonus"};
int totalConfig = sizeof(opcoesConfig) / sizeof(opcoesConfig[0]);

#define COR_PRETO   TFT_BLACK
#define COR_DOURADO tft.color565(212, 175, 55)
#define COR_BRANCO  TFT_WHITE
#define COR_CINZA   TFT_DARKGREY

void mostrarMenuPrincipal();
void mostrarSubmenuQR();
void mostrarSubmenuConfig();
void lerEncoder();
void animarTransicao(int direcao);
void verificarPressionarLongo();
void mostrarLogo();
//! =================================================================================

//* pid

// JsonDocument temperatura;

Carrinho carrinho(mcp);

//* Controle de partida 
static uint32_t ATRASO_PARTIDA_MS = 300; 
static uint32_t instanteInicioAtraso = 0;

//* Flags de estado
static bool emCorrida = false;       
static bool emAtrasoPartida = false; 

void pararCarrinho();
void Callback(char *topic, byte *payload, unsigned int length);
void iniciarSeguirLinha();
void entrarCalibracao();


//* ========== BOTÃO ===============

bool lerBotaoEncoderBruto()
{
  return (digitalRead(ENCODER_BTN) == LOW);
}

//*  clique simples (borda + debounce)
bool botaoEncoderClickSimples()
{
  static bool estadoEstavel = false;
  static bool ultimoEstavel = false;
  static uint32_t instanteMudanca = 0;

  bool agora = lerBotaoEncoderBruto();

  if (agora != estadoEstavel)
  {
    if (millis() - instanteMudanca >= 30) 
    {
      ultimoEstavel = estadoEstavel;
      estadoEstavel = agora;
      instanteMudanca = millis();

      // borda de pressão
      if (estadoEstavel == true && ultimoEstavel == false)
        return true;
    }
  }
  else
  {
    instanteMudanca = millis();
  }

  return false;
}

//* duplo clique 
bool botaoDuploClique()
{
  static uint32_t tempoPrimeiroClique = 0;
  static bool aguardandoSegundo = false;

  if (botaoEncoderClickSimples())
  {
    uint32_t agora = millis();

    if (!aguardandoSegundo)
    {
      aguardandoSegundo = true;
      tempoPrimeiroClique = agora;
      return false; // primeiro clique
    }
    else
    {
      if (agora - tempoPrimeiroClique <= 350) 
      {
        aguardandoSegundo = false;
        return true;
      }
      else
      {
       
        tempoPrimeiroClique = agora;
        return false;
      }
    }
  }

  
  if (aguardandoSegundo && millis() - tempoPrimeiroClique > 350)
    aguardandoSegundo = false;

  return false;
}


//! temperatura
float tempC0, tempC1, tempC2, tempC3;




void setup()
{
  Serial.begin(9600);
  conectaWiFi();
  // ads.begin();

  for (char i = 0; i < 4; i++)
  {
    for (char j = 0; j < 2; j++)
    {
      pinMode(pinMotor[i][j], OUTPUT);
      ledcSetup(chMotor[i][j], frequenciaPWM, resolucaoPWM);
      ledcAttachPin(pinMotor[i][j], chMotor[i][j]);
    }
  }

  mcp.begin_I2C();
  mcp.pinMode(SETA_DIR, OUTPUT);
  mcp.pinMode(SETA_ESQ, OUTPUT);
  mcp.pinMode(FAROL_DIR, OUTPUT);
  mcp.pinMode(FAROL_ESQ, OUTPUT);
  mcp.pinMode(FREIO_DIR, OUTPUT);
  mcp.pinMode(FREIO_ESQ, OUTPUT);

  espClient.setCACert(AWS_ROOT_CA);
  espClient.setCertificate(AWS_CERT);
  espClient.setPrivateKey(AWS_KEY);

  mqtt.setServer(AWS_BROKER, mqtt_port);
  mqtt.setCallback(Callback);
  conectaMQTT();

  tft.init();
  tft.setRotation(1);

  pinMode(ENCODER_A, INPUT_PULLUP);
  pinMode(ENCODER_B, INPUT_PULLUP);
  botao.attach(ENCODER_BTN, INPUT_PULLUP);
  botao.interval(20);

  mostrarLogo();


//* pid
   while (!Serial)
  {
  }

  // pinMode(PINO_BOTAO_ENCODER, INPUT_PULLUP); // ativo em LOW

  Wire.begin();
  mcp.begin_I2C();
  for (uint8_t i = 0; i < 8; ++i)
    mcp.pinMode(i, INPUT);

  carrinho.begin();
  carrinho.setPID(6.0f, 0.5f, 0.5f);    
  carrinho.setVelocidades(25.0f, 0.0f); 

  Serial.println("=== Modo BOTAO (ENCODER pino 19) ===");
  Serial.println("Inicia: faixa central + duplo clique.");
  Serial.println("Parar: duplo clique.");

      
}

#define CALIBRACAO true

// ====================== LOOP ======================
void loop()
{
  checkWiFi();
  if (!mqtt.connected())
    conectaMQTT();

  mqtt.loop();

  botao.update();
  verificarPressionarLongo();

  //* Se estiver na tela de logo, só entra no menu se apertar o botão
  if (estadoAtual == TELA_LOGO) {
    if (botao.fell()) {
      estadoAtual = MENU_PRINCIPAL;
      animarTransicao(1);
      mostrarMenuPrincipal();
    }
    return;
  }

  //* Clique curto
  if (botao.fell()) {
    if (estadoAtual == MENU_PRINCIPAL) {
      if (opcaoPrincipal == 0) {
        estadoAtual = SUBMENU_QR;
        animarTransicao(1);
        mostrarSubmenuQR();
      } else if (opcaoPrincipal == 1) {
        estadoAtual = SUBMENU_CONFIG;
        animarTransicao(1);
        mostrarSubmenuConfig();
      }
    } 
    else if (estadoAtual == SUBMENU_QR) {
      tft.fillScreen(COR_PRETO);
      if (opcaoQR == 0) tft.pushImage(0, 0, 240, 240, rgn);
      else {
        tft.setTextColor(COR_DOURADO);
        tft.setTextSize(2);
        tft.setCursor(20, 100);
        tft.println("QR em desenvolvimento");
      }
      tft.setTextColor(COR_CINZA);
      tft.setCursor(40, 220);
      tft.println("Segure: voltar");
    }
    else if (estadoAtual == SUBMENU_CONFIG) {
      if (opcaoConfig == 0) {
        // Contagem secreta de 5 cliques
        if (millis() - tempoUltimoClique > 1500) contadorBonus = 0;
        contadorBonus++;
        tempoUltimoClique = millis();

        if (contadorBonus >= 5) {
          contadorBonus = 0;
          estadoAtual = JOGANDO_BONUS;
          iniciarJogo(tft, botao, ENCODER_A, ENCODER_B);
          estadoAtual = MENU_PRINCIPAL;
          mostrarMenuPrincipal();
        } else {
          tft.fillScreen(COR_PRETO);
          tft.setTextColor(COR_CINZA);
          tft.setTextSize(2);
          tft.setCursor(20, 100);
          tft.printf("Aperte +%d vez(es)", 5 - contadorBonus);
          delay(400);
          mostrarSubmenuConfig();
        }
      }
    }
  }

  //* Após o jogo: apertar o botão volta ao menu
  if (estadoAtual == JOGANDO_BONUS && botao.fell()) {
    estadoAtual = MENU_PRINCIPAL;
    mostrarMenuPrincipal();
  }

  // 🔹 Encoder navega menus
  if (estadoAtual == MENU_PRINCIPAL || estadoAtual == SUBMENU_QR || estadoAtual == SUBMENU_CONFIG) {
    lerEncoder();
  } 


  //* pid 

  if (CALIBRACAO)
  {
    carrinho.tick();
    return;
  }

  else
  {
    const uint8_t mascaraSensores = carrinho.lerLinhaMascara();
    const bool faixaCentralDetectada = carrinho.detectaCentro(mascaraSensores);

    // ================================
    // Aguardando largada (duplo clique)
    // ================================
    if (!emCorrida && !emAtrasoPartida)
    {
      if (faixaCentralDetectada && botaoDuploClique())
      {
        emAtrasoPartida = true;
        instanteInicioAtraso = millis();
        carrinho.controlarRodas(0.0f, 0.0f, 0.0f);
        Serial.println(">> LARGADA acionada (duplo clique)");
      }
      else
      {
        carrinho.controlarRodas(0.0f, 0.0f, 0.0f);
      }
    }

    // ================================
    // Atraso pós-largada
    // ================================
    if (emAtrasoPartida)
    {
      if (botaoDuploClique())
      {
        pararCarrinho();
      }
      else if (millis() - instanteInicioAtraso >= ATRASO_PARTIDA_MS)
      {
        emAtrasoPartida = false;
        emCorrida = true;
        Serial.println(">> INICIOU corrida");
      }
      else
      {
        carrinho.controlarRodas(0.0f, 0.0f, 0.0f);
      }
    }

    // ================================
    // Seguimento ativo
    // ================================
    if (emCorrida)
    {
      if (botaoDuploClique())
      {
        pararCarrinho();
      }
      else
      {
        carrinho.seguirLinhaStep();
      }
    }
  }


  // //! SENSORES DE TEMPERATURA
  // tempC0 = ads.readADC_SingleEnded(0) * 0.01875;
  // tempC1 = ads.readADC_SingleEnded(1) * 0.01875;
  // tempC2 = ads.readADC_SingleEnded(2) * 0.01875;
  // tempC3 = ads.readADC_SingleEnded(3) * 0.01875;
}

// ====================== MQTT CALLBACK ======================
void Callback(char *topic, byte *payload, unsigned int length)
{
  String msg((char *)payload, length);
  Serial.printf("\nMensagem recebida: %s\n", msg.c_str());

  // temperatura["temperaturas"]["frente_direita"]  = tempC0;
  // temperatura["temperaturas"]["tras_direita"]    = tempC1;
  // temperatura["temperaturas"]["frente_esquerda"] = tempC2;
  // temperatura["temperaturas"]["tras_esquerda"]   = tempC3;

  // serializeJson(temperatura, msg);


  JsonDocument doc;
  if (deserializeJson(doc, msg))
  {
    Serial.println("Erro ao decodificar JSON");
    return;
  }
  
  bool A = doc["Botao0"]; 
  bool B = doc["Botao1"]; 
  bool C = doc["Botao2"]; 
  bool D = doc["Botao3"]; 
  int AnX = doc["AnX"];  

   bool E = doc["Botao4"];

 if (E)  carrinho.iniciarSeguirLinha();
 else  carrinho.entrarCalibracao();

  //* CORRIGE EIXO INVERTIDO
  curvaAtual = -AnX;

  // ******* LEDS (SEM ALTERAÇÃO) *******
  if (A || doc["Farois"] == 1) {
    mcp.digitalWrite(FAROL_DIR, HIGH);
    mcp.digitalWrite(FAROL_ESQ, HIGH);
  } else {
    mcp.digitalWrite(FAROL_DIR, LOW);
    mcp.digitalWrite(FAROL_ESQ, LOW);
  }

  if (C || doc["Farol_Freios"] == 1) {
    mcp.digitalWrite(FREIO_DIR, HIGH);
    mcp.digitalWrite(FREIO_ESQ, HIGH);
  } else {
    mcp.digitalWrite(FREIO_DIR, LOW);
    mcp.digitalWrite(FREIO_ESQ, LOW);
  }

if (B || AnX > 20 || doc["SetaDir"] == 1) {

    if (millis() - tempoSetaDir >= 1000) {   
        tempoSetaDir = millis();
        estadoSetaDir = !estadoSetaDir;     
        mcp.digitalWrite(SETA_DIR, estadoSetaDir);
    }

} else {
  
    estadoSetaDir = false;
    mcp.digitalWrite(SETA_DIR, LOW);
}


if (D || AnX < -20 || doc["SetaEsq"] == 1) {

    if (millis() - tempoSetaEsq >= 1000) {   
        tempoSetaEsq = millis();
        estadoSetaEsq = !estadoSetaEsq;     
        mcp.digitalWrite(SETA_ESQ, estadoSetaEsq);
    }

} else {
    // quando soltar o comando, garante LED apagado
    estadoSetaEsq = false;
    mcp.digitalWrite(SETA_ESQ, LOW);
}

  // ******* MOVIMENTO *******
  if (A) frente(40, curvaAtual);
  else if (C) tras(40);
  else if (D) esquerda_lateral();
  else if (B) direita_lateral();
  else parar();
}

// ====================== FUNÇÕES DE MOVIMENTO ======================
uint8_t velocidade(uint8_t valor)
{
  return valor != 0 ? map(valor, 0, 100, 150, 255) : 0;
}

void mover(int velEsq, int velDir)
{
  velEsq = constrain(velEsq, -100, 100);
  velDir = constrain(velDir, -100, 100);

  uint8_t vE = velocidade(abs(velEsq));
  uint8_t vD = velocidade(abs(velDir));

  // ==== Motores da ESQUERDA agora são M2 e M3 ====
  for (int i = 2; i < 4; i++)
  {
    ledcWrite(chMotor[i][0], velEsq > 0 ? vE : 0);
    ledcWrite(chMotor[i][1], velEsq < 0 ? vE : 0);
  }

  // ==== Motores da DIREITA agora são M0 e M1 ====
  for (int i = 0; i < 2; i++)
  {
    ledcWrite(chMotor[i][0], velDir > 0 ? vD : 0);
    ledcWrite(chMotor[i][1], velDir < 0 ? vD : 0);
  }
}

// ---------- FRENTE COM CURVA CORRIGIDA ----------
void frente(uint8_t vel, int curva)
{
  int velEsq = vel;
  int velDir = vel;

  if (curva < -20) {       // virar esquerda
    velEsq = 60;
    velDir = 3;
  }
  else if (curva > 20) {  // virar direita
    velEsq = 3;
    velDir = 60;
  }

  mover(velEsq, velDir);
}

// ---------- TRÁS ----------
void tras(uint8_t vel)
{
  mover(-vel, -vel);
}

// ---------- LATERAL ESQUERDA (B) ----------
void esquerda_lateral()
{
  ledcWrite(chMotor[0][1], velocidade(40));
  ledcWrite(chMotor[1][0], velocidade(40));
  ledcWrite(chMotor[2][0], velocidade(40));
  ledcWrite(chMotor[3][1], velocidade(40));
}

// ---------- LATERAL DIREITA (D) ----------
void direita_lateral()
{
  ledcWrite(chMotor[0][0], velocidade(40));
  ledcWrite(chMotor[1][1], velocidade(40));
  ledcWrite(chMotor[2][1], velocidade(40));
  ledcWrite(chMotor[3][0], velocidade(40));
}

// ---------- PARAR ----------
void parar()
{
  mover(0, 0);
}

void verificarPressionarLongo() {
  if (botao.read() == LOW && !segurando) {
    tempoPressionado = millis();
    segurando = true;
  } 
  else if (botao.read() == HIGH && segurando) {
    segurando = false;
  }

  if (segurando && millis() - tempoPressionado > 3000) {
    segurando = false;

    // 🔹 Retorna um nível de menu
    if (estadoAtual == SUBMENU_QR || estadoAtual == SUBMENU_CONFIG) {
      estadoAtual = MENU_PRINCIPAL;
      animarTransicao(-1);
      mostrarMenuPrincipal();
    }
    else if (estadoAtual == MENU_PRINCIPAL) {
      estadoAtual = TELA_LOGO;
      animarTransicao(-1);
      mostrarLogo();
    }
    else if (estadoAtual == JOGANDO_BONUS) {
      estadoAtual = MENU_PRINCIPAL;
      animarTransicao(-1);
      mostrarMenuPrincipal();
    }
  }
}

void animarTransicao(int direcao) {
  for (int i = 0; i < tft.width(); i += 15) {
    tft.fillRect(i * direcao, 0, 15, tft.height(), COR_PRETO);
    delay(5);
  }
}

void mostrarLogo() {
  tft.fillScreen(COR_PRETO);
  tft.pushImage(0, 0, 240, 240, rgn_logo);
}

void mostrarMenuPrincipal() {
  tft.fillScreen(COR_PRETO);
  tft.setTextColor(COR_DOURADO);
  tft.setTextSize(3);
  tft.setCursor(30, 30);
  tft.println("Menu");

  tft.setTextSize(2);
  for (int i = 0; i < totalPrincipais; i++) {
    tft.setCursor(40, 80 + i * 40);
    if (i == opcaoPrincipal) {
      tft.setTextColor(COR_DOURADO);
      tft.print("> ");
    } else {
      tft.setTextColor(COR_BRANCO);
      tft.print("  ");
    }
    tft.println(opcoesPrincipais[i]);
  }
}

void mostrarSubmenuQR() {
  tft.fillScreen(COR_PRETO);
  tft.setTextColor(COR_DOURADO);
  tft.setTextSize(3);
  tft.setCursor(30, 30);
  tft.println("QR Code");

  tft.setTextSize(2);
  for (int i = 0; i < totalQR; i++) {
    tft.setCursor(40, 80 + i * 40);
    if (i == opcaoQR) {
      tft.setTextColor(COR_DOURADO);
      tft.print("> ");
    } else {
      tft.setTextColor(COR_BRANCO);
      tft.print("  ");
    }
    tft.println(opcoesQR[i]);
  }
}

void mostrarSubmenuConfig() {
  tft.fillScreen(COR_PRETO);
  tft.setTextColor(COR_DOURADO);
  tft.setTextSize(3);
  tft.setCursor(20, 30);
  tft.println("Configuracoes");

  tft.setTextSize(2);
  for (int i = 0; i < totalConfig; i++) {
    tft.setCursor(40, 100);
    if (i == opcaoConfig) {
      tft.setTextColor(COR_DOURADO);
      tft.print("> ");
    } else {
      tft.setTextColor(COR_BRANCO);
      tft.print("  ");
    }
    tft.println(opcoesConfig[i]);
  }
}

void lerEncoder() {
  bool estadoA = digitalRead(ENCODER_A);
  bool estadoB = digitalRead(ENCODER_B);

  if (estadoA != ultimoA) {
    if (estadoB != estadoA) {
      if (estadoAtual == MENU_PRINCIPAL) {
        opcaoPrincipal = (opcaoPrincipal + 1) % totalPrincipais;
        mostrarMenuPrincipal();
      } else if (estadoAtual == SUBMENU_QR) {
        opcaoQR = (opcaoQR + 1) % totalQR;
        mostrarSubmenuQR();
      } else if (estadoAtual == SUBMENU_CONFIG) {
        opcaoConfig = (opcaoConfig + 1) % totalConfig;
        mostrarSubmenuConfig();
      }
    } else {
      if (estadoAtual == MENU_PRINCIPAL) {
        opcaoPrincipal = (opcaoPrincipal - 1 + totalPrincipais) % totalPrincipais;
        mostrarMenuPrincipal();
      } else if (estadoAtual == SUBMENU_QR) {
        opcaoQR = (opcaoQR - 1 + totalQR) % totalQR;
        mostrarSubmenuQR();
      } else if (estadoAtual == SUBMENU_CONFIG) {
        opcaoConfig = (opcaoConfig - 1 + totalConfig) % totalConfig;
        mostrarSubmenuConfig();
      }
    }
  }
  ultimoA = estadoA;
}

void pararCarrinho()
{
  emCorrida = false;
  emAtrasoPartida = false;
  carrinho.controlarRodas(0.0f, 0.0f, 0.0f);
  Serial.println(">> PAROU (duplo clique)");
}


// ====================== CONECTA MQTT ======================
void conectaMQTT()
{
  while (!mqtt.connected())
  {
    Serial.print("Conectando ao MQTT...");
    if (mqtt.connect(mqtt_id))
    {
      Serial.println("Conectado!");
      mqtt.subscribe(mqtt_SUB);
    }
    else
    {
      Serial.printf("Falhou (%d). Tentando novamente em 5s\n", mqtt.state());
      delay(5000);
    }
  }
}







// #include <Arduino.h>
// #include <WiFiClientSecure.h>
// #include <PubSubClient.h>
// #include <ArduinoJson.h>
// #include "internet.h"
// #include "certificados.h"
// #include <TFT_eSPI.h>
// #include <SPI.h>
// #include <Bounce2.h>
// #include "imagem.h"
// #include "mini_game.h"

// #include <Adafruit_MCP23X17.h>
// Adafruit_MCP23X17 mcp;

// // ====================== CONFIGURAÇÃO DOS MOTORES ======================
// #define pinM0Dir 18
// #define pinM0Esq 3
// #define pinM1Dir 10
// #define pinM1Esq 46
// #define pinM2Dir 13
// #define pinM2Esq 14
// #define pinM3Dir 12
// #define pinM3Esq 11


// #define frequenciaPWM 20000
// #define resolucaoPWM 8

// const uint8_t pinMotor[4][2] = {
//     {pinM0Esq, pinM0Dir},
//     {pinM1Esq, pinM1Dir},
//     {pinM2Esq, pinM2Dir},
//     {pinM3Esq, pinM3Dir}
// };

// const uint8_t chMotor[4][2] = {
//     {0, 1},
//     {2, 3},
//     {4, 5},
//     {6, 7}
// };

// // ====================== PINOS LEDS (MCP23X17) ======================
// #define SETA_DIR       10
// #define SETA_ESQ       13
// #define FAROL_DIR      12
// #define FAROL_ESQ      11
// #define FREIO_DIR      15
// #define FREIO_ESQ      14

// // ====================== VARIÁVEIS LED ======================
// bool estado_farol = 0;
// bool estado_seta_dir = 0;
// bool estado_seta_esq = 0;
// bool estado_freio = 0;

// unsigned long tempoSetaDir = 0;
// unsigned long tempoSetaEsq = 0;

// bool estadoSetaDir = false;
// bool estadoSetaEsq = false;

// // ====================== CONFIGURAÇÃO MQTT ======================
// const int mqtt_port = 8883;
// const char *mqtt_id = "CarrinhoESP32";
// const char *mqtt_SUB = "projeto/carrinho";
// WiFiClientSecure espClient;
// PubSubClient mqtt(espClient);

// uint8_t velocidade(uint8_t);
// void mover(int velEsq, int velDir);
// void frente(uint8_t vel = 40, int curva = 0);
// void tras(uint8_t vel = 40);
// void direita_lateral();
// void esquerda_lateral();
// void parar();
// void Callback(char *topic, byte *payload, unsigned int length);
// void conectaMQTT();

// int curvaAtual = 0;

// //!----------------------- encoder ---------------------------------------
// #define ENCODER_A 38
// #define ENCODER_B 37
// #define ENCODER_BTN 19

// TFT_eSPI tft = TFT_eSPI(); 
// Bounce botao;
// bool ultimoA = HIGH;
// unsigned long tempoPressionado = 0;
// bool segurando = false;

// int contadorBonus = 0;
// unsigned long tempoUltimoClique = 0;

// enum Estado {
//   TELA_LOGO,
//   MENU_PRINCIPAL,
//   SUBMENU_QR,
//   SUBMENU_CONFIG,
//   JOGANDO_BONUS
// };

// Estado estadoAtual = TELA_LOGO;

// int opcaoPrincipal = 0;
// const char* opcoesPrincipais[] = {"QR Code", "Configuracoes"};
// int totalPrincipais = sizeof(opcoesPrincipais) / sizeof(opcoesPrincipais[0]);

// int opcaoQR = 0;
// const char* opcoesQR[] = {"QR Site", "QR App", "QR Dashboard"};
// int totalQR = sizeof(opcoesQR) / sizeof(opcoesQR[0]);

// int opcaoConfig = 0;
// const char* opcoesConfig[] = {"Bonus"};
// int totalConfig = sizeof(opcoesConfig) / sizeof(opcoesConfig[0]);

// #define COR_PRETO   TFT_BLACK
// #define COR_DOURADO tft.color565(212, 175, 55)
// #define COR_BRANCO  TFT_WHITE
// #define COR_CINZA   TFT_DARKGREY

// void mostrarMenuPrincipal();
// void mostrarSubmenuQR();
// void mostrarSubmenuConfig();
// void lerEncoder();
// void animarTransicao(int direcao);
// void verificarPressionarLongo();
// void mostrarLogo();
// //! =================================================================================





// void setup()
// {
//   Serial.begin(9600);
//   conectaWiFi();

//   for (char i = 0; i < 4; i++)
//   {
//     for (char j = 0; j < 2; j++)
//     {
//       pinMode(pinMotor[i][j], OUTPUT);
//       ledcSetup(chMotor[i][j], frequenciaPWM, resolucaoPWM);
//       ledcAttachPin(pinMotor[i][j], chMotor[i][j]);
//     }
//   }

//   mcp.begin_I2C();
//   mcp.pinMode(SETA_DIR, OUTPUT);
//   mcp.pinMode(SETA_ESQ, OUTPUT);
//   mcp.pinMode(FAROL_DIR, OUTPUT);
//   mcp.pinMode(FAROL_ESQ, OUTPUT);
//   mcp.pinMode(FREIO_DIR, OUTPUT);
//   mcp.pinMode(FREIO_ESQ, OUTPUT);

//   espClient.setCACert(AWS_ROOT_CA);
//   espClient.setCertificate(AWS_CERT);
//   espClient.setPrivateKey(AWS_KEY);

//   mqtt.setServer(AWS_BROKER, mqtt_port);
//   mqtt.setCallback(Callback);
//   conectaMQTT();

//   tft.init();
//   tft.setRotation(1);

//   pinMode(ENCODER_A, INPUT_PULLUP);
//   pinMode(ENCODER_B, INPUT_PULLUP);
//   botao.attach(ENCODER_BTN, INPUT_PULLUP);
//   botao.interval(20);

//   mostrarLogo();
// }

// // ====================== LOOP ======================
// void loop()
// {
//   checkWiFi();
//   if (!mqtt.connected())
//     conectaMQTT();

//   mqtt.loop();

//   botao.update();
//   verificarPressionarLongo();

//   // 🔹 Se estiver na tela de logo, só entra no menu ao apertar o botão
//   if (estadoAtual == TELA_LOGO) {
//     if (botao.fell()) {
//       estadoAtual = MENU_PRINCIPAL;
//       animarTransicao(1);
//       mostrarMenuPrincipal();
//     }
//     return;
//   }

//   // 🔹 Clique curto
//   if (botao.fell()) {
//     if (estadoAtual == MENU_PRINCIPAL) {
//       if (opcaoPrincipal == 0) {
//         estadoAtual = SUBMENU_QR;
//         animarTransicao(1);
//         mostrarSubmenuQR();
//       } else if (opcaoPrincipal == 1) {
//         estadoAtual = SUBMENU_CONFIG;
//         animarTransicao(1);
//         mostrarSubmenuConfig();
//       }
//     } 
//     else if (estadoAtual == SUBMENU_QR) {
//       tft.fillScreen(COR_PRETO);
//       if (opcaoQR == 0) tft.pushImage(0, 0, 240, 240, rgn);
//       else {
//         tft.setTextColor(COR_DOURADO);
//         tft.setTextSize(2);
//         tft.setCursor(20, 100);
//         tft.println("QR em desenvolvimento");
//       }
//       tft.setTextColor(COR_CINZA);
//       tft.setCursor(40, 220);
//       tft.println("Segure: voltar");
//     }
//     else if (estadoAtual == SUBMENU_CONFIG) {
//       if (opcaoConfig == 0) {
//         // Contagem secreta de 5 cliques
//         if (millis() - tempoUltimoClique > 1500) contadorBonus = 0;
//         contadorBonus++;
//         tempoUltimoClique = millis();

//         if (contadorBonus >= 5) {
//           contadorBonus = 0;
//           estadoAtual = JOGANDO_BONUS;
//           iniciarJogo(tft, botao, ENCODER_A, ENCODER_B);
//           estadoAtual = MENU_PRINCIPAL;
//           mostrarMenuPrincipal();
//         } else {
//           tft.fillScreen(COR_PRETO);
//           tft.setTextColor(COR_CINZA);
//           tft.setTextSize(2);
//           tft.setCursor(20, 100);
//           tft.printf("Aperte +%d vez(es)", 5 - contadorBonus);
//           delay(400);
//           mostrarSubmenuConfig();
//         }
//       }
//     }
//   }

//   // 🔹 Após o jogo: apertar o botão volta ao menu
//   if (estadoAtual == JOGANDO_BONUS && botao.fell()) {
//     estadoAtual = MENU_PRINCIPAL;
//     mostrarMenuPrincipal();
//   }

//   // 🔹 Encoder navega menus
//   if (estadoAtual == MENU_PRINCIPAL || estadoAtual == SUBMENU_QR || estadoAtual == SUBMENU_CONFIG) {
//     lerEncoder();
//   }
// }

// // ====================== MQTT CALLBACK ======================
// void Callback(char *topic, byte *payload, unsigned int length)
// {
//   String msg((char *)payload, length);
//   Serial.printf("\nMensagem recebida: %s\n", msg.c_str());

//   JsonDocument doc;
//   if (deserializeJson(doc, msg))
//   {
//     Serial.println("Erro ao decodificar JSON");
//     return;
//   }

//   bool A = doc["Botao0"]; 
//   bool B = doc["Botao1"]; 
//   bool C = doc["Botao2"]; 
//   bool D = doc["Botao3"]; 
//   int AnX = doc["AnX"];  

//   // CORRIGE EIXO INVERTIDO
//   curvaAtual = -AnX;

//   // ******* LEDS (SEM ALTERAÇÃO) *******
//   if (A || doc["Farois"] == 1) {
//     mcp.digitalWrite(FAROL_DIR, HIGH);
//     mcp.digitalWrite(FAROL_ESQ, HIGH);
//   } else {
//     mcp.digitalWrite(FAROL_DIR, LOW);
//     mcp.digitalWrite(FAROL_ESQ, LOW);
//   }

//   if (C || doc["Farol_Freios"] == 1) {
//     mcp.digitalWrite(FREIO_DIR, HIGH);
//     mcp.digitalWrite(FREIO_ESQ, HIGH);
//   } else {
//     mcp.digitalWrite(FREIO_DIR, LOW);
//     mcp.digitalWrite(FREIO_ESQ, LOW);
//   }

// if (B || AnX > 20 || doc["SetaDir"] == 1) {

//     if (millis() - tempoSetaDir >= 1000) {   // troca a cada 500 ms
//         tempoSetaDir = millis();
//         estadoSetaDir = !estadoSetaDir;     // inverte HIGH/LOW
//         mcp.digitalWrite(SETA_DIR, estadoSetaDir);
//     }

// } else {
//     // quando soltar o comando, garante LED apagado
//     estadoSetaDir = false;
//     mcp.digitalWrite(SETA_DIR, LOW);
// }


// if (D || AnX < -20 || doc["SetaEsq"] == 1) {

//     if (millis() - tempoSetaEsq >= 1000) {   // troca a cada 300 ms
//         tempoSetaEsq = millis();
//         estadoSetaEsq = !estadoSetaEsq;     // inverte HIGH/LOW
//         mcp.digitalWrite(SETA_ESQ, estadoSetaEsq);
//     }

// } else {
//     // quando soltar o comando, garante LED apagado
//     estadoSetaEsq = false;
//     mcp.digitalWrite(SETA_ESQ, LOW);
// }

//   // ******* MOVIMENTO *******
//   if (A) frente(40, curvaAtual);
//   else if (C) tras(40);
//   else if (D) esquerda_lateral();
//   else if (B) direita_lateral();
//   else parar();
// }

// // ====================== FUNÇÕES DE MOVIMENTO ======================
// uint8_t velocidade(uint8_t valor)
// {
//   return valor != 0 ? map(valor, 0, 100, 150, 255) : 0;
// }

// void mover(int velEsq, int velDir)
// {
//   velEsq = constrain(velEsq, -100, 100);
//   velDir = constrain(velDir, -100, 100);

//   uint8_t vE = velocidade(abs(velEsq));
//   uint8_t vD = velocidade(abs(velDir));

//   // ==== Motores da ESQUERDA agora são M2 e M3 ====
//   for (int i = 2; i < 4; i++)
//   {
//     ledcWrite(chMotor[i][0], velEsq > 0 ? vE : 0);
//     ledcWrite(chMotor[i][1], velEsq < 0 ? vE : 0);
//   }

//   // ==== Motores da DIREITA agora são M0 e M1 ====
//   for (int i = 0; i < 2; i++)
//   {
//     ledcWrite(chMotor[i][0], velDir > 0 ? vD : 0);
//     ledcWrite(chMotor[i][1], velDir < 0 ? vD : 0);
//   }
// }

// // ---------- FRENTE COM CURVA CORRIGIDA ----------
// void frente(uint8_t vel, int curva)
// {
//   int velEsq = vel;
//   int velDir = vel;

//   if (curva < -20) {       // virar esquerda
//     velEsq = 60;
//     velDir = 3;
//   }
//   else if (curva > 20) {  // virar direita
//     velEsq = 3;
//     velDir = 60;
//   }

//   mover(velEsq, velDir);
// }

// // ---------- TRÁS ----------
// void tras(uint8_t vel)
// {
//   mover(-vel, -vel);
// }

// // ---------- LATERAL ESQUERDA (B) ----------
// void esquerda_lateral()
// {
//   ledcWrite(chMotor[0][1], velocidade(40));
//   ledcWrite(chMotor[1][0], velocidade(40));
//   ledcWrite(chMotor[2][0], velocidade(40));
//   ledcWrite(chMotor[3][1], velocidade(40));
// }

// // ---------- LATERAL DIREITA (D) ----------
// void direita_lateral()
// {
//   ledcWrite(chMotor[0][0], velocidade(40));
//   ledcWrite(chMotor[1][1], velocidade(40));
//   ledcWrite(chMotor[2][1], velocidade(40));
//   ledcWrite(chMotor[3][0], velocidade(40));
// }

// // ---------- PARAR ----------
// void parar()
// {
//   mover(0, 0);
// }

// void verificarPressionarLongo() {
//   if (botao.read() == LOW && !segurando) {
//     tempoPressionado = millis();
//     segurando = true;
//   } 
//   else if (botao.read() == HIGH && segurando) {
//     segurando = false;
//   }

//   if (segurando && millis() - tempoPressionado > 3000) {
//     segurando = false;

//     // 🔹 Retorna um nível de menu
//     if (estadoAtual == SUBMENU_QR || estadoAtual == SUBMENU_CONFIG) {
//       estadoAtual = MENU_PRINCIPAL;
//       animarTransicao(-1);
//       mostrarMenuPrincipal();
//     }
//     else if (estadoAtual == MENU_PRINCIPAL) {
//       estadoAtual = TELA_LOGO;
//       animarTransicao(-1);
//       mostrarLogo();
//     }
//     else if (estadoAtual == JOGANDO_BONUS) {
//       estadoAtual = MENU_PRINCIPAL;
//       animarTransicao(-1);
//       mostrarMenuPrincipal();
//     }
//   }
// }

// void animarTransicao(int direcao) {
//   for (int i = 0; i < tft.width(); i += 15) {
//     tft.fillRect(i * direcao, 0, 15, tft.height(), COR_PRETO);
//     delay(5);
//   }
// }

// void mostrarLogo() {
//   tft.fillScreen(COR_PRETO);
//   tft.pushImage(0, 0, 240, 240, rgn_logo);
// }

// void mostrarMenuPrincipal() {
//   tft.fillScreen(COR_PRETO);
//   tft.setTextColor(COR_DOURADO);
//   tft.setTextSize(3);
//   tft.setCursor(30, 30);
//   tft.println("Menu");

//   tft.setTextSize(2);
//   for (int i = 0; i < totalPrincipais; i++) {
//     tft.setCursor(40, 80 + i * 40);
//     if (i == opcaoPrincipal) {
//       tft.setTextColor(COR_DOURADO);
//       tft.print("> ");
//     } else {
//       tft.setTextColor(COR_BRANCO);
//       tft.print("  ");
//     }
//     tft.println(opcoesPrincipais[i]);
//   }
// }

// void mostrarSubmenuQR() {
//   tft.fillScreen(COR_PRETO);
//   tft.setTextColor(COR_DOURADO);
//   tft.setTextSize(3);
//   tft.setCursor(30, 30);
//   tft.println("QR Code");

//   tft.setTextSize(2);
//   for (int i = 0; i < totalQR; i++) {
//     tft.setCursor(40, 80 + i * 40);
//     if (i == opcaoQR) {
//       tft.setTextColor(COR_DOURADO);
//       tft.print("> ");
//     } else {
//       tft.setTextColor(COR_BRANCO);
//       tft.print("  ");
//     }
//     tft.println(opcoesQR[i]);
//   }
// }

// void mostrarSubmenuConfig() {
//   tft.fillScreen(COR_PRETO);
//   tft.setTextColor(COR_DOURADO);
//   tft.setTextSize(3);
//   tft.setCursor(20, 30);
//   tft.println("Configuracoes");

//   tft.setTextSize(2);
//   for (int i = 0; i < totalConfig; i++) {
//     tft.setCursor(40, 100);
//     if (i == opcaoConfig) {
//       tft.setTextColor(COR_DOURADO);
//       tft.print("> ");
//     } else {
//       tft.setTextColor(COR_BRANCO);
//       tft.print("  ");
//     }
//     tft.println(opcoesConfig[i]);
//   }
// }

// void lerEncoder() {
//   bool estadoA = digitalRead(ENCODER_A);
//   bool estadoB = digitalRead(ENCODER_B);

//   if (estadoA != ultimoA) {
//     if (estadoB != estadoA) {
//       if (estadoAtual == MENU_PRINCIPAL) {
//         opcaoPrincipal = (opcaoPrincipal + 1) % totalPrincipais;
//         mostrarMenuPrincipal();
//       } else if (estadoAtual == SUBMENU_QR) {
//         opcaoQR = (opcaoQR + 1) % totalQR;
//         mostrarSubmenuQR();
//       } else if (estadoAtual == SUBMENU_CONFIG) {
//         opcaoConfig = (opcaoConfig + 1) % totalConfig;
//         mostrarSubmenuConfig();
//       }
//     } else {
//       if (estadoAtual == MENU_PRINCIPAL) {
//         opcaoPrincipal = (opcaoPrincipal - 1 + totalPrincipais) % totalPrincipais;
//         mostrarMenuPrincipal();
//       } else if (estadoAtual == SUBMENU_QR) {
//         opcaoQR = (opcaoQR - 1 + totalQR) % totalQR;
//         mostrarSubmenuQR();
//       } else if (estadoAtual == SUBMENU_CONFIG) {
//         opcaoConfig = (opcaoConfig - 1 + totalConfig) % totalConfig;
//         mostrarSubmenuConfig();
//       }
//     }
//   }
//   ultimoA = estadoA;
// }

// // ====================== CONECTA MQTT ======================
// void conectaMQTT()
// {
//   while (!mqtt.connected())
//   {
//     Serial.print("Conectando ao MQTT...");
//     if (mqtt.connect(mqtt_id))
//     {
//       Serial.println("Conectado!");
//       mqtt.subscribe(mqtt_SUB);
//     }
//     else
//     {
//       Serial.printf("Falhou (%d). Tentando novamente em 5s\n", mqtt.state());
//       delay(5000);
//     }
//   }
// }


