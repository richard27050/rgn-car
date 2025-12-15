#include <Arduino.h>
#include "luzes.h"

// ======================= PINOS DAS SETAS =======================
#define PIN_SETA_DIR 40
#define PIN_SETA_ESQ 41

// ======================= VARIÁVEIS =======================
bool setaDir = false;
bool setaEsq = false;

unsigned long lastBlink = 0;
bool blinkState = false;

namespace Luzes {

void begin() {
    pinMode(PIN_SETA_DIR, OUTPUT);
    pinMode(PIN_SETA_ESQ, OUTPUT);
    digitalWrite(PIN_SETA_DIR, LOW);
    digitalWrite(PIN_SETA_ESQ, LOW);
}

void piscarDireita() {
    setaDir = true;
    delay(200);
    setaEsq = false;
}

void piscarEsquerda() {
    setaEsq = true;
    delay(200);
    setaDir = false;
}

void desligaSetas() {
    setaDir = false;
    setaEsq = false;
    digitalWrite(PIN_SETA_DIR, LOW);
    digitalWrite(PIN_SETA_ESQ, LOW);
}

void atualizar() {
    if (!setaDir && !setaEsq) return;

    unsigned long agora = millis();
    if (agora - lastBlink > 300) {  
        lastBlink = agora;
        blinkState = !blinkState;

        if (setaDir) digitalWrite(PIN_SETA_DIR, blinkState);
        else digitalWrite(PIN_SETA_DIR, LOW);

        if (setaEsq) digitalWrite(PIN_SETA_ESQ, blinkState);
        else digitalWrite(PIN_SETA_ESQ, LOW);
    }
}

}
