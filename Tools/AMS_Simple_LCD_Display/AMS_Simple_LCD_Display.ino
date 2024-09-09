#include <LiquidCrystal.h>

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

#define ComPort Serial
String inputString = "";  // a String to hold incoming data from ports
#define MaxDisplayPages 2

String Fuel = "N/A";
String Oil = "N/A";
String Current = "N/A";
String LeftTemp = "N/A";
String RightTemp = "N/A";
String FrontTemp = "N/A";
long LastPostedVsensor, LastPostedTemps, DisplayTimer = 0;
int CurrentDisplayPage = 0;

void setup() {
  ComPort.begin(115200);
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  // Print a message to the LCD.
  lcd.print("hello, world!");


}

void loop() {
  /*
     Need to read the serial port and parse that data and dump to the screen
     save that last posted data and put a time stamp
     check the timestamp if it's older than 10 seconds mark as "NA"

  */

  long CurrentTime = millis();
  if (abs(DisplayTimer - CurrentTime) > 3000) {
    lcd.clear();
    switch (CurrentDisplayPage) {
      case 0:
        lcd.print("Fuel:" + Fuel);
        lcd.setCursor(0, 1);
        lcd.print("Oil: " + Fuel);
        break;
      case 1:
        lcd.print("Amps: " + Current);
        lcd.setCursor(0, 1);
        lcd.print("Front:" + FrontTemp);
        break;
      case 2:
        lcd.print("Right:" + RightTemp);
        lcd.setCursor(0, 1);
        lcd.print("Left: " + LeftTemp);
        break;
      default:
        lcd.print("Screen Not Available");
        break;
    }

    CurrentDisplayPage++;
    if (CurrentDisplayPage > MaxDisplayPages) {
      CurrentDisplayPage = 0;
    }

    DisplayTimer = CurrentTime;
  }

serialEvent();
}

//SensorParsing,ID,Current,ChannelNumber,Value\r
//SensorParsing,100,Current,1,12.3\r

//SensorParsing,ID,Temperature,ChannelNumber,Value\r
//SensorParsing,200,Temperature,0,102.3\r

void serialEvent() {
  int FoundCurrent = 0; 
  int FoundTemp = 0; 
  int FoundPressure = 0; 
  
  while (ComPort.available()) {
    char inChar = (char)ComPort.read();
    inputString += inChar;
    if (inChar == '\r') {
       FoundCurrent = inputString.indexOf(",Current,");
       FoundTemp = inputString.indexOf(",Temperature,");
       FoundPressure = inputString.indexOf(",Pressure,");
       if (FoundCurrent != -1){
        
       }

       if (FoundTemp != -1){
        
       }

       if (FoundPressure != -1){
        
       }
       
    }
  }
}
