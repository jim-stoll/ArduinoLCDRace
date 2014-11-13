// Only modify this file to include
// - function definitions (prototypes)
// - include files
// - extern variable definitions
// In the appropriate section

#ifndef _LCDRace1_H_
#define _LCDRace1_H_
#include "Arduino.h"
//add your includes for the project LCDRace1 here
#include "Streaming.h"
#include "LiquidCrystal_I2C.h"
#include "Wire.h"



//end of add your includes here
#ifdef __cplusplus
extern "C" {
#endif
void loop();
void setup();
#ifdef __cplusplus
} // extern "C"
#endif

//add your function definitions for the project LCDRace1 here

void adjustScore();
void buttonISR();
void checkForWin();
void clearPlayerMarker();
void debugLanes();
void initGame();
void initLanes();
void popLanes();
void printGameStatus();
void printLanes();
void printLap();
void printPlayerMarker();
void printScore();
void readPos();
void startNewLap();



//Do not add code below this line
#endif /* _LCDRace1_H_ */
