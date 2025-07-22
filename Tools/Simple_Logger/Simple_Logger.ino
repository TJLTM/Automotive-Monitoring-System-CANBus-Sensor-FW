/*
Simple AMS CANBus logger using the SeeedStudio CANBus Shield v2.0
*/

#include <SPI.h>
#include <SD.h>

File logfile;
const int chipSelect = 4;

// CANBus
#include "mcp2515_can.h"
mcp2515_can CAN(9);
#define MAX_DATA_SIZE 8
byte cdata[MAX_DATA_SIZE] = { 0 };


void setup() {
  Serial.begin(115200);
  Serial.print("\nInitializing SD card...");

  if (!SD.begin(chipSelect)) {
    Serial.println("Card init. failed!");
  }
  char filename[15];
  strcpy(filename, "AMSLOG00.TXT");
  for (uint8_t i = 0; i < 100; i++) {
    filename[6] = '0' + i / 10;
    filename[7] = '0' + i % 10;
    // create if does not exist, do not open existing, write, sync after write
    if (!SD.exists(filename)) {
      break;
    }
    Serial.print("\nInitializing CAN Bus...");
    CANBusSetup();
  }

  logfile = SD.open(filename, FILE_WRITE);
  if (!logfile) {
    Serial.print("Couldnt create ");
    Serial.println(filename);
  }
  Serial.print("Writing to ");
  Serial.println(filename);
}

void loop() {
  //check canbus for anyinformation
  String AMS_Data = CANBusRecieveCheck();

  if (AMS_Data != "") {
    LogIt(AMS_Data);
  }
}

void LogIt(String Info) {
  Serial.println(Info);
  logfile.println(Info);
  logfile.flush();
}

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
    Serial.println("CAN init fail, retrying. This is unlikely to recover");
    delay(1000);
  }
  Serial.println(F("CAN init ok!"));
}

String CANBusRecieveCheck() {
  String DataToReturn = "";

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
  int ID = CAN.getCanId();
  Serial.print("got some CAN Data:ID:");
  Serial.print(ID);

  Serial.print(" Data:");
  for (uint8_t i = 0; i < 8; i++) {
    Serial.print(i);
    Serial.print(": ");
    Serial.print(cdata[i], HEX);
    Serial.print(",");
  }
  Serial.println();

  switch (cdata[1]) {
    case 1:  //status
      break;

    case 4:  //units
      break;

    case 6:  //Error
      break;

    case 10:  //Device Temp
      break;

    case 17:  //Sensor Channel Type
      break;

    default:
      break;
  }

  return DataToReturn;
}