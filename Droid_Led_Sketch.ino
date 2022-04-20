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


   setRequestMovement();
   moveDroid();

   checkLight();

   // Sound Code
   tryRandSound();
   checkMovingSound();
   // Visual Code

   
   printOutput(output);
}

void checkLight() {
  if(millis() > LEDmillis && LEDsOn) {
    LEDControl.setPWM(0, 0);
    LEDControl.setPWM(1, ledMaxBright);
    LEDControl.write();
    LEDsOn = false;
    LEDmillis = millis() + flashInterval;
  } else if (millis() > LEDmillis) {
    LEDControl.setPWM(0, ledMaxBright);
    LEDControl.setPWM(1, 0);
    LEDControl.write();
    LEDsOn = true;
    LEDmillis = millis() + flashInterval;
  }
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
            if (playRandomSound) {
              playRandomSound = false;
            }
            else if (!playRandomSound) {
              playRandomSound = true;
              randomSoundInit = false;
            }
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
            

     }
     
     if (PS3Controller->PS3Connected && PS3Controller->getButtonPress(RIGHT) && !extraClicks)
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: RIGHT Selected.\r\n");
            #endif       
            
            previousMillis = millis();
            extraClicks = true;
            
                     
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
            
              
     }
     

     if (PS3Controller->PS3Connected && PS3Controller->getButtonPress(SQUARE) && !extraClicks)
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: SQUARE Selected.\r\n");
            #endif       
            
            previousMillis = millis();
            extraClicks = true;
            
              
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
    if(requestSpeed < 0 && !movingForward) {
      setupForwardSound();
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
    // play forward sound, check time
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
      // play stop sound
      playStopSound = true;
      resetForwardSound();
      droidStopped = true;
    }
  }
}

// =======================================================================================
//          Sound Functions
// =======================================================================================
void tryRandSound() {
  // No sound playing
  if(playRandomSound) {
    if(!randomSoundInit) {
      soundInit();
    }
  }
  // Sound is already playing
  if (playRandomSound && randomSoundInit) {
   if(playRandSoundMillis < millis()) {
      //MP3Trigger.setVolume(100);
      MP3Trigger.trigger(rndsnd);
      displayRandSound();
      randomSoundInit = false;
   }
  }
}

void soundInit() {
  rndInterval = random(1000, 5000);
  rndsnd = random(1, 5);
  randomSoundInit = true;
  playRandSoundMillis = millis() + rndInterval;
}

void checkMovingSound() {
  if(movingForward && !playForwardSoundInit) {
    playForwardSound();
  }
  volumeTransition();
  stopSound();
}

void stopSound() {
  if (playStopSound && !playStopSoundInit) {
    //MP3Trigger.setVolume(100);
    MP3Trigger.trigger(1); // replace with actual number , 6 second sound
    displayStop();
    stopSoundMillis = millis() + 6000;
    playStopSoundInit = true;
  }
  if (playStopSoundInit && millis() > stopSoundMillis) {
    playStopSoundInit = false;
    playStopSound = false;
    playRandomSound = true;
  }
}

void volumeTransition() {
  if (forwardSoundStart && millis() > forwardSoundMillis) {
    forwardSoundStart = false;
    forwardSoundMid = true;
    //MP3Trigger.setVolume(150);
    playForwardSoundInit = false;
  } else if (forwardSoundMid && millis() > forwardSoundMillis) {
    forwardSoundMid = false;
    forwardSoundFast = true;
    //MP3Trigger.setVolume(200);
    playForwardSoundInit = false;
  } else if(forwardSoundFast && millis() > forwardSoundMillis) {
    playForwardSoundInit = false;
  }
}

void playForwardSound() {
  if (forwardSoundStart) {
    forwardSoundMillis = millis() + 2000;
    MP3Trigger.trigger(1);
    playForwardSoundInit = true;
  } else if (forwardSoundMid) {
    forwardSoundMillis = millis() + 2000;
    MP3Trigger.trigger(1);
    playForwardSoundInit = true;
  } else if (forwardSoundFast) {
    forwardSoundMillis = millis() + 2000;
    MP3Trigger.trigger(1);
    playForwardSoundInit = true;
  }
  displayForwards();
}

void setupForwardSound() {
  playRandomSound = false;
  movingForward = true;
  forwardSoundStart = true;
}

void resetForwardSound() {
  movingForward = false;
  playForwardSoundInit = false;
  forwardSoundStart = false;
  forwardSoundMid = false;
  forwardSoundFast = false;
}

// =======================================================================================
//          Display Functions
// =======================================================================================
void displayRandSound() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,0); 
  if(rndsnd == 1) {
    display.println("RANDOM");
    display.println("DAD GUM");
  } else if (rndsnd == 2) {
    display.println("RANDOM");
    display.println("Backing Up");
  } else if (rndsnd == 3) {
    display.println("RANDOM");
    display.println("Beep beep");
  } else if (rndsnd == 4) {
    display.println("RANDOM");
    display.println("well shoot");
  } else if (rndsnd == 5) {
    display.println("RANDOM");
    display.println("my name is m8tr");
  }
  display.display();
}

void displayForwards() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  if (forwardSoundStart) {
    display.println("Going Slow");
  } else if (forwardSoundMid) {
    display.println("Going Mid");
  } else if (forwardSoundFast) {
    display.println("Going Fast");
  }
  display.display();
}

void displayStop() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Stopping");
  display.display();
}
