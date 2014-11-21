#include "LCDRace1.h"

//#define CLR_HIGH_SCORES

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
const unsigned long oncomingUpdateMillisBase = 1000; //could also be shortened for higher levels
const unsigned long posChangeUpdateMillis = 100;
const int lapBonusTimeBase = 8000;
const int lapBonusTimeInc = 2000;
const int numLaps = 4;

typedef struct lanePositionStruct {
	enum oncomingEnum {EMPTY, ONCOMING_CAR, FUEL} type;
	bool passedFlag;
} lanePositionType;

const lanePositionType NEW_EMPTY = {lanePositionType::EMPTY, false};
const lanePositionType NEW_ONCOMING_CAR = {lanePositionType::ONCOMING_CAR, false};
const lanePositionType NEW_FUEL = {lanePositionType::FUEL, false};

int posX, posY;
const byte aXPin = A0;
const byte aYPin = A1;
volatile int aX, aY;
lanePositionType lanes[numLanes][numPos];
int score = 0;
bool reset = true;
int lapNum;
unsigned long lapStartMillis;
bool buttonPressed = false;

const byte minPlayLevel = 1;
const byte maxPlayLevel = 4;
const byte laneSparsityThreshold = 2;

const float jsCenterX = 512;
const float jsCenterY = 512;
const float jsMoveBnd = 512;
const float jsMoveThresh = 320;//256;
const int jsMoveLoThresh = jsCenterX - jsMoveThresh;
const int jsMoveHiThresh = jsCenterX + jsMoveThresh;

byte playLevel = 1;

//these are potential configurable items, such as might be set/changed based on difficulty level, and/or via menu prefs (such as joystick threshold pct)
int lapClearPos = 3; //num rows to clear from bottom when starting new lap
int speedChangeInc = 10; //could  be adjusted for harder levels - controls speed diff as move up track, as well as in accumulated laps
unsigned long joystickReadMicros = 100000;
int lapScoreBonus = 10;
int fuelBonus = 10;
byte fuelMarkerPctChance = 5; //modified for level 1, so not const
// end config params

unsigned long oncomingUpdateMillis = oncomingUpdateMillisBase;

// these custom chars can't be consts, as the lcd.createCharacter method rejects them, if they're consts
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
//	0b00000,
//	0b11011,
//	0b11011,
//	0b01110,
//	0b11100,
//	0b01110,
//	0b11011,
//	0b11011
	0b11011,
	0b11011,
	0b01110,
	0b11110,
	0b11110,
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
//		0b00000,
//		0b01100,
//		0b01101,
//		0b11111,
//		0b11111,
//		0b01101,
//		0b01100,
//		0b00000

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

const uint8_t finishLineMarker = 0;
const uint8_t finishLineOncomingMarker = 1;
const uint8_t playerMarker = 2;
const uint8_t oncomingMarker = 3;
const uint8_t wreckMarker = 4;
const uint8_t lap3Marker = 5;
const uint8_t lap4Marker = 6;
const uint8_t fuelMarker = 7;
//NOTE - using this array based on gameState - the 'win' marker and the 'inplay' marker are the same marker, so using it in 2 places in this array
const uint8_t playerMarkers[] = {wreckMarker, playerMarker, playerMarker};


typedef struct gameStatusStruct {
	enum gameStatusEnum {WRECK, WIN, INPLAY} status;
	char *statusString;
	uint8_t playerMarker;
} gameStatusType;

const gameStatusType wreckStatus = {gameStatusType::WRECK, "WRECK!!", wreckMarker};
const gameStatusType winStatus = {gameStatusType::WIN, "WIN!!", playerMarker};
const gameStatusType inplayStatus = {gameStatusType::INPLAY, "       ", playerMarker};
gameStatusType gameStatus;

const int highScoreEEPROMBaseAddr = 0;

typedef struct highScoreStruct {
	int score;
	char initials[2];
} highScoreType;

byte highScoreByteSize = 4;
highScoreType highScores[maxPlayLevel];
char playerInitials[2] = {' ',' '};


