#include <EEPROM.h>
#include <SPI.h>

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

//Device Configuration setting
long PacingTimer;
#define DeviceType 7
#define MaxChannelNumber 0
const int SensorPins[] PROGMEM = { A1, A2, A3, A4 }; // Check wiring of board to set these correctly
const int SensorType[] PROGMEM = { 1, 1, 1, 1 }; //reference the API sheet for types
const int SensorMin[] PROGMEM = { 0, 0, 0, 0 }; // min value of channel
const int SensorMax[] PROGMEM = { 1000, 1000, 1000, 1000 }; // max value of channel
uint8_t ErrorNumber = 0;
uint8_t ErrorCommandNumber = 0;
char UNITS = 'I';

#define ComPort Serial
String inputString = "";  // a String to hold incoming data from ports

const char *const AcceptedCommands[] PROGMEM = {
  "ERROR?",
  "STREAMING?",
  "USYSTEM?",
  "RESETERR",
  "REBOOT",
  "PACING?",
  "ADDR?",
  "TEMP?"
};

const char *const ParameterCommands[] PROGMEM = {
  "SETUNITSYSTEM",
  "SETSTREAMING",
  "SETPACINGTIME",
  "SETDEVICEADDR",
  "GETUABR"
};

void setup() {
  ComPort.begin(115200);
  GetDeviceAddressFromMemory();
  CANBusSetup();

  ComPort.print("UnitSystem:");
  ComPort.println(GetUnitSystemFromMemory());

  ComPort.print("Streaming:");
  ComPort.println(GetStreamingFromMemory());

  if (PacingValueCheck(GetPacingTimeFromMemory()) == false) {
    UpdatePacingTime(2500);
  }

  ComPort.print("Pacing:");
  ComPort.println(GetPacingTimeFromMemory());

  DiscoveryResponse();
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

  if (ErrorNumber <= 1 && ErrorNumber <= 4) {
    ResetError();
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
        UnitsSystemSet(true, FilteredCommand);
      } else {
        SendSerial("%R,Error,Invalid Parameter, I or M");
      }
      break;
    case 1:
      //SETSTREAMING
      if (ThingToTest == "0" || ThingToTest == "1") {
        StreamingModeSet(true, ThingToTest.toInt());
      } else {
        SendSerial("%R,Error,Invalid Parameter, 0 or 1");
      }
      break;
    case 2:
      //SETPACINGTIME
      PacingSet(true, ThingToTest.toInt());
      break;
    case 3:
      //SETDEVICEADDRESS
      SetDeviceAddress(ThingToTest.toInt());
      break;
    case 4:
      //GETUNITABR
      UnitsABRResponse(true, ThingToTest.toInt());
      break;
  }
}

