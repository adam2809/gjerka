#include <cppQueue.h>

#define LED_FLASH_LENGTH 100
#define PLAYER_X_POS 1
#define WALLS_COUNT 2

#define ANALOG_INPUT_RANGE 1024
#define ANALOG_OUTPUT_RANGE 256
#define SPEED_LED_PIN_COUNT 3

#define MIN_DIFFICULTY 1000
#define MAX_DIFFICULTY 600

#define Y_0 2
#define Y_1 3
#define Y_2 4
#define Y_3 5
#define Y_4 6
#define Y_5 7
#define Y_6 8
#define Y_7 9

#define X_0 10
#define X_1 11
#define X_2 12
#define X_3 13
#define X_4 30
#define X_5 31
#define X_6 32
#define X_7 33

const byte xPins[] = {
  X_0,X_1, X_2, X_3, X_4, X_5, X_6, X_7
};
const byte yPins[] = {
  Y_0,Y_1,Y_2, Y_3, Y_4, Y_5, Y_6, Y_7
};

typedef struct {
  boolean prev;
  boolean wasPressed;
  int pin;
} Button;

Button upButton = {false, false, 42};
Button downButton = {false, false, 43};
Button shootButton = {false, false, 44};

typedef struct {
  long flashStartTime;
  int pin;
} Led;

Led upLed = {0,41};
Led downLed = {0,40};
Led shootLed = {0,39};

Led* buttonLeds[] = {&upLed,&downLed,&shootLed};

int speedLedPins[] = {36,37,38};
int speedInputPin = A1;

int playerYPos = 4;

long lastWallPropagation = millis();

int difficulty = MAX_DIFFICULTY / 2;

//structure example: 01111111 wall will have a gap on top
typedef struct {
  byte xPos;
  byte structure;
} Wall;
Wall wall1 = {0,B10000011};
Wall wall2 = {0,B00100010};

Queue walls(sizeof(Wall*), WALLS_COUNT, FIFO);

byte frame[8] = {
  B00000000,
  B00000000,
  B00000000,
  B00000000,
  B00000000,
  B00000000,
  B00000000,
  B00000000,
};

void setup() {
  Serial.begin(9600);
  
  initPins();

  initWalls();
}

void initPins(){
  initButtonInputPins();

  initLedMatrixPins();
  
  initLedOutputPins();
}


void initButtonInputPins(){
  pinMode(upButton.pin, INPUT);
  pinMode(downButton.pin, INPUT);
  pinMode(shootButton.pin, INPUT);
}

void initLedMatrixPins(){
  for(byte i = 0;i < 8;i++){
    pinMode(xPins[i],OUTPUT);
    pinMode(yPins[i],OUTPUT);

    digitalWrite(xPins[i],LOW);
    digitalWrite(yPins[i],HIGH);
  }
}

void initLedOutputPins(){
  pinMode(upLed.pin, OUTPUT);
  pinMode(downLed.pin, OUTPUT);
  pinMode(shootLed.pin, OUTPUT);

  for(int i = 0;i < SPEED_LED_PIN_COUNT;i++){
    pinMode(speedLedPins[i],OUTPUT);
  }
}

void initWalls(){
  wall1.xPos = 5;
  Wall* ptr = &wall1;
  walls.push(&ptr);

  wall2.xPos = 7;
  ptr = &wall2;
  walls.push(&ptr);
}

void loop() {
  handlePlayerInput();
  endLedFlashes();

  if(millis() - lastWallPropagation > difficulty){
    propagateWalls();
    lastWallPropagation = millis();
  }
  
  if(walls.nbRecs() < WALLS_COUNT){
    buildNewWall();
  }

  updateFrame();
  displayFrame();
  
  checkIfPlayerHitWall();
}

void updateFrame(){
  clearFrame();
  
  for(byte i = 0;i < WALLS_COUNT;i++){
    Wall* currWall;
    if(!walls.peekIdx(&currWall, i)){
      continue;
    }
    
    frame[currWall->xPos] = currWall->structure;
  }
  
  frame[PLAYER_X_POS] = (B00000001 << playerYPos) | frame[PLAYER_X_POS];
}

void clearFrame(){
  for(int i = 0;i < 8;i++){
    frame[i] = B00000000;
  }
}

void displayFrame(){
  for(int i = 0;i < 8;i++){
    digitalWrite(xPins[i],HIGH);
    setFrameColumn(frame[i]);
    digitalWrite(xPins[i],LOW);
    resetYPins();
  }
}

void setFrameColumn(byte column){
  for(int j = 0;j < 8;j++){
    digitalWrite(yPins[j],!((column >> j) % 2));
  }
}

