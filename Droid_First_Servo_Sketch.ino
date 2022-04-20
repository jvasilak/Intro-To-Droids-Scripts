 // =======================================================================================
//                     PS3 Starting Sketch for Notre Dame Droid Class
// =======================================================================================
//                          Last Revised Date: 02/06/2021
//                             Revised By: Prof McLaughlin
// =======================================================================================
// ---------------------------------------------------------------------------------------
//                          Libraries
// ---------------------------------------------------------------------------------------
#include <PS3BT.h>
#include <usbhub.h>
#include <Sabertooth.h>
#include <Adafruit_TLC5947.h>
#include <MP3Trigger.h>
#include <Servo.h> 
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <NewPing.h>

// ---------------------------------------------------------------------------------------
//                       Debug - Verbose Flags
// ---------------------------------------------------------------------------------------
#define SHADOW_DEBUG       //uncomment this for console DEBUG output

// ---------------------------------------------------------------------------------------
//                        Autonomous Control Flags
// ---------------------------------------------------------------------------------------
//  Assign the sonar sensors to pins on the Arduino board
#define TRIGGER_PIN_FRONT 11
#define ECHO_PIN_FRONT 12
#define TRIGGER_PIN_BACK 9
#define ECHO_PIN_BACK 10
#define TRIGGER_PIN_SIDE 7
#define ECHO_PIN_SIDE 8

#define MAX_DISTANCE 400 // in cm

NewPing sonarFront(TRIGGER_PIN_FRONT, ECHO_PIN_FRONT, MAX_DISTANCE);
NewPing sonarBack(TRIGGER_PIN_BACK, ECHO_PIN_BACK, MAX_DISTANCE);
NewPing sonarSide(TRIGGER_PIN_SIDE, ECHO_PIN_SIDE, MAX_DISTANCE);
// ---------------------------------------------------------------------------------------
//                 Setup for USB, Bluetooth Dongle, & PS3 Controller
// ---------------------------------------------------------------------------------------
USB Usb;
BTD Btd(&Usb);
PS3BT *PS3Controller=new PS3BT(&Btd);

// ---------------------------------------------------------------------------------------
//    Output String for Serial Monitor Output
// ---------------------------------------------------------------------------------------
char output[300];

// ---------------------------------------------------------------------------------------
//    Deadzone range for joystick to be considered at nuetral
// ---------------------------------------------------------------------------------------
byte joystickDeadZoneRange = 15;

// ---------------------------------------------------------------------------------------
//    Used for PS3 Fault Detection
// ---------------------------------------------------------------------------------------
uint32_t msgLagTime = 0;
uint32_t lastMsgTime = 0;
uint32_t currentTime = 0;
uint32_t lastLoopTime = 0;
int badPS3Data = 0;

boolean isPS3ControllerInitialized = false;
boolean mainControllerConnected = false;
boolean WaitingforReconnect = false;
boolean isFootMotorStopped = true;

// ---------------------------------------------------------------------------------------
//    Used for PS3 Controller Click Management
// ---------------------------------------------------------------------------------------
long previousMillis = millis();
boolean extraClicks = false;

// ---------------------------------------------------------------------------------------
//    Used for Pin 13 Main Loop Blinker
// ---------------------------------------------------------------------------------------
long blinkMillis = millis();
boolean blinkOn = false;

// ---------------------------------------------------------------------------------------
//    Used for Servo Controls
// ---------------------------------------------------------------------------------------
Servo myservo;
boolean RequestServo0 = false;
boolean RequestServo180 = false;
int CurrentServoPosition;

// ---------------------------------------------------------------------------------------
//    Used for Motor Controls
// ---------------------------------------------------------------------------------------
int driveDeadBandRange = 10;
#define SABERTOOTH_ADDR 128
Sabertooth *ST=new Sabertooth(SABERTOOTH_ADDR, Serial1);

int requestSpeed = 0;
int requestTurn = 0;
int currentSpeed = 0;
int currentTurn = 0;
boolean requestStop = true;
boolean droidStopped = true;

