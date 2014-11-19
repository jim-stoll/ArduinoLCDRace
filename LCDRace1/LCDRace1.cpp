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
const byte oncomingEmpty = 0;
const byte oncomingCar = 1;
const byte oncomingFuel = 2;
const unsigned long switchDebounceMillis = 200;
const unsigned long splashDelayMillis = 1000;
unsigned long lastOncomingMillis = 0;
unsigned long lastPosChangeMillis = 0;
unsigned long posChangeUpdateMillis = 100;

enum GameStatus {WRECK, WIN, INPLAY};
const char *gameStatusStrings[] = {"CRASH!!", "WIN!!", "       "};
GameStatus gameStatus;

enum OncomingType {EMPTY, ONCOMING_CAR, FUEL};

struct lanePositionStruct {
	OncomingType type;
	bool passedFlag;
};

const struct lanePositionStruct NEW_EMPTY = {EMPTY, false};
const struct lanePositionStruct NEW_ONCOMING_CAR = {ONCOMING_CAR, false};
const struct lanePositionStruct NEW_FUEL = {FUEL, false};

int posX, posY;
const byte aXPin = A0;
const byte aYPin = A1;
volatile int aX, aY;
lanePositionStruct lanes[numLanes][numPos];
int score = 0;
bool reset = true;
int numLaps = 4;
int lapNum;
unsigned long lapStartMillis;
int lapBonusTimeBase = 8000;
int lapBonusTimeInc = 2000;
bool buttonPressed = false;

//volatile bool xReleased;
//volatile bool yReleased;

//these are potential configurable items, such as might be set/changed based on difficulty level, and/or via menu prefs (such as joystick threshold pct)
int lapClearPos = 3;
int speedChangeInc = 10; //could  be adjusted for harder levels - controls speed diff as move up track, as well as in accumulated laps
const unsigned long oncomingUpdateMillisBase = 1000; //could also be shortened for higher levels
int joystickThreshPct = 25; //could be pref for responsiveness of joystick
unsigned long joystickReadMicros = 100000;
int lapScoreBonus = 10;
const int joystickXAutorepeatDelayMillis = 200; //could be pref for responsiveness of car
const int joystickYAutorepeatDelayMillis = 200;
byte playLevel = 1;
byte minPlayLevel = 1;
byte maxPlayLevel = 2;
byte laneSparsityThreshold = 2;
const byte fuelMarkerPctChance = 5;
int fuelBonus = 10;
// end config params

unsigned long oncomingUpdateMillis = oncomingUpdateMillisBase;

int loThresh = 511 - 512*(joystickThreshPct/100.0);//1023*joystickThreshPct/100.0;
int hiThresh = 511 + 512*(joystickThreshPct/100.0);//1023*(1.0-joystickThreshPct/100.0);

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
//	0b10100,
//	0b01001,
//	0b10101,
//	0b00001,
//	0b00001,
//	0b10101,
//	0b01001,
//	0b10100
		0b00001,
		0b01001,
		0b11011,
		0b11111,
		0b11111,
		0b11011,
		0b01001,
		0b00001
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

byte fuelCustomChar[8] = {
	0b00000,
	0b00000,
	0b10000,
	0b10100,
	0b10100,
	0b11111,
	0b00000,
	0b00000
};

uint8_t finishLineMarker = 0;
uint8_t finishLineOncomingMarker = 1;
uint8_t playerMarker = 2;
uint8_t oncomingMarker = 3;
uint8_t wreckMarker = 4;
uint8_t lap3Marker = 5;
uint8_t lap4Marker = 6;
uint8_t fuelMarker = 7;
//NOTE - using this array based on gameState - the 'win' marker and the 'inplay' marker are the same marker, so using it in 2 places in this array
uint8_t playerMarkers[] = {wreckMarker, playerMarker, playerMarker};

void initLanes() {
  for (int laneNum = minLaneNum; laneNum < numLanes; laneNum++) {
    for (int posNum = minLanePos; posNum < numPos; posNum ++) {
      lanes[laneNum][posNum] = NEW_EMPTY;
    }
  }
}

void showSplash() {
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
	  delay(splashDelayMillis);
}

