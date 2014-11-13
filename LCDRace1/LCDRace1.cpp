// Do not remove the include below
#include "LCDRace1.h"


#define I2C_ADDR      0x27 // I2C address of PCF8574A
#define BACKLIGHT_PIN 3
#define En_pin        2
#define Rw_pin        1
#define Rs_pin        0
#define D4_pin        4
#define D5_pin        5
#define D6_pin        6
#define D7_pin        7

LiquidCrystal_I2C lcd(I2C_ADDR,En_pin,Rw_pin,Rs_pin,D4_pin,D5_pin,D6_pin,D7_pin, BACKLIGHT_PIN, POSITIVE);
const int buttonPin = 2;
const int buttonInt = 0;
const int posXMin = 0;
const int posXMax = 3;
const int posYMin = 0;
const int posYMax = 18;
const int numLanes = 4;
const int numPos = 19;
const int maxPosNum = 18;
const int maxLanePos = 18;
const int minLanePos = 0;
const int maxLaneNum = 3;
const int minLaneNum = 0;
const int scoreScale = 1;
const int resultCol = 2;
const int resultRow = 0;
const int lapCol = 0;
const int lapRow = 3;
const int scoreCol = 0;
const int scoreRow = 0;
const unsigned long oncomingUpdateMillisBase = 1000;
unsigned long oncomingUpdateMillis = oncomingUpdateMillisBase;
unsigned long posChangeUpdateMillis = 50;

int posX, posY;
const byte aXPin = A1;
const byte aYPin = A0;
int aX, aY;
bool lanes[numLanes][numPos];
unsigned long lastOncomingMillis = 0;
unsigned long lastPosChangeMillis = 0;
bool gameOver = false;
int score = 0;
bool reset = true;
int numLaps = 4;
int lapNum = 1;

//these are potential configurable items, such as might be set/changed based on difficulty level, and/or via menu prefs (such as joystick threshold pct)
int lapClearPos = 3;
int speedChangeInc = 1;
int joystickThreshPct = 30;
int lapScoreBonus = 10;
int loopDelay = 200;
// end config params

int loThresh = 1023*joystickThreshPct/100.0;
int hiThresh = 1023*(1.0-joystickThreshPct/100.0);

byte finishLineCustomChar[8] = {
	0b00001,
	0b00001,
	0b00001,
	0b00001,
	0b00001,
	0b00001,
	0b00001,
	0b00001
};

byte finishLineOncomingCustomChar[8] = {
	0b00001,
	0b00001,
	0b11011,
	0b01111,
	0b11111,
	0b01111,
	0b11011,
	0b00001
};

byte playerCustomChar[8] = {
	0b00000,
	0b11011,
	0b11011,
	0b01110,
	0b11100,
	0b01110,
	0b11011,
	0b11011
};

byte oncomingCustomChar[8] = {
	0b00000,
	0b00000,
	0b11011,
	0b01111,
	0b11110,
	0b01111,
	0b11011,
	0b00000
};

byte wreckCustomChar[8] = {
	0b10100,
	0b01001,
	0b10101,
	0b00001,
	0b00001,
	0b10101,
	0b01001,
	0b10100
};

byte lap3CustomChar[8] = {
	0b00000,
	0b11000,
	0b11000,
	0b00000,
	0b00000,
	0b11011,
	0b11011,
	0b00000
};

byte lap4CustomChar[8] = {
	0b00000,
	0b11011,
	0b11011,
	0b00000,
	0b00000,
	0b11011,
	0b11011,
	0b00000
};

byte finishLineMarker = 0;
byte finishLineOncomingMarker = 1;
byte playerMarker = 2;
byte oncomingMarker = 3;
byte wreckMarker = 4;
byte lap3Marker = 5;
byte lap4Marker = 6;

void initLanes() {
  for (int laneNum = minLaneNum; laneNum < numLanes; laneNum++) {
    for (int posNum = minLanePos; posNum < numPos; posNum ++) {
      lanes[laneNum][posNum] = false;
    }
  }
}

void initGame() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Auto Race!!");
  lcd.setCursor(0,1);
  lcd.write((uint8_t)oncomingMarker);
  lcd.write((uint8_t)oncomingMarker);
  lcd.write((uint8_t)oncomingMarker);
  lcd.setCursor(8,1);
  lcd.write((uint8_t)playerMarker);
  lcd.write((uint8_t)playerMarker);
  lcd.write((uint8_t)playerMarker);

  posX = random(numLanes);
  posY = posYMin;
  score = 0;
  lapNum = 1;
  initLanes();
  reset = false;
  gameOver = false;
}

void buttonISR() {
	reset = true;
}