// ---------------------------------------------------------------------------------------
//    Used for Autonomous Controls
// ---------------------------------------------------------------------------------------
boolean autoMode = false;
boolean autoInit = false;
boolean autoGoStraight = false;
boolean autoMakeTurn = false;
boolean autoGoBackwards = false;
int walldistance = 30;
float currentDistanceFront = 0;
float currentDistanceBack = 0;
float currentDistanceSide = 0;
int turnCount = 0;
float adjustLeftTurnAmount = 0;
long autoTurnTimer = millis();
long autoInitTimer = millis();
int autoInitWait = 2000;
int pingFrequency = 100; // frequency of ping, in ms
long pingTimerFront; // Holds the next ping time for the front
long pingTimerBack; // Holds the next ping time for the back
long pingTimerSide; // Holds the next ping time for the sides
int frontPingCount = 0;
long frontPingValues[9] = {0,0,0,0,0,0,0,0,0};
int backPingCount = 0;
long backPingValues[9] = {0,0,0,0,0,0,0,0,0};
int sidePingCount = 0;
long sidePingValues[9] = {0,0,0,0,0,0,0,0,0};
int autoSpeed = 40;

// =======================================================================================
//                                 Main Program
// =======================================================================================
// =======================================================================================
//                          Initialize - Setup Function
// =======================================================================================
void setup()
{
  
    //Debug Serial for use with USB Debugging
    Serial.begin(115200);
    while (!Serial);
    
    if (Usb.Init() == -1)
    {
        Serial.print(F("\r\nOSC did not start"));
        while (1); //halt
    }

    strcpy(output, "");
    
    Serial.print(F("\r\nBluetooth Library Started"));
    
    //Setup for PS3 Controller
    PS3Controller->attachOnInit(onInitPS3Controller); // onInitPS3Controller is called upon a new connection

    pinMode(13, OUTPUT);
    digitalWrite(13, LOW);

    //Setup of Servo
    myservo.attach(9);
    CurrentServoPosition = myservo.read();

    // Setup of Motors
    Serial1.begin(9600);
    ST->autobaud();
    ST->setTimeout(200);
    ST->setDeadband(driveDeadBandRange);

    // Setup Autonomous Code
     pingTimerFront = millis(); // Start now
     pingTimerBack = pingTimerFront + 30;
     pingTimerSide = pingTimerFront + 60;
}

// =======================================================================================
//           Main Program Loop - This is the recurring check loop for entire sketch
// =======================================================================================
void loop()
{   
   // Make sure the Bluetooth Dongle is working - skip main loop if not
   if ( !readUSB() )
   {
     //We have a fault condition that we want to ensure that we do NOT process any controller data
     printOutput(output);
     return;
   }
    
   // Check and output PS3 Controller inputs
   checkController();
  
   // Ignore extra inputs from the PS3 Controller
   if (extraClicks)
   {
      if ((previousMillis + 500) < millis())
      {
          extraClicks = false;
      }
   }
  
   // Control the Main Loop Blinker
   if ((blinkMillis + 500) < millis()) {
      if (blinkOn) {
        digitalWrite(13, LOW);
        blinkOn = false;
      } else {
        digitalWrite(13, HIGH);
        blinkOn = true;
      }
      blinkMillis = millis();
   }
   //pingSonarSensors();
   
   if(!autoMode) {
    setRequestMovement();
    moveDroid();
   }
   else if (autoMode) {
    autonomousCourse(); 
   }
   printOutput(output);
}