void selectLevel() {
	lcd.clear();
	lcd.setCursor(0,0);
	lcd.print("Level: ");

	static unsigned long lastXMillis = 0;
	static unsigned long lastYMillis = 0;

	//call getButtonPressed to clear flag, if its set
	getButtonPressed();

	while (!buttonPressed) {
		lcd.setCursor(7,0);
		lcd.print(playLevel);
		if (((aY < loThresh || aY > hiThresh) && ((millis() - lastYMillis > joystickYAutorepeatDelayMillis))) || (aX < loThresh || aX > hiThresh) && ((millis() - lastXMillis > joystickXAutorepeatDelayMillis))) {
			lastXMillis = millis();
			lastYMillis = millis();

			if (aY > hiThresh || aX > hiThresh) {
				playLevel = playLevel + 1;
				if (playLevel > maxPlayLevel) {
				  playLevel = maxPlayLevel;
				}
			} else if (aY < loThresh || aX < loThresh) {
				playLevel = playLevel - 1;
				if (playLevel < minPlayLevel) {
				  playLevel = minPlayLevel;
				}
			}
		}

	}

	for (int x = 0; x < 3; x++) {
		lcd.setCursor(0,0);
		lcd.print("        ");
		delay(125);
		lcd.setCursor(0,0);
		lcd.print("Level: ");
		lcd.print(playLevel);
		delay(125);
	}

	delay(125);

}

void initGame() {
  posX = random(numLanes);
  posY = posYMin;
  score = 0;
  lapNum = 0;
  initLanes();
  selectLevel();
  for (int x = 0; x < 10; x++) {
	  popLanes(laneSparsityThreshold);
  }
  reset = false;
  gameStatus = INPLAY;
  startNewLap();
}

bool getButtonPressed() {
	if (buttonPressed) {
		buttonPressed = false;
	}

	return buttonPressed;
}

void buttonISR() {
	static unsigned long lastMillis = 0;

	if (millis() - lastMillis > switchDebounceMillis) {
		lastMillis = millis();
		reset = true;
		buttonPressed = true;
	}
}
//take joystick readings on a regular basis (vis Timer1 interrupt), to ensure getting timely input
void readJoystick() {
	//read from joystick
	aX = analogRead(aXPin);
	aY = 1023 - analogRead(aYPin);

//	//track whether joystick has been released back to neutral, for autorepeat purposes
//	if (aX > loThresh && aX < hiThresh) {
//		xReleased = true;
//	} else {
//		xReleased = false;
//	}
//	if (aY > loThresh && aY < hiThresh) {
//		yReleased = true;
//	} else {
//		yReleased = false;
//	}
}

void enableButtonInterrupt(bool enable) {
	if (enable) {
		attachInterrupt(buttonInt, buttonISR, FALLING);
	} else {
		detachInterrupt(buttonInt);
	}
}

void setup() {
  Serial.begin(115200);
  //first analogRead on a pin can take longer than normal, so do a quick read on each now
  analogRead(aXPin);
  analogRead(aYPin);

  Timer1.initialize(joystickReadMicros);
  Timer1.attachInterrupt(readJoystick);

  pinMode(buttonPin, INPUT_PULLUP);
  //attachInterrupt(buttonInt, buttonISR, FALLING);
  enableButtonInterrupt(true);

  lcd.begin(20,4);
  lcd.createChar(finishLineMarker, finishLineCustomChar);
  lcd.createChar(finishLineOncomingMarker,finishLineOncomingCustomChar);
  lcd.createChar(playerMarker, playerCustomChar);
  lcd.createChar(oncomingMarker, oncomingCustomChar);
  lcd.createChar(wreckMarker, wreckCustomChar);
  lcd.createChar(lap3Marker, lap3CustomChar);
  lcd.createChar(lap4Marker, lap4CustomChar);
  lcd.createChar(fuelMarker, fuelCustomChar);

  showSplash();
}

void clearPlayerMarker() {
  lcd.setCursor((19-maxLanePos) + posYMax - posY, posXMax - posX);
  lcd.print(" ");
}

void printPlayerMarker() {
  lcd.setCursor((19-maxLanePos) + posYMax - posY, posXMax - posX);
  lcd.write(playerMarkers[gameStatus]);
}

int pnpoly(int nvert, float *vertx, float *verty, float testx, float testy) {
  int i, j, c = 0;
  for (i = 0, j = nvert-1; i < nvert; j = i++) {
    if ( ((verty[i]>testy) != (verty[j]>testy)) &&
     (testx < (vertx[j]-vertx[i]) * (testy-verty[i]) / (verty[j]-verty[i]) + vertx[i]) )
       c = !c;
  }
  return c;
}

