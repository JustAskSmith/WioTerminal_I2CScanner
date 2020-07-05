/*
  Wio Terminal I2C scanner version 1, July 1, 2020
  Author: Charles Smith, charlie@justasksmith.com
  
  requires the Seeed_Ardunio_LCD library provided with the WioTerminal board definitions
  https://wiki.seeedstudio.com/Seeed_Arduino_Boards/

  scaning based on the the I2C scanner by Nick Gammon
  https://www.gammon.com.au/forum/?id=10896

  scrolling groked from the TFT_Matrix example by Bodmer the author
  of the TFT_eSPI library forked by Seeed
  https://github.com/Bodmer/TFT_eSPI
    
  Plug a I2C sensor or a hub with muiltiple sensors into the Grove port closest to the power switch
  Press the user button closest to the power switch to start a scan
  Stores the results in a circluar buffer that can be scrolled with the joystick
*/

#include <Wire.h>
#include <TFT_eSPI.h>

// uncomment this to output results to the serial monitor also
// do not uncomment unless you have an active serial monitor on the USB serial port
// or it will block the execution of the sketch

//#define SERIAL_OUTPUT

//#define TESTING // uncomment this line to load test data. Used to test scrolling behavior

// uncomment only one block of start and end addresses for the scan

#define SCAN_START 8  // uncomment these to skip the reserved addresses
#define SCAN_END 119

// #define SCAN_START = 1  // uncomment these  to include the reserved addresses
// #define SCAN_END = 127

#define TEXT_HEIGHT 16 // Height of text to be printed in pixels
#define DISPLAY_HEIGHT 320 // Height of the display in pixels
#define SCROLL_AREA_HEIGHT (DISPLAY_HEIGHT - BOT_FIXED_AREA) // Height of the scrollable area in pixels
#define SCROLL_AREA_TEXT_LINES (SCROLL_AREA_HEIGHT / TEXT_HEIGHT) // Height of the scrollable area in text lines
#define BOT_FIXED_AREA 16  // Number of pixel lines in bottom fixed area (lines counted from bottom of screen)
#define SCROLL_DELAY 100 // Number of milliseconds for the scroll repeat 

struct device_t
{
  uint8_t address;
  uint8_t response;
};

device_t results[127]; // holds the address and return code of the discovered devices.
uint16_t numberOfDevices, topDevice, scrollPointer;
bool scanDone;

TFT_eSPI tft = TFT_eSPI();

void setup()
{
  pinMode(WIO_KEY_C, INPUT_PULLUP);
  pinMode(WIO_5S_LEFT, INPUT_PULLUP);
  pinMode(WIO_5S_RIGHT, INPUT_PULLUP);

  Wire.begin();
  scanDone = false;
  numberOfDevices = 0; // number of devices that were discovered
  topDevice = 0;  // index of the device displayed at the top of the screen
  scrollPointer = 0; //current location of the display buffer that is being displayed at screen 0,0

  tft.init();
  tft.setRotation(0);
  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.fillScreen(ILI9341_BLACK);
  setupScrollArea(0, BOT_FIXED_AREA);
  clearFooter(BOT_FIXED_AREA);
  printFooter("<- Press button to start scan");

  #ifdef TESTING
    numberOfDevices = testData();
    scanDone = true;
    topDevice = fillFirstPage(numberOfDevices, BOT_FIXED_AREA);

  #endif

#if defined SERIAL_OUTPUT
  Serial.begin(115200);
  while (!Serial)
    ;
  Serial.println("\nWio Terminal I2C Scanner");
#endif
}

void loop()
{
  if (startPushed()) 
  {
    numberOfDevices = 0;
    numberOfDevices = scanBuss();
    
    if (numberOfDevices == 0){
      printFooter("No devices found");
    }
    else{
      String footerText;
      footerText = numberOfDevices;
      if (numberOfDevices > 1){
        footerText = footerText + " Devices Found";
      }
      else{
        footerText = footerText + " Device Found";
      }
      printFooter(footerText);
    }

    topDevice = fillFirstPage(numberOfDevices, BOT_FIXED_AREA);

#ifdef SERIAL_OUTPUT
    if (numberOfDevices == 0){
      Serial.println("No devices found");
    }
    else
    {
      Serial.print(numberOfDevices);
      if (numberOfDevices > 1){
        Serial.println(" devices found");
      }
      else
      {
        Serial.println(" device found");
      }
    }
#endif
  }

  if (scanDone && scrollUpPushed())
    scrollUp(scrollPointer, topDevice);

  if (scanDone && scrollDownPushed())
    scrollDown(scrollPointer, topDevice);
}

