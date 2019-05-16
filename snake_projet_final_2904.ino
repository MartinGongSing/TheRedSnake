//We have mainly based our code on this source :  https://github.com/ondt/arduino-snake/
//We have added the lcd screen displaying real time score and a small introduction message

#include "LedControl.h"
#include<LiquidCrystal.h>
LiquidCrystal lcd(7,6,5,4,3,2);

// Pins are definded
struct Pin {
  static const short joystickX = A2;   // joystick X axis pin
  static const short joystickY = A3;   // joystick Y axis pin
  static const short joystickVCC = 15; // virtual VCC for the joystick (Analog 1) (to make the joystick connectable right next to the arduino)
  static const short joystickGND = 14; // virtual GND for the joystick (Analog 0) (to make the joystick connectable right next to the arduino)

  static const short potentiometer = A7; // potentiometer for snake speed control // does not apply here

  static const short CLK = 10;   // clock for LED matrix
  static const short CS  = 11;  // chip-select for LED matrix
  static const short DIN = 12; // data-in for LED matrix
};

// LED matrix brightness: between 0(darkest) and 15(brightest)
const short intensity = 8;

// lower = faster message scrolling
const short messageSpeed = 5;

// initial snake length (1->63)
const short initialSnakeLength = 3;


void setup() {
  Serial.begin(9600);  // set the same baud rate on the Serial Monitor
  initialize();         // initialize pins & LED matrix
  calibrateJoystick(); // calibrate the joystick home
  showSnakeMessage(); // scrolls the 'snake' message around the matrix

// Intro LCD screen steup
  lcd.begin(16,2);                  //Initialize LCD screen
  lcd.setCursor(0,0);               // Where to begin printing
  lcd.print("  Snake game    ");    //
  lcd.setCursor(0,1);               //
  lcd.print("Yannick & Martin");    
  delay(2000);                      // time the message is displayed (2 seconds)
  lcd.setCursor(0,1);
  
  
}


void loop() {
  generateFood();    // if there is no food, generate one
  scanJoystick();    // watches joystick movements & blinks with food
  calculateSnake();  // calculates snake parameters
  handleGameStates();
}





// --------------------------------------------------------------- //
// -------------------- supporting variables --------------------- //
// --------------------------------------------------------------- //

LedControl matrix(Pin::DIN, Pin::CLK, Pin::CS, 1);

struct Point {
  int row = 0, col = 0;
  Point(int row = 0, int col = 0): row(row), col(col) {}
};

struct Coordinate {
  int x = 0, y = 0;
  Coordinate(int x = 0, int y = 0): x(x), y(y) {}
};

bool win = false;
bool gameOver = false;

// primary snake head coordinates (snake head), it will be randomly generated
Point snake;

// food is not anywhere yet
Point food(-1, -1);

// construct with default values in case the user turns off the calibration
Coordinate joystickHome(500, 500);

// snake parameters
int snakeLength = initialSnakeLength;
int snakeSpeed = 1; // snakespeed by default
int snakeDirection = 0; // if it is 0, the snake does not move

// direction constants
const short up     = 1;
const short right  = 2;
const short down   = 3;
const short left   = 4;

// threshold where movement of the joystick will be accepted
const int joystickThreshold = 160;



// the age array: holds an 'age' of the every pixel in the matrix. If age > 0, it glows.
// on every frame, the age of all pixels is incremented.
// when the age of some pixel exceeds the length of the snake, it goes out.
// age 1 is added in the current snake direction next to the last position of the snake head.
int age[8][8] = {};




// --------------------------------------------------------------- //
// -------------------------- functions -------------------------- //
// --------------------------------------------------------------- //


// if there is no food, generate one, also check for victory
void generateFood() {
  if (food.row == -1 || food.col == -1) {
    // self-explanatory
    if (snakeLength >= 64) {
      win = true;
      return; // prevent the food generator from running, in this case it would run forever, because it will not be able to find a pixel without a snake
    }

    // generate food until it is in the right position
    do {
      food.col = random(8);
      food.row = random(8);
    } while (age[food.row][food.col] > 0);
  }
}


