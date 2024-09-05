#include <EEPROM.h>
#include <SPI.h>
#include <Adafruit_MAX31865.h>

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
int DeviceAddress = 200;
//uint32_t id;
//uint8_t type;  // bit0: ext, bit1: rtr
//const uint8_t len;
byte cdata[MAX_DATA_SIZE] = { 0 };

//Device Configuration setting
long PacingTimer;
#define DeviceType 2
#define MaxChannelNumber 2
int SensorMin[] = { -40, -40, -40};
int SensorMax[] = {650, 650, 650};
Adafruit_MAX31865 Channel0 = Adafruit_MAX31865(3);
Adafruit_MAX31865 Channel1 = Adafruit_MAX31865(4);
Adafruit_MAX31865 Channel2 = Adafruit_MAX31865(5);
Adafruit_MAX31865 *RTDPointers [3] = {&Channel0, &Channel1, &Channel2};
#define RREF      430.0
#define RNOMINAL  100.0

uint8_t ErrorNumber = 0;
char UNITS = 'I';

#define ComPort Serial
String inputString = "";  // a String to hold incoming data from ports

const char *const AcceptedCommands[] = {
  "UNITS?",
  "ERROR?",
  "STREAMING?",
  "UNITSYSTEM?",
  "RESETERROR",
  "REBOOT",
  "PACING?",
};

const char *ParameterCommands[] = {
  "SETUNITSYSTEM",
  "SETSTREAMING",
  "SETPACINGTIME",
  "SETDEVICEADDRESS"
};


//Function Prototypes
//void CanBusSend(int DataLength, byte Zero, byte One=0x00, byte Two=0x00, byte Three=0x00, byte Four=0x00, byte Five=0x00, byte Six=0x00, byte Seven=0x00);

void setup() {
  ComPort.begin(115200);
  GetDeviceAddressFromMemory();
  CANBusSetup();

  ComPort.print("UnitSystem:");
  ComPort.println(GetUnitSystemFromMemory());

  ComPort.print("Streaming:");
  ComPort.println(GetStreamingFromMemory());

  ComPort.print("Pacing:");
  ComPort.println(GetPacingTimeFromMemory());

  DiscoveryResponse();
  /*
      Please Reference Adafruit_MAX31865
      docs for setup of these boards if
      you are going to use 4 or 2 wire
  */
  Channel0.begin(MAX31865_3WIRE);
  Channel1.begin(MAX31865_3WIRE);
  Channel2.begin(MAX31865_3WIRE);
}

void loop() {
  CANBusRecieveCheck();

  long CurrentTime = millis();
  if (GetStreamingFromMemory() == 1) {
    if (abs(PacingTimer - CurrentTime) > GetPacingTimeFromMemory()) {
      for (uint8_t i = 0; i <= MaxChannelNumber; i++) {
        StatusResponse(i);
      }
      PacingTimer = CurrentTime;
    }
  }

  serialEvent();
}

//----------------------------------------------------------------------------------------------------
//Serial Port Handling Functions
//----------------------------------------------------------------------------------------------------
void SendSerial(String Data, bool CR = true) {
  /*
    :param Data: String to be sent out of the Serial Port
    :type Data: String
    :return: None
    :rtype: None
  */
  if (CR == true) {
    ComPort.println(Data);
  } else {
    ComPort.print(Data);
  }
}

void serialEvent() {
  while (ComPort.available()) {
    char inChar = (char)ComPort.read();
    inputString += inChar;
    if (inChar == '\r') {
      PainlessInstructionSet(inputString);
    }
  }
}

/*
  SCC = start command character
  case 1 - no SCC found and there is data in the buffer - dump the buffer
  case 2 - SCC is found and not at position 0 - trim the buffer up to the SCC and insert error
  case 3 - SCC is found and at position 0  - process command
  case 4 - SCC is found and no delimiter found and there is data in the buffer  - add back to the buffer
  case 5 - SCC is found no delimiter found and another scc is found trim up to the second
  case 6 - No SCC and No Delimiter and there is data in teh buffer - dump the buffer
  case 7 - Valid SSC and Delimiter is found but the command is not in the list of commands - tell the user
*/