bool startPushed() // logic to trigger only one start for each button push. Uses falling edge detection
{
  static bool startDisabled; // static variable is saved between calls

  if (digitalRead(WIO_KEY_C) == LOW) // start button is pushed
  {
    if (startDisabled) // button has not been released since last detected push
    {  
       return false;
    }
    else
    {
      startDisabled = true; // we have detected a falling edge
      return true;      
    }
  }
  else{
    startDisabled = false; // start button is released so reset the falling edge detector
    return false;
  }  
}

bool scrollUpPushed() 
{
  if (digitalRead(WIO_5S_RIGHT) == LOW) // Wio terminal held with the right side up to allow hardware scrolling of the TFT
  { 
    return true;
  }
  else
    return false;
}

bool scrollDownPushed() 
{
  if (digitalRead(WIO_5S_LEFT) == LOW) // Wio terminal held with the right side up to allow hardware scrolling of the TFT
  { 
    return true;
  }
  else
    return false;
}

int scanBuss()
{
  uint8_t error, address, deviceNum;

#ifdef SERIAL_OUTPUT
  Serial.println ("Scanning ...");
#endif
  printFooter("Scanning...");
  scanDone = false;
  deviceNum = 0;
  for (address = SCAN_START; address <= SCAN_END; address++)
  {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if ((error == 0) || (error == 4))
    {
      device_t thisDevice;
      thisDevice.address = address;
      thisDevice.response = error;
      results[deviceNum] = thisDevice;
      deviceNum++;
    }
#ifdef SERIAL_OUTPUT
    serialPrintDevice(address, error);
#endif
  }
  scanDone = true;
  return deviceNum;
}

#ifdef SERIAL_OUTPUT
void serialPrintDevice(uint8_t address, uint8_t error)
{

  if (error == 0)
  {
      Serial.print ("Found device at: ");
      Serial.print (address, DEC);
      Serial.print (" (0x");
      Serial.print (address, HEX);
      Serial.println (")");
  }
  else if (error == 2)
  {
    Serial.print("Address NACK received at:");
      Serial.print (address, DEC);
      Serial.print (" (0x");
      Serial.print (address, HEX);
      Serial.println (")");
  }
  else if (error == 3)
  {
    Serial.print("Data NACK received at:");
      Serial.print (address, DEC);
      Serial.print (" (0x");
      Serial.print (address, HEX);
      Serial.println (")");
  }
  else if (error == 4)
  {
    Serial.print("Unknown error received at:");
      Serial.print (address, DEC);
      Serial.print (" (0x");
      Serial.print (address, HEX);
      Serial.println (")");
  }
}
#endif

void scrollUp(uint16_t &scrollPointer, uint16_t &topDevice)
{
   if(topDevice > 0)
  {
    scrollPointer = moveCircular(scrollPointer, TEXT_HEIGHT * -1, SCROLL_AREA_HEIGHT);
    scrollAddress(scrollPointer);
    topDevice--;
    clearLine(scrollPointer, TFT_BLACK);
    printDevice(topDevice, scrollPointer);
  }
  delay(SCROLL_DELAY);
}

void scrollDown(uint16_t &scrollPointer, uint16_t &topDevice)
{
   if(getBottomDevice() < numberOfDevices -1)
  {
    scrollPointer = moveCircular(scrollPointer, TEXT_HEIGHT, SCROLL_AREA_HEIGHT);
    scrollAddress(scrollPointer);
    topDevice++;
    clearLine(getBottomLineY(), TFT_BLACK);
    printDevice(getBottomDevice(), getBottomLineY());
  }
  delay(SCROLL_DELAY);
}

void setupScrollArea(uint16_t TFA, uint16_t BFA) {
    tft.startWrite(); // as of July 1, 2020 the Seeed_Ardunio_LCD version of TFT_eSPI has a bug in .writecommand and .writedata that requires this line as a fix
    tft.writecommand(ILI9341_VSCRDEF); // Vertical scroll definition
    tft.writedata(TFA >> 8);
    tft.writedata(TFA);
    tft.writedata((DISPLAY_HEIGHT - TFA - BFA) >> 8);
    tft.writedata(DISPLAY_HEIGHT - TFA - BFA);
    tft.writedata(BFA >> 8);
    tft.writedata(BFA);
    tft.endWrite(); // as of July 1, 2020 the Seeed_Ardunio_LCD version of TFT_eSPI has a bug in .writecommand and .writedata that requires this line as a fix
}