// =======================================================================================
//          Check Controller Function to show all PS3 Controller inputs are Working
// =======================================================================================
void checkController()
{
       if (PS3Controller->PS3Connected && PS3Controller->getButtonPress(UP) && !extraClicks)
     {              
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: UP Selected.\r\n");
            #endif
            
            previousMillis = millis();
            extraClicks = true;
            
     }
  
     if (PS3Controller->PS3Connected && PS3Controller->getButtonPress(DOWN) && !extraClicks)
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: DOWN Selected.\r\n");
            #endif                     
            
            previousMillis = millis();
            extraClicks = true;
       
     }

     if (PS3Controller->PS3Connected && PS3Controller->getButtonPress(LEFT) && !extraClicks)
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: LEFT Selected.\r\n");
            #endif  
            
            previousMillis = millis();
            extraClicks = true;
            if(!RequestServo0 && !RequestServo180) {
              servo180();
            }

     }
     
     if (PS3Controller->PS3Connected && PS3Controller->getButtonPress(RIGHT) && !extraClicks)
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: RIGHT Selected.\r\n");
            #endif       
            
            previousMillis = millis();
            extraClicks = true;
            if(!RequestServo0 && !RequestServo180) {
              servo0();
            }
                     
     }
     
     if (PS3Controller->PS3Connected && PS3Controller->getButtonPress(CIRCLE) && !extraClicks)
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: CIRCLE Selected.\r\n");
            #endif      
            
            previousMillis = millis();
            extraClicks = true;
           
     }

     if (PS3Controller->PS3Connected && PS3Controller->getButtonPress(CROSS) && !extraClicks)
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: CROSS Selected.\r\n");
            #endif       
            
            previousMillis = millis();
            extraClicks = true;
              
     }
     
     if (PS3Controller->PS3Connected && PS3Controller->getButtonPress(TRIANGLE) && !extraClicks)
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: TRIANGLE Selected.\r\n");
            #endif       
            
            previousMillis = millis();
            extraClicks = true;
            concludeAutoMode();
              
     }
     

     if (PS3Controller->PS3Connected && PS3Controller->getButtonPress(SQUARE) && !extraClicks)
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: SQUARE Selected.\r\n");
            #endif       
            
            previousMillis = millis();
            extraClicks = true;
            initializeAutoMode();
              
     }
     
     if (PS3Controller->PS3Connected && !extraClicks && PS3Controller->getButtonPress(L1))
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: LEFT 1 Selected.\r\n");
            #endif       
            
            previousMillis = millis();
            extraClicks = true;
     }

     if (PS3Controller->PS3Connected && !extraClicks && PS3Controller->getButtonPress(L2))
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: LEFT 2 Selected.\r\n");
            #endif       
            
            previousMillis = millis();
            extraClicks = true;
     }

     if (PS3Controller->PS3Connected && !extraClicks && PS3Controller->getButtonPress(R1))
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: RIGHT 1 Selected.\r\n");
            #endif       
            
            previousMillis = millis();
            extraClicks = true;
     }

     if (PS3Controller->PS3Connected && !extraClicks && PS3Controller->getButtonPress(R2))
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: RIGHT 2 Selected.\r\n");
            #endif       
            
            previousMillis = millis();
            extraClicks = true;
     }

     if (PS3Controller->PS3Connected && !extraClicks && PS3Controller->getButtonPress(SELECT))
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: SELECT Selected.\r\n");
            #endif       
            
            previousMillis = millis();
            extraClicks = true;
     }

     if (PS3Controller->PS3Connected && !extraClicks && PS3Controller->getButtonPress(START))
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: START Selected.\r\n");
            #endif       
            
            previousMillis = millis();
            extraClicks = true;
     }

     if (PS3Controller->PS3Connected && !extraClicks && PS3Controller->getButtonPress(PS))
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: PS Selected.\r\n");
            #endif       
            
            previousMillis = millis();
            extraClicks = true;
     }

     if (PS3Controller->PS3Connected && ((abs(PS3Controller->getAnalogHat(LeftHatY)-128) > joystickDeadZoneRange) || (abs(PS3Controller->getAnalogHat(LeftHatX)-128) > joystickDeadZoneRange)))
     {
            
            int currentValueY = PS3Controller->getAnalogHat(LeftHatY) - 128;
            int currentValueX = PS3Controller->getAnalogHat(LeftHatX) - 128;
            
            char yString[5];
            itoa(currentValueY, yString, 10);

            char xString[5];
            itoa(currentValueX, xString, 10);

            #ifdef SHADOW_DEBUG
                strcat(output, "LEFT Joystick Y Value: ");
                strcat(output, yString);
                strcat(output, "\r\n");
                strcat(output, "LEFT Joystick X Value: ");
                strcat(output, xString);
                strcat(output, "\r\n");
            #endif       
     }

     if (PS3Controller->PS3Connected && ((abs(PS3Controller->getAnalogHat(RightHatY)-128) > joystickDeadZoneRange) || (abs(PS3Controller->getAnalogHat(RightHatX)-128) > joystickDeadZoneRange)))
     {
            int currentValueY = PS3Controller->getAnalogHat(RightHatY) - 128;
            int currentValueX = PS3Controller->getAnalogHat(RightHatX) - 128;

            char yString[5];
            itoa(currentValueY, yString, 10);

            char xString[5];
            itoa(currentValueX, xString, 10);

            #ifdef SHADOW_DEBUG
                strcat(output, "RIGHT Joystick Y Value: ");
                strcat(output, yString);
                strcat(output, "\r\n");
                strcat(output, "RIGHT Joystick X Value: ");
                strcat(output, xString);
                strcat(output, "\r\n");
            #endif       
     }
}