void setup() {
  Serial.begin(9600);

  pinMode(buttonPin, INPUT_PULLUP);
  attachInterrupt(buttonInt, buttonISR, FALLING);

  lcd.begin(20,4);
  lcd.createChar(finishLineMarker, finishLineCustomChar);
  lcd.createChar(finishLineOncomingMarker,finishLineOncomingCustomChar);
  lcd.createChar(playerMarker, playerCustomChar);
  lcd.createChar(oncomingMarker, oncomingCustomChar);
  lcd.createChar(wreckMarker, wreckCustomChar);
  lcd.createChar(lap3Marker, lap3CustomChar);
  lcd.createChar(lap4Marker, lap4CustomChar);
}

void clearPos() {
  lcd.setCursor((19-maxLanePos) + posYMax - posY, posXMax - posX);
  lcd.print(" ");
}

void printPos() {
  lcd.setCursor((19-maxLanePos) + posYMax - posY, posXMax - posX);
  lcd.write((uint8_t)playerMarker);
}

void readPos() {
  aX = analogRead(aXPin);
  aY = analogRead(aYPin);

  if (aX > hiThresh) {
    posX = posX + 1;
    if (posX > posXMax) {
      posX = posXMax;
    }
  } else if (aX < loThresh) {
    posX = posX - 1;
    if (posX < posXMin) {
      posX = posXMin;
    }
  }

  if (aY > hiThresh) {
    posY = posY + 1;
    if (posY > posYMax) {
      posY = posYMax;
    }
  } else if (aY < loThresh) {
    posY = posY - 1;
    if (posY < posYMin) {
      posY = posYMin;
    }
  }

//  Serial << "aX" << ": " << aX << " aY" << ": " << aY << " posX: " << posX << " posY: " << posY << endl;

}

void printLap() {
	lcd.setCursor(lapCol, lapRow);

	if (lapNum == 1) {
		lcd.print(".");
	} else if (lapNum == 2) {
		lcd.print(":");
	} else if (lapNum == 3) {
		lcd.write(lap3Marker);
	} else {
		lcd.write(lap4Marker);
	}

}

void printScore() {
	int huns = 0;
	int tens = 0;
	int ones = 0;
	byte printRow = scoreRow;

	if (score > 99) {
		lcd.setCursor(scoreCol, printRow);
		huns = score/100;
		lcd.print(huns);
		printRow = printRow + 1;
	}

	if (score - 100*huns > 9) {
		tens = (score - 100*huns)/10;
		lcd.setCursor(scoreCol, printRow);
		lcd.print(tens);
		printRow = printRow + 1;
	}

	lcd.setCursor(scoreCol, printRow);
	lcd.print(score - 100*huns - 10*tens);

}

void printLanes() {
  lcd.clear();
  printLap();
  printScore();
  for (int laneNum = minLaneNum; laneNum < numLanes; laneNum++) {
    for (int posNum = minLanePos; posNum < numPos; posNum++) {
      if (lanes[laneNum][posNum]) {
        lcd.setCursor((19-maxLanePos) + maxLanePos - posNum, maxLaneNum - laneNum);
        if (posNum < maxPosNum) {
        	lcd.write((uint8_t)oncomingMarker);
        } else {
        	lcd.write((uint8_t)finishLineOncomingMarker);
        }
      } else {
    	  if (posNum == maxPosNum) {
    	      lcd.setCursor((19-maxLanePos) + maxLanePos - posNum, maxLaneNum - laneNum);
    		  lcd.write((uint8_t)finishLineMarker);
    	  }
      }
    }
  }

}

void debugLanes() {
  for (int laneNum = minLaneNum; laneNum < numLanes; laneNum++) {
    Serial << "L" << laneNum << ": ";
    for (int posNum = minLanePos; posNum < numPos; posNum++) {
      if (lanes[laneNum][posNum]) {
        Serial << "1";
      } else {
        Serial << "0";
      }
    }
    Serial << endl;
  }

}

void popLanes() {
	int newLane = 0;
	int emptyLanes[numLanes];
	int emptyLaneCt = 0;
	bool foundEmptyLane = true;
	int emptyLaneIdx = 0;

	//check for empty lanes
	for (int laneNum = minLaneNum; laneNum < numLanes; laneNum++) {
		foundEmptyLane = true;
		Serial << "checking lane: " << laneNum << endl;
		for (int posNum = minLanePos; posNum < numPos; posNum++) {
			if (lanes[laneNum][posNum]) {
				foundEmptyLane = false;
				break;
			}
		}
		if (foundEmptyLane) {
			emptyLanes[emptyLaneIdx] = laneNum;
			emptyLaneCt = emptyLaneCt + 1;
			emptyLaneIdx = emptyLaneIdx + 1;
		}
	}

	//fill a random empty lane, if there are empty lanes
	if (emptyLaneCt > 0) {
		int emptyLaneIdx = random(emptyLaneCt);
		newLane = emptyLanes[emptyLaneIdx];
		Serial << "choosing empty lane: " << newLane << endl;
	//otherwise, just pick a random lane
	} else {
		newLane = random(numLanes);
	}

	for (int laneNum = minLaneNum; laneNum < numLanes; laneNum++) {
		for (int posNum = minLanePos; posNum < maxLanePos; posNum++) {
			lanes[laneNum][posNum] = false;
			if (lanes[laneNum][posNum+1]) {
				lanes[laneNum][posNum] = true;
			}
		}
		lanes[laneNum][maxLanePos] = false;
	}

	lanes[newLane][maxLanePos] = true;

}

