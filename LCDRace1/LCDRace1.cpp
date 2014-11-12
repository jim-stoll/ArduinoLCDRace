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

byte finishLine[8] = {
	0b00001,
	0b00001,
	0b00001,
	0b00001,
	0b00001,
	0b00001,
	0b00001,
	0b00001
};

byte finishLineOncoming[8] = {
	0b10001,
	0b01001,
	0b00101,
	0b00011,
	0b00101,
	0b01001,
	0b10001,
	0b00001
};

void initLanes() {
  for (int laneNum = 0; laneNum < numLanes; laneNum++) {
    for (int posNum = 0; posNum < numPos; posNum ++) {
      lanes[laneNum][posNum] = false;
    }
  }
}

void initGame() {
  lcd.clear();
  posX = random(numLanes);
  posY = posYMin;
  points = 0;
  initLanes();
  gameOver = false;
}

void setup() {
  Serial.begin(9600);
  pinMode(0, INPUT_PULLUP);
  aXPin = A1;
  aYPin = A0;
  lcd.begin(20,4);
  lcd.createChar(0, finishLine);
  lcd.createChar(1,finishLineOncoming);
  initGame();
}

void clearPos() {
  lcd.setCursor(posYMax - posY, posXMax - posX);
  lcd.print(" ");
}

void printPos() {
//  lcd.clear();
  lcd.setCursor(posYMax - posY, posXMax - posX);
  lcd.print("<");
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
        	lcd.print(">");
        } else {
        	lcd.write((uint8_t)1);
        }
      } else {
    	  if (posNum == maxPosNum) {
    	      lcd.setCursor(maxLanePos - posNum, maxLaneNum - laneNum);
    		  lcd.write((uint8_t)0);
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
      lcd.print("X");

      delay(500);
      for (int t = 0; t < 3; t++) {
      	lcd.setCursor(0,0);
      	lcd.print("       ");
        lcd.setCursor(maxLanePos - posY, maxLaneNum - posX);
        lcd.print("X");
     	delay(500);
      	lcd.setCursor(0,0);
      	lcd.print("WRECK!!");
        lcd.setCursor(maxLanePos - posY, maxLaneNum - posX);
        lcd.print("X");
      	delay(500);
      }
      gameOver = true;
      return true;
    }// else {
//      return false;
//    }
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
  if (!digitalRead(0)) {
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