void resetYPins(){
  for(int i = 0;i < 8;i++){
    digitalWrite(yPins[i],HIGH);
  }
}

void checkIfPlayerHitWall() {
  Wall* frontWall;
  if (!walls.peek(&frontWall)) {
    Serial.println("NO WALLS");
    return;
  }

  if (playerIsTouchingWall(frontWall)) {
    restart();
  }
}

bool playerIsTouchingWall(Wall* wall) {
  if (wall->xPos != PLAYER_X_POS) {
    return false;
  }

  return ((wall->structure) >> playerYPos) % 2;
}

void propagateWalls() {
  for(int i = WALLS_COUNT-1;i >= 0;i--){
    Wall* currWall;
    if(!walls.peekIdx(&currWall, i)){
      continue;
    }
    
    if(currWall->xPos == 0){
      walls.drop();
      continue;
    }
    (currWall->xPos)--;
  }
}

void buildNewWall(){
  Wall* wallMaterial;
  if(wall1.xPos < 1){
    wallMaterial = &wall1;
  }else{
    wallMaterial = &wall2;
  }

  wallMaterial->structure = generateWallStructure();
  wallMaterial->xPos = 7;

  walls.push(&wallMaterial);
}

long generateWallStructure(){
  byte structure = 0;
  
  for(int i = 0;i < 8;i++){
    structure = (structure + random(0,2)) << 1;
  }

  return structure;
}

void restart() {
  flashAllLeds(20);
  setup();
}

void handlePlayerInput() {
  handleButtonActions();

  handleSpeedAction();
}

void handleButtonActions() {
  if (listenForPress(&upButton)) {
    Serial.println("UP");
    playerYPos++;
    startLedFlash(&upLed);
  }

  if (listenForPress(&downButton)) {
    Serial.println("DOWN");
    playerYPos--;
    startLedFlash(&downLed);
  }

  if (listenForPress(&shootButton)) {
    Serial.println("SHOOT");
    startLedFlash(&shootLed);
  }
}

void handleSpeedAction() {
  updateSpeedLeds();

  updateDifficulty();
}

void updateSpeedLeds(){
  int speedInputValue = analogRead(speedInputPin);
  int rangeForOneLed = ANALOG_INPUT_RANGE / SPEED_LED_PIN_COUNT;
  
  int maxLedIndex = speedInputValue / rangeForOneLed;
  for(int i = maxLedIndex;i>=0;i--){
    analogWrite(speedLedPins[i],ANALOG_OUTPUT_RANGE - 1);
  }
  
  int maxLedOutputValue = ((double) (speedInputValue % rangeForOneLed) / rangeForOneLed) * ANALOG_OUTPUT_RANGE;
  analogWrite(speedLedPins[maxLedIndex],maxLedOutputValue);
}

void updateDifficulty(){
  difficulty = map(
    analogRead(speedInputPin),
    0,ANALOG_INPUT_RANGE-1,
    MIN_DIFFICULTY,MAX_DIFFICULTY
  );
}

void startLedFlash(Led* led) {
  led->flashStartTime = millis();
  digitalWrite(led->pin, HIGH);  
}

void endLedFlashes(){
  for(int i = 0;i < 3;i++){
    Led* led = buttonLeds[i];
    if(digitalRead(led->pin) && millis()-led->flashStartTime>LED_FLASH_LENGTH){
      digitalWrite(led->pin,LOW);
    }
  }
}

boolean listenForPress(Button* button) {
  boolean curr = digitalRead(button->pin);

  if (button->prev == LOW && curr == HIGH) {
    button->prev = curr;
    return true;
  }
  button->prev = curr;
  return false;
}

boolean listenForRelease(Button* button) {
  boolean curr = digitalRead(button->pin);


  if (button->prev == LOW && curr == HIGH) {
    button->wasPressed = true;
  }

  if (button->wasPressed == true && curr == LOW) {
    button->wasPressed = false;
    button->prev = curr;
    return true;
  }
  button->prev = curr;
  return false;
}

void flashAllLeds(int flashGap){
  for(int x = 0;x < 8;x++){
    for(int y = 0;y < 8;y++){
      flashLed(x,y,flashGap);
    }
  }
}

void flashLed(int x,int y,int flashGap){
  digitalWrite(xPins[x],HIGH);
  digitalWrite(yPins[y],LOW);

  delay(flashGap);

  digitalWrite(xPins[x],LOW);
  digitalWrite(yPins[y],HIGH);
}

void clearArray(byte arr[]){
  for(int i=0;i<8;i++){
    arr[i] = 0;
  }
}

void printArray(byte arr[]){
  for(int i=0;i<8;i++){
    Serial.println(arr[i]);
  }
}
