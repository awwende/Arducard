/*
  Arducard - The Arduino business card
   with Worm 360
  
  Copyright (c) 2014, Alex Wende - awwende@gmail.com
  All rights reserved.
  
  This code has been modified from the work of Jim Lindblom of Sparkfun Electronics for the MPR121 touch sensing example code
  and pota92 on the arduino forum for their work on the Snake Game 360: http://forum.arduino.cc/index.php?topic=140773.0
  
  8/14-Rev1:
        1.Removed momentary switch code of UI and modified to work with capacitive touch sensing
        2.Worm 360 modified to use less SRAM and work on ATMEGA328 hardware
        3.Capacitive touch sensitivity turned down to reduce false readings (like touching traces on bottom copper layer)
        4.Added intro splash screen for first 3 seconds of boot
        5.Added high score recording to make the game that much more addicting
  9/14-Rev2:
        1.Fixed bug that slowed gameplay as worm grew.
        2.Increased capacitive sensor response time by replacing delay(110) with if(millis()%110)<10){}
        3.Removed inverted text in game over screen to increase battery life
  
  Redistribution and use in source and binary forms, with or without modification, 
  are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice, this list 
    of conditions and the following disclaimer.
    
  * Redistributions in binary form must reproduce the above copyright notice, this 
    list of conditions and the following disclaimer in the documentation and/or other 
    materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND 
  CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR 
  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT 
  NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, 
  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
  ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <U8glib.h>
#include "mpr121.h"
#include <Wire.h>
#include <EEPROM.h>
//Pin Config
U8GLIB_SSD1306_128X64 u8g(9, 10, 12); //D0=13, D1=11, RST=12, DC=10

//Key defs
#define KEY_NONE 0
#define KEY_LEFT 1
#define KEY_RIGHT 2
#define KEY_START 3

//Ui Defs
#define UI_INTRO 0
#define UI_WELCOME 1
#define UI_READY 2
#define UI_PLAY 3
#define UI_GAMEOVER 4


//Control variables
uint8_t uiState = UI_WELCOME;
uint8_t uiKeyCode = KEY_NONE;

//InGame variables
uint8_t startBlinkInterval = 0;
uint8_t readyInterval = 0;
int wormRadius = 4;
int wormMaxDelay = 0;
int wormDelay = 0;
int wormPieces;
int wormStartPieces = 6;
float wormDirection = 0.0;
int wormX[200];
int wormY[200];
int pelletX = 0;
int pelletY = 0;
boolean pelletVisible = true;
int pelletBlink = 0;
int pelletBlinkDelay = 2;
String scoreStr = "";
String highscoreStr = "";
char score[]="000000";
char highscore[]="000000";
int irqpin = 2;  // Digital 2
boolean touchStates[12]; //to keep track of the touch states
void draw(){
  int i;  
  switch(uiState) {
    case UI_INTRO:
      u8g.setFont(u8g_font_6x13B);
      u8g.drawStr(1, 15, "Personalized"); 
      u8g.drawStr(1, 30, "intro text"); 
      u8g.drawStr(1, 45, "goes here");
      u8g.drawStr(20, 60, "- your name");     
      break;
      
   case UI_WELCOME:
      u8g.setFont(u8g_font_fub11);
      u8g.drawStr(24, 20, "360 Snake");
      u8g.setFont(u8g_font_6x13);
      u8g.drawStr(10, 35, "Use left and right");
      u8g.drawStr(10, 45, "   keys to move"); 
      startBlinkInterval++;
      if (startBlinkInterval <= 40) {
         u8g.setFont(u8g_font_helvB08);
         u8g.drawStr(25, 60, "Press A to start!");
      } else if (startBlinkInterval >= 80) {
         startBlinkInterval = 0;
      }
      break;      
    case UI_READY:
      drawPieces();     
      readyInterval++;     
      u8g.setFont(u8g_font_gdr12);      
      if (readyInterval <= 40) {      
         u8g.drawStr(40, 40, "Ready");
      } else if (readyInterval <= 80 && readyInterval > 40) {
         u8g.drawStr(43, 40, "Set");        
      } else if (readyInterval <= 120 && readyInterval > 80) {
         u8g.drawStr(47, 40, "Go!");        
      } else if (readyInterval > 120) {
        readyInterval = 0;
        uiState = UI_PLAY;
      }
      break;
    case UI_PLAY:
      drawPieces();    
      break;    
    case UI_GAMEOVER:
      u8g.setFont(u8g_font_helvB08);      
      u8g.drawStr(35, 25, "Game over!");
      u8g.drawStr(40, 40, "Score -");
      u8g.drawStr(80, 40, score);
      u8g.drawStr(20, 55, " Highscore -");
      u8g.drawStr(81, 55, highscore);
      u8g.setColorIndex(1);      
      break;
  }  
}
void drawPieces(){
  int i, x, y;   
  for(i=0; i<wormPieces; i++) {
     x = round(wormX[i]);
     y = round(wormY[i]);
     u8g.drawCircle(x, y, 1);
     u8g.drawPixel(x, y);
  } 
  if (pelletVisible) {
     u8g.drawCircle(pelletX, pelletY, 2);
     u8g.drawPixel(pelletX, pelletY);
  }  
}

void controlStep(void) {
    uiKeyCode = KEY_NONE;
    if (touchStates[5]) {
      //Serial.println("L");
      uiKeyCode = KEY_LEFT;
    } else if (touchStates[7]) {
      uiKeyCode = KEY_RIGHT;
      //Serial.println("R");
    }
    if (touchStates[10]) {
      uiKeyCode = KEY_START; 
      //Serial.println("A"); 
      while(touchStates[10]){
        readTouchInputs();
      }
    }
}
//Set READY to uiState, prepare initial values.
void startNewGame() {
    int i, x, y;   
    uiState = UI_READY;
    wormPieces = wormStartPieces;
    wormDirection = 0.0;   
    x=30; y=20;
    for (i=wormPieces-1; i>=0; i--) {
      wormX[i] = x;
      wormY[i] = y;
      x-=wormRadius;
    }   
    resetPelletPosition();
}

//Collision test from two centres and a radius
boolean circularCollision(int x1, int y1, int x2, int y2, int r){ 
  int hyp = sqrt(pow(abs(x2-x1), 2) + pow(abs(y2-y1), 2));  
  if (hyp <= r) return true;  
  return false;
}
//This will pick random posible pellet positions until its far away from the worm and the corners
void resetPelletPosition(){
  boolean validPosition=false;
  int randomX, randomY, i;  
  do {
    randomX = random(4, 124);
    randomY = random(4, 60);   
    validPosition=true;   
    //Near worm
    for(i=0; i<wormPieces; i++) {
      if (circularCollision(wormX[i], wormY[i], randomX, randomY, 4)) {
        validPosition=false;
        break;
      }
    }    
    //Near corners
    if (circularCollision(0, 0, randomX, randomY, 10) ||
        circularCollision(0, 64, randomX, randomY, 10) ||
        circularCollision(128, 0, randomX, randomY, 10) ||
        circularCollision(128, 64, randomX, randomY, 10)) {
      validPosition=false;
    }   
  } while(!validPosition); 
  pelletX = randomX;
  pelletY = randomY;
}

void gameStep(void) {
  int i; 
  //Press start to play game...
  if (uiKeyCode == KEY_START && uiState == UI_WELCOME) {
    startNewGame();
  } 
  //Game over restart
  if (uiKeyCode == KEY_START && uiState == UI_GAMEOVER) {
    uiState = UI_WELCOME;
  }  
  //Gameplay
  if (uiState == UI_PLAY) {
     int getHighScore;
     wormDelay++;
     if (wormDelay >= wormMaxDelay) {    
       //Pellet Blink Animation
       pelletBlink++;
       if (pelletBlink >= pelletBlinkDelay) {
         pelletBlink=0;
         if (pelletVisible) {
           pelletVisible = false; 
         } else {
           pelletVisible = true; 
         }
       }   
       //Pellet Collision
       if (circularCollision(wormX[wormPieces-1], wormY[wormPieces-1], pelletX, pelletY, 4)) {
         wormPieces++;
         wormX[wormPieces-1] = wormX[wormPieces-2];
         wormY[wormPieces-1] = wormY[wormPieces-2];         
         resetPelletPosition();
       }      
       //Sides Collision (Game Over)
       if (wormX[wormPieces-1] > 128 ||
           wormX[wormPieces-1] < 0   ||
           wormY[wormPieces-1] > 64  ||
           wormY[wormPieces-1] < 0) {             
          scoreStr = String((wormPieces - wormStartPieces) * 10);
          scoreStr.toCharArray(score, 6);
          if((EEPROM.read(0)*10)<((wormPieces - wormStartPieces) * 10))EEPROM.write(0,(wormPieces - wormStartPieces));
          highscoreStr = String(EEPROM.read(0)*10);
          highscoreStr.toCharArray(highscore,6);
          uiState = UI_GAMEOVER;
       }      
       //Worm Collision (Game Over)
       for(i=0; i<wormPieces-wormStartPieces; i++) {
         if (circularCollision(wormX[wormPieces-1], wormY[wormPieces-1], wormX[i], wormY[i], 2)) {        
           scoreStr = String((wormPieces - wormStartPieces) * 10);
           scoreStr.toCharArray(score, 6); 
           if((EEPROM.read(0)*10)<((wormPieces - wormStartPieces) * 10))EEPROM.write(0,(wormPieces - wormStartPieces));
           highscoreStr = String(EEPROM.read(0)*10);
           highscoreStr.toCharArray(highscore,6);         
           uiState = UI_GAMEOVER;
           break;
         }
       }      
       //Worm Movement:
       for(i=0; i<wormPieces-1; i++) {
         wormX[i] = wormX[i+1];
         wormY[i] = wormY[i+1];         
       }
       wormX[wormPieces-1] += cos(wormDirection) * wormRadius;
       wormY[wormPieces-1] += sin(wormDirection) * wormRadius;           
       //Worm direction:
       switch (uiKeyCode) {
         case KEY_LEFT:
           wormDirection -= (PI/7);
           break;
         case KEY_RIGHT:
           wormDirection += (PI/7);        
           break;
       }      
       if (wormDirection > (2*PI)) {
         wormDirection = 0; 
       } else if (wormDirection < 0) {
         wormDirection = (2*PI);
       }
       //Reset delay
       wormDelay = 0;
     } 
  }
}


void setup() {
  if(EEPROM.read(0)==255)EEPROM.write(0,0); //Clears default memory to enable high score recording
  pinMode(irqpin, INPUT);
  digitalWrite(irqpin, HIGH); //enable pullup resistor
  Wire.begin();
  mpr121_setup();
  //Serial.begin(9600); //Serial log
  uiState=UI_INTRO;
}

void loop() { 
  readTouchInputs();
  if(millis()%110<10){  //update screen every 110ms. while checking touch sensor every 5ms.
    controlStep();
    u8g.firstPage();
    do  {
      draw();
    } while(u8g.nextPage());
    gameStep(); 
    if(uiState==UI_INTRO && millis()>3500)uiState=UI_WELCOME;
  }
  delay(5);
}

void readTouchInputs(){
  if(!checkInterrupt()){  
    //read the touch state from the MPR121
    Wire.requestFrom(0x5A,2); 
    byte LSB = Wire.read();
    byte MSB = Wire.read();   
    uint16_t touched = ((MSB << 8) | LSB); //16bits that make up the touch states  
    for (int i=0; i < 12; i++){  // Check what electrodes were pressed
      if(touched & (1<<i)){   
        touchStates[i] = 1;      
      }
      else{        
        touchStates[i] = 0;
      }
    } 
  }
}

void mpr121_setup(void){
  set_register(0x5A, ELE_CFG, 0x00); 
  // Section A - Controls filtering when data is > baseline.
  set_register(0x5A, MHD_R, 0x01);
  set_register(0x5A, NHD_R, 0x01);
  set_register(0x5A, NCL_R, 0x00);
  set_register(0x5A, FDL_R, 0x00);
  // Section B - Controls filtering when data is < baseline.
  set_register(0x5A, MHD_F, 0x01);
  set_register(0x5A, NHD_F, 0x01);
  set_register(0x5A, NCL_F, 0xFF);
  set_register(0x5A, FDL_F, 0x02); 
  // Section C - Sets touch and release thresholds for each electrode
  set_register(0x5A, ELE5_T, TOU_THRESH);
  set_register(0x5A, ELE5_R, REL_THRESH); 
  set_register(0x5A, ELE7_T, TOU_THRESH);
  set_register(0x5A, ELE7_R, REL_THRESH);
  set_register(0x5A, ELE10_T, TOU_THRESH);
  set_register(0x5A, ELE10_R, REL_THRESH);
  // Section D
  // Set the Filter Configuration
  // Set ESI2
  set_register(0x5A, FIL_CFG, 0x04); 
  // Section E
  // Electrode Configuration
  // Set ELE_CFG to 0x00 to return to standby mode
  set_register(0x5A, ELE_CFG, 0x0C);  // Enables all 12 Electrodes 
  // Section F
  // Enable Auto Config and auto Reconfig
  set_register(0x5A, ELE_CFG, 0x0C);
}

boolean checkInterrupt(void){
  return digitalRead(irqpin);
}

void set_register(int address, unsigned char r, unsigned char v){
    Wire.beginTransmission(address);
    Wire.write(r);
    Wire.write(v);
    Wire.endTransmission();
}