// =======================================================================================
//           PPS3 Controller Device Mgt Functions
// =======================================================================================
// =======================================================================================
//           Initialize the PS3 Controller Trying to Connect
// =======================================================================================
void onInitPS3Controller()
{
    PS3Controller->setLedOn(LED1);
    isPS3ControllerInitialized = true;
    badPS3Data = 0;

    mainControllerConnected = true;
    WaitingforReconnect = true;

    #ifdef SHADOW_DEBUG
       strcat(output, "\r\nWe have the controller connected.\r\n");
       Serial.print("\r\nDongle Address: ");
       String dongle_address = String(Btd.my_bdaddr[5], HEX) + ":" + String(Btd.my_bdaddr[4], HEX) + ":" + String(Btd.my_bdaddr[3], HEX) + ":" + String(Btd.my_bdaddr[2], HEX) + ":" + String(Btd.my_bdaddr[1], HEX) + ":" + String(Btd.my_bdaddr[0], HEX);
       Serial.println(dongle_address);
    #endif
}

// =======================================================================================
//           Determine if we are having connection problems with the PS3 Controller
// =======================================================================================
boolean criticalFaultDetect()
{
    if (PS3Controller->PS3Connected)
    {
        
        currentTime = millis();
        lastMsgTime = PS3Controller->getLastMessageTime();
        msgLagTime = currentTime - lastMsgTime;            
        
        if (WaitingforReconnect)
        {
            
            if (msgLagTime < 200)
            {
             
                WaitingforReconnect = false; 
            
            }
            
            lastMsgTime = currentTime;
            
        } 
        
        if ( currentTime >= lastMsgTime)
        {
              msgLagTime = currentTime - lastMsgTime;
              
        } else
        {

             msgLagTime = 0;
        }
        
        if (msgLagTime > 300 && !isFootMotorStopped)
        {
            #ifdef SHADOW_DEBUG
              strcat(output, "It has been 300ms since we heard from the PS3 Controller\r\n");
              strcat(output, "Shut down motors and watching for a new PS3 message\r\n");
            #endif
            
//          You would stop all motors here
            isFootMotorStopped = true;
        }
        
        if ( msgLagTime > 10000 )
        {
            #ifdef SHADOW_DEBUG
              strcat(output, "It has been 10s since we heard from Controller\r\n");
              strcat(output, "\r\nDisconnecting the controller.\r\n");
            #endif
            
//          You would stop all motors here
            isFootMotorStopped = true;
            
            PS3Controller->disconnect();
            WaitingforReconnect = true;
            return true;
        }

        //Check PS3 Signal Data
        if(!PS3Controller->getStatus(Plugged) && !PS3Controller->getStatus(Unplugged))
        {
            //We don't have good data from the controller.
            //Wait 15ms - try again
            delay(15);
            Usb.Task();   
            lastMsgTime = PS3Controller->getLastMessageTime();
            
            if(!PS3Controller->getStatus(Plugged) && !PS3Controller->getStatus(Unplugged))
            {
                badPS3Data++;
                #ifdef SHADOW_DEBUG
                    strcat(output, "\r\n**Invalid data from PS3 Controller. - Resetting Data**\r\n");
                #endif
                return true;
            }
        }
        else if (badPS3Data > 0)
        {

            badPS3Data = 0;
        }
        
        if ( badPS3Data > 10 )
        {
            #ifdef SHADOW_DEBUG
                strcat(output, "Too much bad data coming from the PS3 Controller\r\n");
                strcat(output, "Disconnecting the controller and stop motors.\r\n");
            #endif
            
//          You would stop all motors here
            isFootMotorStopped = true;
            
            PS3Controller->disconnect();
            WaitingforReconnect = true;
            return true;
        }
    }
    else if (!isFootMotorStopped)
    {
        #ifdef SHADOW_DEBUG      
            strcat(output, "No PS3 controller was found\r\n");
            strcat(output, "Shuting down motors and watching for a new PS3 message\r\n");
        #endif
        
//      You would stop all motors here
        isFootMotorStopped = true;
        
        WaitingforReconnect = true;
        return true;
    }
    
    return false;
}

