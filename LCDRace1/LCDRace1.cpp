#include "LCDRace1.h"

//**TODO: don't repaint player marker, if hasn't moved from last position?

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
const int resultCol = 3;
const int resultRow = 0;
const int lapCol = 0;
const int lapRow = 3;
const int scoreCol = 0;
const int scoreRow = 0;
const unsigned long switchDebounceMillis = 200;
const unsigned long splashDelayMillis = 1000;
unsigned long lastOncomingMillis = 0;
unsigned long lastPosChangeMillis = 0;
unsigned long posChangeUpdateMillis = 100;

enum GameStatus {WRECK, WIN, INPLAY};
const char *gameStatusStrings[] = {"WRECK!!", "WIN!!", "       "};
GameStatus gameStatus;

int posX, posY;
const byte aXPin = A1;
const byte aYPin = A0;
volatile int aX, aY;
byte lanes[numLanes][numPos];
int score = 0;
bool reset = true;
int numLaps = 4;
int lapNum;
unsigned long lapStartMillis;
int lapBonusTimeBase = 5000;
int lapBonusTimeInc = 1000;

volatile bool xReleased;
volatile bool yReleased;

//these are potential configurable items, such as might be set/changed based on difficulty level, and/or via menu prefs (such as joystick threshold pct)
int lapClearPos = 3;
int speedChangeInc = 10; //could  be adjusted for harder levels - controls speed diff as move up track, as well as in accumulated laps
const unsigned long oncomingUpdateMillisBase = 1000; //could also be shortened for higher levels
int joystickThreshPct = 25; //could be pref for responsiveness of joystick
int lapScoreBonus = 10;
const int joystickXAutorepeatDelayMillis = 200; //could be pref for responsiveness of car
const int joystickYAutorepeatDelayMillis = 200;
// end config params

unsigned long oncomingUpdateMillis = oncomingUpdateMillisBase;

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

uint8_t finishLineMarker = 0;
uint8_t finishLineOncomingMarker = 1;
uint8_t playerMarker = 2;
uint8_t oncomingMarker = 3;
uint8_t wreckMarker = 4;
uint8_t lap3Marker = 5;
uint8_t lap4Marker = 6;
//NOTE - using this array based on gameState - the 'win' marker and the 'inplay' marker are the same marker, so using it in 2 places in this array
uint8_t playerMarkers[] = {wreckMarker, playerMarker, playerMarker};

void initLanes() {
  for (int laneNum = minLaneNum; laneNum < numLanes; laneNum++) {
    for (int posNum = minLanePos; posNum < numPos; posNum ++) {
      lanes[laneNum][posNum] = 0;
    }
  }
}

void initGame() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("LCD Race!!");
  lcd.setCursor(0,1);
  lcd.write(oncomingMarker);
  lcd.write(oncomingMarker);
  lcd.write(oncomingMarker);
  lcd.setCursor(8,1);
  lcd.write(playerMarker);
  lcd.write(playerMarker);
  lcd.write(playerMarker);

  posX = random(numLanes);
  posY = posYMin;
  score = 0;
  lapNum = 0;
  initLanes();
  reset = false;
  gameStatus = INPLAY;
  startNewLap();
}

void buttonISR() {
	static unsigned long lastMillis = 0;

	if (millis() - lastMillis > switchDebounceMillis) {
		lastMillis = millis();
		reset = true;
	}
}
//take joystick readings on a regular basis (vis Timer1 interrupt), to ensure getting timely input
void readJoystick() {
	//read from joystick
	aX = analogRead(aXPin);
	aY = analogRead(aYPin);

	//track whether joystick has been released back to neutral, for autorepeat purposes
	if (aX > loThresh && aX < hiThresh) {
		xReleased = true;
	} else {
		xReleased = false;
	}
	if (aY > loThresh && aY < hiThresh) {
		yReleased = true;
	} else {
		yReleased = false;
	}
}