void clearHighScores() {
	int highScore = 0;
	for (int lvl = 1; lvl <= maxPlayLevel; lvl++) {
		EEPROM.write(highScoreEEPROMBaseAddr + highScoreByteSize*lvl - 4, 0);
		EEPROM.write(highScoreEEPROMBaseAddr + highScoreByteSize*lvl - 3, 0);
		EEPROM.write(highScoreEEPROMBaseAddr + highScoreByteSize*lvl - 2, 32);
		EEPROM.write(highScoreEEPROMBaseAddr + highScoreByteSize*lvl - 1, 32);
		highScores[lvl-1].score = 0;
		highScores[lvl-1].initials[0] = ' ';
		highScores[lvl-1].initials[1] = ' ';
	}
}

void inputInitials() {
	//call getButtonPressed to clear flag, if its set
	getButtonPressed();
	char initials[2] = {'A', 'A'};
	int initialNum = 1;
	char *initialsPrompt = "Enter Initials: ";
	unsigned long lastMillis = 0;
	int joystickAutorepeatDelayMillis = 300;

	lcd.setCursor(0, 3);
	lcd.print(initialsPrompt);
	while (!buttonPressed) {
//		lcd.setCursor(12,0);
//		lcd.print(playLevel);
		lcd.setCursor(strlen(initialsPrompt) + 0, 3);
		lcd.print(initials[1]);
		lcd.setCursor(strlen(initialsPrompt) + 1, 3);
		lcd.print(initials[0]);

		if ((aX < jsMoveLoThresh || aX > jsMoveHiThresh) && (millis() - lastMillis > joystickAutorepeatDelayMillis)) {
			lastMillis = millis();

			if (aX > jsMoveHiThresh) {
				initials[initialNum]++;
				if (initials[initialNum] > 'Z') {
					initials[initialNum] = 'Z';
				}
			} else if (aX < jsMoveLoThresh) {
				initials[initialNum]--;
				if (initials[initialNum] < 'A') {
					initials[initialNum] = 'A';
				}
			}
			lcd.setCursor(strlen(initialsPrompt) + 1 - initialNum, 3);
			lcd.print(initials[initialNum]);
		} else if (aY < jsMoveLoThresh) {
			initialNum--;
			if (initialNum < 0) {
				initialNum = 0;
			}
			lcd.setCursor(strlen(initialsPrompt) + 1 - initialNum, 3);
		} else if (aY > jsMoveHiThresh) {
			initialNum++;
			if (initialNum > 1) {
				initialNum = 1;
			}
			lcd.setCursor(strlen(initialsPrompt) + 1 - initialNum, 3);
		}
	}
	playerInitials[0] = initials[0];
	playerInitials[1] = initials[1];
}

void saveHighScore() {
	byte msb = score/255;
	byte lsb = score - 255*msb;
	EEPROM.write(highScoreEEPROMBaseAddr + highScoreByteSize*playLevel - 4, msb);
	EEPROM.write(highScoreEEPROMBaseAddr + highScoreByteSize*playLevel - 3, lsb);
	EEPROM.write(highScoreEEPROMBaseAddr + highScoreByteSize*playLevel - 2, playerInitials[1]);
	EEPROM.write(highScoreEEPROMBaseAddr + highScoreByteSize*playLevel - 1, playerInitials[0]);
	highScores[playLevel-1].score = score;
	highScores[playLevel-1].initials[1] = playerInitials[1];
	highScores[playLevel-1].initials[0] = playerInitials[0];
}

void retrieveHighScores() {
	int highScore = 0;
	for (int lvl = 1; lvl <= maxPlayLevel; lvl++) {
		highScore = 0;
		highScore = 255*EEPROM.read(highScoreEEPROMBaseAddr + highScoreByteSize*lvl - 4);
		highScore += EEPROM.read(highScoreByteSize*lvl-3);
		highScores[lvl-1].score = highScore;
		highScores[lvl-1].initials[1] = EEPROM.read(highScoreByteSize*lvl - 2);
		highScores[lvl-1].initials[0] = EEPROM.read(highScoreByteSize*lvl - 1);
	}
}

void initLanes() {
  for (int laneNum = minLaneNum; laneNum < numLanes; laneNum++) {
    for (int posNum = minLanePos; posNum < numPos; posNum ++) {
      lanes[laneNum][posNum] = NEW_EMPTY;
    }
  }
}