// =======================================================================================
//           USB Read Function - Supports Main Program Loop
// =======================================================================================
boolean readUSB()
{
  
     Usb.Task();
     
    //The more devices we have connected to the USB or BlueTooth, the more often Usb.Task need to be called to eliminate latency.
    if (PS3Controller->PS3Connected) 
    {
        if (criticalFaultDetect())
        {
            //We have a fault condition that we want to ensure that we do NOT process any controller data!!!
            printOutput(output);
            return false;
        }
        
    } else if (!isFootMotorStopped)
    {
        #ifdef SHADOW_DEBUG      
            strcat(output, "No controller was found\r\n");
            strcat(output, "Shuting down motors, and watching for a new PS3 foot message\r\n");
        #endif
        
//      You would stop all motors here
        isFootMotorStopped = true;
        
        WaitingforReconnect = true;
    }
    
    return true;
}

// =======================================================================================
//          Print Output Function
// =======================================================================================

void printOutput(const char *value)
{
    if ((strcmp(value, "") != 0))
    {
        if (Serial) Serial.println(value);
        strcpy(output, ""); // Reset output string
    }
}

// =======================================================================================
//          Servo Rotation Functions
// =======================================================================================
void servo0() {
  if(!RequestServo0) {
    RequestServo0 = true;
  }
  myservo.write(0);
  RequestServo0 = false;
  CurrentServoPosition = myservo.read();
}

void servo180() {
  if(!RequestServo180) {
    RequestServo180 = true;
  }
  myservo.write(170);
  RequestServo180 = false;
  CurrentServoPosition = myservo.read();
}

void servoL() {
  if (CurrentServoPosition < 170) {
    myservo.write(CurrentServoPosition+1);
    CurrentServoPosition = myservo.read();
  }
}

void servoR() {
  if (CurrentServoPosition > 0) {
    myservo.write(CurrentServoPosition-1);
    CurrentServoPosition = myservo.read();
  }
}


// =======================================================================================
//          Droid Movement Functions
// =======================================================================================
void setRequestMovement() {
  if (PS3Controller->PS3Connected){
    requestSpeed = PS3Controller->getAnalogHat(LeftHatY) - 128;
    requestTurn = PS3Controller->getAnalogHat(LeftHatX) - 128;
    //requestSpeed = PS3Controller->getAnalogHat(LeftHatY) - 188;
    //requestTurn = PS3Controller->getAnalogHat(LeftHatX) - 188;
    if (abs(requestSpeed) <= joystickDeadZoneRange) {
      requestSpeed = 0;
    }
    if (abs(requestTurn) <= joystickDeadZoneRange) {
      requestTurn = 0; 
    }
    
    if (requestSpeed == 0 && requestTurn == 0) {
      requestStop = true;
    }
    else {
      requestStop = false;
    }
  }
}

void moveDroid() {
  if(!requestStop) {
    if(requestSpeed > 50) {
      requestSpeed = 50;
    }
    else if (requestSpeed < -50) {
      requestSpeed = -50;
    }
    if(requestTurn > 40) {
      requestTurn = 40;
    }
    else if (requestTurn < -40) {
      requestTurn = -40;
    }
    currentSpeed = -1 * requestSpeed;
    currentTurn = requestTurn;
    ST->turn(currentTurn);
    ST->drive(currentSpeed);
    if(droidStopped) {
      droidStopped = false;
    }
  }
  else {
    if(!droidStopped) {
      ST->stop();
      droidStopped = true;
    }
  }
}

// =======================================================================================
//          Autonomous Mode Functions
// =======================================================================================
void pingSonarSensors() {
  pingFrontSonar();
  pingBackSonar();
  pingSideSonar();
}

void pingFrontSonar() {
  if(millis() >= pingTimerFront) {
    //Serial.println("Test pingFrontSonar");
    pingTimerFront = millis() + pingFrequency;
    sonarFront.ping_timer(setFrontDistance);
  }
}

void pingBackSonar() {
  if(millis() >= pingTimerBack) {
    pingTimerBack = millis() + pingFrequency;
    sonarBack.ping_timer(setBackDistance);
  }
}

void pingSideSonar() {
  if(millis() >= pingTimerSide) {
    pingTimerSide = millis() + pingFrequency;
    sonarSide.ping_timer(setSideDistance);
  }
}

void setFrontDistance() {
  if(sonarFront.check_timer()) {
    float fDist = (sonarFront.ping_result / US_ROUNDTRIP_CM) * 0.39370079;
    Serial.print("Front Ping: ");
    Serial.print(fDist);
    Serial.println("in");
    if (frontPingCount < 9) {
      frontPingValues[frontPingCount] = fDist;
    } else {
      sortFrontArray();
      currentDistanceFront = (frontPingValues[3] + frontPingValues[4] + frontPingValues[5])/3;
      frontPingCount = 0;
      frontPingValues[frontPingCount] = fDist;
    }
    frontPingCount++;
  }
}

