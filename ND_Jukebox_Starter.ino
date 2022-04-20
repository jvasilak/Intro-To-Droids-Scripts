//************************************************
// ND Jukebox
//************************************************
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
//                 Setup for USB, Bluetooth Dongle, & PS3 Controller
// ---------------------------------------------------------------------------------------
USB Usb;
BTD Btd(&Usb);
PS3BT *PS3Controller=new PS3BT(&Btd);


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


MP3Trigger MP3Trigger;

// ---------------------------------------------------------------------------------------
//    Used for Display
// ---------------------------------------------------------------------------------------
#define SCREEN_WIDTH 128 // OLED display width in pixels
#define SCREEN_HEIGHT 64 // OLED display height in pixels
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
//************************************************
// Juke Box Variables
//************************************************
// State Variables
//************************************************
bool requestSongStart = false;
bool requestSongStop = false;
bool requestScrollUp = false;
bool requestScrollDown = false;
bool requestVolUp = false;
bool requestVolDown = false;

int currentSelectedSongNumber = 1;
int currentSelectedSongLength = 166;
String currentSelectedSongTitle = "Walk the Line";
long currentSelectedSongMillisStart = millis();
int currentSelectedSongPercentComplete = 0;
bool currentSelectedSongAtEnd = false;
bool currentSelectedSongPlaying = false;

int currentVolumeNumber = 50;  // from 0 HIGH to 100 LOW
float currentVolumePercentage = .5;

bool onMainMenu = true;
bool initMainScreenComplete = false;
bool onSongDetailScreen = false;
bool initSongDetailComplete = false;
bool onVolDetailScreen = false;
bool initVolumeScreenComplete = false;
int currentTopScrollSongNumber = 1;

long scrollScreenDelayMillis = millis();
int scrollScreenDelayInterval = 500;
long refreshPercentCompleteMillis = millis();
int refreshPercentCompleteInterval = 500;
long refreshPercentVolumeMillis = millis();
int refreshPercentVolumeInterval = 500;

String songTitle[36] = {"Walk the Line",
                    "Ring of Fire",
                    "Blue Suede Shoes",
                    "So Lonesome",
                    "Folsom Prison",
                    "Cheatin Heart",
                    "Jolene",
                    "Big River",
                    "Blues Eyes Cryin",
                    "Imagine",
                    "Long Tall Sally",
                    "Pretty Woman",
                    "Peggy Sue",
                    "Everyday",
                    "La Bamba",
                    "Sweet Dreams",
                    "Desperado",
                    "The Twist",
                    "Respect",
                    "People Get Ready",
                    "Dock of the Bay",
                    "Dancing Streets",
                    "My Imagination",
                    "Stay Together",
                    "Papa New Bag",
                    "Stany By Me",
                    "Who Do You Love",
                    "My Generation",
                    "Yesterday",
                    "Mr Tambourine",
                    "Fighting Man",
                    "Paranoid",
                    "Highway to Hell",
                    "Roxanne",
                    "Lola",
                    "Love Rock N Roll"};
                    
int songTrack[36]={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36};

long songLength[36]={166,157,136,169,165,163,162,168,140,184,129,180,150,128,124,211,121,156,148,159,162,158,181,198,128,178,150,199,127,150,139,173,208,195,250,175};

void setup() {
  //Debug Serial for use with USB Debugging
    Serial.begin(115200);
    while (!Serial);
    
    if (Usb.Init() == -1)
    {
        Serial.print(F("\r\nOSC did not start"));
        while (1); //halt
    }

    //strcpy(output, "");
    
    Serial.print(F("\r\nBluetooth Library Started"));
    
    //Setup for PS3 Controller
    PS3Controller->attachOnInit(onInitPS3Controller); // onInitPS3Controller is called upon a new connection

    pinMode(13, OUTPUT);
    digitalWrite(13, LOW);

    // Setup of Sound
    MP3Trigger.setup(&Serial2);
    Serial2.begin(MP3Trigger::serialRate());
    MP3Trigger.setVolume(0);
    // Setup of Display
    display.begin(SSD1306_SWITCHCAPVCC, 0X3C);
    display.display();
    delay(2000);
    display.clearDisplay();
}