String PainlessInstructionSet(String &TestString) {
  int Search = 1;
  while (Search == 1) {
    bool ParamCommandCalled = false;
    bool CommandCalled = false;
    int FindStart = TestString.indexOf('%');
    int Param = TestString.indexOf('*');
    int FindEnd = TestString.indexOf('\r');
    if (TestString != "") {
      if (FindStart != -1) {   //case 1
        if (FindStart != 0) {  //case 2
          //Serial.println("PIS Case 2");
          SendSerial("%R,Error,BAD Command Format No Start or Stop Delimiters");
          TestString.remove(0, FindStart);
        } else {  //Case 3 & Case 5 & Case 4
          String Case5Test = TestString.substring(FindStart + 1);
          int FindStart1 = Case5Test.indexOf('%');
          int FindEnd1 = Case5Test.indexOf('\r');
          if ((FindEnd1 > FindStart1) && (FindStart1 != -1)) {
            SendSerial("%R,Error,BAD Command Format - No End Delimiter");
            //Serial.println("PIS Case 5");
            TestString.remove(0, FindStart1 + 1);
          } else {
            if (FindEnd != -1 || FindEnd1 != -1) {
              //Serial.println("PIS Case 3");
              String CommandCandidate = TestString.substring(FindStart + 1, FindEnd);
              CommandCandidate.toUpperCase();
              if ((Param < FindEnd) && Param != -1) {
                for (int i = 0; i < (sizeof(ParameterCommands) / sizeof(int)); i++) {
                  //Serial.println("PIS Case 3A");
                  String ParamHeader = CommandCandidate.substring(FindStart, Param - 1);
                  ParamHeader.toUpperCase();
                  if (ParamHeader == ParameterCommands[i]) {
                    ParamCommandToCall(i, CommandCandidate);
                    ParamCommandCalled = true;
                    break;
                  }
                }
              } else {
                for (int i = 0; i < (sizeof(AcceptedCommands) / sizeof(int)); i++) {
                  //Non Parameter Commands
                  if (CommandCandidate == AcceptedCommands[i]) {
                    //Serial.println("PIS Case 3B");
                    CommandToCall(i);
                    CommandCalled = true;
                    break;
                  }
                }
              }
              if (CommandCalled == false && ParamCommandCalled == false) {
                //Serial.println("PIS Case 7");
                SendSerial("%R,Error,Command not recognized");
              }
              TestString.remove(0, FindEnd + 1);
            } else {
              //Serial.println("PIS Case 4");
              Search = 0;
            }
          }
        }
      } else {  //Case 1 Dump the buffer if ther is no start character is found
        //Serial.println("PIS Case 1");
        Search = 0;
        TestString = "";
        SendSerial("%R,Error,BAD Command Format - No Start Command Character");
      }
    }       //if TestString is empty
    else {  //Exit Search While if Buffer is empty
      Search = 0;
      //Serial.println("PIS Case 6");
    }
  }  //End of Search While
  return TestString;
}  //End of PIS Function

void ParamCommandToCall(int Index, String CommandRaw) {
  int ParamDelimIndex = CommandRaw.indexOf("*");
  int End = CommandRaw.indexOf("\r");
  String ThingToTest = CommandRaw.substring(ParamDelimIndex + 1, End - 1);

  switch (Index) {
    case 0:
      //SETUNITSYSTEM
      if (ThingToTest == "I" || ThingToTest == "M") {
        char FilteredCommand = 'I';
        if (ThingToTest == "M") {
          FilteredCommand = 'M';
        }
        UnitsSystemSet(-1, FilteredCommand);
      } else {
        SendSerial("%R,Error,Invalid Parameter, I or M");
      }
      break;
    case 1:
      //SETSTREAMING
      if (ThingToTest == "0" || ThingToTest == "1") {
        StreamingModeSet(-1, ThingToTest.toInt());
      } else {
        SendSerial("%R,Error,Invalid Parameter, 0 or 1");
      }
      break;
    case 2:
      //SETPACINGTIME

      PacingSet(-1, ThingToTest.toInt());
      break;
    case 3:
      //SETDEVICEADDRESS
      SetDeviceAddress(ThingToTest.toInt());
      break;
  }
}

