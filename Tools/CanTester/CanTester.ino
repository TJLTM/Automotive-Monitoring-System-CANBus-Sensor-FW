#include <EEPROM.h>
#include <SPI.h>

// CANBus
#include "mcp2515_can.h"
mcp2515_can CAN(9);
#define MAX_DATA_SIZE 8
byte cdata[MAX_DATA_SIZE] = { 0 };


String inputString = "";      // a String to hold incoming data
bool stringComplete = false;  // whether the string is complete

int DeviceSubnet = 0;
int TargetDeviceAddress = (DeviceSubnet * 256) + 100;
int Address = 1;
int ThisDeviceAddress = (DeviceSubnet * 256) + Address;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  while (!Serial)
    ;
  inputString.reserve(200);
  CANBusSetup();

  for (int i = 0; i <= 15; i++) {
    SendSomething(i);
    delay(2000);
    CANBusRecieveCheck();
  }
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

void loop() {
  //do nothin over and over
}

int ConvertTargetAddressToInt() {
  return TargetDeviceAddress -  (DeviceSubnet * 256);
}

void SendSomething(int Test) {
  switch (Test) {
    case 0:
      Serial.println("Discovery");
      CanBusSend(ThisDeviceAddress, 8, ConvertTargetAddressToInt(), 0x00, byte('?'), 0x00, 0xFF, 0x00, 0xFF, 0x00);
      break;
    case 1:
      Serial.println("Status");
      CanBusSend(ThisDeviceAddress, 4, ConvertTargetAddressToInt(), 0x01, byte('?'), 0x00, 0x00, 0x00, 0x00, 0x00);
      break;
    case 2:
      Serial.println("Streaming Set off");
      CanBusSend(ThisDeviceAddress, 5, ConvertTargetAddressToInt(), 0x02, byte('S'), 0x00, 0x00, 0x00, 0x00, 0x00);
      break;
    case 3:
      Serial.println("Streaming Set on");
      CanBusSend(ThisDeviceAddress, 5, ConvertTargetAddressToInt(), 0x02, byte('S'), 0xFF, 0x00, 0x00, 0x00, 0x00);
      break;
    case 4:
      Serial.println("Streaming Query");
      CanBusSend(ThisDeviceAddress, 4, ConvertTargetAddressToInt(), 0x02, byte('?'), 0x00, 0x00, 0x00, 0x00, 0x00);
      break;
    case 5:
      Serial.println("Pacing Set 0");
      CanBusSend(ThisDeviceAddress, 6, ConvertTargetAddressToInt(), 0x03, byte('S'), 0x00, 0x00, 0x00, 0x00, 0x00);
      break;
    case 6:
      Serial.println("Pacing Set 1000");
      CanBusSend(ThisDeviceAddress, 6, ConvertTargetAddressToInt(), 0x03, byte('S'), highByte(1000), lowByte(1000), 0x00, 0x00, 0x00);
      break;
    case 7:
      Serial.println("Pacing Set 999999");
      CanBusSend(ThisDeviceAddress, 6, ConvertTargetAddressToInt(), 0x03, byte('S'), highByte(999999), lowByte(999999), 0x00, 0x00, 0x00);
      break;
    case 8:
      Serial.println("Pacing Query");
      CanBusSend(ThisDeviceAddress, 4, ConvertTargetAddressToInt(), 0x03, byte('?'), 0x00, 0x00, 0x00, 0x00, 0x00);
      break;
    case 9:
      Serial.println("Unit System Set I");
      CanBusSend(ThisDeviceAddress, 5, ConvertTargetAddressToInt(), 0x04, byte('S'), byte('I'), 0x00, 0x00, 0x00, 0x00);
      break;
    case 10:
      Serial.println("Unit System Set M");
      CanBusSend(ThisDeviceAddress, 5, ConvertTargetAddressToInt(), 0x04, byte('S'), byte('M'), 0x00, 0x00, 0x00, 0x00);
      break;
    case 11:
      Serial.println("Unit System Query");
      CanBusSend(ThisDeviceAddress, 4, ConvertTargetAddressToInt(), 0x04, byte('?'), 0x00, 0x00, 0x00, 0x00, 0x00);
      break;
    case 12:
      Serial.println("Units ABR");
      CanBusSend(ThisDeviceAddress, 4, ConvertTargetAddressToInt(), 0x07, byte('?'), 0x00, 0x00, 0x00, 0x00, 0x00);
      break;
    case 13:
      Serial.println("IO Query");
      CanBusSend(ThisDeviceAddress, 4, ConvertTargetAddressToInt(), 0x05, byte('?'), 0x00, 0x00, 0x00, 0x00, 0x00);
      break;
    case 14:
      Serial.println("IO Set Out1 to 1");
      CanBusSend(ThisDeviceAddress, 8, ConvertTargetAddressToInt(), 0x05, byte('S'), 0x00, 0b00000001, 0x00, 0x00, 0x00);
      break;
    case 15:
      Serial.println("IO Set Out1 to 0");
      CanBusSend(ThisDeviceAddress, 8, ConvertTargetAddressToInt(), 0x05, byte('S'), 0x00, 0b00000000, 0x00, 0x00, 0x00);
      break;
    case 16:
      Serial.println("Error State");
      CanBusSend(ThisDeviceAddress, 8, ConvertTargetAddressToInt(), 0x08, byte('?'), 0x00, 0x00, 0x00, 0x00, 0x00);
      break;
    case 17:
      Serial.println("Reset Error State");
      CanBusSend(ThisDeviceAddress, 8, ConvertTargetAddressToInt(), 0x08, byte('S'), 0x0A, 0x0A, 0xFA, 0xFF, 0x00);
      break;
    case 18:
      Serial.println("DeviceTemp");
      CanBusSend(ThisDeviceAddress, 8, ConvertTargetAddressToInt(), 0x0A, byte('?'), 0x00, 0x00, 0x00, 0x00, 0x00);
      break;
    case 19:
      Serial.println("Max Sensor Channel");
      CanBusSend(ThisDeviceAddress, 8, ConvertTargetAddressToInt(), 0x0B, byte('?'), 0x00, 0x00, 0x00, 0x00, 0x00);
      break;
    case 20:
      Serial.println("Sensor Channel Range Max");
      CanBusSend(ThisDeviceAddress, 8, ConvertTargetAddressToInt(), 0x0C, byte('?'), 0x00, 0x00, 0x00, 0x00, 0x00);
      break;
    case 21:
      Serial.println("Sensor Channel Range Min");
      CanBusSend(ThisDeviceAddress, 8, ConvertTargetAddressToInt(), 0x0D, byte('?'), 0x00, 0x00, 0x00, 0x00, 0x00);
      break;
    case 22:
      Serial.println("Sensor Channel Range type");
      CanBusSend(ThisDeviceAddress, 8, ConvertTargetAddressToInt(), 0x0E, byte('?'), 0x00, 0x00, 0x00, 0x00, 0x00);
      break;
    case 23:
      Serial.println("RGB Color set");
      CanBusSend(ThisDeviceAddress, 8, ConvertTargetAddressToInt(), 0x0F, byte('S'), 0xFF, 0xFF, 0xFF, 0x00, 0x00);
      break;
    case 24:
      Serial.println("RGB Color query");
      CanBusSend(ThisDeviceAddress, 8, ConvertTargetAddressToInt(), 0x0F, byte('?'), 0x00, 0x00, 0x00, 0x00, 0x00);
      break;
    case 25: //always put this at the end of the test
      Serial.println("Reboot Device");
      CanBusSend(ThisDeviceAddress, 8, ConvertTargetAddressToInt(), 0x09, byte('S'), 0x0A, 0x0A, 0x0A, 0x0A, 0x0A);
      break;
    default:
      Serial.print("Test: ");
      Serial.print(Test);
      Serial.println(" is not supported");
      break;
  }
}