void adjustPos() {
	static unsigned long lastMillis = 0;
unsigned long joystickAutorepeatDelayMillis = 200;
int q = 0;
	static float rBnd = 512;
	static float r = 256;
	static float hs = r*.4142;
	static float centerX = 512;
	static float centerY = 512;
	static bool released = true;

	//NOTE: adding extra vertex to base of each 4-vertext polygon, to standardize array size
	static float polyXPoints[8][5] = {
	/*0*/	{centerX-hs, centerX+hs, centerX+hs, centerX, centerX-hs},
	/*1*/	{centerX+hs, centerX+rBnd, centerX+rBnd, centerX+r, centerX+hs},
	/*2*/	{centerX+r, centerX+rBnd, centerX+rBnd, centerX+r, centerX+r},
	/*3*/	{centerX+r, centerX+rBnd, centerX+rBnd, centerX+hs, centerX+hs},
	/*4*/	{centerX-hs, centerX, centerX+hs, centerX+hs, centerX-hs},
	/*5*/	{centerX-rBnd, centerX-r, centerX-hs, centerX-hs, centerX-rBnd},
	/*6*/	{centerX-rBnd, centerX-r, centerX-r, centerX-r, centerX-rBnd},
	/*7*/	{centerX-rBnd, centerX-hs, centerX-hs, centerX-r, centerX-rBnd}
	};

	static float polyYPoints[8][5] = {
	/*0*/	{centerY+rBnd, centerY+rBnd, centerY+r, centerY+r, centerY+r},
	/*1*/	{centerY+rBnd, centerY+rBnd, centerY+hs, centerY+hs, centerY+r},
	/*2*/	{centerY+hs, centerY+hs, centerY-hs, centerY-hs, centerY},
	/*3*/	{centerY-hs, centerY-hs, centerY-rBnd, centerY-rBnd, centerY-r},
	/*4*/	{centerY-r, centerY-r, centerY-r, centerY-rBnd, centerY-rBnd},
	/*5*/	{centerY-hs, centerY-hs, centerY-r, centerY-rBnd, centerY-rBnd},
	/*6*/	{centerY+hs, centerY+hs, centerY, centerY-hs, centerY-hs},
	/*7*/	{centerY+rBnd, centerY+rBnd, centerY+r, centerY+hs, centerY+hs}

	};

//	for (int p = 0; p < 8; p++) {
//		Serial << p << endl;
//		for (int c = 0; c < 5; c++) {
//			Serial << polyXPoints[p][c] << "," << polyYPoints[p][c] << endl;
//		}
//		Serial << endl;
//	}

	static float centerXPoints[8] = {centerX-hs, centerX+hs, centerX+r, centerX+r, centerX+hs, centerX-hs, centerX-r, centerX-r};
	static float centerYPoints[8] = {centerY+r, centerY+r, centerY+hs, centerY-hs, centerY-r, centerY-r, centerY-hs, centerY+hs};

	//add an extra point in the base of each 4-vertex poly, to up to 5 points for sake of consistency of arrays
Serial << aX << ":" << aY << ":" << pnpoly(8, centerXPoints, centerYPoints, aX, aY) << endl;

//skip interrupts, so don't twiddle w/ values as we're working w/ them
	noInterrupts();

	if (pnpoly(8, centerXPoints, centerYPoints, aX, aY) == 0) {
		if ((pnpoly(5, polyXPoints[0], polyYPoints[0], aX, aY) == 1) && (released || (millis() - lastMillis > joystickAutorepeatDelayMillis))) {
			posY++;
			q=0;
		}
		else if ((pnpoly(5, polyXPoints[1], polyYPoints[1], aX, aY) == 1) && (released || (millis() - lastMillis > joystickAutorepeatDelayMillis))) {
			posX++;
			posY++;
			q=1;
		}
		else if ((pnpoly(5, polyXPoints[2], polyYPoints[2], aX, aY) == 1) && (released || (millis() - lastMillis > joystickAutorepeatDelayMillis))) {
			posX++;
			q=2;
		}
		else if ((pnpoly(5, polyXPoints[3], polyYPoints[3], aX, aY) == 1) && (released || (millis() - lastMillis > joystickAutorepeatDelayMillis))) {
			posX++;
			posY--;
			q=3;
		}
		else if ((pnpoly(5, polyXPoints[4], polyYPoints[4], aX, aY) == 1) && (released || (millis() - lastMillis > joystickAutorepeatDelayMillis))) {
			posY--;
			q=4;
		}
		else if ((pnpoly(5, polyXPoints[5], polyYPoints[5], aX, aY) == 1) && (released || (millis() - lastMillis > joystickAutorepeatDelayMillis))) {
			posX--;
			posY--;
			q=5;
		}
		else if ((pnpoly(5, polyXPoints[6], polyYPoints[6], aX, aY) == 1) && (released || (millis() - lastMillis > joystickAutorepeatDelayMillis))) {
			posX--;
			q=6;
		}
		else if ((pnpoly(5, polyXPoints[7], polyYPoints[7], aX, aY) == 1) && (released || (millis() - lastMillis > joystickAutorepeatDelayMillis))) {
			posX--;
			posY++;
			q=7;
		} else {
			q=-1;
		}
		if (posX > posXMax) {
			posX = posXMax;
		} else if (posX < posXMin) {
			posX = posXMin;
		}
		if (posY > posYMax) {
			posY = posYMax;
		} else if (posY < posYMin) {
			posY = posYMin;
		}
		if (millis() - lastMillis > joystickAutorepeatDelayMillis) {
			lastMillis = millis();
		}
		released = false;
	} else {
		q = 0;
		released = true;
	}

Serial << q << endl;

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
        lcd.setCursor((19-maxLanePos) + maxLanePos - posNum, maxLaneNum - laneNum);
		if (lanes[laneNum][posNum].type == ONCOMING_CAR) {
			if (posNum < maxPosNum) {
				lcd.write(oncomingMarker);
			} else {
				lcd.write(finishLineOncomingMarker);
			}
		} else if (lanes[laneNum][posNum].type == FUEL) {
			if (posNum < maxPosNum) {
				lcd.write(fuelMarker);
			}
		} else if (lanes[laneNum][posNum].type == EMPTY) {
			if (posNum == maxPosNum) {
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
    	Serial << lanes[laneNum][posNum].type;
    }
    Serial << endl;
  }

}