void CommandToCall(int Index) {
  switch (Index) {
    case 0:
      //UNITS?
      UnitsABRResponse(-1);
      break;
    case 1:
      //ERROR?
      GetError(-1);
      break;
    case 2:
      //STREAMING?
      StreamingModeResponse(-1);
      break;
    case 3:
      //UNITSYSTEM?
      UnitsSystemResponse(-1);
      break;
    case 4:
      //RESETERROR
      SendSerial("Error Reset:0x05");
      ResetError(-1);
      break;
    case 5:
      //REBOOT
      SendSerial("Rebooting:0x06");
      RebootDevice(-1);
      break;
    case 6:
      //PACING?
      PacingResponse(-1);
      break;
  }
}
//----------------------------------------------------------------------------------------------------
//End Of Serial Port Handling Functions
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
//System related functions
//----------------------------------------------------------------------------------------------------
void (*resetFunc)(void) = 0;  // declare reset fuction at address 0

bool WPacingValueCheck(int Value) {
  Serial.println("PacingValueCheck");
  Serial.println(Value);
  if (Value >= 250 && Value <= 65535) {
    Serial.println("asdf");
    return true;
  } else {
    SendSerial("%R,Error,Invalid Parameter 250 <= x <= 65535");
    return false;
  }
}

void SetError(int Number, int CommandNumber) {
  ErrorNumber = Number;

}

void ResetError(int ReplyToAddress) {
  ErrorNumber = 0;
  GetError(ReplyToAddress);
}
//----------------------------------------------------------------------------------------------------
//End Of System related functions
//----------------------------------------------------------------------------------------------------

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

  //Check if Discovery do not filter by ID

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

  if ((CAN.getCanId() > 9 && CAN.getCanId() < 20) && (cdata[0] == 0x00 && cdata[1] == 0x3F && cdata[2] == 0x00 && cdata[3] == 0xFF && cdata[4] == 0x00 && cdata[5] == 0xFF && cdata[6] == 0x00 && cdata[7] == 0xFF)) {
    DiscoveryResponse();
  } //else {
  //    //Check if this is the target device
  //    if (CAN.getCanId() == DeviceAddress){
  //      int CommandNumber = cdata[0];
  //      int OtherData = cdata[1];
  //      Serial.print("CommandNumber:");
  //      Serial.print(CommandNumber);
  //      Serial.print("  ::OtherData:");
  //      Serial.println(OtherData);
  //      switch (CommandNumber) {
  //        case 1:  //State
  //          StatusResponse(OtherData);
  //          break;
  //        case 3:  //Streaming
  //          if (OtherData == '?') {
  //            StreamingModeResponse();
  //          }
  //          if (OtherData == 'S') {
  //            if (cdata[4] == 0x00) {
  //              StreamingModeSet(ReplyAddress, false);
  //            }
  //            if (cdata[4] == 0xFF) {
  //              StreamingModeSet(ReplyAddress, true);
  //            }
  //          }
  //          break;
  //        case 4:  //Pacing
  //          if (WhatKindOfCommand == '?') {
  //            PacingResponse(ReplyAddress);
  //          }
  //          if (WhatKindOfCommand == 'S') {
  //            int Data = cdata[4] << 8 + cdata[5];
  //            PacingSet(ReplyAddress, Data);
  //          }
  //          break;
  //        case 5:  //Unit System
  //          if (WhatKindOfCommand == '?') {
  //            UnitsSystemResponse(ReplyAddress);
  //          }
  //          if (WhatKindOfCommand == 'S') {
  //            UnitsSystemSet(ReplyAddress, cdata[4]);
  //          }
  //          break;
  //        case 6:  //I/O
  //          if (WhatKindOfCommand == 'S') {
  //            IOSet(ReplyAddress, cdata[4], cdata[5]);
  //          }
  //          break;
  //        case 7:  //Error?
  //          if (WhatKindOfCommand == '?') {
  //            GetError(ReplyAddress);
  //          }
  //          break;
  //        case 8:  //Unit ABR
  //          if (WhatKindOfCommand == '?') {
  //            UnitsABRResponse(ReplyAddress);
  //          }
  //          break;
  //        case 9:  //Error Reset
  //          if (WhatKindOfCommand == 'S') {
  //            ResetError(ReplyAddress);
  //          }
  //          break;
  //        case 10:  //Reboot
  //          if (WhatKindOfCommand == 'S') {
  //            RebootDevice(ReplyAddress);
  //          }
  //          break;
  //        case 11:  //Device Temp
  //          if (WhatKindOfCommand == '?') {
  //            DeviceTemp(ReplyAddress);
  //          }
  //          break;
  //        case 12:  //Max Sensor Channel
  //          if (WhatKindOfCommand == '?') {
  //            MaxSensorChannel(ReplyAddress);
  //          }
  //          break;
  //        case 13:  //Sensor Channel Range Max
  //          if (WhatKindOfCommand == '?') {
  //            MaxSensorChannelRange(ReplyAddress, cdata[4]);
  //          }
  //          break;
  //        case 14:  //Sensor Channel Range Min
  //          if (WhatKindOfCommand == '?') {
  //            MinSensorChannelRange(ReplyAddress, cdata[4]);
  //          }
  //          break;
  //        default:
  //          ErrorNumber = 0x01;
  //          GetError(ReplyAddress);
  //          ErrorNumber = 0x00;
  //          break;
  //      }
  //    }
  //  }
}