//**TODO: consider moving result flashing to a separate function, as it could also be called from checkForWin (would need to be sure to re-print wreck marker, though...)
bool checkForCollision() {
  for (int laneNum = minLaneNum; laneNum < numLanes; laneNum++) {
    if (lanes[posX][posY]) {
		gameOver = true;
		lcd.setCursor(resultCol, resultRow);
		lcd.print("WRECK!! ");
		lcd.print(score);

		lcd.setCursor((19-maxLanePos) + maxLanePos - posY, maxLaneNum - posX);
		lcd.write((uint8_t)wreckMarker);

		delay(500);

		for (int t = 0; t < 3; t++) {
			if (reset) {
			  return true;
			}

			lcd.setCursor(resultCol, resultRow);
			lcd.print("       ");
			lcd.setCursor((19-maxLanePos) + maxLanePos - posY, maxLaneNum - posX);
			lcd.write((uint8_t)wreckMarker);
			delay(500);
			lcd.setCursor(resultCol, resultRow);
			lcd.print("WRECK!!");
			lcd.setCursor((19-maxLanePos) + maxLanePos - posY, maxLaneNum - posX);
			lcd.write((uint8_t)wreckMarker);

			delay(500);
		}

		gameOver = true;
		return true;
    }
  }

  return false;
}

void startNewLap() {
	lapNum = lapNum + 1;

	//clear out the bottom 2 lanes, to avoid immediate collision
	for (int laneNum = minLaneNum; laneNum < numLanes; laneNum++) {
		for (int posNum = minLanePos; posNum <= lapClearPos; posNum++) {
			lanes[laneNum][posNum] = false;
		}
	}

	score = score + lapNum * lapScoreBonus;
	//printLanes does printLap and printScore, in addition to printing lanes
	//**TODO: might consider if/what printLanes should be doing, vs breaking out a bit more cleanly
	printLanes();
	posY = minLanePos;
	printPos();
}

//**TODO: might want to split out the lap vs win functionality here
void checkForWin() {
  if (posY == maxLanePos-1) {
	  if (lapNum == numLaps) {
		lcd.setCursor(resultCol, resultRow);
		lcd.print("WIN!! ");
		lcd.print(score);
		delay(500);
		for (int t = 0; t < 3; t++) {
			if (reset) {
			  return;
			}
			lcd.setCursor(resultCol, resultRow);
			lcd.print("     ");
			delay(250);
			lcd.setCursor(resultCol, resultRow);
			lcd.print("WIN!!");
			delay(500);
		}

		gameOver = true;
	  } else {
		  startNewLap();
	  }
  }
}

void adjustOncomingSpeed() {
    oncomingUpdateMillis = oncomingUpdateMillisBase - (speedChangeInc * posY + speedChangeInc*(numPos * (lapNum-1)));
}

//**TODO: figure out how to handle score, so that can't game system by putzing up and down a lane, to double-count already-passed oncomings
//**TODO: may want to adjust how/when calling this, so that get credit for oncomings passed when going forward faster than oncomings are progressing
//        (presently, score is adjusted when oncomings are moved down the track, but the faster player can move past multiple oncomings in that time, thus only getting
//         credit for the ones immediately behind the player, when the oncomings are adjusted)
//**TODO: seem to be getting occasional discrepancy between score reported at top vs side
void adjustScore() {
  if (posY > 0) {
    for (int laneNum = minLaneNum; laneNum < numLanes; laneNum++) {
       if (lanes[laneNum][posY - 1]) {
         score = score + posY * scoreScale;
       }
    }
  }
//  Serial << "Points: " << points << endl;
}

void loop() {
  if (reset) {
    initGame();
    delay(2000);
  }

  if (!gameOver) {
    if (millis() - lastOncomingMillis > oncomingUpdateMillis) {
      lastOncomingMillis = millis();
      popLanes();
  //    debugLanes();
      printLanes();
      if (!checkForCollision()) {
        adjustScore();
        printScore();
      }
    }

    if (!checkForCollision()) {
    	if (millis() - lastPosChangeMillis > posChangeUpdateMillis) {
		  clearPos();
		  readPos();
		  printPos();
		  if (!checkForCollision()) {
			checkForWin();
		  }
		  adjustOncomingSpeed();
    	}
    }
  }
  delay(loopDelay);
}
