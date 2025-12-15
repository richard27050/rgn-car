#ifndef MINI_GAME_H
#define MINI_GAME_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Bounce2.h>

#define COR_PRETO     TFT_BLACK
#define COR_DOURADO   tft.color565(212, 175, 55)
#define COR_DOURADO_ESCURO tft.color565(150, 120, 30)
#define COR_CINZA     tft.color565(80, 80, 80)
#define COR_AZUL      TFT_BLUE
#define COR_VERMELHO  TFT_RED
#define COR_BRANCO    TFT_WHITE

struct Obstaculo {
  int x, y;
};

void desenharCarro(TFT_eSPI &tft, int x, int y) {
  // Base dourada principal
  tft.fillRect(x, y, 20, 20, COR_DOURADO);
  tft.drawRect(x, y, 20, 20, COR_DOURADO_ESCURO);

  // Capô e teto (janelas em cinza)
  tft.fillRect(x + 4, y + 3, 12, 6, COR_CINZA);
  tft.fillRect(x + 4, y + 10, 12, 3, COR_CINZA);

  // Parachoques
  tft.fillRect(x + 2, y - 2, 16, 2, COR_DOURADO_ESCURO);
  tft.fillRect(x + 2, y + 20, 16, 2, COR_DOURADO_ESCURO);

  // Rodas (pretas)
  tft.fillRect(x - 2, y + 2, 3, 5, COR_PRETO);
  tft.fillRect(x + 19, y + 2, 3, 5, COR_PRETO);
  tft.fillRect(x - 2, y + 13, 3, 5, COR_PRETO);
  tft.fillRect(x + 19, y + 13, 3, 5, COR_PRETO);

  // Detalhes frontais (faróis)
  tft.fillRect(x + 4, y - 3, 4, 2, COR_BRANCO);
  tft.fillRect(x + 12, y - 3, 4, 2, COR_BRANCO);
}

void iniciarJogo(TFT_eSPI &tft, Bounce &botao, int pinA, int pinB) {
  tft.fillScreen(COR_PRETO);
  int carroX = 110;
  int carroY = 200;
  bool ultimoA = HIGH;

  Obstaculo obstaculos[3];
  for (int i = 0; i < 3; i++) {
    obstaculos[i].x = random(20, 200);
    obstaculos[i].y = -i * 80;
  }

  int obstaculosDesviados = 0;
  bool venceu = false;

  while (true) {
    botao.update();
    bool estadoA = digitalRead(pinA);
    bool estadoB = digitalRead(pinB);

    if (estadoA != ultimoA) {
      if (estadoB != estadoA) carroX += 15;
      else carroX -= 15;
      if (carroX < 20) carroX = 20;
      if (carroX > 200) carroX = 200;
    }
    ultimoA = estadoA;

    // Atualiza posição dos obstáculos
    for (int i = 0; i < 3; i++) {
      obstaculos[i].y += 5;
      if (obstaculos[i].y > 240) {
        obstaculos[i].y = -random(40, 120);
        obstaculos[i].x = random(20, 200);
        obstaculosDesviados++;
      }

      // Colisão
      if (abs(carroX - obstaculos[i].x) < 18 && abs(carroY - obstaculos[i].y) < 18) {
        tft.fillScreen(COR_VERMELHO);
        tft.setTextColor(COR_BRANCO);
        tft.setTextSize(2);
        tft.setCursor(50, 110);
        tft.println("Game Over!");
        delay(1500);
        return;
      }
    }

    // Vitória
    if (obstaculosDesviados >= 10) {
      venceu = true;
      break;
    }

    // Desenha pista e elementos
    tft.fillScreen(COR_PRETO);
    tft.drawRect(10, 0, 220, 240, COR_BRANCO);

    // Linhas da pista (tracejadas)
    for (int i = 0; i < 240; i += 40) {
      tft.fillRect(118, i, 4, 20, COR_CINZA);
    }

    // Obstáculos azuis
    for (int i = 0; i < 3; i++) {
      tft.fillRect(obstaculos[i].x, obstaculos[i].y, 20, 20, COR_AZUL);
    }

    // Carro dourado detalhado
    desenharCarro(tft, carroX, carroY);

    delay(40);
  }

  // Mensagem de vitória piscando
  if (venceu) {
    unsigned long tempo = millis();
    while (millis() - tempo < 5000) {
      tft.fillScreen(COR_PRETO);
      if ((millis() / 300) % 2 == 0)
        tft.setTextColor(COR_VERMELHO);
      else
        tft.setTextColor(COR_BRANCO);
      tft.setTextSize(2);
      tft.setCursor(10, 110);
      tft.println("PARABENS VOCE VENCEU");
      tft.setCursor(40, 140);
      tft.println("A RGN CUP!");
      delay(100);
    }
  }
}

#endif