void CanBusSend(int PacketIdentifier, int DataLength, byte Zero, byte One, byte Two, byte Three, byte Four, byte Five, byte Six, byte Seven) {
  // ID, ext, len, byte: data
  //ext = 0 for standard frame
  byte DataPacket[8] = { Zero, One, Two, Three, Four, Five, Six, Seven };  //construct data packet array
  CAN.sendMsgBuf(PacketIdentifier, 0, DataLength+1, DataPacket);
}
//----------------------------------------------------------------------------------------------------
//End Of CAN Bus Functions
//----------------------------------------------------------------------------------------------------


//----------------------------------------------------------------------------------------------------
//EEPROM Functions
//----------------------------------------------------------------------------------------------------
void SetDeviceAddress(int Address) {
  if (Address >= 100 && Address <= 2047) {
    EEPROM.update(1, highByte(Address));
    EEPROM.update(0, lowByte(Address));
    GetDeviceAddressFromMemory();
  } else {
    SendSerial("Address must be between 100 and 2047");
  }
}

void GetDeviceAddressFromMemory() {
  DeviceAddress = EEPROM.read(1) << 8 | EEPROM.read(0);
  SendSerial("CAN Bus Address: " + String(DeviceAddress));
}

char GetUnitSystemFromMemory() {
  //Read Units out of EEPROM
  char TempValue = EEPROM.read(4);
  if (TempValue != 'I' || TempValue != 'M') {
    EEPROM.update(4, 'I');
  }
  return TempValue;
}

int GetStreamingFromMemory() {
  //Read Stream Value out of EEPROM
  int TempValue = EEPROM.read(5);
  if (TempValue < 0 || TempValue > 1) {
    EEPROM.update(5, 0);
    TempValue = 0;
  }
  return TempValue;
}

unsigned int GetPacingTimeFromMemory() {
  //Read Pacing value out of EEPROM
  //  unsigned int Value = EEPROM.read(3) << 8 || EEPROM.read(2);
  //  if (Value > 250 && Value < 65535) {
  //     EEPROM.update(3, highByte(250));
  //     EEPROM.update(2, lowByte(250));
  //  }
  return 250;
}
//----------------------------------------------------------------------------------------------------
// End Of EEPROM Functions
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
//General API Functions
//----------------------------------------------------------------------------------------------------
void DiscoveryResponse() {
  /*
    :param ReplyToAddress: reply address to put into the CAN packet header
    :type ReplyToAddress: int
    :return: None
    :rtype: None
  */
  CanBusSend(byte(DeviceAddress), 7, 0x00, 0xFF, byte(DeviceType), byte(DeviceType), byte(DeviceType), byte(GetUnitSystemFromMemory()), byte(GetUnitSystemFromMemory()), byte(GetUnitSystemFromMemory()));
}

