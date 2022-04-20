 // =======================================================================================
//                     Droids Final Starting Sketch
// =======================================================================================
//                          Last Revised Date: 04/11/2022
//                             Revised By: Jonathon Vasilak
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
//                 TODO: Assign Ports for Sonar Sensors
// ---------------------------------------------------------------------------------------

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
//    Used for PS3 Controller Click Management
// ---------------------------------------------------------------------------------------
long previousMillis = millis();
boolean extraClicks = false;

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
//    Used for Pin 13 Main Loop Blinker
// ---------------------------------------------------------------------------------------
long blinkMillis = millis();
boolean blinkOn = false;

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
boolean limitSpeed = false;

// ---------------------------------------------------------------------------------------
//    Used for Autonomous Functions
// ---------------------------------------------------------------------------------------
boolean autoMode = false;
boolean autoInit = false;
boolean autoStraight = false;
boolean autoTurn = false;

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
// ---------------------------------------------------------------------------------------
//    Used for Sound
// ---------------------------------------------------------------------------------------
MP3Trigger MP3Trigger;
boolean playRandomSound = true;
long playRandSoundMillis = millis();
int rndsnd = 1;
int rndInterval = 1000;
boolean randomSoundInit = false;
boolean playForwardSoundInit = false;
boolean playStopSoundInit = false;

boolean movingForward = false;
boolean playStopSound = false;
boolean forwardSoundStart = false;
boolean forwardSoundMid = false;
boolean forwardSoundFast = false;
long forwardSoundMillis = millis();
long stopSoundMillis = millis();

// ---------------------------------------------------------------------------------------
//    Used for Display
// ---------------------------------------------------------------------------------------
#define SCREEN_WIDTH 128 // OLED display width in pixels
#define SCREEN_HEIGHT 64 // OLED display height in pixels
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
int currentScreen = 0;

// ---------------------------------------------------------------------------------------
//    Used for LED Lights
// ---------------------------------------------------------------------------------------
#define clock 3
#define data 4
#define latch 2
Adafruit_TLC5947 LEDControl = Adafruit_TLC5947(1, clock, data, latch);
int ledMaxBright = 4000; // 4095 is MAX brightness
long LEDmillis = millis();
int flashInterval = 500;
boolean LEDsOn = false;

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

    // Setup of Motors
    Serial1.begin(9600);
    ST->autobaud();
    ST->setTimeout(200);
    ST->setDeadband(driveDeadBandRange);

    // Setup of Sound
    MP3Trigger.setup(&Serial2);
    MP3Trigger.setVolume(5);
    //MP3Trigger.play(6);
    Serial2.begin(MP3Trigger::serialRate());

    // Setup of Display
    display.begin(SSD1306_SWITCHCAPVCC, 0X3C);
    display.display();
    delay(2000);
    display.clearDisplay();

    LEDControl.begin();
}

void loop() {
  // Make sure the Bluetooth Dongle is working - skip main loop if not
   if ( !readUSB() )
   {
     //We have a fault condition that we want to ensure that we do NOT process any controller data
     printOutput(output);
     return;
   }
  
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
   checkController();

   if(!autoMode) {
    setRequestMovement();
    moveDroid();
   }
   else if (autoMode) {
    //autonomousCourse(); 
   }

   //checkLight();

   // Sound Code
   //tryRandSound();
   //checkMovingSound();
   // Visual Code

   
   printOutput(output);
}



// =======================================================================================
//           PPS3 Controller Device Mgt Functions
// =======================================================================================

// =======================================================================================
//           Check PS3 Controller for inputs
// =======================================================================================
void checkController() {
  // Will change the speed limiter on the droid's manual movement
  if (PS3Controller->PS3Connected && !extraClicks && PS3Controller->getButtonPress(START)) {
    setLimiter();
    previousMillis = millis();
    extraClicks = true;
  }

  if (PS3Controller->PS3Connected && !extraClicks && PS3Controller->getButtonPress(UP)) {
    // integratedRoutine1();
    previousMillis = millis();
    extraClicks = true;
  }

  if (PS3Controller->PS3Connected && !extraClicks && PS3Controller->getButtonPress(LEFT)) {
    // integratedRoutine2();
    previousMillis = millis();
    extraClicks = true;
  }

  if (PS3Controller->PS3Connected && !extraClicks && PS3Controller->getButtonPress(RIGHT)) {
    // integratedRoutine3();
    previousMillis = millis();
    extraClicks = true;
  }

  if (PS3Controller->PS3Connected && !extraClicks && PS3Controller->getButtonPress(DOWN)) {
    // autonomousCourse();
    previousMillis = millis();
    extraClicks = true;
  }
}

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
//          Droid Movement Functions
// =======================================================================================
void setRequestMovement() {
  if (PS3Controller->PS3Connected){
    requestSpeed = PS3Controller->getAnalogHat(LeftHatY) - 128;
    requestTurn = PS3Controller->getAnalogHat(LeftHatX) - 128;
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
    if (limitSpeed) {
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

void setLimiter() {
  if(limitSpeed) {
    limitSpeed = false;
  }
  else {
    limitSpeed = true;
  }
}
// TODO: ramping function

// =======================================================================================
//          Integrated Routines
// =======================================================================================

// Tow another car using the servo
void integratedRoutine1() {
  
}

// Sneak up on a tractor, make noise, then flee
void integratedRoutine2() {
  /*
  if(!routine2Init) {
    // Turn off lights
    for (int i = 0; i < 24; i++) {
      LEDControl.setPWM(i, 0);
    }
    LEDControl.write();

    // Lower speaker volume
    MP3Trigger.setVolume(50);

    routine2Init = true;
    sneakingMillis = millis() + 2000;
  } else if (!sneakingComplete) {
    if(droidStopped) {
      droidStopped = false;
    }
    if(millis() > sneakingMillis) {
      sneakingComplete = true;
    }
    ST->turn(0);
    ST->drive(sneakingSpeed);
  } else if (!scaringComplete) {
    Make noise, flash lights and back up slowly
  } else if (!routine2Complete) {
    Make a 180 turn and return to where droid started

    ST->turn();
  } else {
    routine2 = false;
    routine2Init = false;
    sneakingComplete = false;
    scaringComplete = false;
    routine2Complete = false;
  }
  
  */
}

// TODO: think of another integrated routine
void integratedRoutine3() {
  
}