void scrollAddress(uint16_t VSP) // rotates the pixels in the non-fixed area of the display in a circular manner. Does not alter the data buffer! 
 {
    tft.startWrite();  // as of July 1, 2020 the Seeed_Ardunio_LCD version of TFT_eSPI has a bug in .writecommand that requires this line as a fix
    tft.writecommand(ILI9341_VSCRSADD); // Vertical scrolling start address
    tft.writedata(VSP >> 8);
    tft.writedata(VSP);
    tft.endWrite(); // as of July 1, 2020 the Seeed_Ardunio_LCD version of TFT_eSPI has a bug in .writecommand that requires this line as a fix
}

void clearFooter(uint16_t BFA)
{
  tft.fillRect(0,DISPLAY_HEIGHT-BFA,240,BFA, TFT_BLUE);
}

int fillFirstPage(int numberOfDevices, uint16_t BFA) // clears the data area of the display and displays the last 19 devices
{
  int firstDeviceToDisplay = numberOfDevices - 19;
  if (firstDeviceToDisplay  < 0) firstDeviceToDisplay  = 0;
  tft.setCursor(0,0);
  tft.fillRect(0,0,240,DISPLAY_HEIGHT-BFA, TFT_BLACK);
  
  for (int i=firstDeviceToDisplay ; i < numberOfDevices; i++){
    printlnDevice(i);
  }
  return firstDeviceToDisplay; // returns the zero based index of the device at the top of the display
}

void printlnDevice(int deviceNum) // prints the device information at the current cursor and moves the cursor to the next line
{
    printDevice(deviceNum, tft.getCursorY());
    tft.println ("");
}

void printDevice(int deviceNum,  uint16_t dispY) // prints the device information at the location specified
{
  tft.setCursor(0, dispY);
  tft.setTextColor(TFT_WHITE,TFT_BLACK);
  tft.print("Found device at: ");
  tft.print (results[deviceNum].address, DEC);
  tft.print (" (0x");
  tft.print (results[deviceNum].address, HEX);
  tft.print (")");
}

void clearLine(uint16_t dispY, uint16_t color)  //overwrites a line of text with a color. Each text line is 16 vertical pixels.
{ 
  uint8_t firstY;

  if (dispY > DISPLAY_HEIGHT - BOT_FIXED_AREA - TEXT_HEIGHT) return; // do nothing if a pixel line below the scrollable area is specified
  tft.fillRect(0,dispY,240,TEXT_HEIGHT, color);
}

uint16_t moveCircular(uint16_t start, int distanceToMove, uint16_t bufferSize) // moves a pointer backwards and forwards in a zero based circular buffer
{
  while(distanceToMove < 0) distanceToMove  = distanceToMove  + bufferSize; // Arduino modulo operator cannot handle negative numbers
  return (start + distanceToMove) % bufferSize; // buffersize wraps to 0, -1 wraps to buffersize -1
}

void printFooter(String textToPrint)
{
  uint16_t currX, currY;
  
  currX = tft.getCursorX(); // save the current cursor position
  currY = tft.getCursorY();
  tft.setCursor(0,DISPLAY_HEIGHT-BOT_FIXED_AREA);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  clearFooter(BOT_FIXED_AREA);
  tft.print(textToPrint);
  tft.setCursor(currX, currY); // restore the cursor

}

int testData()
{
  uint8_t address, deviceNum;
  deviceNum = 0;
  for (address = SCAN_START; address <= SCAN_END; address++)
  {
    device_t thisDevice;
    thisDevice.address = address;
    thisDevice.response = 0;
    results[deviceNum] = thisDevice;
    deviceNum++;
  }
  scanDone = true;
  printFooter("Test Data Loaded");
  return deviceNum;
}

uint16_t getBottomDevice()
{
  uint16_t returnValue;
  returnValue = topDevice + SCROLL_AREA_TEXT_LINES - 1;
  if(returnValue > numberOfDevices -1) returnValue = numberOfDevices - 1;
  return returnValue;
}

uint16_t getBottomLineY()
{
  return moveCircular(scrollPointer, TEXT_HEIGHT * -1, SCROLL_AREA_HEIGHT);

}