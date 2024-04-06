#include <EEPROM.h>
#include <CAN.h>

bool Streaming;
char UnitSystem;
long PacingTimer;
long PacingTime;
int DeviceType = 0;
int PacketIdentifer = 0;
byte DataPacket[8] = {};

#define ComPort Serial
String inputString = "";      // a String to hold incoming data from ports
bool stringComplete = false;  // whether the string is complete for each respective port

#define ErrorLEDpin 3
int ErrorNumber = 0;

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
  ComPort.begin(9600);
  pinMode(ErrorLEDpin, OUTPUT);
  digitalWrite(ErrorLEDpin, HIGH);
  delay(250);
  digitalWrite(ErrorLEDpin, LOW);

  GetDeviceAddressFromMemory();

  // Setup CAN Bus
  CAN.setPins(9, 2);
  if (!CAN.begin(500E3)) {
    ComPort.println("Starting CAN failed!");
    ErrorNumber = 2;
  }

  // register the receive callback
  CAN.onReceive(onReceive);

  GetUnitSystemFromMemory();
  ComPort.print("UnitSystem:");
  ComPort.println(UnitSystem);

  GetStreamingFromMemory();
  ComPort.print("Streaming:");
  ComPort.println(Streaming);

  GetPacingTimeFromMemory();
  ComPort.print("Pacing:");
  ComPort.println(PacingTime);

  //Announce
  DiscoveryResponse(0);
}

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

void (*resetFunc)(void) = 0;  // declare reset fuction at address 0

void CANBusSend(int NumberOfBytes, bool RequestResponse, byte Data[]) {
  /*

  :param ReplyToAddress: reply address to put into the CAN packet header, defaults to -1
  :type ReplyToAddress: int
  :return: None
  :rtype: None
    */
  CAN.beginPacket(PacketIdentifer, NumberOfBytes, RequestResponse);

  for (uint8_t Index = 0; Index < NumberOfBytes; Index++) {
    CAN.write(Data[Index]);
  }

  CAN.endPacket();
}

void GetDeviceAddressFromMemory() {
  PacketIdentifer = EEPROM.read(1) << 8 | EEPROM.read(0);
  SendSerial("CAN Bus Address: " + String(PacketIdentifer));
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

void RebootDevice(int ReplyToAddress) {
  byte DataPacket[] = { highByte(ReplyToAddress), lowByte(ReplyToAddress), byte("R"), 0x10, 0xFF, 0x10, 0xFF, 0x10 };
  CANBusSend(7, false, DataPacket);
  SendSerial("Device is Rebooting");
  delay(2500);
  resetFunc();
}

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
    CANBusSend(5, false, DataPacket);
  } else {
    SendSerial("Error:0x07:" + String(ErrorNumber));
  }
}

void ResetError(int ReplyToAddress) {
  ErrorNumber = 0;
  GetError(ReplyToAddress);
}

void loop() {
  if (ErrorNumber != 0) {
    // blink the error LED the number of times of the error and then wait 1 second to blink it again
    for (int i = 0; i < ErrorNumber; i++) {
      digitalWrite(ErrorLEDpin, HIGH);
      delay(150);
      digitalWrite(ErrorLEDpin, LOW);
      delay(500);
    }
    delay(1000);
  } else {
    //if no error run the main program
    long CurrentTime = millis();
    if (Streaming == true) {
      if (PacingTime != 0) {
        if (abs(PacingTimer - CurrentTime) > PacingTime) {
          //StatusResponse();
          PacingTimer = CurrentTime;
        }
      } else {
        //StatusResponse();
      }
    }
  }

  serialEvent();

  if (stringComplete) {
    inputString = PainlessInstructionSet(inputString);
    stringComplete = false;
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

void SetDeviceAddress(int Address) {
  if (Address >= 1 && Address <= 2047) {
    EEPROM.update(1, highByte(Address));
    EEPROM.update(0, lowByte(Address));
    GetDeviceAddressFromMemory();
  } else {
    SendSerial("Address must be between 1 and 2047");
  }
}

void onReceive(int packetSize) {
  /*
  :param packetSize: CAN packet size
  :type packetSize: int
  :return: None
  :rtype: None
    */
  int ID = CAN.packetId();
  byte RXData[packetSize];
  if (ID != PacketIdentifer) {
    if (CAN.available()) {
      for (uint8_t Index = 0; Index < packetSize; Index++)
        RXData[Index] = CAN.read();
    }

    //Discovery

    if (RXData[2] == '?' && RXData[2] == 0x01) {  // Handle the Discovery Packet
      DiscoveryResponse(RXData[0]);
    }

    //Check Device Address
    byte ReplyDeviceAddress = RXData[0] << 8 + RXData[1];

    if (ReplyDeviceAddress != PacketIdentifer) {
      if (RXData[2] == '?') {  // check if the packet is a Query
        switch (RXData[2]) {
          case 0x02:
            //Status
            StatusResponse(ReplyDeviceAddress);
            break;
          case 0x03:
            // Streaming Mode
            StreamingModeResponse(ReplyDeviceAddress);
            break;
          case 0x04:
            // Pacing
            PacingResponse(ReplyDeviceAddress);
            break;
          case 0x05:
            // Units
            UnitsSystemResponse(ReplyDeviceAddress);
            break;
          case 0x06:
            // i/o
            break;
          default:
            // return Error that this command isn't supported
            break;
        }

      } else if (RXData[2] == 'S') {  // check if the packet is a Set
        switch (RXData[2]) {
          case 0x03:
            // Streaming Mode
            StreamingModeResponse(ReplyDeviceAddress);
            break;
          case 0x04:
            // Pacing
            PacingResponse(ReplyDeviceAddress);
            break;
          case 0x05:
            // Units
            UnitsSystemResponse(ReplyDeviceAddress);
            break;
          case 0x06:
            // i/o
            break;
          case 0x08:
            // Unit ABR
            UnitsABRResponse(ReplyDeviceAddress);
            break;
          default:
            // return Error that this command isn't supported
            break;
        }
      }
    }
  } else {
    //if ID == PacketIdentifer then there are possibly two devices on the BUS with the same ID/PacketIdentifer post an error
  }
}

void DiscoveryResponse(int ReplyToAddress) {
  /*
  :param ReplyToAddress: reply address to put into the CAN packet header
  :type ReplyToAddress: int
  :return: None
  :rtype: None
    */
  byte DataPacket[] = { highByte(ReplyToAddress), lowByte(ReplyToAddress), byte("R"), 0x01, 0x01, byte(DeviceType), byte(DeviceType), byte(DeviceType) };
  CANBusSend(7, false, DataPacket);
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
    CANBusSend(7, false, DataPacket);
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
    CANBusSend(5, false, DataPacket);
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
    CANBusSend(6, false, DataPacket);
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
    CANBusSend(5, false, DataPacket);
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
    CANBusSend(2, false, ABR);
  } else {
    SendSerial("UnitABR:0x08:" + String(ABR));
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