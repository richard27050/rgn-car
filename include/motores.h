#ifndef MOTORES_H
#define MOTORES_H

#define enM0A 48
#define enM0B 47
#define enM1A 16
#define enM1B 17
#define enM2A 7
#define enM2B 15
#define enM3A 21
#define enM3B 20

void setupMotores() {
  pinMode(enM0A, OUTPUT); pinMode(enM0B, OUTPUT);
  pinMode(enM1A, OUTPUT); pinMode(enM1B, OUTPUT);
  pinMode(enM2A, OUTPUT); pinMode(enM2B, OUTPUT);
  pinMode(enM3A, OUTPUT); pinMode(enM3B, OUTPUT);
}

void pararMotores() {
  digitalWrite(enM0A, LOW); digitalWrite(enM0B, LOW);
  digitalWrite(enM1A, LOW); digitalWrite(enM1B, LOW);
  digitalWrite(enM2A, LOW); digitalWrite(enM2B, LOW);
  digitalWrite(enM3A, LOW); digitalWrite(enM3B, LOW);
}

void moverFrente(int velocidade) {
  analogWrite(enM0A, velocidade); analogWrite(enM0B, LOW);
  analogWrite(enM1A, velocidade); analogWrite(enM1B, LOW);
  analogWrite(enM2A, velocidade); analogWrite(enM2B, LOW);
  analogWrite(enM3A, velocidade); analogWrite(enM3B, LOW);
}

void moverTras(int velocidade) {
  analogWrite(enM0A, LOW); analogWrite(enM0B, velocidade);
  analogWrite(enM1A, LOW); analogWrite(enM1B, velocidade);
  analogWrite(enM2A, LOW); analogWrite(enM2B, velocidade);
  analogWrite(enM3A, LOW); analogWrite(enM3B, velocidade);
}

void moverDireita(int velocidade) {
  analogWrite(enM0A, LOW); analogWrite(enM0B, velocidade);
  analogWrite(enM1A, velocidade); analogWrite(enM1B, LOW);
  analogWrite(enM2A, LOW); analogWrite(enM2B, velocidade);
  analogWrite(enM3A, velocidade); analogWrite(enM3B, LOW);
}

void moverEsquerda(int velocidade) {
  analogWrite(enM0A, velocidade); analogWrite(enM0B, LOW);
  analogWrite(enM1A, LOW); analogWrite(enM1B, velocidade);
  analogWrite(enM2A, velocidade); analogWrite(enM2B, LOW);
  analogWrite(enM3A, LOW); analogWrite(enM3B, velocidade);
}

void moverCurvaDireita(int velExterno, int velInterno) {
  analogWrite(enM0A, velExterno); analogWrite(enM0B, LOW);
  analogWrite(enM1A, velInterno); analogWrite(enM1B, LOW);
  analogWrite(enM2A, velExterno); analogWrite(enM2B, LOW);
  analogWrite(enM3A, velInterno); analogWrite(enM3B, LOW);
}

void moverCurvaEsquerda(int velInterno, int velExterno) {
  analogWrite(enM0A, velInterno); analogWrite(enM0B, LOW);
  analogWrite(enM1A, velExterno); analogWrite(enM1B, LOW);
  analogWrite(enM2A, velInterno); analogWrite(enM2B, LOW);
  analogWrite(enM3A, velExterno); analogWrite(enM3B, LOW);
}

#endif