void GetError(int ReplyToAddress) {
  /*
    :param ReplyToAddress: reply address to put into the CAN packet header
    :type ReplyToAddress: int
    :return: None
    :rtype: None
  */
  if (ReplyToAddress != -1) {
    CanBusSend(DeviceAddress, 5, highByte(ReplyToAddress), lowByte(ReplyToAddress), byte("R"), 0x07, byte(ErrorNumber), 0x00, 0x00, 0x00);
  } else {
    SendSerial("Error:0x07:" + String(ErrorNumber));
  }
}

void RebootDevice(int ReplyToAddress) {
  CanBusSend(DeviceAddress, 7, highByte(ReplyToAddress), lowByte(ReplyToAddress), byte("R"), 0x10, 0xFF, 0x10, 0xFF, 0x10);
  SendSerial("Device is Rebooting");
  delay(2500);
  resetFunc();
}

void StatusResponse(int ChannelNumber) {
  /*
    :param ReplyToAddress: reply address to put into the CAN packet header, defaults to -1
    :type ReplyToAddress: int
    :return: None
    :rtype: None
  */
  if (ChannelNumber >= 0 && ChannelNumber <= MaxChannelNumber) {

    int ReturnedValue = SensorCode(ChannelNumber); // value returned will be an int for a fixed point number

    CanBusSend(DeviceAddress, 4, 0x01, byte(ChannelNumber), highByte(ReturnedValue), lowByte(ReturnedValue), byte(DeviceType), 0x00, 0x00, 0x00);
    SendSerial("StatusResponse:0x01:" + String(ChannelNumber) + ":" + String(ReturnedValue) + ":" + String(DeviceType));
  } else {
    // return error that channel doesn't exist
    ErrorNumber = 3;
    SendSerial("Error:0x01" + ErrorNumber);
    ErrorNumber = 0;
  }
}

void StreamingModeResponse(int ReplyToAddress) {
  /*
    :param ReplyToAddress: reply address to put into the CAN packet header
    :type ReplyToAddress: int
    :return: None
    :rtype: None
  */
  if (ReplyToAddress != -1) {
    CanBusSend(DeviceAddress, 5, highByte(ReplyToAddress), lowByte(ReplyToAddress), byte("R"), 0x03, byte(GetStreamingFromMemory()), 0x00, 0x00, 0x00);
  } else {
    SendSerial("StreamingMode:0x03:" + String(GetStreamingFromMemory()));
  }
}

void StreamingModeSet(int ReplyToAddress, int Data) {
  /*
    :param ReplyToAddress: reply address to put into the CAN packet header, defaults to -1
    :type ReplyToAddress: int
    :return: None
    :rtype: None
  */
  Serial.print("StreamingModeSet");
  Serial.println(Data);
  if (Data == 0 || Data == 1) {
    EEPROM.update(5, Data);
  } else {
    Serial.print("Error");
  }
  StreamingModeResponse(ReplyToAddress);
}

void PacingResponse(int ReplyToAddress) {
  /*
    :param ReplyToAddress: reply address to put into the CAN packet header, defaults to -1
    :type ReplyToAddress: int
    :return: None
    :rtype: None
  */
  if (ReplyToAddress != -1) {
    CanBusSend(DeviceAddress, 6, highByte(ReplyToAddress), lowByte(ReplyToAddress), byte("R"), 0x04, highByte(GetPacingTimeFromMemory()), lowByte(GetPacingTimeFromMemory()), 0x00, 0x00);
  } else {
    SendSerial("Pacing:0x04:" + String(GetPacingTimeFromMemory()));
  }
}

