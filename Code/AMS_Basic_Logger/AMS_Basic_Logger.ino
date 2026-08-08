#include <EEPROM.h>
#include <SPI.h>
#include <SD.h>
#include "RTClib.h"

// CANBus
#include "mcp2515_can.h"
mcp2515_can CAN(9);
#define MAX_DATA_SIZE 8
int DeviceAddress = 1;
byte cdata[MAX_DATA_SIZE] = { 0 };

RTC_DS3231 rtc;
int TimeStamps = 0;

#define ComPort Serial
String inputString = "";  // a String to hold incoming data from ports
#define SDCS 4
File logfile;

void setup() {
  ComPort.begin(115200);
  ComPort.println("Starting up...");
  rtc.begin();

  CANBusSetup();
  SetupSDCard();
  ComPort.println("Finished Loading");
}

void loop() {
  CANBusRecieveCheck();
  serialEvent();
}

void serialEvent() {
  while (ComPort.available()) {
    char inChar = (char)ComPort.read();
    inputString += inChar;
    if (inChar == '\r') {
      //PainlessInstructionSet(inputString);
    }
  }
}


//----------------------------------------------------------------------------------------------------
//CAN Bus Functions
//----------------------------------------------------------------------------------------------------
void CANBusSetup() {
#if MAX_DATA_SIZE > 8
  /*
        To compatible with MCP2515 API,
        default mode is CAN_CLASSIC_MODE
        Now set to CANFD mode.
  */
  CAN.setMode(CAN_NORMAL_MODE);
#endif

  while (CAN_OK != CAN.begin(CAN_500KBPS)) {  // init can bus : baudrate = 500k
    ComPort.println("CAN init fail, retrying. This is unlikely to recover");
    delay(1000);
  }
  ComPort.println("CAN init ok!");
}

void CANBusRecieveCheck() {
  // check if data coming
  if (CAN_MSGAVAIL != CAN.checkReceive()) {
    return;
  }

  CAN.readMsgBuf(8, cdata);

  //type = (CAN.isExtendedFrame() << 0) | (CAN.isRemoteRequest() << 1);
  /*
       MCP2515(or this driver) could not handle properly
       the data carried by remote frame

       Displayed type:

       0x00: standard data frame
       0x02: extended data frame
       0x30: standard remote frame
       0x32: extended remote frame
  */


//  Serial.print("got some CAN Data:ID:");
//  Serial.print(CAN.getCanId());
//
//  Serial.print(" Data:");
//  for (uint8_t i = 0; i < 8; i++) {
//    Serial.print(i);
//    Serial.print(": ");
//    Serial.print(cdata[i], HEX);
//    Serial.print(",");
//  }
//  Serial.println();

  int ID = CAN.getCanId();
  int CommandNumber = cdata[1];
  switch (CommandNumber) {
    case 1:  //State
      SensorParsing(ID, cdata[2], cdata[5], cdata[3], cdata[4]);
      break;
    default:
      String Message = "Raw CANBus," + String(ID) + ",";
      for (uint8_t i = 0; i < 8; i++) {
        Message += String(cdata[i]);
      }
      OutputAndMaybeLogIt(Message);
      break;
  }
}

//void CanBusSend(byte Zero, byte One, byte Two, byte Three, byte Four, byte Five, byte Six, byte Seven) {
//  byte DataPacket[8] = { Zero, One, Two, Three, Four, Five, Six, Seven };  //construct data packet array
//  CAN.sendMsgBuf(PacketIdentifier, 0, 8, DataPacket);
//}
//----------------------------------------------------------------------------------------------------
//End of CAN Bus Functions
//----------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------
//Parsing Functions
//----------------------------------------------------------------------------------------------------
void SensorParsing(int ID, int ChannelNumber, int DeviceType, int UpperValue, int LowerValue ) {
  String Message = "Sensor," + String(ID);
  int Value = UpperValue << 8 | LowerValue;

  switch (DeviceType) {
    case 1:  //Current
      Message += ",Current," + String(ChannelNumber) + "," + String(double(Value) / 10.0);
      break;
    case 2:  //Temp
      Message += ",Temperature," + String(ChannelNumber) + "," + String(double(Value) / 10.0);
      break;
    case 3:  //Votlage
      Message += ",Votlage," + String(ChannelNumber) + "," + String(double(Value) / 10.0);
      break;
    case 4:  //Pressure
      Message += ",Pressure," + String(ChannelNumber) + "," + String(double(Value) / 10.0);
      break;
    case 5:  //Vacuum
      Message += ",Vacuum," + String(ChannelNumber) + "," + String(double(Value) / 10.0);
      break;
    case 6:  //IO
      Message += ",IO," + String(ChannelNumber) + "," + String(Value);
      break;
    case 7:  //RPM
      Message += ",RPM," + String(ChannelNumber) + "," + String(Value);
      break;
    default:
      Message += "," + String(ChannelNumber) + "," + String(DeviceType) + "," + String(Value) + ",Not Supported";
      break;
  }

  OutputAndMaybeLogIt(Message);
}

//----------------------------------------------------------------------------------------------------
//End of Parsing Functions
//----------------------------------------------------------------------------------------------------

String GetCurrentTime() {
  DateTime now = rtc.now();
  char buf1[20];
  sprintf(buf1, "%02d:%02d:%02d-%02d/%02d/%02d",  now.hour(), now.minute(), now.second(), now.day(), now.month(), now.year());
  return buf1;
}

//----------------------------------------------------------------------------------------------------
//Logging and Output Functions
//----------------------------------------------------------------------------------------------------
void SetupSDCard() {
  if (SD.begin(SDCS)) {
    String filename = "AMS0000.LOG";
    for (uint8_t i = 0; i < 10000; i++) {
      // create if does not exist, do not open existing, write, sync after write
      if (!SD.exists(filename)) {
        break;
      } else {
        String WhatToPutIn = "";
        if (i < 10) {
          WhatToPutIn = "000" + String(i);
        }
        if (i >= 10 && i < 100) {
          WhatToPutIn = "00" + String(i);
        }
        if (i >= 100 && i < 1000) {
          WhatToPutIn = "0" + String(i);
        }
        if (i >= 1000 && i < 10000) {
          WhatToPutIn = String(i);
        }
        filename = "AMS" + WhatToPutIn + ".LOG";  //update the file name
      }
    }
    logfile = SD.open(filename, FILE_WRITE);
    ComPort.println("Log File," + filename + ",Created");
  } else {
    ComPort.println("Card init. failed! NOT LOGGING TO FILE");
  }
}

void FileSizeCheck() {

  if (logfile.size() > 5242880 ) { // 5MB in bytes
    logfile.close();
    SetupSDCard();
  }
}

void OutputAndMaybeLogIt(String Data) {
  if (TimeStamps == 1) {
    logfile.println(Data + "," + GetCurrentTime());
    logfile.flush();
    FileSizeCheck();
    ComPort.println(Data + "," + GetCurrentTime());
  } else {
    logfile.println(Data);
    logfile.flush();
    FileSizeCheck();
    ComPort.println(Data);
  }
}
//----------------------------------------------------------------------------------------------------
//End of Logging and Output Functions
//----------------------------------------------------------------------------------------------------