void setup() {
  Serial.begin(9600);
  //first analogRead on a pin can take longer than normal, so do a quick read on each now
  analogRead(aXPin);
  analogRead(aYPin);

  Timer1.initialize(50000);
  Timer1.attachInterrupt(readJoystick);

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

void clearPlayerMarker() {
  lcd.setCursor((19-maxLanePos) + posYMax - posY, posXMax - posX);
  lcd.print(" ");
}

void printPlayerMarker() {
  lcd.setCursor((19-maxLanePos) + posYMax - posY, posXMax - posX);
  lcd.write(playerMarkers[gameStatus]);
}

void adjustPos() {
	//time counters for autorepeat delay (to avoid overcontrolling on autorepeat)
	static unsigned long lastXMillis = 0;
	static unsigned long lastYMillis = 0;

	//skip interrupts, so don't twiddle w/ values as we're working w/ them
	noInterrupts();

	//if this is an input that started from the neutral position, react immediately
	// if this is an autorepeat (ie, stick hasn't been released back to neutral since last pos check), then delay the autorepeat to avoid overcontrol
	if ((aX < loThresh || aX > hiThresh) && (xReleased || (millis() - lastXMillis > joystickXAutorepeatDelayMillis))) {
		lastXMillis = millis();

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
	}

	if ((aY < loThresh || aY > hiThresh) && (yReleased || (millis() - lastYMillis > joystickYAutorepeatDelayMillis))) {
		lastYMillis = millis();
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
	}

	//back to accepting interrupts
	interrupts();

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

void getDigits(int num, byte *digits) {
	digits[0] = 0;
	digits[1] = 0;
	digits[2] = 0;

	if (num == 0 || num > 999) {
		return;
	}

	digits[2] = num/100;
	digits[1] = (num - 100*digits[2])/10;
	digits[0] = num - 100*digits[2] - 10*digits[1];
//Serial << "digits: " << digits[2] << digits[1] << digits[0] << endl;
}

void printScore() {
	byte printRow = scoreRow;

	byte digits[3];
	getDigits(score, digits);

	lcd.setCursor(scoreCol, printRow);

	if (digits[2] > 0) {
		lcd.print(digits[2]);
		printRow = printRow + 1;
	}

	if (digits[1] > 0 || digits[2] > 0) {
		lcd.setCursor(scoreCol, printRow);
		lcd.print(digits[1]);
		printRow = printRow + 1;
	}

	lcd.setCursor(scoreCol, printRow);
	lcd.print(digits[0]);

}

void printLanes() {
  lcd.clear();
  printLap();
  printScore();
  for (int laneNum = minLaneNum; laneNum < numLanes; laneNum++) {
    for (int posNum = minLanePos; posNum < numPos; posNum++) {
      if (lanes[laneNum][posNum] > 0) {
        lcd.setCursor((19-maxLanePos) + maxLanePos - posNum, maxLaneNum - laneNum);
        if (posNum < maxPosNum) {
        	lcd.write(oncomingMarker);
        } else {
        	lcd.write(finishLineOncomingMarker);
        }
      } else {
    	  if (posNum == maxPosNum) {
    	      lcd.setCursor((19-maxLanePos) + maxLanePos - posNum, maxLaneNum - laneNum);
    		  lcd.write(finishLineMarker);
    	  }
      }
    }
  }

}

void debugLanes() {
  for (int laneNum = minLaneNum; laneNum < numLanes; laneNum++) {
    Serial << "L" << laneNum << ": ";
    for (int posNum = minLanePos; posNum < numPos; posNum++) {
      if (lanes[laneNum][posNum] > 0) {
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
		for (int posNum = minLanePos; posNum < numPos; posNum++) {
			if (lanes[laneNum][posNum] > 0) {
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
	//otherwise, just pick a random lane
	} else {
		newLane = random(numLanes);
	}

	for (int laneNum = minLaneNum; laneNum < numLanes; laneNum++) {
		for (int posNum = minLanePos; posNum < maxLanePos; posNum++) {
			lanes[laneNum][posNum] = 0;
			if (lanes[laneNum][posNum+1] > 0) {
				lanes[laneNum][posNum] = lanes[laneNum][posNum+1];
			}
		}
		lanes[laneNum][maxLanePos] = 0;
	}

	lanes[newLane][maxLanePos] = 1;

}

void printGameStatus() {

	if (gameStatus == WRECK || gameStatus == WIN) {

		//print status
		lcd.setCursor(resultCol, resultRow);
		lcd.print(gameStatusStrings[gameStatus]);

		//in case the status covers the final player position, re-print final position and appropriate marker
		printPlayerMarker();

		delay(500);

		//celebration/shame-abration - flash the status and player position
		for (int t = 0; t < 3; t++) {
			//allow breaking out of status display
			if (reset) {
			  return;
			}
			//clear the status and player marker
			lcd.setCursor(resultCol, resultRow);
			lcd.print(gameStatusStrings[INPLAY]);
			lcd.setCursor((19-maxLanePos) + maxLanePos - posY, maxLaneNum - posX);
			clearPlayerMarker();

			delay(250);

			//print status and last player marker
			lcd.setCursor(resultCol, resultRow);
			lcd.print(gameStatusStrings[gameStatus]);
			lcd.setCursor((19-maxLanePos) + maxLanePos - posY, maxLaneNum - posX);
			printPlayerMarker();

			delay(250);
		}
	}
}

bool checkForCollision() {
  for (int laneNum = minLaneNum; laneNum < numLanes; laneNum++) {
    if (lanes[posX][posY] > 0) {
    	gameStatus = WRECK;

		printGameStatus();

		return true;
    }
  }

  return false;
}

//**TODO: might want to split out the lap vs win functionality here
void checkForLap() {
  if (posY == maxLanePos-1) {
	  if (lapNum == numLaps) {
		gameStatus = WIN;
		printGameStatus();
	  } else {
		  startNewLap();
	  }
  }
}

void startNewLap() {
	score = score + lapNum * lapScoreBonus;

	if (lapNum == 0) {
		lapStartMillis = 0;
	}

	unsigned long currMillis = millis();
//	Serial << "laptime: " << currMillis - lapStartMillis << endl;

	unsigned long lapBonusTargetMillis = lapBonusTimeBase + lapNum * lapBonusTimeInc;
	int lapBonus = 0;

	if (lapStartMillis > 0 && (currMillis - lapStartMillis) < lapBonusTargetMillis) {
		//award bounus point for each .1 sec faster than target time
		lapBonus = (lapBonusTargetMillis - (currMillis - lapStartMillis))/100;
//		Serial << "time bonus: " <<  lapBonus << endl;
		score = score + lapBonus;
		lcd.setCursor(0,0);
		lcd.print("+");
		lcd.print(lapBonus);
		lcd.print(" SPEED!");
		delay(125);
		lcd.setCursor(0,0);
		lcd.print("           ");
		delay(125);
		lcd.setCursor(0,0);
		lcd.print("+");
		lcd.print(lapBonus);
		lcd.print(" SPEED!");

		delay(500);

	}

	lapNum = lapNum + 1;

	//clear pass flags, so new lap will give points for all cars that are passed
	//clear bottom rows to avoid iimediate collision at start of next lap
	for (int laneNum = minLaneNum; laneNum < numLanes; laneNum++) {
		for (int posNum = minLanePos; posNum <= maxLanePos; posNum++) {
			if (lanes[laneNum][posNum] > 0) {
				lanes[laneNum][posNum] = 1;
			}

			if (posNum <= lapClearPos) {
				lanes[laneNum][posNum] = 0;
			}
		}
	}

	//printLanes does printLap and printScore, in addition to printing lanes
	//**TODO: might consider if/what printLanes should be doing, vs breaking out a bit more cleanly
	printLanes();
	posY = minLanePos;
	printPlayerMarker();
	delay(250);
	lapStartMillis = millis();
}

void adjustOncomingSpeed() {
    oncomingUpdateMillis = oncomingUpdateMillisBase - (speedChangeInc * posY + speedChangeInc*(numPos * (lapNum-1)));
}

void adjustScore() {
  if (posY > minLanePos) {
    for (int laneNum = minLaneNum; laneNum < numLanes; laneNum++) {
       if (lanes[laneNum][posY - 1] == 1) {
    	   lanes[laneNum][posY - 1] = 2;
//         score = score + posY * scoreScale;
    	   score = score + lapNum * scoreScale;
       }
    }
  }
//  Serial << "Points: " << points << endl;
}

void loop() {

	if (reset) {
		initGame();
		delay(splashDelayMillis);
	} else {

		if (gameStatus == INPLAY) {
			if (millis() - lastOncomingMillis > oncomingUpdateMillis) {
			  lastOncomingMillis = millis();
			  popLanes();
			//    debugLanes();
			  printLanes();
			  adjustScore();
			}

			if (!checkForCollision()) {
				if (millis() - lastPosChangeMillis > posChangeUpdateMillis) {
					lastPosChangeMillis = millis();
					clearPlayerMarker();
					adjustPos();
					printPlayerMarker();

				  adjustOncomingSpeed();
				}

				adjustScore();
				printScore();
				checkForLap();
			}

		}
	}
}