void CanBusSend(int PacketIdentifier, int DataLength, byte Zero, byte One, byte Two, byte Three, byte Four, byte Five, byte Six, byte Seven) {
  // ID, ext, len, byte: data
  //ext = 0 for standard frame
  byte DataPacket[8] = { Zero, One, Two, Three, Four, Five, Six, Seven };  //construct data packet array
  Serial.print("Sending CAN Data:");
  for (uint8_t i = 0; i < DataLength; i++) {
    Serial.print(i);
    Serial.print(": ");
    Serial.print(DataPacket[i], HEX);
    Serial.print(",");
  }
  Serial.println();

  CAN.sendMsgBuf(PacketIdentifier, 0, DataLength, DataPacket);
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

  Serial.print("got some CAN Data:ID:");
  Serial.print(CAN.getCanId());
  Serial.print(" Data:");
  for (uint8_t i = 1; i < 8; i++) {
    Serial.print(i);
    Serial.print(": ");
    Serial.print(cdata[i], HEX);
    Serial.print(",");
  }
  Serial.println();
}



void serialEvent() {
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read();
    // add it to the inputString:
    inputString += inChar;
    // if the incoming character is a newline, set a flag so the main loop can
    // do something about it:
    if (inChar == '\r') {
      stringComplete = true;
    }
  }
}
