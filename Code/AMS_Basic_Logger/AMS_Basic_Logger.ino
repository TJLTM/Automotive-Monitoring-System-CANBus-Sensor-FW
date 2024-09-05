#include <EEPROM.h>
#include <SPI.h>
#include <SD.h>

//CAN_2515 or CAN_2518FD
// #include "mcp2518fd_can.h"
// mcp2518fd CAN(SPI_CS_PIN);  // Set CS pin
// // To TEST MCP2518FD CAN2.0 data transfer
// #define MAX_DATA_SIZE 8
// // To TEST MCP2518FD CANFD data transfer, uncomment below lines
// // #define MAX_DATA_SIZE 64

// #include "mcp2515_can.h"
// mcp2515_can CAN(SPI_CS_PIN);

// CANBus
#include "mcp2515_can.h"
mcp2515_can CAN(9);
#define MAX_DATA_SIZE 8
int DeviceAddress = 1;
//uint32_t id;
//uint8_t type;  // bit0: ext, bit1: rtr
//const uint8_t len;
byte cdata[MAX_DATA_SIZE] = { 0 };


#define ComPort Serial
#define SDCS 4
bool PrintAll = false;
File logfile;

void setup() {
  ComPort.begin(115200);
  ComPort.println("Starting up...");
  CANBusSetup();
  SetupSDCard();
  ComPort.println("Finished Loading");
}

void loop() {
  CANBusRecieveCheck();
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
  ComPort.println(F("CAN init ok!"));
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

  if (PrintAll == true) {
    Serial.print("got some CAN Data:ID:");
    Serial.print(CAN.getCanId());

    Serial.print(" Data:");
    for (uint8_t i = 0; i < 8; i++) {
      Serial.print(i);
      Serial.print(": ");
      Serial.print(cdata[i], HEX);
      Serial.print(",");
    }
    Serial.println();
  }

  int ID = CAN.getCanId();
  int CommandNumber = cdata[0];
  int OtherData = cdata[1];
//  Serial.print("CommandNumber:");
//  Serial.print(CommandNumber);
//  Serial.print("  ::OtherData:");
//  Serial.println(OtherData);
  switch (CommandNumber) {
    case 1:  //State
      SensorParsing(ID, cdata[1], cdata[2] << 8 | cdata[3], cdata[4]);
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
//----------------------------------------------------------------------------------------------------
//End of CAN Bus Functions
//----------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------
//Parsing Functions
//----------------------------------------------------------------------------------------------------
void SensorParsing(int ID, int ChannelNumber, int Value, int DeviceType) {
  String Message = "SensorParsing," + String(ID);
  double HRValue = double(Value);
  switch (DeviceType) {
    case 1:  //Current
      Message += ",Current," + String(ChannelNumber) + "," + String(HRValue / 10.0);
      break;
    case 2:  //Temp
      Message += ",Temperature,"  + String(ChannelNumber) + "," + String(HRValue / 10.0);
      break;
    case 3:  //Votlage
      Message += ",Votlage," + String(ChannelNumber) + "," + String(DeviceType) + ",Not Supported";
      break;
    case 4:  //Pressure
      Message += ",Pressure," + String(ChannelNumber) + "," + String(HRValue / 100.0);
      break;
    case 5:  //Vacuum
      Message += ",Vacuum," + String(ChannelNumber) + "," + String(DeviceType) + ",Not Supported";
      break;
    case 6:  //IO
      Message += ",IO," + String(ChannelNumber) + "," + String(DeviceType) + ",Not Supported";
      break;
    case 7:  //RPM
      Message += ",RPM," + String(ChannelNumber) + "," + String(DeviceType) + ",Not Supported";
      break;
    default:
      Message += "," + String(ChannelNumber) + "," + String(DeviceType) + "," +String(Value) + ",Not Supported";
      break;
  }

  OutputAndMaybeLogIt(Message);
}

//----------------------------------------------------------------------------------------------------
//End of Parsing Functions
//----------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------
//Logging and Output Functions
//----------------------------------------------------------------------------------------------------
void SetupSDCard() {
  if (SD.begin(SDCS)) {
    String filename = "AMS0000.LOG";
    for (uint8_t i = 0; i < 10000; i++) {
      // create if does not exist, do not open existing, write, sync after write
      if (! SD.exists(filename)) {
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
        filename = "AMS" + WhatToPutIn + ".LOG"; //update the file name
      }

    }
    logfile = SD.open(filename, FILE_WRITE);
    ComPort.println("Log File," + filename + ",Created");
  } else {
    ComPort.println("Card init. failed! NOT LOGGING TO FILE");
  }
}

void OutputAndMaybeLogIt(String Data) {
  if (logfile) {
    logfile.println(Data);
  }
  ComPort.println(Data);
}
//----------------------------------------------------------------------------------------------------
//End of Logging and Output Functions
//----------------------------------------------------------------------------------------------------