bool checkHighScore() {
	if (score > highScores[playLevel-1].score) {
		highScores[playLevel-1].score = score;
//		saveHighScore();
		return true;
	}

	return false;
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
	lcd.print("Pick Level: ");
	lcd.setCursor(0, 2);
	lcd.print("High Score: ");
	lcd.print(highScores[playLevel-1].score);
	lcd.print(" ");
	lcd.print(highScores[playLevel-1].initials[1]);
	lcd.print(highScores[playLevel-1].initials[0]);

	static unsigned long lastMillis = 0;
	unsigned long joystickAutorepeatDelayMillis = 500;

	//call getButtonPressed to clear flag, if its set
	getButtonPressed();

	while (!buttonPressed) {
		lcd.setCursor(12,0);
		lcd.print(playLevel);
		if ((aY < jsMoveLoThresh || aY > jsMoveHiThresh || aX < jsMoveLoThresh || aX > jsMoveHiThresh) && (millis() - lastMillis > joystickAutorepeatDelayMillis)) {
			lastMillis = millis();

			if (aY > jsMoveHiThresh || aX > jsMoveHiThresh) {
				playLevel = playLevel + 1;
				if (playLevel > maxPlayLevel) {
				  playLevel = maxPlayLevel;
				}
			} else if (aY < jsMoveLoThresh || aX < jsMoveLoThresh) {
				playLevel = playLevel - 1;
				if (playLevel < minPlayLevel) {
				  playLevel = minPlayLevel;
				}
			}
			lcd.setCursor(12,2);
			lcd.print(highScores[playLevel-1].score);
			lcd.print("   ");
			lcd.setCursor(16,2);
			lcd.print(highScores[playLevel-1].initials[1]);
			lcd.print(highScores[playLevel-1].initials[0]);
		}


	}

	for (int x = 0; x < 3; x++) {
		lcd.setCursor(0,0);
		lcd.print("             ");
		delay(125);
		lcd.setCursor(0,0);
		lcd.print("Pick Level: ");
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

  if (playLevel == 1) {
	  fuelMarkerPctChance = .7*fuelMarkerPctChance;
  }

  for (int x = 0; x < 10; x++) {
	  popLanes(laneSparsityThreshold);
  }
  reset = false;
  gameStatus = inplayStatus;
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
}

void enableButtonInterrupt(bool enable) {
	if (enable) {
		attachInterrupt(buttonInt, buttonISR, FALLING);
	} else {
		detachInterrupt(buttonInt);
	}
}

void setup() {
  randomSeed(analogRead(2));
  random(2147483647);
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

#ifdef CLR_HIGH_SCORES
  clearHighScores();
#endif

  retrieveHighScores();

  showSplash();
}

void clearPlayerMarker() {
  lcd.setCursor((19-maxLanePos) + posYMax - posY, posXMax - posX);
  lcd.print(" ");
}

void printPlayerMarker() {
  lcd.setCursor((19-maxLanePos) + posYMax - posY, posXMax - posX);
  lcd.write(gameStatus.playerMarker);
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
	static float hs = jsMoveThresh*.4142;
	static bool released = true;

	//NOTE: adding extra vertex to base of each 4-vertext polygon, to standardize array size
	static float polyXPoints[8][5] = {
	/*0*/	{jsCenterX-hs, jsCenterX+hs, jsCenterX+hs, jsCenterX, jsCenterX-hs},
	/*1*/	{jsCenterX+hs, jsCenterX+jsMoveBnd, jsCenterX+jsMoveBnd, jsCenterX+jsMoveThresh, jsCenterX+hs},
	/*2*/	{jsCenterX+jsMoveThresh, jsCenterX+jsMoveBnd, jsCenterX+jsMoveBnd, jsCenterX+jsMoveThresh, jsCenterX+jsMoveThresh},
	/*3*/	{jsCenterX+jsMoveThresh, jsCenterX+jsMoveBnd, jsCenterX+jsMoveBnd, jsCenterX+hs, jsCenterX+hs},
	/*4*/	{jsCenterX-hs, jsCenterX, jsCenterX+hs, jsCenterX+hs, jsCenterX-hs},
	/*5*/	{jsCenterX-jsMoveBnd, jsCenterX-jsMoveThresh, jsCenterX-hs, jsCenterX-hs, jsCenterX-jsMoveBnd},
	/*6*/	{jsCenterX-jsMoveBnd, jsCenterX-jsMoveThresh, jsCenterX-jsMoveThresh, jsCenterX-jsMoveThresh, jsCenterX-jsMoveBnd},
	/*7*/	{jsCenterX-jsMoveBnd, jsCenterX-hs, jsCenterX-hs, jsCenterX-jsMoveThresh, jsCenterX-jsMoveBnd}
	};

	static float polyYPoints[8][5] = {
	/*0*/	{jsCenterY+jsMoveBnd, jsCenterY+jsMoveBnd, jsCenterY+jsMoveThresh, jsCenterY+jsMoveThresh, jsCenterY+jsMoveThresh},
	/*1*/	{jsCenterY+jsMoveBnd, jsCenterY+jsMoveBnd, jsCenterY+hs, jsCenterY+hs, jsCenterY+jsMoveThresh},
	/*2*/	{jsCenterY+hs, jsCenterY+hs, jsCenterY-hs, jsCenterY-hs, jsCenterY},
	/*3*/	{jsCenterY-hs, jsCenterY-hs, jsCenterY-jsMoveBnd, jsCenterY-jsMoveBnd, jsCenterY-jsMoveThresh},
	/*4*/	{jsCenterY-jsMoveThresh, jsCenterY-jsMoveThresh, jsCenterY-jsMoveThresh, jsCenterY-jsMoveBnd, jsCenterY-jsMoveBnd},
	/*5*/	{jsCenterY-hs, jsCenterY-hs, jsCenterY-jsMoveThresh, jsCenterY-jsMoveBnd, jsCenterY-jsMoveBnd},
	/*6*/	{jsCenterY+hs, jsCenterY+hs, jsCenterY, jsCenterY-hs, jsCenterY-hs},
	/*7*/	{jsCenterY+jsMoveBnd, jsCenterY+jsMoveBnd, jsCenterY+jsMoveThresh, jsCenterY+hs, jsCenterY+hs}

	};

	static float centerXPoints[8] = {jsCenterX-hs, jsCenterX+hs, jsCenterX+jsMoveThresh, jsCenterX+jsMoveThresh, jsCenterX+hs, jsCenterX-hs, jsCenterX-jsMoveThresh, jsCenterX-jsMoveThresh};
	static float centerYPoints[8] = {jsCenterY+jsMoveThresh, jsCenterY+jsMoveThresh, jsCenterY+hs, jsCenterY-hs, jsCenterY-jsMoveThresh, jsCenterY-jsMoveThresh, jsCenterY-hs, jsCenterY+hs};

	if (playLevel == 1) {
		joystickAutorepeatDelayMillis = 300;
	}

	//skip interrupts, so don't twiddle w/ values as we're working w/ them
	noInterrupts();

	if (pnpoly(8, centerXPoints, centerYPoints, aX, aY) == 0) {
		if ((pnpoly(5, polyXPoints[0], polyYPoints[0], aX, aY) == 1) && (released || (millis() - lastMillis > joystickAutorepeatDelayMillis))) {
			posY++;
		}
		else if ((pnpoly(5, polyXPoints[1], polyYPoints[1], aX, aY) == 1) && (released || (millis() - lastMillis > joystickAutorepeatDelayMillis))) {
			posX++;
			posY++;
		}
		else if ((pnpoly(5, polyXPoints[2], polyYPoints[2], aX, aY) == 1) && (released || (millis() - lastMillis > joystickAutorepeatDelayMillis))) {
			posX++;
		}
		else if ((pnpoly(5, polyXPoints[3], polyYPoints[3], aX, aY) == 1) && (released || (millis() - lastMillis > joystickAutorepeatDelayMillis))) {
			posX++;
			posY--;
		}
		else if ((pnpoly(5, polyXPoints[4], polyYPoints[4], aX, aY) == 1) && (released || (millis() - lastMillis > joystickAutorepeatDelayMillis))) {
			posY--;
		}
		else if ((pnpoly(5, polyXPoints[5], polyYPoints[5], aX, aY) == 1) && (released || (millis() - lastMillis > joystickAutorepeatDelayMillis))) {
			posX--;
			posY--;
		}
		else if ((pnpoly(5, polyXPoints[6], polyYPoints[6], aX, aY) == 1) && (released || (millis() - lastMillis > joystickAutorepeatDelayMillis))) {
			posX--;
		}
		else if ((pnpoly(5, polyXPoints[7], polyYPoints[7], aX, aY) == 1) && (released || (millis() - lastMillis > joystickAutorepeatDelayMillis))) {
			posX--;
			posY++;
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
		released = true;
	}

	//back to accepting interrupts
	interrupts();

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
		if (lanes[laneNum][posNum].type == lanePositionType::ONCOMING_CAR) {
			if (posNum < maxPosNum) {
				lcd.write(oncomingMarker);
			} else {
				lcd.write(finishLineOncomingMarker);
			}
		} else if (lanes[laneNum][posNum].type == lanePositionType::FUEL) {
			if (posNum < maxPosNum) {
				lcd.write(fuelMarker);
			}
		} else if (lanes[laneNum][posNum].type == lanePositionType::EMPTY) {
			if (posNum == maxPosNum) {
				lcd.write(finishLineMarker);
			}
		}
    }
  }

}