// watches joystick movements & blinks with food
void scanJoystick() {
  int previousDirection = snakeDirection; // save the last direction
  long timestamp = millis() + snakeSpeed; // when the next frame will be rendered

  while (millis() < timestamp) {
    // calculate snake speed logarithmically (10...1000ms)
    float raw = mapf(analogRead(Pin::potentiometer), 0, 1023, 0, 1);
    snakeSpeed = 442;                     // default speed
    if (snakeSpeed == 0) snakeSpeed = 1; // safety: speed can not be 0

    // determine the direction of the snake
    analogRead(Pin::joystickY) < joystickHome.y - joystickThreshold ? snakeDirection = up    : 0;
    analogRead(Pin::joystickY) > joystickHome.y + joystickThreshold ? snakeDirection = down  : 0;
    analogRead(Pin::joystickX) < joystickHome.x - joystickThreshold ? snakeDirection = left  : 0;
    analogRead(Pin::joystickX) > joystickHome.x + joystickThreshold ? snakeDirection = right : 0;

    // ignore directional change by 180 degrees (no effect for non-moving snake)
    snakeDirection + 2 == previousDirection && previousDirection != 0 ? snakeDirection = previousDirection : 0;
    snakeDirection - 2 == previousDirection && previousDirection != 0 ? snakeDirection = previousDirection : 0;

    // intelligently blink with the food
    matrix.setLed(0, food.row, food.col, millis() % 100 < 50 ? 1 : 0);
  }
}


// calculate snake movement data
void calculateSnake() {
  switch (snakeDirection) {
  case up:
    snake.row--; //snake goes up, erases one led at the time on the row
    fixEdge();
    matrix.setLed(0, snake.row, snake.col, 1); //Led switches on
    break;

  case right:
    snake.col++; //snake goes down, adds one led at the time on the column
    fixEdge();
    matrix.setLed(0, snake.row, snake.col, 1); //Led switches on
    break;

  case down:
    snake.row++; //snake goes down, adds one led at the time on the row
    fixEdge();
    matrix.setLed(0, snake.row, snake.col, 1); //Led switches on
    break;

  case left:
    snake.col--; //snake goes left, erase one led at the time on the column
    fixEdge();
    matrix.setLed(0, snake.row, snake.col, 1); //Led switches on
    break;

  default: // if the snake is not moving, exit
    return;
  }

  // if there is any age (snake body), this will cause the end of the game (snake must be moving)
  if (age[snake.row][snake.col] != 0 && snakeDirection != 0) {
    gameOver = true;
    return;
  }

  // check if the food was eaten
  if (snake.row == food.row && snake.col == food.col) {
    snakeLength++;
    food.row = -1; // reset food
    food.col = -1;
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Score: ");
    lcd.print(snakeLength);
  }

  // increment ages if all lit leds
  updateAges();

  // change the age of the snake head from 0 to 1
  age[snake.row][snake.col]++;
}


// causes the snake to appear on the other side of the screen if it gets out of the edge
void fixEdge() {
  snake.col < 0 ? snake.col += 8 : 0;
  snake.col > 7 ? snake.col -= 8 : 0;
  snake.row < 0 ? snake.row += 8 : 0;
  snake.row > 7 ? snake.row -= 8 : 0;
}


// increment ages if all lit leds, turn off too old ones depending on the length of the snake
void updateAges() {
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      // if the led is lit, increment it's age
      if (age[row][col] > 0 ) {
        age[row][col]++;
      }

      // if the age exceeds the length of the snake, switch it off
      if (age[row][col] > snakeLength) {
        matrix.setLed(0, row, col, 0);
        age[row][col] = 0;
      }
    }
  }
}


