#include <EEPROM.h>
#include <SPI.h>

#define CAN_2515  //CAN_2515 or CAN_2518FD
// Set SPI CS Pins
const int SPI_CS_PIN = 9;
const int CAN_INT_PIN = 2;

#ifdef CAN_2518FD
#include "mcp2518fd_can.h"
mcp2518fd CAN(SPI_CS_PIN);  // Set CS pin
// To TEST MCP2518FD CAN2.0 data transfer
#define MAX_DATA_SIZE 8
// To TEST MCP2518FD CANFD data transfer, uncomment below lines
// #undef  MAX_DATA_SIZE
// #define MAX_DATA_SIZE 64
#endif

#ifdef CAN_2515
#include "mcp2515_can.h"
mcp2515_can CAN(SPI_CS_PIN);  // Set CS pin
#define MAX_DATA_SIZE 8
#endif

// CANBus 
int DeviceAddress = 0;
uint32_t id;
uint8_t type;  // bit0: ext, bit1: rtr
uint8_t len;
byte cdata[MAX_DATA_SIZE] = { 0 };
unsigned char DataPacket[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

//Device Configuration setting 
bool Streaming;
char UnitSystem;
long PacingTimer;
long PacingTime;
int DeviceType = 0;

#define ErrorLEDpin 3
int ErrorNumber = 0;

#define ComPort Serial
String inputString = "";      // a String to hold incoming data from ports
bool stringComplete = false;  // whether the string is complete for each respective port

const char* AcceptedCommands[] = {
  "UNITS?",
  "ERROR?",
  "STREAMING?",
  "UNITSYSTEM?",
  "STATE?",
  "RESETERROR",
  "REBOOT",
};

const char* ParameterCommands[] = {
  "SETUNITSYSTEM",
  "SETSTREAMING",
  "SETPACINGTIME",
  "SETDEVICEADDRESS",
};

void setup() {
  ComPort.begin(115200);
  GetDeviceAddressFromMemory();

  CANBusSetup();

  GetUnitSystemFromMemory();
  ComPort.print("UnitSystem:");
  ComPort.println(UnitSystem);

  GetStreamingFromMemory();
  ComPort.print("Streaming:");
  ComPort.println(Streaming);

  GetPacingTimeFromMemory();
  ComPort.print("Pacing:");
  ComPort.println(PacingTime);


  DiscoveryResponse(0);
}

void loop() {
  CANBusRecieveCheck();
  //delay(100);
  //CanBusSend(8);
  //delay(100);
}

//----------------------------------------------------------------------------------------------------
//
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
    // get the new byte:
    char inChar = (char)ComPort.read();
    // add it to the inputString:
    inputString += inChar;
    // if the incoming character is a carriage return, set a flag so the main loop can
    // do something about it:
    if (inChar == '\r') {
      stringComplete = true;
    }
  }
}

//----------------------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------------------




//----------------------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------------------
void CANBusSetup() {
#if MAX_DATA_SIZE > 8
  /*
     * To compatible with MCP2515 API,
     * default mode is CAN_CLASSIC_MODE
     * Now set to CANFD mode.
     */
  CAN.setMode(CAN_NORMAL_MODE);
#endif

  while (CAN_OK != CAN.begin(CAN_500KBPS)) {  // init can bus : baudrate = 500k
    ComPort.println(F("CAN init fail, retry..."));
    delay(100);
  }
  ComPort.println(F("CAN init ok!"));
}

void CANBusRecieveCheck() {
  // check if data coming
  if (CAN_MSGAVAIL != CAN.checkReceive()) {
    return;
  }

  char prbuf[32 + MAX_DATA_SIZE * 3];
  int i, n;

  unsigned long t = millis();
  // read data, len: data length, buf: data buf
  CAN.readMsgBuf(&len, cdata);

  id = CAN.getCanId();
  type = (CAN.isExtendedFrame() << 0) | (CAN.isRemoteRequest() << 1);
  /*
     * MCP2515(or this driver) could not handle properly
     * the data carried by remote frame
     */

  n = sprintf(prbuf, "%04lu.%03d ", t / 1000, int(t % 1000));
  /* Displayed type:
     *
     * 0x00: standard data frame
     * 0x02: extended data frame
     * 0x30: standard remote frame
     * 0x32: extended remote frame
     */
  static const byte type2[] = { 0x00, 0x02, 0x30, 0x32 };
  n += sprintf(prbuf + n, "RX: [%08lX](%02X) ", (unsigned long)id, type2[type]);
  // n += sprintf(prbuf, "RX: [%08lX](%02X) ", id, type);

  for (i = 0; i < len; i++) {
    n += sprintf(prbuf + n, "%02X ", cdata[i]);
  }
  ComPort.println(prbuf);
}