bool checkAdjacents(int row) {
	for (int p = 0; p < numLanes - 1; p++) {
		if (lanes[p][row].type == lanePositionType::ONCOMING_CAR && lanes[p+1][row].type == lanePositionType::ONCOMING_CAR) {
			return true;
		}
	}

	return false;
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
	typedef struct laneSparsityStruct {
		int laneNum;
		int sparseCt;
	} laneSparcityType;
	laneSparcityType laneSparsities[numLanes];
	laneSparcityType maxSparseLanes[numLanes];

	//check for sparsity in current content of lanes, producing array of sparcity counts (num positions from top, in which there are no oncomings)
	for (int laneNum = minLaneNum; laneNum < numLanes; laneNum++) {
		laneSparsity = 0;
		for (int posNum = maxLanePos; posNum >= minLanePos; posNum--) {
			if (lanes[laneNum][posNum].type == lanePositionType::ONCOMING_CAR) {
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

	if (playLevel > 1 || random(10) > 3) {
		lanes[newLane][maxLanePos] = NEW_ONCOMING_CAR;
	}

	if (playLevel > 2 && random(10) > 8-playLevel) {
			int x = random(numLanes);
			while (x == newLane || (checkAdjacents(maxLanePos-1) && (x == newLane-1 || x == newLane+1))) {

				x = random(numLanes);
			}
			newLane = x;
			lanes[newLane][maxLanePos] = NEW_ONCOMING_CAR;

	}

	if (random(100) <= fuelMarkerPctChance) {
		int emptyLanes[numLanes];
		int emptyLaneIdx = 0;
		for (int x = 0; x < numLanes; x++) {
			if (lanes[x][maxLanePos].type == lanePositionType::EMPTY) {
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

	if (gameStatus.status == gameStatusType::WRECK || gameStatus.status == gameStatusType::WIN) {

		//print status
		lcd.setCursor(resultCol, resultRow);
		lcd.print(gameStatus.statusString);//gameStatusStrings[gameStatus]);

		bool isHighScore = checkHighScore();
		if (isHighScore) {
			lcd.setCursor(resultCol, resultRow + 2);
			lcd.print("NEW HIGH SCORE!!!");
		}

		//in case the status covers the final player position, re-print final position and appropriate marker
		printPlayerMarker();

		delay(500);

		//celebration/shame-abration - flash the status and player position
		for (int t = 0; t < 3; t++) {
			//allow breaking out of status display
			if (reset) {
			  break;
			}
			//clear the status and player marker
			lcd.setCursor(resultCol, resultRow);
			lcd.print(gameStatus.statusString);//gameStatusStrings[INPLAY]);
			lcd.setCursor(resultCol, resultRow + 2);
			if (isHighScore) {
				lcd.setCursor(resultCol, resultRow + 2);
				lcd.print("NEW HIGH SCORE!!!");
			}
			lcd.setCursor((19-maxLanePos) + maxLanePos - posY, maxLaneNum - posX);
			clearPlayerMarker();

			delay(250);

			//print status and last player marker
			lcd.setCursor(resultCol, resultRow);
			lcd.print(gameStatus.statusString);//gameStatusStrings[gameStatus]);
			if (isHighScore) {
				lcd.setCursor(resultCol, resultRow + 2);
				lcd.print("NEW HIGH SCORE!!!");
			}
			lcd.setCursor((19-maxLanePos) + maxLanePos - posY, maxLaneNum - posX);
			printPlayerMarker();

			delay(250);
		}

		//print status and last player marker
		lcd.setCursor(resultCol, resultRow);
		lcd.print(gameStatus.statusString);//gameStatusStrings[gameStatus]);
		if (isHighScore) {
			lcd.setCursor(resultCol, resultRow + 2);
			lcd.print("NEW HIGH SCORE!!!");
		}
		lcd.setCursor((19-maxLanePos) + maxLanePos - posY, maxLaneNum - posX);
		printPlayerMarker();

		if (isHighScore) {
			inputInitials();
			saveHighScore();
		}
	}
}

int checkForOncoming() {
  for (int laneNum = minLaneNum; laneNum < numLanes; laneNum++) {
    if (lanes[posX][posY].type == lanePositionType::ONCOMING_CAR) {
    	gameStatus = wreckStatus;
		adjustScore();
		printScore();

		printGameStatus();

		return lanePositionType::ONCOMING_CAR;
    } else if (lanes[posX][posY].type == lanePositionType::FUEL) {
    	//if got fuel, make it disappear
    	lanes[posX][posY] = NEW_EMPTY;
    	score = score + lapNum * scoreScale * fuelBonus + playLevel;

//    	showBonus("FUEL", lapNum * scoreScale * fuelBonus);

    	return lanePositionType::FUEL;
    }
  }

  return lanePositionType::EMPTY;
}

//**TODO: might want to split out the lap vs win functionality here
void checkForLap() {
  if (posY == maxLanePos-1) {
	  if (lapNum == numLaps) {
		gameStatus = winStatus;
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

	unsigned long lapBonusTargetMillis = lapBonusTimeBase + lapNum * lapBonusTimeInc;
	int lapBonus = 0;

	if (lapStartMillis > 0 && (currMillis - lapStartMillis) < lapBonusTargetMillis) {
		//award bounus point for each .1 sec faster than target time
		lapBonus = (lapBonusTargetMillis - (currMillis - lapStartMillis))/100;
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
	aX = jsCenterX;
	aY = jsCenterY;
	delay(250);
	lapStartMillis = millis();
}

void adjustOncomingSpeed() {
    oncomingUpdateMillis = oncomingUpdateMillisBase - (speedChangeInc * posY + speedChangeInc*(numPos * (lapNum-1)));

    if (playLevel == 1 && lapNum > 1) {
    	oncomingUpdateMillis = 1.5*oncomingUpdateMillis;
    } else if (playLevel == 2 || playLevel == 3) {
    	oncomingUpdateMillis = oncomingUpdateMillis - 20;
    } else {
    	oncomingUpdateMillis = oncomingUpdateMillis - playLevel * 10;
    }
}

void adjustScore() {
  if (posY > minLanePos) {
    for (int laneNum = minLaneNum; laneNum < numLanes; laneNum++) {
       if (lanes[laneNum][posY - 1].type == lanePositionType::ONCOMING_CAR && lanes[laneNum][posY - 1].passedFlag == false) {
    	   lanes[laneNum][posY - 1].passedFlag = true;
    	   score = score + lapNum * scoreScale + (playLevel - 1);
       }
    }
  }
}

void loop() {
	static unsigned long lastOncomingMillis = 0;
	static unsigned long lastPosChangeMillis = 0;


	if (reset) {
		initGame();
		delay(splashDelayMillis);
	} else {

		if (gameStatus.status == gameStatusType::INPLAY) {
			if (millis() - lastOncomingMillis > oncomingUpdateMillis) {
			  lastOncomingMillis = millis();
			  popLanes(laneSparsityThreshold);
			//    debugLanes();
			  printLanes();
			  adjustScore();
//Serial << freeMemory() << endl;
			}

			if (checkForOncoming() != lanePositionType::ONCOMING_CAR) {
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
