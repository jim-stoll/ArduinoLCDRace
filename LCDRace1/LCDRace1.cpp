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
const int loThresh = 255;
const int hiThresh = 768;
const int posXMin = 0;
const int posXMax = 3;
const int posYMin = 0;
const int posYMax = 19;
const int numLanes = 4;
const int numPos = 20;
const int maxPosNum = 19;
const int maxLanePos = 19;
const int minLanePos = 0;
const int maxLaneNum = 3;
const int minLaneNum = 0;
const int speedChangeInc = 20;
const int pointsScale = 1;
const unsigned long oncomingUpdateMillisBase = 1000;
unsigned long oncomingUpdateMillis = oncomingUpdateMillisBase;

int posX, posY;
byte aXPin, aYPin;
int aX, aY;
bool lanes[numLanes][numPos];
unsigned long lastOncomingMillis = 0;
bool gameOver = false;
int points = 0;
bool reset = true;

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

byte finishLineMarker = 0;
byte finishLineOncomingMarker = 1;
byte playerMarker = 2;
byte oncomingMarker = 3;
byte wreckMarker = 4;

void initLanes() {
  for (int laneNum = 0; laneNum < numLanes; laneNum++) {
    for (int posNum = 0; posNum < numPos; posNum ++) {
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
  points = 0;
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
  aXPin = A1;
  aYPin = A0;
  lcd.begin(20,4);
  lcd.createChar(finishLineMarker, finishLineCustomChar);
  lcd.createChar(finishLineOncomingMarker,finishLineOncomingCustomChar);
  lcd.createChar(playerMarker, playerCustomChar);
  lcd.createChar(oncomingMarker, oncomingCustomChar);
  lcd.createChar(wreckMarker, wreckCustomChar);
}

void clearPos() {
  lcd.setCursor(posYMax - posY, posXMax - posX);
  lcd.print(" ");
}

void printPos() {
  lcd.setCursor(posYMax - posY, posXMax - posX);
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

void printLanes() {
  lcd.clear();
  for (int laneNum = 0; laneNum < numLanes; laneNum++) {
    for (int posNum = 0; posNum < numPos; posNum++) {
      if (lanes[laneNum][posNum]) {
        lcd.setCursor(maxLanePos - posNum, maxLaneNum - laneNum);
        if (posNum < maxPosNum) {
        	lcd.write((uint8_t)oncomingMarker);
        } else {
        	lcd.write((uint8_t)finishLineOncomingMarker);
        }
      } else {
    	  if (posNum == maxPosNum) {
    	      lcd.setCursor(maxLanePos - posNum, maxLaneNum - laneNum);
    		  lcd.write((uint8_t)finishLineMarker);
    	  }
      }
    }
  }

}

void debugLanes() {
  for (int laneNum = 0; laneNum < numLanes; laneNum++) {
    Serial << "L" << laneNum << ": ";
    for (int posNum = 0; posNum < numPos; posNum++) {
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
  int newLane = random(4);
  Serial << "newLane: " << newLane << endl;

  for (int laneNum = 0; laneNum < numLanes; laneNum++) {
    for (int posNum = 0; posNum < maxLanePos; posNum++) {
      lanes[laneNum][posNum] = false;
      if (lanes[laneNum][posNum+1]) {
        lanes[laneNum][posNum] = true;
      }
    }
    lanes[laneNum][maxLanePos] = false;
  }

  lanes[newLane][maxLanePos] = true;

}

bool checkForCollision() {
  for (int laneNum = 0; laneNum < numLanes; laneNum++) {
    if (lanes[posX][posY]) {
		gameOver = true;
		lcd.setCursor(0,0);
		lcd.print("WRECK!! ");
		lcd.print(points);

		lcd.setCursor(maxLanePos - posY, maxLaneNum - posX);
		lcd.write((uint8_t)wreckMarker);

		delay(500);

		for (int t = 0; t < 3; t++) {
			if (reset) {
			  return true;
			}
			lcd.setCursor(0,0);
			lcd.print("       ");
			lcd.setCursor(maxLanePos - posY, maxLaneNum - posX);
			lcd.write((uint8_t)wreckMarker);
			delay(500);
			lcd.setCursor(0,0);
			lcd.print("WRECK!!");
			lcd.setCursor(maxLanePos - posY, maxLaneNum - posX);
			lcd.write((uint8_t)wreckMarker);

			delay(500);
		}

		gameOver = true;
		return true;
    }
  }

  return false;
}

void checkForWin() {
  if (posY == maxLanePos-1) {
    lcd.setCursor(0,0);
    lcd.print("WIN!! ");
    lcd.print(points);
    delay(500);
    for (int t = 0; t < 3; t++) {
		if (reset) {
		  return;
		}
    	lcd.setCursor(0,0);
    	lcd.print("     ");
    	delay(250);
    	lcd.setCursor(0,0);
    	lcd.print("WIN!!");
    	delay(500);
    }

    gameOver = true;
  }
}

void adjustOncomingSpeed() {
    oncomingUpdateMillis = oncomingUpdateMillisBase - speedChangeInc * posY;
}

void adjustPoints() {
  if (posY > 0) {
    for (int laneNum = 0; laneNum < numLanes; laneNum++) {
       if (lanes[laneNum][posY - 1]) {
         points = points + posY * pointsScale;
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
        adjustPoints();
      }
    }

    if (!checkForCollision()) {
      clearPos();
      readPos();
      printPos();
      if (!checkForCollision()) {
        checkForWin();
      }
      adjustOncomingSpeed();
    }
  }
  delay(200);
}