void popLanes(byte sparseThreshold) {
	int newLane = 0;
	int laneSparsity = 0;
	int sparseLaneCt = 0;
	int sparseLaneIdx = 0;
	int maxSparsity = 0;
	int maxSparseLaneCt = 0;
	int maxSparseLanesIdx = 0;
	bool maxSparseLaneChosen = false;
	struct laneSparsityStruct {
		int laneNum;
		int sparseCt;
	};
	struct laneSparsityStruct laneSparsities[numLanes];
	struct laneSparsityStruct maxSparseLanes[numLanes];

	//check for sparsity in current content of lanes, producing array of sparcity counts (num positions from top, in which there are no oncomings)
	for (int laneNum = minLaneNum; laneNum < numLanes; laneNum++) {
		laneSparsity = 0;
		for (int posNum = maxLanePos; posNum >= minLanePos; posNum--) {
			if (lanes[laneNum][posNum].type == ONCOMING_CAR) {
				break;
			} else {
				laneSparsity++;
			}
		}
		//if blank spots from top exceeds the threshold, note the lane
		if (laneSparsity >= sparseThreshold) {
			laneSparsities[sparseLaneIdx].laneNum = laneNum;
			laneSparsities[sparseLaneIdx].sparseCt = laneSparsity;

			sparseLaneIdx++;
			sparseLaneCt++;

			//track which lane(s) have the max empty spots
			if (laneSparsity >= maxSparsity) {
				maxSparsity = laneSparsity;
				if (maxSparseLaneCt > 0) {
					for (int x = 0; x < maxSparseLaneCt; x++) {
						if (maxSparseLanes[x].sparseCt < maxSparsity) {
							//shift the array contents, to remove the element that is no longer the lane w/ the max
							for (int y = x; y < numLanes-1; y++) {
								maxSparseLanes[y] = maxSparseLanes[y+1];
							}
							maxSparseLanes[numLanes-1].laneNum = 0; //last element will have zero shifted in
							maxSparseLanes[numLanes-1].sparseCt = 0;
						}
					}
					maxSparseLaneCt--;
					maxSparseLanesIdx--;
				}

				maxSparseLanes[maxSparseLanesIdx].laneNum = laneNum;
				maxSparseLanes[maxSparseLanesIdx].sparseCt = maxSparsity;
				maxSparseLaneCt++;
				maxSparseLanesIdx++;

			}
		}
	}

	//advance the current contents of the lanes
	for (int laneNum = minLaneNum; laneNum < numLanes; laneNum++) {
		for (int posNum = minLanePos; posNum < maxLanePos; posNum++) {
			lanes[laneNum][posNum] = lanes[laneNum][posNum+1];
		}
		lanes[laneNum][maxLanePos] = NEW_EMPTY;
	}

	//if sparse lanes found
	if (sparseLaneCt > 0) {
		//let there be a chance to pick a non-sparse lane, to prevent patterns from occurring
		//double the odds of choosing a sparse lane, though
		int adjRand = random(2*sparseLaneCt + 1);

		//allow a chance for a non-sparse lane to be chosen (1/2*sparseLaneCt)
		//if didn't get a sparse lane, just choose randomly
		if (adjRand == 2*sparseLaneCt) {
			newLane = random(numLanes);
		//if got a sparse lane, double the chances of getting a maxSparse lane, if not one of those
		} else {
			//check to see if newLane is one of the maxSparseLanes
			for (int x = 0; x < maxSparseLaneCt; x++) {
				if (maxSparseLanes[x].laneNum == laneSparsities[adjRand/2].laneNum) {
					maxSparseLaneChosen = true;
					break;
				}
			}
			//if a max sparse lane not chosen, give it another chance
			if (!maxSparseLaneChosen) {
				adjRand = random(sparseLaneCt*2);
			}
			newLane = laneSparsities[adjRand/2].laneNum;

		}

	//otherwise, just pick a random lane
	} else {
		newLane = random(numLanes);
	}

//	lanes[newLane][maxLanePos] = NEW_ONCOMING_CAR;

	if (playLevel > 1 && random(10) > 7-playLevel) {
			int x = random(numLanes);
			while (x == newLane) {
				x = random(numLanes);
			}
			newLane = x;
//			lanes[newLane][maxLanePos] = NEW_ONCOMING_CAR;
	}

	if (random(100) <= fuelMarkerPctChance) {
		int emptyLanes[numLanes];
		int emptyLaneIdx = 0;
		for (int x = 0; x < numLanes; x++) {
			if (lanes[x][maxLanePos].type == EMPTY) {
				emptyLanes[emptyLaneIdx] = x;
				emptyLaneIdx++;
			}
		}
		if (emptyLaneIdx > 0) {
			int fuelLane = emptyLanes[random(emptyLaneIdx)];
			lanes[fuelLane][maxLanePos] = NEW_FUEL;
		}
	}
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

int checkForOncoming() {
  for (int laneNum = minLaneNum; laneNum < numLanes; laneNum++) {
    if (lanes[posX][posY].type == ONCOMING_CAR) {
    	gameStatus = WRECK;

		printGameStatus();

		return ONCOMING_CAR;
    } else if (lanes[posX][posY].type == FUEL) {
    	//if got fuel, make it disappear
    	lanes[posX][posY] = NEW_EMPTY;
    	score = score + lapNum * scoreScale * fuelBonus;

//    	showBonus("FUEL", lapNum * scoreScale * fuelBonus);

    	return FUEL;
    }
  }

  return EMPTY;
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

void showBonus(char message[], int points) {
	enableButtonInterrupt(false);
	for (int flashX = 0; flashX < 2; flashX++) {
		lcd.setCursor(0,0);
		lcd.print("+");
		lcd.print(points);
		lcd.print(" ");
		lcd.print(message);
		lcd.print("!");
		delay(125);
		lcd.setCursor(0,0);
		lcd.print("           ");
		delay(125);
	}
	enableButtonInterrupt(true);
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

		showBonus("SPEED", lapBonus);

		delay(500);

	}

	lapNum = lapNum + 1;

	//clear pass flags, so new lap will give points for all cars that are passed on this lap
	//clear bottom rows to avoid immediate collision at start of next lap
	for (int laneNum = minLaneNum; laneNum < numLanes; laneNum++) {
		for (int posNum = minLanePos; posNum <= maxLanePos; posNum++) {
			lanes[laneNum][posNum].passedFlag = false;

			if (posNum <= lapClearPos) {
				lanes[laneNum][posNum] = NEW_EMPTY;
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
       if (lanes[laneNum][posY - 1].type == ONCOMING_CAR && lanes[laneNum][posY - 1].passedFlag == false) {
    	   lanes[laneNum][posY - 1].passedFlag = true;
    	   score = score + lapNum * scoreScale;
       }
    }
  }
}

void loop() {

	if (reset) {
		initGame();
		delay(splashDelayMillis);
//		selectLevel();
	} else {

		if (gameStatus == INPLAY) {
			if (millis() - lastOncomingMillis > oncomingUpdateMillis) {
			  lastOncomingMillis = millis();
			  popLanes(laneSparsityThreshold);
			//    debugLanes();
			  printLanes();
			  adjustScore();
			}

			if (checkForOncoming() != ONCOMING_CAR) {
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
