#include <EEPROM.h>
#include <SPI.h>

// CANBus
#include "mcp2515_can.h"
mcp2515_can CAN(9);
#define MAX_DATA_SIZE 8
int DeviceAddress = 1;  // within the  subnet
int DeviceSubnet = 0;
int PacketIdentifier = NULL;  //complete Device Address

//Device Configuration setting
long PacingTimer;
#define DeviceType 3
#define MaxChannelNumber 3
const int SensorPins[] = {A1, A2, A3, A4};
const int SensorType[] = {4, 4, 3, 1};
const int SensorMin[] = {0, 0, 0, -150};
const int SensorMax[] = {150, 150, 18, 150};
uint8_t ErrorNumber = 0;
uint8_t ErrorCommandNumber = 0;
char UNITS = 'I';

#define ComPort Serial
String inputString = "";  // a String to hold incoming data from ports

char *const AcceptedCommands[] = {
  "ERROR?",
  "STREAMING?",
  "UNITSYSTEM?",
  "RESETERROR",
  "REBOOT",
  "PACING?",
  "SUBNET?",
  "ADDRESS?",
  "IDENTIFIER?"
};

char *const ParameterCommands[] = {
  "SETUNITSYSTEM",
  "SETSTREAMING",
  "SETPACINGTIME",
  "SETDEVICEADDRESS",
  "SETDEVICESSUBNET"
};


void setup() {
  ComPort.begin(115200);
  SerialGetPacketIdentifier();
  CANBusSetup();

  ComPort.print("UnitSystem:");
  ComPort.println(GetUnitSystemFromMemory());

  ComPort.print("Streaming:");
  ComPort.println(GetStreamingFromMemory());

  ComPort.print("Pacing:");
  ComPort.println(GetPacingTimeFromMemory());

  DiscoveryResponse(0x00);
}