void setBackDistance() {
  if(sonarBack.check_timer()) {
    float fDist = (sonarBack.ping_result / US_ROUNDTRIP_CM) * 0.39370079;
    Serial.print("Back Ping: ");
    Serial.print(fDist);
    Serial.println("in");
    if (backPingCount < 9) {
      backPingValues[backPingCount] = fDist;
    } else {
      sortBackArray();
      currentDistanceBack = (backPingValues[3] + backPingValues[4] + backPingValues[5])/3;
      backPingCount = 0;
      backPingValues[backPingCount] = fDist;
    }
    backPingCount++;
  }
}

void setSideDistance() {
  if(sonarSide.check_timer()) {
    float fDist = (sonarSide.ping_result / US_ROUNDTRIP_CM) * 0.39370079;
    Serial.print("Side Ping: ");
    Serial.print(fDist);
    Serial.println("in");
    if (sidePingCount < 9) {
      sidePingValues[sidePingCount] = fDist;
    } else {
      sortSideArray();
      currentDistanceSide = (sidePingValues[3] + sidePingValues[4] + sidePingValues[5])/3;
      sidePingCount = 0;
      sidePingValues[sidePingCount] = fDist;
    }
    sidePingCount++;
  }
}

void sortFrontArray() {
  for(int i = 0; i < 9; i++) {
    for(int j = 0; j < 8; j++) {
      if(frontPingValues[j] > frontPingValues[j+1]) {
        int temp = frontPingValues[j];
        frontPingValues[j] = frontPingValues[j+1];
        frontPingValues[j+1] = temp;
      }
    }
  }
}

void sortBackArray() {
  for(int i = 0; i < 9; i++) {
    for(int j = 0; j < 8; j++) {
      if(backPingValues[j] > backPingValues[j+1]) {
        int temp = backPingValues[j];
        backPingValues[j] = backPingValues[j+1];
        backPingValues[j+1] = temp;
      }
    }
  }
}

void sortSideArray() {
  for(int i = 0; i < 9; i++) {
    for(int j = 0; j < 8; j++) {
      if(sidePingValues[j] > sidePingValues[j+1]) {
        int temp = sidePingValues[j];
        sidePingValues[j] = sidePingValues[j+1];
        sidePingValues[j+1] = temp;
      }
    }
  }
}

void autoModeExecution() {
  if(autoGoStraight) {
    autoStraight();
    // checkwallSides();
    if (currentDistanceFront <= walldistance) {
      autoGoStraight = false;
      autoMakeTurn = true;
      autoTurnTimer = millis();
    }
  }
  else if(autoMakeTurn) {
    autoTurn();
    if (millis() >= autoTurnTimer + 880) {
      turnCount++;
      autoMakeTurn = false;
      autoGoStraight = true;
    }
    if (turnCount == 4) {
      concludeAutoMode();
      return;
    }
  }
}

void initializeAutoMode() {
  autoMode = true;
  autoGoStraight = true;
  autoMakeTurn = false;
  autoInitTimer = millis();
  turnCount = 0;
  autoInit = true;
}

void concludeAutoMode() {
  autoMode = false;
  autoGoStraight = false;
  autoMakeTurn = false;
  autoInit = false;
  turnCount = 0;
}

void autoStraight() {
  if(droidStopped) {
      droidStopped = false;
  }
  ST->turn(0);
  ST->drive(autoSpeed);
}

void autoTurn() {
  if(droidStopped) {
    droidStopped = false;
  }
  // positive values turn right, negative turns left
  ST->turn(autoSpeed);
}

void autonomousCourse() {
  if(autoInit == false) {
    initializeAutoMode();
  }
  else if(autoInitTimer + autoInitWait <= millis()) {
    autoModeExecution();
  }
}

/*
void autonomousCourse() {
  start moving droid forward
  if(forward == true) {
    moveForward();
    detect a wall
    if(wall detected) {
      forward = false;
      left turn = true
    }
  }

turn left
  if(left turn == true) {
    turn left();
    if turn done
      stop turning
      turncount++
      if turncount == 4
        done = true
      else
         foward = true
  }

  if done == true
    autoConclude();
*/