void handleGameStates() {
  if (gameOver || win) {
    unrollSnake();

// 
    showScoreMessage(snakeLength);
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Score: ");
    lcd.print(snakeLength);

    if (gameOver) showGameOverMessage();
    else if (win) showWinMessage();

    // re-init the game
    win = false;
    gameOver = false;
    snake.row = random(8);
    snake.col = random(8);
    food.row = -1;
    food.col = -1;
    snakeLength = initialSnakeLength;
    snakeDirection = 0;
    memset(age, 0, sizeof(age[0][0]) * 8 * 8);
    matrix.clearDisplay(0);
    lcd.clear();
    lcd.begin(16,2);
    lcd.setCursor(0,0);
    lcd.print("  Snake game    ");
    lcd.setCursor(0,1);
    lcd.print("Yannick & Martin");
    delay(2000);
    lcd.setCursor(0,1);
    
  }
}


void unrollSnake() {
  // switch off the food LED
  matrix.setLed(0, food.row, food.col, 0);

  delay(600);

  for (int i = 1; i <= snakeLength; i++) {
    for (int row = 0; row < 8; row++) {
      for (int col = 0; col < 8; col++) {
        if (age[row][col] == i) {
          matrix.setLed(0, row, col, 0);
          delay(100);
        }
      }
    }
  }
}


// calibrate the joystick home for 10 times
void calibrateJoystick() {
  Coordinate values;

  for (int i = 0; i < 10; i++) {
    values.x += analogRead(Pin::joystickX);
    values.y += analogRead(Pin::joystickY);
  }

  joystickHome.x = values.x / 10;
  joystickHome.y = values.y / 10;
}


void initialize() {
  pinMode(Pin::joystickVCC, OUTPUT);
  digitalWrite(Pin::joystickVCC, HIGH);

  pinMode(Pin::joystickGND, OUTPUT);
  digitalWrite(Pin::joystickGND, LOW);

//  pinMode(Pin::joystickKEY, INPUT_PULLUP);

  matrix.shutdown(0, false);
  matrix.setIntensity(0, intensity);
  matrix.clearDisplay(0);

  randomSeed(analogRead(A5));
  snake.row = random(8);
  snake.col = random(8);
}


void dumpGameBoard() {
  String buff = "\n\n\n";
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      if (age[row][col] < 10) buff += " ";
      if (age[row][col] != 0) buff += age[row][col];
      else if (col == food.col && row == food.row) buff += "@";
      else buff += "-";
      buff += " ";
    }
    buff += "\n";
  }
  Serial.println(buff);
}





// --------------------------------------------------------------- //
// -------------------------- messages --------------------------- //
// --------------------------------------------------------------- //


const PROGMEM bool snakeMessage[8][56] = {
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,1,1,0,0,0,1,1,0,0,1,1,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,1,1,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1,1,1,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,1,1,0,1,1,1,1,0,0,1,1,1,1,1,1,0,0,1,1,1,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,1,0,0,1,1,0,0,1,1,0,0,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,0,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,1,1,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,1,1,0,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0}
};

const PROGMEM bool gameOverMessage[8][90] = {
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,1,1,0,0,0,1,1,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,1,1,1,1,0,0,0,1,1,0,0,1,1,0,0,1,1,1,1,1,1,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,1,0,1,1,1,0,0,1,1,0,0,0,0,0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,1,1,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,1,1,1,1,1,0,0,1,1,0,0,0,0,0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,1,1,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1,1,1,1,1,0,0,1,1,0,1,0,1,1,0,0,1,1,1,1,1,0,0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,1,1,1,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,1,1,0,1,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,0,1,1,0,0,1,1,0,0,0,0,0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,0,1,1,0,0,1,1,0,0,0,0,0,0,0,0,0,0,1,1,0,0,1,1,0,0,0,1,1,1,1,0,0,0,1,1,0,0,0,0,0,0,1,1,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,1,1,0,0,1,1,0,0,1,1,0,0,0,1,1,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,1,1,0,0,0,0,1,1,1,1,1,1,0,0,1,1,0,0,1,1,0,0,0,1,1,0,0,0,0,0,0,0,0,0}
};