void loop() {
  CANBusRecieveCheck();

  long CurrentTime = millis();
  if (GetStreamingFromMemory() == 1) {
    if (abs(PacingTimer - CurrentTime) > GetPacingTimeFromMemory()) {
      for (uint8_t i = 0; i <= MaxChannelNumber; i++) {
        StatusResponse(i, 0);
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
      if (PacingValueCheck(ThingToTest.toInt()) == true) {
        PacingSet(-1, ThingToTest.toInt());
      }
      break;
    case 3:
      //SETDEVICEADDRESS
      SetDeviceAddress(ThingToTest.toInt());
      break;
    case 4:
      //SETDEVICESSUBNET
      SetDeviceSubnet(ThingToTest.toInt());
      break;
  }
}

void CommandToCall(int Index) {
  switch (Index) {
    case 0:
      //ERROR?
      GetError(-1);
      break;
    case 1:
      //STREAMING?
      StreamingModeResponse(-1);
      break;
    case 2:
      //UNITSYSTEM?
      UnitsSystemResponse(-1);
      break;
    case 3:
      //RESETERROR
      SendSerial("Error Reset:0x05");
      ResetError(-1);
      break;
    case 4:
      //REBOOT
      SendSerial("Rebooting:0x06");
      RebootDevice(-1);
      break;
    case 5:
      //PACING?
      PacingResponse(-1);
      break;
    case 6:
      //SUBNET?
      GetDeviceSubnetFromMemory(1);
      break;
    case 7:
      //ADDRESS?
      GetDeviceAddressFromMemory(1);
      break;
    case 8:
      //IDENTIFIER?
      SerialGetPacketIdentifier();
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

bool PacingValueCheck(unsigned int Value) {
  if (Value >= 250 && Value <= 65535) {
    return true;
  } else {
    SendSerial("%R,Error,Invalid Parameter 250 <= x <= 65535");
    return false;
  }
}

int CalcAddress(int ReplyToAddress) {
  // converts the full packet identifier to the 0-254 int to put into the CAN Bus Packet
  int Subnet_Start = DeviceSubnet * 256;
  return ReplyToAddress - Subnet_Start;
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
  byte cdata[MAX_DATA_SIZE] = { 0 };

  CAN.readMsgBuf(8, cdata);

  // Serial.print("got some CAN Data:ID:");
  // Serial.print(CAN.getCanId());

//  Serial.print(" Data:");
//  for (uint8_t i = 0; i < 8; i++) {
//    Serial.print(i);
//    Serial.print(": ");
//    Serial.print(cdata[i], HEX);
//    Serial.print(",");
//  }
//  Serial.println();

  //Check if message is coming in on correct subnet
  if (CAN.getCanId() >= (DeviceSubnet * 256) && CAN.getCanId() <= (DeviceSubnet * 256) + 254) {
    //Check if this is the target device
    if (cdata[0] == DeviceAddress) {
      int CommandNumber = cdata[1];
      int CommandType = cdata[2];
//      Serial.print("CommandNumber:");
//      Serial.print(CommandNumber);
//      Serial.print(" ::Command Type:");
//      Serial.print(CommandType);
//      Serial.println();

      switch (CommandNumber) {
        case 0:  // Discovery
          if (cdata[2] == 0x3F && cdata[3] == 0x00 && cdata[4] == 0xFF && cdata[5] == 0x00 && cdata[6] == 0xFF && cdata[7] == 0x00) {
            DiscoveryResponse(CAN.getCanId());
          }
          break;
        case 1:  //State
          if (CommandType == 0x3F) { // 0x3F == ?
            StatusResponse(CAN.getCanId(), cdata[3]);
          }
          break;
        case 2:  // Streaming
          if (CommandType == 0x3F) { // 0x3F == ?
            StreamingModeResponse(CAN.getCanId());
          } else if (CommandType == 0x53) { // 0x53 == S
            StreamingModeSet(CAN.getCanId(), cdata[3]);
          }
          break;
        case 3:  // Pacing
          if (CommandType == 0x3F) { // 0x3F == ?
            PacingResponse(CAN.getCanId());
          } else if (CommandType == 0x53) { // 0x53 == S
            PacingSet(CAN.getCanId(), cdata[3]);
          }
          break;
        case 4:  // Units
          if (CommandType == 0x3F) { // 0x3F == ?
            UnitsSystemResponse(CAN.getCanId());
          } else if (CommandType == 0x53) { // 0x53 == S
            UnitsSystemSet(CAN.getCanId(), cdata[3]);
          }
          break;
        case 5:  // I/O
          if (CommandType == 0x3F) { // 0x3F == ?
            GetError(CAN.getCanId());
          } else if (CommandType == 0x53) { // 0x53 == S
          }
          break;
        case 6:  // Error
          if (CommandType == 0x3F) { // 0x3F == ?
            GetError(CAN.getCanId());
          } else if (CommandType == 0x53) { // 0x53 == S
            if (cdata[3] == 0x0A && cdata[4] == 0x0A && cdata[5] == 0x0A && cdata[6] == 0xFF && cdata[7] == 0xFF) {
              ResetError(CAN.getCanId());
            }
          }
          break;
        case 7:  // Unit ABR
          if (CommandType == 0x3F) { // 0x3F == ?
            UnitsABRResponse(CAN.getCanId(), cdata[3]);
          }
          break;
        case 9:  // Device Reboot
          if (CommandType == 0x53) { // 0x53 == S
            if (cdata[3] == 0x0A && cdata[4] == 0x0A && cdata[5] == 0x0A && cdata[6] == 0x0A && cdata[7] == 0x0A) {
              RebootDevice(CAN.getCanId());
            }
          }
          break;
        case 10:  // Device Temperature
          if (CommandType == 0x3F) { // 0x3F == ?
            DeviceTemp(CAN.getCanId());
          }
          break;
        case 11:  // Max Sensor Channel
          if (CommandType == 0x3F) { // 0x3F == ?
            MaxSensorChannel(CAN.getCanId());
          }
          break;
        case 12:  // Sensor Channel Range Max
          if (CommandType == 0x3F) { // 0x3F == ?
            MaxSensorChannelRange(CAN.getCanId(), cdata[3]);
          }
          break;
        case 13:  // Sensor Channel Range Min
          if (CommandType == 0x3F) { // 0x3F == ?
            MinSensorChannelRange(CAN.getCanId(), cdata[3]);
          }
          break;
        case 14:  // Sensor Channel Type
          if (CommandType == 0x3F) { // 0x3F == ?
            SensorChannelType(CAN.getCanId(), cdata[3]);
          }
          break;
        default:
          SetError(CAN.getCanId(), 1, CommandNumber, true);
          break;
      }
    }
  }
}

void CanBusSend(byte Zero, byte One, byte Two, byte Three, byte Four, byte Five, byte Six, byte Seven) {
  byte DataPacket[8] = { Zero, One, Two, Three, Four, Five, Six, Seven };  //construct data packet array
  CAN.sendMsgBuf(PacketIdentifier, 0, 8, DataPacket);
}
//----------------------------------------------------------------------------------------------------
//End Of CAN Bus Functions
//----------------------------------------------------------------------------------------------------


//----------------------------------------------------------------------------------------------------
//EEPROM Functions
//----------------------------------------------------------------------------------------------------
int GetIdentifier() {
  GetDeviceAddressFromMemory(0);
  GetDeviceSubnetFromMemory(0);
  PacketIdentifier = (DeviceSubnet * 256) + DeviceAddress;
  return PacketIdentifier;
}

void SerialGetPacketIdentifier() {
  SendSerial("CAN Bus Identifier: " + String(GetIdentifier()));
}

void SetDeviceAddress(int Address) {
  if (Address >= 1 && Address <= 254) {
    EEPROM.update(1, Address);
    GetDeviceAddressFromMemory(1);
  } else {
    SendSerial("Address must be between 1 and 254");
  }
}

void GetDeviceAddressFromMemory(int PrintSerial) {
  DeviceAddress = EEPROM.read(1);
  if (PrintSerial == 1) {
    SendSerial("CAN Bus Address: " + String(DeviceAddress));
  }
}

void SetDeviceSubnet(int Subnet) {
  if (Subnet >= 0 && Subnet <= 7) {
    EEPROM.update(0, Subnet);
    GetDeviceSubnetFromMemory(1);
  } else {
    SendSerial("Subnet must be between 0 and 7");
  }
}

void GetDeviceSubnetFromMemory(int PrintSerial) {
  DeviceSubnet = EEPROM.read(0);
  if (PrintSerial == 1) {
    SendSerial("CAN Bus Subnet: " + String(DeviceSubnet));
  }
}

char GetUnitSystemFromMemory() {
  //Read Units out of EEPROM
  char TempValue = EEPROM.read(4);
  if (TempValue != 'I' && TempValue != 'M') {
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

void PutPacingInMemory(int Value) {
  EEPROM.write(3, highByte(Value));
  EEPROM.write(2, lowByte(Value));
}

unsigned int GetPacingTimeFromMemory() {
  //Read Pacing value out of EEPROM
  unsigned int Value = EEPROM.read(3) << 8 | EEPROM.read(2);
  if (PacingValueCheck(Value) == false) {
    PutPacingInMemory(250);
    Value = 250;
  }
  return Value;
}
//----------------------------------------------------------------------------------------------------
// End Of EEPROM Functions
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
//General API Functions
//----------------------------------------------------------------------------------------------------
void DiscoveryResponse(int ReplyAddress) {
  /*
    :param ReplyToAddress: reply address to put into the CAN packet header
    :type ReplyToAddress: int
    :return: None
    :rtype: None
  */
  byte unt_system = byte(GetUnitSystemFromMemory());
  CanBusSend(CalcAddress(ReplyAddress), 0x00, 0xFF, byte(DeviceType), byte(DeviceType), byte(DeviceType), unt_system, unt_system);
}

void SetError(int ReplyToAddress, int ErrorNum, int CommandNum, bool AutoReset) {
  ErrorNumber = ErrorNum;
  ErrorCommandNumber = CommandNum;
  GetError(ReplyToAddress);
  if (AutoReset == true) {
    ErrorNumber = 0;
    ErrorCommandNumber = 0;
  }
}

void GetError(int ReplyToAddress) {
  if (ReplyToAddress != -1) {
    CanBusSend(CalcAddress(ReplyToAddress), 0x06, 0x52, byte(ErrorCommandNumber), byte(ErrorNumber), 0x00, 0x00, 0x00);
  } else {
    SendSerial("Error:0x06:" + String(ErrorCommandNumber) + ":" + String(ErrorNumber));
  }
}

void ResetError(int ReplyToAddress) {
  SetError(ReplyToAddress, 0, 0, false);
}

void RebootDevice(int ReplyToAddress) {
  CanBusSend(CalcAddress(ReplyToAddress), 0x09, 0xFF, 0x10, 0x10, 0xFF, 0x10, 0x10);
  SendSerial("Device is Rebooting");
  delay(2500);
  resetFunc();
}

void StatusResponse(int ChannelNumber, int ReplyToAddress) {
  /*
    :param ReplyToAddress: reply address to put into the CAN packet header, defaults to -1
    :type ReplyToAddress: int
    :return: None
    :rtype: None
  */
  if (ChannelNumber >= 0 && ChannelNumber <= MaxChannelNumber) {

    int ReturnedValue = SensorCode(ChannelNumber);

    CanBusSend(CalcAddress(ReplyToAddress), 0x01, byte(ChannelNumber), highByte(ReturnedValue), lowByte(ReturnedValue), byte(SensorType[ChannelNumber]), 0x00, 0x00);

    SendSerial("StatusResponse:0x01:" + String(ChannelNumber) + ":" + String(ReturnedValue) + ":" + String(SensorType[ChannelNumber]));
  } else {
    // return error that channel doesn't exist
    SetError(ReplyToAddress, 3, 0, true);
    SendSerial("Error:0x01" + ErrorNumber);
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
    CanBusSend(CalcAddress(ReplyToAddress), 0x02, 0x52, byte(GetStreamingFromMemory()), 0x00, 0x00, 0x00, 0x00);
  } else {
    SendSerial("StreamingMode:0x02:" + String(GetStreamingFromMemory()));
  }
}

void StreamingModeSet(int ReplyToAddress, int Data) {
  /*
    :param ReplyToAddress: reply address to put into the CAN packet header, defaults to -1
    :type ReplyToAddress: int
    :return: None
    :rtype: None
  */
  //Serial.println("Streaming:" + String(Data));
  if (Data == 0 || Data == 1) {
    EEPROM.update(5, Data);
  } else {
    SetError(ReplyToAddress, 3, 2, true);
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
    CanBusSend(CalcAddress(ReplyToAddress), 0x52, 0x03, highByte(GetPacingTimeFromMemory()), lowByte(GetPacingTimeFromMemory()), 0x00, 0x00, 0x00);
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

  if (PacingValueCheck(Data) == true) {
    PutPacingInMemory(Data);
  } else {
    SetError(ReplyToAddress, 3, 4, true);
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
    CanBusSend(CalcAddress(ReplyToAddress), 0x52, 0x05, byte(GetUnitSystemFromMemory()), 0x00, 0x00, 0x00, 0x00);
  }
  SendSerial("UnitsSystem:0x05:" + String(GetUnitSystemFromMemory()));
}

void UnitsSystemSet(int ReplyToAddress, char Data) {
  /*
    :param ReplyToAddress: reply address to put into the CAN packet header
    :type ReplyToAddress: int
    :return: None
    :rtype: None
  */
  if (Data == 'I' || Data == 'M') {
    EEPROM.write(4, Data);
    UNITS = GetUnitSystemFromMemory();
  } else {
    SetError(ReplyToAddress, 3, 5, true);
  }
  UnitsSystemResponse(ReplyToAddress);
}

void UnitsABRResponse(int ReplyToAddress, int ChannelNumber) {
  /*
      :param ReplyToAddress: reply address to put into the CAN packet header
      :type ReplyToAddress: int
      :return: None
      :rtype: None
  */

  byte ABR = 0x00;
  switch (SensorType[ChannelNumber]) {
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
    CanBusSend(CalcAddress(ReplyToAddress), 0x52, 0x07, ABR, 0x00, 0x00, 0x00, 0x00);
  } else {
    SendSerial("UnitABR:0x08:" + String(ABR));
  }
}

void DeviceTemp(int ReplyToAddress) {
  float Resistance = log(10000 * ((5.0 / ((5.0 / 1023) * ReadAnalog(10, A0))) - 1));
  int Value = ConvertCtoF(NTCReadInC(10000, Resistance)) * 100;

  if (ReplyToAddress != -1) {
    CanBusSend(CalcAddress(ReplyToAddress), 0x52, 0x0A, highByte(Value), lowByte(Value), 0x00, 0x00, 0x00);
  } else {
    SendSerial("DeviceTemp:0x0A:" + String(Value));
  }
}

void MaxSensorChannel(int ReplyToAddress) {
  if (ReplyToAddress != -1) {
    CanBusSend(CalcAddress(ReplyToAddress), 0x52, 0x0B, byte(MaxChannelNumber), 0x00, 0x00, 0x00, 0x00);
  } else {
    SendSerial("Max Sensor Channel:0x0B:" + String(MaxChannelNumber));
  }
}

void MaxSensorChannelRange(int ReplyToAddress, int Channel) {
  if (Channel >= 0 && Channel <= MaxChannelNumber) {
    if (ReplyToAddress != -1) {
      CanBusSend(CalcAddress(ReplyToAddress), 0x52, 0x0C, byte(SensorMax[Channel]), 0x00, 0x00, 0x00, 0x00);
    } else {
      SendSerial("Max Sensor Channel Range:0x0C:" + String(SensorMax[Channel]));
    }
  } else {
    SetError(ReplyToAddress, 3, 0x0C, true);
  }
}

void MinSensorChannelRange(int ReplyToAddress, int Channel) {
  if (Channel >= 0 && Channel <= MaxChannelNumber) {
    if (ReplyToAddress != -1) {
      CanBusSend(CalcAddress(ReplyToAddress), 0x52, 0x0D, byte(SensorMin[Channel]), 0x00, 0x00, 0x00, 0x00);
    } else {
      SendSerial("Min Sensor Channel Range:0x0D:" + String(SensorMin[Channel]));
    }
  } else {
    SetError(ReplyToAddress, 3, 0x0D, true);
  }
}

void SensorChannelType(int ReplyToAddress, int Channel) {
  if (Channel >= 0 && Channel <= MaxChannelNumber) {

    if (ReplyToAddress != -1) {
      CanBusSend(CalcAddress(ReplyToAddress), 0x52, 0x0E, byte(SensorType[Channel]), 0x00, 0x00, 0x00, 0x00);
    } else {
      SendSerial("Min Sensor Channel Range:0x0E:" + String(SensorType[Channel]));
    }
  } else {
    SetError(ReplyToAddress, 3, 0x0E, true);
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

float ConvertPSItoKPa(float PSI) {
  float KPA = 6.8947572932 * PSI;
  return KPA;
}

int FloatToIntFixed(double Data, int NumberOfDecimals) {
  double Multipler = pow(10, NumberOfDecimals);
  return String(round(Data * Multipler)).substring(0, String(round(Data * Multipler)).indexOf('.')).toInt();
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
  if (ChannelNumber < 2) {
    Value = PressureSensor(ChannelNumber);
  } else if (ChannelNumber == 3) {
    Value = CurrentSensor(3);
  } else if (ChannelNumber == 2) {
    Value = VoltageSensor(2);
  }

  return Value;
}

int CurrentSensor(int ChannelNumber) {
  /*
    The sensor i'm using is Electronics-Salon Panel Mount AC/DC Current Sensor Module Board, Based on ACS758 (+/-150Amp)
    with the documents this thing has i'm wiring it per it's spec and polarity. I'm using the 5v based input that will
    Center the 0 amp output around Vcc/2 ~2.5v and depending on polarity will be either above or below 2.5 volts.
    If you're finding that your system is constantly "discharging" when you know the system is worrking correctly then
    your options are to either flip the cases below or to flip the wiring on your sensor.
  */

  int DN = ReadAnalog(50, SensorPins[ChannelNumber]);
  int Center = 509; //measure this from the device. should be around 511
  double AmpPermV = 0.013275; //get this from DataSheet for sensor or do a bit of calibration yourself
  int DNAdjusted = 0;
  if (DN > Center) {
    DNAdjusted = (DN - Center) * (-1); //Negative Case
  }
  if (DN < Center) {
    DNAdjusted = map(DN, Center, 0, 0, Center); //Positive Case
  }
  double Amps = (DNAdjusted * 0.00488758) / AmpPermV;
  return FloatToIntFixed(Amps, 1);
}

int VoltageSensor(int ChannelNumber){
  /*
   * returns the 0-5 volt read off of the ADC if you need this scaled then you have to put that in yourself
   */
  float Voltage = ReadAnalog(50, SensorPins[ChannelNumber]);
  return FloatToIntFixed(Voltage, 2);
}

int PressureSensor(int ChannelNumber) {
  float Pressure = 25.00 * 0.0048875 * ReadAnalog(50, SensorPins[ChannelNumber]) - 12.5; //PSI reading cause i work in freedom units
  if (Pressure <= 0) { // this value check is for where the senssor is giving a voltage but it is technically zero cause it's *mostly linear anything less than 0.5volts zero PSI
    Pressure = 0;
  }
  if (UNITS == 'M') {
    Pressure = ConvertPSItoKPa(Pressure);
  }

  return FloatToIntFixed(Pressure, 2);
}
//----------------------------------------------------------------------------------------------------
//End Of Specific Sensor Code
//----------------------------------------------------------------------------------------------------