void CommandToCall(int Index) {
  switch (Index) {
    case 0:
      //ERROR?
      GetError(true);
      break;
    case 1:
      //STREAMING?
      StreamingModeResponse(true);
      break;
    case 2:
      //UNITSYSTEM?
      UnitsSystemResponse(true);
      break;
    case 3:
      //RESETERROR
      ResetError();
      break;
    case 4:
      //REBOOT
      RebootDevice();
      break;
    case 5:
      //PACING?
      PacingResponse(true);
      break;
    case 6:
      //TEMP?
      DeviceTemp(true);
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

bool PacingValueCheck(int Value) {
  if (Value >= 250 && Value <= 65535) {
    return true;
  } else {
    SendSerial("%R,Error,Invalid Parameter 250 <= x <= 65535");
    return false;
  }
}

void SetError(int Number, int CommandNumber) {
  ErrorNumber = Number;
  ErrorCommandNumber = CommandNumber;
  GetError(false);
  GetError(true);
}

void ResetError() {
  CanBusSend(DeviceAddress, 7, byte('R'), ErrorNumber, ErrorCommandNumber, 0x0A, 0xFF, 0x0A, 0xFF, 0x0A);
  SendSerial("Reset Error:0x08:" + String(ErrorNumber) + ":" + String(ErrorCommandNumber));
  ErrorNumber = 0;
  ErrorCommandNumber = 0;
}

void GetError(bool FromSerial) {
  /*
    
    :return: None
    :rtype: None
  */
  if (FromSerial == false) {
    CanBusSend(DeviceAddress, 3, byte('R'), ErrorNumber, ErrorCommandNumber, 0x00, 0x00, 0x00, 0x00, 0x00);
  } else {
    SendSerial("Error:0x06:" + String(ErrorNumber) + ":" + String(ErrorCommandNumber));
  }
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

  // Serial.print("got some CAN Data:ID:");
  // Serial.print(CAN.getCanId());

  // Serial.print(" Data:");
  // for (uint8_t i = 0; i < 8; i++) {
  //   Serial.print(i);
  //   Serial.print(": ");
  //   Serial.print(cdata[i], HEX);
  //   Serial.print(",");
  // }
  // Serial.println();

  if ((CAN.getCanId() > 9 && CAN.getCanId() < 20) && (cdata[0] == 0x00 && cdata[1] == 0x3F && cdata[2] == 0x00 && cdata[3] == 0xFF && cdata[4] == 0x00 && cdata[5] == 0xFF && cdata[6] == 0x00 && cdata[7] == 0xFF)) {
    DiscoveryResponse();
  } else {
    //Check if this is the target device
    if (CAN.getCanId() == DeviceAddress) {
      switch (cdata[0]) {
        case 1:  //State
          if (cdata[1] == '?') {
            StatusResponse(cdata[2]);
          } else {
            SetError(3, cdata[0]);
          }
          break;

        case 2:  //Streaming
          switch (cdata[1]) {
            case '?':
              // do your query
              StreamingModeResponse(false);
              break;
            case 'S':
              // do your set
              StreamingModeSet(false, cdata[2]);
              break;
            default:
              SetError(3, cdata[0]);
              break;
          }
          break;

        case 3:  //Pacing
          switch (cdata[1]) {
            case '?':
              // do your query
              break;
            case 'S':
              // do your set
              break;
            default:
              SetError(3, cdata[0]);
              break;
          }
          break;

        case 4:  //Units
          switch (cdata[1]) {
            case '?':
              UnitsSystemResponse(false);
              break;
            case 'S':
              UnitsSystemSet(false, cdata[2]);
              break;
            default:
              SetError(3, cdata[0]);
              break;
          }
          break;

        case 6:  //Error Query
          if (cdata[1] == '?') {
            GetError(false);
          } else {
            SetError(3, cdata[0]);
          }
          break;

        case 7:  //Unit ABR Query
          if (cdata[1] == '?') {
            UnitsABRResponse(false, cdata[2]);
          } else {
            SetError(3, cdata[0]);
          }
          break;

        case 8:  //Error Reset
          if (cdata[1] == 'S' && cdata[1] == 0x0A && cdata[1] == 0x0A && cdata[1] == 0x0A && cdata[1] == 0xFF && cdata[1] == 0xFF && cdata[1] == 0xFF) {
            ResetError();
          } else {
            SetError(3, cdata[0]);
          }
          break;

        case 9:  //Reboot Device
          //need to do a complete packet check3
          if (cdata[1] == 0xFF && cdata[1] == 0x0A && cdata[1] == 0x0A && cdata[1] == 0xFF && cdata[1] == 0x0A && cdata[1] == 0x0A && cdata[1] == 0x0A) {
            RebootDevice();
          } else {
            SetError(3, cdata[0]);
          }
          break;

        case 10:  //Device Temp
          if (cdata[1] == '?') {
            DeviceTemp(false);
          } else {
            SetError(3, cdata[0]);
          }
          break;

        case 11:  //Max Sensor channels
          if (cdata[1] == '?') {
            MaxSensorChannel(false);
          } else {
            SetError(3, cdata[0]);
          }
          break;

        case 12:  //Sensor channel Range Max
          if (cdata[1] == '?') {
            MaxSensorChannelRange(false, cdata[2]);
          } else {
            SetError(3, cdata[0]);
          }
          break;

        case 13:  //Sensor channel Range Min
          if (cdata[1] == '?') {
            MinSensorChannelRange(false, cdata[2]);
          } else {
            SetError(3, cdata[0]);
          }
          break;

        case 17:  //Sensor channel Type
          if (cdata[1] == '?') {
            SensorChannelType(false, cdata[2]);
          } else {
          }
          SetError(3, cdata[0]);
          break;

        default:
          SetError(1, cdata[0]);
          break;
      }
    }
  }
}

void CanBusSend(int PacketIdentifier, int DataLength, byte Zero, byte One, byte Two, byte Three, byte Four, byte Five, byte Six, byte Seven) {
  // ID, ext, len, byte: data
  //ext = 0 for standard frame
  byte DataPacket[8] = { Zero, One, Two, Three, Four, Five, Six, Seven };  //construct data packet array
  CAN.sendMsgBuf(PacketIdentifier, 0, DataLength + 1, DataPacket);
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
  unsigned int Value = word(EEPROM.read(3), EEPROM.read(2));
  return Value;
}

void UpdatePacingTime(unsigned int Data) {
  EEPROM.update(3, highByte(Data));
  EEPROM.update(2, lowByte(Data));
}

//----------------------------------------------------------------------------------------------------
// End Of EEPROM Functions
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
//General API Functions
//----------------------------------------------------------------------------------------------------
void DiscoveryResponse() {
  /*
    
    :type ReplyToAddress: int
    :return: None
    :rtype: None
  */
  CanBusSend(byte(DeviceAddress), 7, 0x00, 0xFF, byte(DeviceType), byte(DeviceType), byte(DeviceType), byte(GetUnitSystemFromMemory()), byte(GetUnitSystemFromMemory()), byte(GetUnitSystemFromMemory()));
}

void RebootDevice() {
  CanBusSend(DeviceAddress, 7, 0x09, 0xFF, 0x0A, 0x0A, 0xFF, 0x0A, 0x0A, 0x0A);
  SendSerial("Device is Rebooting");
  delay(2500);
  resetFunc();
}

void StatusResponse(int ChannelNumber) {
  /*
    :type ReplyToAddress: int
    :return: None
    :rtype: None
  */
  if (ChannelRangeCheck(ChannelNumber) == true) {
    int ReturnedValue = SensorCode(ChannelNumber);  // value returned will be an int for a fixed point number
    CanBusSend(DeviceAddress, 4, 0x01, byte(ChannelNumber), highByte(ReturnedValue), lowByte(ReturnedValue), byte(SensorType[ChannelNumber]), SensorType[ChannelNumber], 0x00, 0x00);
    SendSerial("StatusResponse:0x01:" + String(ChannelNumber) + ":" + String(ReturnedValue) + ":" + String(SensorType[ChannelNumber]));
  } else {
    // return error that channel doesn't exist
    SetError(3, 0x1);
  }
}

void StreamingModeResponse(bool FromSerial) {
  /*
    
    :type ReplyToAddress: int
    :return: None
    :rtype: None
  */
  if (FromSerial == false) {
    CanBusSend(DeviceAddress, 2, 0x02, byte(GetStreamingFromMemory()), 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
  } else {
    SendSerial("StreamingMode:0x03:" + String(GetStreamingFromMemory()));
  }
}

void StreamingModeSet(bool FromSerial, int Data) {
  /*
    :return: None
    :rtype: None
  */

  if (Data == 0 || Data == 1) {
    EEPROM.update(5, Data);
    StreamingModeResponse(FromSerial);
  } else {
    SetError(3, 0x02);
  }
}

void PacingResponse(bool FromSerial) {
  /*
    , defaults to -1
    :type ReplyToAddress: int
    :return: None
    :rtype: None
  */
  if (FromSerial == false) {
    CanBusSend(DeviceAddress, 3, 0x03, byte("R"), highByte(GetPacingTimeFromMemory()), lowByte(GetPacingTimeFromMemory()), 0x00, 0x00, 0x00, 0x00);
  } else {
    SendSerial("Pacing:0x04:" + String(GetPacingTimeFromMemory()));
  }
}

void PacingSet(bool FromSerial, int Data) {
  /*
    
    :type ReplyToAddress: int
    :return: None
    :rtype: None
  */
  if (PacingValueCheck(Data) == true) {
    UpdatePacingTime(Data);
    PacingResponse(FromSerial);
  } else {
    SetError(3, 0x03);
  }
}

void UnitsSystemResponse(bool FromSerial) {
  /*
    :return: None
    :rtype: None
  */

  if (FromSerial == false) {
    CanBusSend(DeviceAddress, 1, 0x04, byte(GetUnitSystemFromMemory()), 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
  } else {
    SendSerial("UnitsSystem:0x04:" + String(GetUnitSystemFromMemory()));
  }
}

void UnitsSystemSet(bool FromSerial, char Data) {
  /*
    
    :type ReplyToAddress: int
    :return: None
    :rtype: None
  */

  if (Data == 'I' || Data == 'M') {
    EEPROM.update(4, Data);
    UnitsSystemResponse(FromSerial);
  } else {
    SetError(3, 0x04);
  }
}

void UnitsABRResponse(bool FromSerial, int Channel) {
  /*
    :return: None
    :rtype: None
  */
  if (ChannelRangeCheck(Channel) == true) {
    byte ABR = 0x00;
    switch (SensorType[Channel]) {
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

    if (FromSerial == false) {
      CanBusSend(DeviceAddress, 3, 0x07, byte("R"), Channel, ABR, 0x00, 0x00, 0x00, 0x00);
    } else {
      SendSerial("UnitABR:0x07:" + String(Channel) + ":" + String(ABR));
    }
  } else {
    SetError(3, 0x07);
  }
}

void MaxSensorChannel(bool FromSerial) {
  if (FromSerial == false) {
    CanBusSend(DeviceAddress, 1, 0x0B, byte(MaxChannelNumber), 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
  } else {
    SendSerial("Max Sensor Channel:0x0B:" + String(MaxChannelNumber));
  }
}

void MaxSensorChannelRange(bool FromSerial, int Channel) {
  if (ChannelRangeCheck(Channel) == true) {
    if (FromSerial == false) {
      CanBusSend(DeviceAddress, 4, 0x0C, byte("R"), byte(Channel), highByte(SensorMax[Channel]), lowByte(SensorMax[Channel]), 0x00, 0x00, 0x00);
    } else {
      SendSerial("Max Sensor Channel Range:0x0C:" + String(Channel) + ":" + String(SensorMax[Channel]));
    }
  } else {
    SetError(3, 0x0C);
  }
}

void MinSensorChannelRange(bool FromSerial, int Channel) {
  if (ChannelRangeCheck(Channel) == true) {
    if (FromSerial == false) {
      CanBusSend(DeviceAddress, 4, 0x0D, byte("R"), byte(Channel), highByte(SensorMin[Channel]), lowByte(SensorMin[Channel]), 0x00, 0x00, 0x00);
    } else {
      SendSerial("Min Sensor Channel Range:0x0D:" + String(Channel) + ":" + String(SensorMin[Channel]));
    }
  } else {
    SetError(3, 0x0D);
  }
}

void SensorChannelType(bool FromSerial, int Channel) {
  if (ChannelRangeCheck(Channel) == true) {
    if (FromSerial == false) {
      CanBusSend(DeviceAddress, 4, 0x11, byte("R"), byte(Channel), highByte(SensorType[Channel]), lowByte(SensorType[Channel]), 0x00, 0x00, 0x00);
    } else {
      SendSerial("Min Sensor Channel Range:0x11:" + String(Channel) + ":" + String(SensorType[Channel]));
    }
  } else {
    SetError(3, 0x11);
  }
}

//----------------------------------------------------------------------------------------------------
//End Of General API Functions
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
//Device Temp
//----------------------------------------------------------------------------------------------------
void DeviceTemp(bool FromSerial) {
  float Resistance = log(10000 * ((5.0 / ((5.0 / 1023) * ReadAnalog(10, A0))) - 1));
  int Value = 0;
  if (GetUnitSystemFromMemory() == 'I') {
    Value = FloatToIntFixed(ConvertCtoF(NTCReadInC(10000, Resistance)), 2).toInt();
  } else {
    Value = FloatToIntFixed(NTCReadInC(10000, Resistance), 2).toInt();
  }

  if (FromSerial == false) {
    CanBusSend(DeviceAddress, 5, 0x0A, byte("R"), highByte(Value), lowByte(Value), 0x00, 0x00, 0x00, 0x00);
  } else {
    SendSerial("DeviceTemp:0x0B:" + String(Value));
  }
}
//----------------------------------------------------------------------------------------------------
//End Of Device Temp
//----------------------------------------------------------------------------------------------------


//----------------------------------------------------------------------------------------------------
//Sensor Helpers
//----------------------------------------------------------------------------------------------------
bool ChannelRangeCheck(int Channel) {
  if (Channel >= 0 && Channel <= MaxChannelNumber) {
    return true;
  } else {
    return false;
  }
}

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

float ConvertPSItoKPa(float PSI) {
  float KPA = 6.8947572932 * PSI;
  return KPA;
}

String FloatToIntFixed(double Data, int NumberOfDecimals) {
  double Multipler = pow(10, NumberOfDecimals);
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

  int Value = ChannelNumber;


  return Value;
}
//----------------------------------------------------------------------------------------------------
//End Of Specific Sensor Code
//----------------------------------------------------------------------------------------------------