const PROGMEM bool scoreMessage[8][58] = {
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,1,1,1,1,1,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,1,1,1,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,1,1,0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,1,1,0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,1,1,0,0,1,1,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0}
};

const PROGMEM bool WinMessage[8][32] = {
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,1,1,0,0,0,1,1,0,0,1,1,1,1,0,0,0,1,1,0,0,0,1,1},
  {0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,0,1,1,1,0,0,1,1},
  {0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,0,1,1,1,1,0,1,1},
  {0,1,1,0,1,0,1,1,0,0,0,1,1,0,0,0,0,1,1,0,1,1,1,1},
  {0,1,1,1,1,1,1,1,0,0,0,1,1,0,0,0,0,1,1,0,0,1,1,1},
  {0,1,1,1,0,1,1,1,0,0,0,1,1,0,0,0,0,1,1,0,0,0,1,1},
  {0,1,1,0,0,0,1,1,0,0,1,1,1,1,0,0,0,1,1,0,0,0,1,1}
};

const PROGMEM bool digits[][8][8] = {
  {
    {0,0,0,0,0,0,0,0},
    {0,0,1,1,1,1,0,0},
    {0,1,1,0,0,1,1,0},
    {0,1,1,0,1,1,1,0},
    {0,1,1,1,0,1,1,0},
    {0,1,1,0,0,1,1,0},
    {0,1,1,0,0,1,1,0},
    {0,0,1,1,1,1,0,0}
  },
  {
    {0,0,0,0,0,0,0,0},
    {0,0,0,1,1,0,0,0},
    {0,0,0,1,1,0,0,0},
    {0,0,1,1,1,0,0,0},
    {0,0,0,1,1,0,0,0},
    {0,0,0,1,1,0,0,0},
    {0,0,0,1,1,0,0,0},
    {0,1,1,1,1,1,1,0}
  },
  {
    {0,0,0,0,0,0,0,0},
    {0,0,1,1,1,1,0,0},
    {0,1,1,0,0,1,1,0},
    {0,0,0,0,0,1,1,0},
    {0,0,0,0,1,1,0,0},
    {0,0,1,1,0,0,0,0},
    {0,1,1,0,0,0,0,0},
    {0,1,1,1,1,1,1,0}
  },
  {
    {0,0,0,0,0,0,0,0},
    {0,0,1,1,1,1,0,0},
    {0,1,1,0,0,1,1,0},
    {0,0,0,0,0,1,1,0},
    {0,0,0,1,1,1,0,0},
    {0,0,0,0,0,1,1,0},
    {0,1,1,0,0,1,1,0},
    {0,0,1,1,1,1,0,0}
  },
  {
    {0,0,0,0,0,0,0,0},
    {0,0,0,0,1,1,0,0},
    {0,0,0,1,1,1,0,0},
    {0,0,1,0,1,1,0,0},
    {0,1,0,0,1,1,0,0},
    {0,1,1,1,1,1,1,0},
    {0,0,0,0,1,1,0,0},
    {0,0,0,0,1,1,0,0}
  },
  {
    {0,0,0,0,0,0,0,0},
    {0,1,1,1,1,1,1,0},
    {0,1,1,0,0,0,0,0},
    {0,1,1,1,1,1,0,0},
    {0,0,0,0,0,1,1,0},
    {0,0,0,0,0,1,1,0},
    {0,1,1,0,0,1,1,0},
    {0,0,1,1,1,1,0,0}
  },
  {
    {0,0,0,0,0,0,0,0},
    {0,0,1,1,1,1,0,0},
    {0,1,1,0,0,1,1,0},
    {0,1,1,0,0,0,0,0},
    {0,1,1,1,1,1,0,0},
    {0,1,1,0,0,1,1,0},
    {0,1,1,0,0,1,1,0},
    {0,0,1,1,1,1,0,0}
  },
  {
    {0,0,0,0,0,0,0,0},
    {0,1,1,1,1,1,1,0},
    {0,1,1,0,0,1,1,0},
    {0,0,0,0,1,1,0,0},
    {0,0,0,0,1,1,0,0},
    {0,0,0,1,1,0,0,0},
    {0,0,0,1,1,0,0,0},
    {0,0,0,1,1,0,0,0}
  },
  {
    {0,0,0,0,0,0,0,0},
    {0,0,1,1,1,1,0,0},
    {0,1,1,0,0,1,1,0},
    {0,1,1,0,0,1,1,0},
    {0,0,1,1,1,1,0,0},
    {0,1,1,0,0,1,1,0},
    {0,1,1,0,0,1,1,0},
    {0,0,1,1,1,1,0,0}
  },
  {
    {0,0,0,0,0,0,0,0},
    {0,0,1,1,1,1,0,0},
    {0,1,1,0,0,1,1,0},
    {0,1,1,0,0,1,1,0},
    {0,0,1,1,1,1,1,0},
    {0,0,0,0,0,1,1,0},
    {0,1,1,0,0,1,1,0},
    {0,0,1,1,1,1,0,0}
  }
};