void CanBusCommandDispatcher(){
  //Check if Discovery do not filter by ID 
  

  //Check if this is the target Device 


}

void CanBusSend(int DataLength) {
  // ID, ext, len, byte: data
  //ext = 0 for standard frame
  CAN.sendMsgBuf(2047, 0, DataLength, DataPacket);
}
//----------------------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------------------

void (*resetFunc)(void) = 0;  // declare reset fuction at address 0

void RebootDevice(int ReplyToAddress) {
  byte DataPacket[] = { highByte(ReplyToAddress), lowByte(ReplyToAddress), byte("R"), 0x10, 0xFF, 0x10, 0xFF, 0x10 };
  CanBusSend(7);
  SendSerial("Device is Rebooting");
  delay(2500);
  resetFunc();
}

//----------------------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------------------


//----------------------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------------------
void SetDeviceAddress(int Address) {
  if (Address >= 1 && Address <= 2047) {
    EEPROM.update(1, highByte(Address));
    EEPROM.update(0, lowByte(Address));
    GetDeviceAddressFromMemory();
  } else {
    SendSerial("Address must be between 1 and 2047");
  }
}

void GetDeviceAddressFromMemory() {
  DeviceAddress = EEPROM.read(1) << 8 | EEPROM.read(0);
  SendSerial("CAN Bus Address: " + String(DeviceAddress));
}

void GetUnitSystemFromMemory() {
  //Read Units out of EEPROM
  char TempValue = EEPROM.read(4);
  if (TempValue == 'I' || TempValue == 'M') {
    UnitSystem = TempValue;
  } else {
    UnitSystem = 'I';
    EEPROM.update(0, 'I');
  }
}

void GetStreamingFromMemory() {
  //Read Stream Value out of EEPROM
  int TempValue = EEPROM.read(5);
  if (TempValue == 0 || TempValue == 1) {
    Streaming = TempValue;
  } else {
    Streaming = 1;
    EEPROM.update(1, 1);
  }
}

void GetPacingTimeFromMemory() {
  //Read Pacing value out of EEPROM
  int PacingTime = EEPROM.read(3) << 8 || EEPROM.read(2);
}

//----------------------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------------------

void SetError(int Number) {
  switch (Number) {
    case 1:
      ErrorNumber = 1;
      break;
  }
}

void GetError(int ReplyToAddress) {
  /*
  :param ReplyToAddress: reply address to put into the CAN packet header, defaults to -1
  :type ReplyToAddress: int
  :return: None
  :rtype: None
    */
  byte DataPacket[] = { highByte(ReplyToAddress), lowByte(ReplyToAddress), byte("R"), 0x07, byte(ErrorNumber) };
  if (ReplyToAddress != -1) {
    CanBusSend(5);
  } else {
    SendSerial("Error:0x07:" + String(ErrorNumber));
  }
}

void ResetError(int ReplyToAddress) {
  ErrorNumber = 0;
  GetError(ReplyToAddress);
}

//----------------------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------------------

void DiscoveryResponse(int ReplyToAddress) {
  /*
  :param ReplyToAddress: reply address to put into the CAN packet header
  :type ReplyToAddress: int
  :return: None
  :rtype: None
    */
  byte DataPacket[] = { highByte(ReplyToAddress), lowByte(ReplyToAddress), byte("R"), 0x01, 0x01, byte(DeviceType), byte(DeviceType), byte(DeviceType) };
  CanBusSend(7);
}

void StatusResponse(int ReplyToAddress) {
  /*
  :param ReplyToAddress: reply address to put into the CAN packet header, defaults to -1
  :type ReplyToAddress: int
  :return: None
  :rtype: None
    */
  int ChannelNumber = 0;
  int ReturnedValue = 0;
  byte DataPacket[] = { highByte(ReplyToAddress), lowByte(ReplyToAddress), byte("R"), 0x02, byte(ChannelNumber), highByte(ReturnedValue), lowByte(ReturnedValue), byte(DeviceType) };
  if (ReplyToAddress != -1) {
    CanBusSend(7);
  } else {
    SendSerial("StatusResponse:0x02:" + String(ReturnedValue) + ":" + String(DeviceType));
  }
}

void StreamingModeResponse(int ReplyToAddress) {
  /*
  :param ReplyToAddress: reply address to put into the CAN packet header, defaults to -1
  :type ReplyToAddress: int
  :return: None
  :rtype: None
    */
  byte DataPacket[] = { highByte(ReplyToAddress), lowByte(ReplyToAddress), byte("R"), 0x03, byte(Streaming) };
  if (ReplyToAddress != -1) {
    CanBusSend(5);
  } else {
    SendSerial("StreamingMode:0x03:" + String(Streaming));
  }
}