void PacingSet(int ReplyToAddress, int Data) {
  /*
    :param ReplyToAddress: reply address to put into the CAN packet header
    :type ReplyToAddress: int
    :return: None
    :rtype: None
  */
  if (Data >= 250 && Data <= 65535) {
    EEPROM.update(3, highByte(Data));
    EEPROM.update(2, lowByte(Data));
  }
  PacingResponse(ReplyToAddress);
}

void UnitsSystemResponse(int ReplyToAddress) {
  /*
    :param ReplyToAddress: reply address to put into the CAN packet header
    :type ReplyToAddress: int
    :return: None
    :rtype: None
  */

  if (ReplyToAddress != -1) {
    CanBusSend(DeviceAddress, 5, highByte(ReplyToAddress), lowByte(ReplyToAddress), byte("R"), 0x05, byte(GetUnitSystemFromMemory()), 0x00, 0x00, 0x00);
  } else {
    SendSerial("UnitsSystem:0x05:" + String(GetUnitSystemFromMemory()));
  }
}

void UnitsSystemSet(int ReplyToAddress, char Data) {
  /*
    :param ReplyToAddress: reply address to put into the CAN packet header
    :type ReplyToAddress: int
    :return: None
    :rtype: None
  */

  if (Data == 'I' || Data == 'M') {
    EEPROM.update(4, Data);
  }
  UnitsSystemResponse(ReplyToAddress);
}

void UnitsABRResponse(int ReplyToAddress) {
  /*

    :param ReplyToAddress: reply address to put into the CAN packet header
    :type ReplyToAddress: int
    :return: None
    :rtype: None
  */
  byte ABR = 0x00;
  switch (DeviceType) {
    case 1:  // Current
      ABR = 0x01;
      break;
    case 2:  // Temp
      if (GetUnitSystemFromMemory() == 'I') {
        ABR = 0x03;
      } else {
        ABR = 0x02;
      }
      break;
    case 3:  // Voltage
      ABR = 0x04;
      break;
    case 4:  // Pressure
      if (GetUnitSystemFromMemory() == 'I') {
        ABR = 0x06;
      } else {
        ABR = 0x05;
      }
      break;
    case 5:  // Vacuum
      if (GetUnitSystemFromMemory() == 'I') {
        ABR = 0x08;
      } else {
        ABR = 0x07;
      }
      break;
    case 6:  // I/O
      ABR = 0x09;
      break;
    case 7:  // RPM
      ABR = 0x0A;
      break;
  }

  if (ReplyToAddress != -1) {
    CanBusSend(DeviceAddress, 4, highByte(ReplyToAddress), lowByte(ReplyToAddress), byte("R"), 0x08, ABR, 0x00, 0x00, 0x00);
  } else {
    SendSerial("UnitABR:0x08:" + String(ABR));
  }
}

void DeviceTemp(int ReplyToAddress) {
  float Resistance = log(10000 * ((5.0 / ((5.0 / 1023) * ReadAnalog(10, A0))) - 1));
  int Value = ConvertCtoF(NTCReadInC(10000, Resistance)) * 100;

  if (ReplyToAddress != -1) {
    CanBusSend(DeviceAddress, 5, highByte(ReplyToAddress), lowByte(ReplyToAddress), byte("R"), 0x0B, highByte(Value), lowByte(Value), 0x00, 0x00);
  } else {
    SendSerial("DeviceTemp:0x0B:" + String(Value));
  }
}

void MaxSensorChannel(int ReplyToAddress) {
  if (ReplyToAddress != -1) {
    CanBusSend(DeviceAddress, 5, highByte(ReplyToAddress), lowByte(ReplyToAddress), byte("R"), 0x0C, byte(MaxChannelNumber), 0x00, 0x00, 0x00);
  } else {
    SendSerial("Max Sensor Channel:0x0C:" + String(MaxChannelNumber));
  }
}

void MaxSensorChannelRange(int ReplyToAddress, int Channel) {
  if (ReplyToAddress != -1) {
    CanBusSend(DeviceAddress, 5, highByte(ReplyToAddress), lowByte(ReplyToAddress), byte("R"), 0x0D, byte(MaxChannelNumber), 0x00, 0x00, 0x00);
  } else {
    SendSerial("Max Sensor Channel Range:0x0D:" + String(MaxChannelNumber));
  }
}