void loop() {
  // Make sure the Bluetooth Dongle is working - skip main loop if not
   if ( !readUSB() )
   {
     //We have a fault condition that we want to ensure that we do NOT process any controller data
     //printOutput(output);
     return;
   }
    
   // Check and output PS3 Controller inputs
   checkController();

   if(requestScrollUp) {
    scrollUp();
   } else if (requestScrollDown) {
    scrollDown();
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
   
   if(onMainMenu && !initMainScreenComplete){
      displayMenu();
      initMainScreenComplete = true;
   }
   else if(onSongDetailScreen && !initSongDetailComplete){
      displayProgressScreen();
      initSongDetailComplete = true;
   }
   else if(onVolDetailScreen && !initVolumeScreenComplete){
      displayVolumeScreen();
      initVolumeScreenComplete = true;
   }

   if(onSongDetailScreen && millis() > refreshPercentCompleteMillis) {
      calculateCompletion();
      displayProgressScreen();
   }
}

// Other Functions

// General display function, calls ohter function based on states


// executes playing a song
void playSong() {
  currentSelectedSongPlaying = true;
  MP3Trigger.trigger(currentSelectedSongNumber);
  onMainMenu = false;
  onVolDetailScreen = false;
  onSongDetailScreen = true;
  initSongDetailComplete = false;
  currentSelectedSongMillisStart = millis();
  //currentSelectedSongPercentComplete = 0;
  
}

void stopSong(){ // COME BACK TO
  currentSelectedSongPlaying = false;
  onSongDetailScreen = false;
  returnToMainMenu();
}

void calculateCompletion(){
  currentSelectedSongPercentComplete = ((millis()-currentSelectedSongMillisStart) / (songLength[currentSelectedSongNumber-1] * 1000)) * 100;
}

void volumeDown() {
  currentVolumeNumber = currentVolumeNumber - 1;
  MP3Trigger.setVolume(currentVolumeNumber);
  onVolDetailScreen = true;
  initVolumeScreenComplete = false;
  refreshPercentVolumeMillis = millis() + 200;
}

void volumeUp() {
  currentVolumeNumber = currentVolumeNumber + 1;
  MP3Trigger.setVolume(currentVolumeNumber);
  onVolDetailScreen = true;
  initVolumeScreenComplete = false;
  refreshPercentVolumeMillis = millis() + 200;
}

void scrollUp() {
  if(requestScrollUp && currentSelectedSongNumber > 1 && scrollScreenDelayMillis < millis()) {
    currentSelectedSongNumber = currentSelectedSongNumber - 1;
    scrollScreenDelayMillis = millis() + scrollScreenDelayInterval;
    requestScrollUp = false;
    currentSelectedSongTitle=songTitle[currentSelectedSongNumber-1];
    currentSelectedSongLength=songLength[currentSelectedSongNumber-1];
    initMainScreenComplete = false;
    
  }
}

void scrollDown() {
  if(requestScrollDown && currentSelectedSongNumber < 36 && scrollScreenDelayMillis < millis()) {
    currentSelectedSongNumber++;
    scrollScreenDelayMillis = millis() + scrollScreenDelayInterval;
    requestScrollDown = false;
    currentSelectedSongTitle=songTitle[currentSelectedSongNumber-1];
    currentSelectedSongLength=songLength[currentSelectedSongNumber-1];
    initMainScreenComplete = false;
  }
}

void returnToMainMenu(){
  onMainMenu = true;
  initMainScreenComplete = false;
  // update screen
}


void displayMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  if(currentSelectedSongNumber > 1) {
    display.println(songTitle[currentSelectedSongNumber-1]);
  }
  display.print(">> ");
  display.println(currentSelectedSongTitle);
  if(currentSelectedSongNumber < 36) {
    display.println(songTitle[currentSelectedSongNumber]);
  }
  display.display();
}

void displayVolumeScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Volume: ");
  display.print(currentVolumeNumber);
  display.println();
  if(currentSelectedSongPlaying == true){
    display.print("% Complete: ");
    display.println(currentSelectedSongPercentComplete);
    display.display();
  }
  refreshPercentVolumeMillis = millis() + refreshPercentVolumeInterval;
}

void displayProgressScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.print("Song Title:");
  display.println(currentSelectedSongTitle);
  display.print("% Complete: ");
  display.println(currentSelectedSongPercentComplete);
  display.display();
  refreshPercentCompleteMillis = millis() + refreshPercentCompleteInterval;
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

boolean readUSB()
{
  
     Usb.Task();
     
    //The more devices we have connected to the USB or BlueTooth, the more often Usb.Task need to be called to eliminate latency.
    if (PS3Controller->PS3Connected) 
    {
        if (criticalFaultDetect())
        {
            //We have a fault condition that we want to ensure that we do NOT process any controller data!!!
            //printOutput(output);
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
//          Check Controller Function to show all PS3 Controller inputs are Working
// =======================================================================================
void checkController()
{
     if (PS3Controller->PS3Connected && PS3Controller->getButtonPress(CIRCLE) && !extraClicks)
     {
               
            
            previousMillis = millis();
            extraClicks = true;
            if(currentSelectedSongPlaying){
              stopSong();
            }
           
     }

     if (PS3Controller->PS3Connected && PS3Controller->getButtonPress(CROSS) && !extraClicks)
     {
               
            
            previousMillis = millis();
            extraClicks = true;
            if(onMainMenu == true && currentSelectedSongPlaying  == false){
              playSong();
            }
         
              
     }

     if (PS3Controller->PS3Connected && ((abs(PS3Controller->getAnalogHat(LeftHatY)-128) > joystickDeadZoneRange) || (abs(PS3Controller->getAnalogHat(LeftHatX)-128) > joystickDeadZoneRange)))
     {
            
            int currentValueY = PS3Controller->getAnalogHat(LeftHatY) - 128;
            int currentValueX = PS3Controller->getAnalogHat(LeftHatX) - 128;

            if (abs(currentValueY) >= joystickDeadZoneRange && onMainMenu) {
              if (currentValueY > 0) {
                requestScrollDown = true;
              } else if (currentValueY < 0) {
                requestScrollUp = true;
              }
              //initMainScreenComplete = false;
            }
            
            char yString[5];
            itoa(currentValueY, yString, 10);

            char xString[5];
            itoa(currentValueX, xString, 10);
            
              
     }

     if (PS3Controller->PS3Connected && ((abs(PS3Controller->getAnalogHat(RightHatY)-128) > joystickDeadZoneRange) || (abs(PS3Controller->getAnalogHat(RightHatX)-128) > joystickDeadZoneRange)))
     {
            int currentValueY = PS3Controller->getAnalogHat(RightHatY) - 128;
            int currentValueX = PS3Controller->getAnalogHat(RightHatX) - 128;
            if (abs(currentValueY) >= joystickDeadZoneRange) {
              if (currentValueY > 0) {
                requestVolDown = true;
              } else if (currentValueY < 0) {
                requestVolUp = true;
              }
              onVolDetailScreen = true;
              initVolumeScreenComplete = false;
            }
            char yString[5];
            itoa(currentValueY, yString, 10);

            char xString[5];
            itoa(currentValueX, xString, 10);

         
     }
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
