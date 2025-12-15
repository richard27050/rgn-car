#ifndef LUZES_H
#define LUZES_H

#include <Arduino.h>
#include <Adafruit_MCP23X17.h>


extern Adafruit_MCP23X17 mcp;


#define LUZ_FAROL_DIR   12
#define LUZ_FAROL_ESQ   11
#define LUZ_RE_DIR      15
#define LUZ_RE_ESQ      14
#define LUZ_SETA_DIR    10
#define LUZ_SETA_ESQ    13


void setupLuzes();
void luzFrente();           
void luzTras();             
void luzDireita();         
void luzEsquerda();       
void luzCurvaDireita();    
void luzCurvaEsquerda();    
void luzApagar();          

#endif