// scrolls the 'snake' message around the matrix
void showSnakeMessage() {
  for (int d = 0; d < sizeof(snakeMessage[0]) - 7; d++) {
    for (int col = 0; col < 8; col++) {
      delay(messageSpeed);
      for (int row = 0; row < 8; row++) {
        // this reads the byte from the PROGMEM and displays it on the screen
        matrix.setLed(0, row, col, pgm_read_byte(&(snakeMessage[row][col + d])));
      }
    }
  }
}


// scrolls the 'game over' message around the matrix
void showGameOverMessage() {
  for (int d = 0; d < sizeof(gameOverMessage[0]) - 7; d++) {
    for (int col = 0; col < 8; col++) {
      delay(messageSpeed);
      for (int row = 0; row < 8; row++) {
        // this reads the byte from the PROGMEM and displays it on the screen
        matrix.setLed(0, row, col, pgm_read_byte(&(gameOverMessage[row][col + d])));
      }
    }
  }
}


// scrolls the 'win' message around the matrix
void showWinMessage() {
 for (int d = 0; d < sizeof(WinMessage[0]) - 7; d++) {
    for (int col = 0; col < 8; col++) {
      delay(messageSpeed);
      for (int row = 0; row < 8; row++) {
        // this reads the byte from the PROGMEM and displays it on the screen
        matrix.setLed(0, row, col, pgm_read_byte(&(WinMessage[row][col + d])));
      }
    }
  }
}


// scrolls the 'score' message with numbers around the matrix
void showScoreMessage(int score) {
  if (score < 0 || score > 99) return;

  // specify score digits
  int second = score % 10;
  int first = (score / 10) % 10;

  for (int d = 0; d < sizeof(scoreMessage[0]) + 2 * sizeof(digits[0][0]); d++) {
    for (int col = 0; col < 8; col++) {
      delay(messageSpeed);
      for (int row = 0; row < 8; row++) {
        if (d <= sizeof(scoreMessage[0]) - 8) {
          matrix.setLed(0, row, col, pgm_read_byte(&(scoreMessage[row][col + d])));
        }

        int c = col + d - sizeof(scoreMessage[0]) + 6; // move 6 px in front of the previous message

        // if the score is < 10, shift out the first digit (zero)
        if (score < 10) c += 8;

        if (c >= 0 && c < 8) {
          if (first > 0) matrix.setLed(0, row, col, pgm_read_byte(&(digits[first][row][c]))); // show only if score is >= 10 (see above)
        } else {
          c -= 8;
          if (c >= 0 && c < 8) {
            matrix.setLed(0, row, col, pgm_read_byte(&(digits[second][row][c]))); // show always
          }
        }
      }
    }
  }
}


// standard map function, but with floats (scaling)
float mapf(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