void StreamingModeSet(bool Data, int ReplyToAddress) {
  /*
  :param ReplyToAddress: reply address to put into the CAN packet header, defaults to -1
  :type ReplyToAddress: int
  :return: None
  :rtype: None
    */
  if (Data == true || Data == false) {
    EEPROM.update(1, Data);
    Streaming = Data;
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
  byte DataPacket[] = { highByte(ReplyToAddress), lowByte(ReplyToAddress), byte("R"), 0x04, highByte(PacingTime), lowByte(PacingTime) };
  if (ReplyToAddress != -1) {
    CanBusSend(6);
  } else {
    SendSerial("Pacing:0x04:" + String(PacingTime));
  }
}

void PacingSet(int Data, int ReplyToAddress) {
  /*
  :param ReplyToAddress: reply address to put into the CAN packet header, defaults to -1
  :type ReplyToAddress: int
  :return: None
  :rtype: None
    */
  if (Data >= 0 && Data <= 65535) {
    EEPROM.update(2, highByte(Data));
    EEPROM.update(3, lowByte(Data));
    PacingTime = Data;
  }
  PacingResponse(ReplyToAddress);
}

void UnitsSystemResponse(int ReplyToAddress) {
  /*
  :param ReplyToAddress: reply address to put into the CAN packet header, defaults to -1
  :type ReplyToAddress: int
  :return: None
  :rtype: None
    */

  byte DataPacket[] = { highByte(ReplyToAddress), lowByte(ReplyToAddress), byte("R"), 0x05, byte(UnitSystem) };
  if (ReplyToAddress != -1) {
    CanBusSend(5);
  } else {
    SendSerial("UnitsSystem:0x05:" + String(UnitSystem));
  }
}

void UnitsSystemSet(char Data, int ReplyToAddress) {
  /*
  :param ReplyToAddress: reply address to put into the CAN packet header, defaults to -1
  :type ReplyToAddress: int
  :return: None
  :rtype: None
    */

  if (Data == 'I' || Data == 'M') {
    UnitSystem = Data;
    EEPROM.update(0, Data);
  }
  UnitsSystemResponse(ReplyToAddress);
}

void UnitsABRResponse(int ReplyToAddress) {
  /*

  :param ReplyToAddress: reply address to put into the CAN packet header, defaults to -1
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
      if (UnitSystem == 'I') {
        ABR = 0x03;
      } else {
        ABR = 0x02;
      }
      break;
    case 3:  // Voltage
      ABR = 0x04;
      break;
    case 4:  // Pressure
      if (UnitSystem == 'I') {
        ABR = 0x06;
      } else {
        ABR = 0x05;
      }
      break;
    case 5:  // Vacuum
      if (UnitSystem == 'I') {
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
    CanBusSend(2);
  } else {
    SendSerial("UnitABR:0x08:" + String(ABR));
  }
}

//----------------------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------------------

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

String PainlessInstructionSet(String& TestString) {
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
                  }
                }
              } else {
                for (int i = 0; i < (sizeof(AcceptedCommands) / sizeof(int)); i++) {
                  //Non Parameter Commands
                  if (CommandCandidate == AcceptedCommands[i]) {
                    //Serial.println("PIS Case 3B");
                    CommandToCall(i);
                    CommandCalled = true;
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
  ComPort.println(ThingToTest);

  switch (Index) {
    case 0:
      //SETUNITSYSTEM
      if (ThingToTest == "I" || ThingToTest == "M") {
        char FilteredCommand = 'I';
        if (ThingToTest == "M") { FilteredCommand = 'M'; }
        UnitsSystemSet(FilteredCommand, -1);
      } else {
        SendSerial("%R,Error,Invalid Parameter, I or M");
      }
      break;
    case 1:
      //SETSTREAMING
      if (ThingToTest == "0" || ThingToTest == "1") {
        StreamingModeSet(ThingToTest.toInt(), -1);
      } else {
        SendSerial("%R,Error,Invalid Parameter, 0 or 1");
      }
      break;
    case 2:
      //SETPACINGTIME
      if (ThingToTest.toInt() >= 0 && ThingToTest.toInt() <= 65535) {
        PacingSet(ThingToTest.toInt(), -1);
      } else {
        SendSerial("%R,Error,Invalid Parameter, 0 < x < 65535");
      }

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
      //STATE?
      StatusResponse(-1);
      break;
    case 5:
      //RESETERROR
      SendSerial("Error Reset:0x05");
      ResetError(-1);
      break;
    case 6:
      //REBOOT
      SendSerial("Rebooting:0x06");
      RebootDevice(-1);
      break;
  }
}

//----------------------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------------------