void MinSensorChannelRange(int ReplyToAddress, int Channel) {
  if (ReplyToAddress != -1) {
    CanBusSend(DeviceAddress, 5, highByte(ReplyToAddress), lowByte(ReplyToAddress), byte("R"), 0x0E, byte(MaxChannelNumber), 0x00, 0x00, 0x00);
  } else {
    SendSerial("Min Sensor Channel Range:0x0E:" + String(MaxChannelNumber));
  }
}
//----------------------------------------------------------------------------------------------------
//End Of General API Functions
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
//Device Temp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
//End Of Device Temp
//----------------------------------------------------------------------------------------------------


//----------------------------------------------------------------------------------------------------
//Sensor Helpers
//----------------------------------------------------------------------------------------------------
float ReadAnalog(int Samples, int PinNumber) {
  long Sum = 0;
  float Value = 0;
  for (int x = 0; x < Samples; x++) {
    Sum = Sum + analogRead(PinNumber);
  }
  Value = (Sum / Samples);
  return Value;
}

float NTCReadInC(int R2, float ResistenceRead) {
  /*
     Using the Resistence that is calced from an ADC read, a known calibrated resistence
     value, and https://en.wikipedia.org/wiki/Steinhart%E2%80%93Hart_equation to get the
     tempetature from these values.

     int R2 == Calibrated static resistor used
     float ResistenceRead == Log() of the resistence value read
  */
  float c1 = 1.009249522e-03;
  float c2 = 2.378405444e-04;
  float c3 = 2.019202697e-07;
  float C = (1.0 / (c1 + c2 * ResistenceRead + c3 * ResistenceRead * ResistenceRead * ResistenceRead)) - 273.15;
  return C;
}

float ConvertCtoF(float C) {
  float F = (1.8 * C) + 32;
  return F;
}

String FloatToIntFixed(double Data, int NumberOfDecimals) {
  int Multipler = pow(10, NumberOfDecimals);
  return String(round(Data * Multipler)).substring(0, String(round(Data * Multipler)).indexOf('.'));
}
//----------------------------------------------------------------------------------------------------
//End Of Sensor Helpers
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
//Specific Sensor Code
//----------------------------------------------------------------------------------------------------
int SensorCode(int ChannelNumber) {
  /*
    Read Sensor Value here for that channel
    convert that to fixed point value as an INT and return it.
  */
  int Value = 0;
  float Temp = RTDPointers[ChannelNumber]->temperature(RNOMINAL, RREF);
  uint8_t fault = RTDPointers[ChannelNumber]->readFault();

  if (fault) {
    SetError(4, 1);
    Temp = 0.0;
    Serial.print("Fault 0x"); Serial.println(fault, HEX);
    if (fault & MAX31865_FAULT_HIGHTHRESH) {
      Serial.println("RTD High Threshold");
    }
    if (fault & MAX31865_FAULT_LOWTHRESH) {
      Serial.println("RTD Low Threshold");
    }
    if (fault & MAX31865_FAULT_REFINLOW) {
      Serial.println("REFIN- > 0.85 x Bias");
    }
    if (fault & MAX31865_FAULT_REFINHIGH) {
      Serial.println("REFIN- < 0.85 x Bias - FORCE- open");
    }
    if (fault & MAX31865_FAULT_RTDINLOW) {
      Serial.println("RTDIN- < 0.85 x Bias - FORCE- open");
    }
    if (fault & MAX31865_FAULT_OVUV) {
      Serial.println("Under/Over voltage");
    }
    RTDPointers[ChannelNumber]->clearFault();
  }

  if (UNITS == 'I') {
    Value = ConvertCtoF(Value);
  }
  Serial.println("Value:"+String(Value));
  return FloatToIntFixed(Value, 1).toInt();
}
//----------------------------------------------------------------------------------------------------
//End Of Specific Sensor Code
//----------------------------------------------------------------------------------------------------
