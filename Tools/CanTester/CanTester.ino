#include <EEPROM.h>
#include <SPI.h>

// CANBus
#include "mcp2515_can.h"
mcp2515_can CAN(9);
#define MAX_DATA_SIZE 8
byte cdata[MAX_DATA_SIZE] = { 0 };


String inputString = "";      // a String to hold incoming data
bool stringComplete = false;  // whether the string is complete

int TargetDeviceAddress = 100;
int ThisDeviceAddress = 1543;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  while (!Serial)
    ;
  inputString.reserve(200);

  CANBusSetup();
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
  CANBusRecieveCheck();
  delay(1000);
  SendSomething("1");
  delay(1000);
}



void Old(){
  // try to parse packet

  if (stringComplete) {
    
    SendSomething(inputString);
    // clear the string:
    inputString = "";
    stringComplete = false;
    Serial.println("Send which command? \n 0. Discovery\n 1. ");
  }
}

void SendSomething(String Command) {
  Serial.print(highByte(ThisDeviceAddress),HEX);
  Serial.println(lowByte(ThisDeviceAddress),HEX);
  switch (Command.toInt()) {
    case 0:
      Serial.println("Discovery");
      CanBusSend(ThisDeviceAddress, 8, highByte(ThisDeviceAddress),lowByte(ThisDeviceAddress),byte('?'),0x01,0x00,0xFF,0xFF,0x00);
      break;
    case 1:
      Serial.println("Status");
      CanBusSend(ThisDeviceAddress, 4, highByte(TargetDeviceAddress),lowByte(TargetDeviceAddress),byte('?'),0x02,0x00,0x00,0x00,0x00);
      break;
    case 2:
      Serial.println("Streaming Set 0");
      CanBusSend(ThisDeviceAddress, 5, highByte(TargetDeviceAddress),lowByte(TargetDeviceAddress),byte('S'),0x03,0x00,0x00,0x00,0x00);
      break;
    case 3:
      Serial.println("Streaming Set 1");
      CanBusSend(ThisDeviceAddress, 5, highByte(TargetDeviceAddress),lowByte(TargetDeviceAddress),byte('?'),0x03,0xFF,0x00,0x00,0x00);
      break;
    case 4:
      Serial.println("Streaming Query");
      CanBusSend(ThisDeviceAddress, 4, highByte(TargetDeviceAddress),lowByte(TargetDeviceAddress),byte('?'),0x03,0x00,0x00,0x00,0x00);
      break;
    case 5:
      Serial.println("Pacing Set 0");
      CanBusSend(ThisDeviceAddress, 6, highByte(TargetDeviceAddress),lowByte(TargetDeviceAddress),byte('S'),0x04,0x00,0x00,0x00,0x00);
      break;
    case 6:
      Serial.println("Pacing Set 1000");
      CanBusSend(ThisDeviceAddress, 6, highByte(TargetDeviceAddress),lowByte(TargetDeviceAddress),byte('S'),0x04,highByte(1000),lowByte(1000),0x00,0x00);
      break;
    case 7:
      Serial.println("Pacing Set 999999");
      CanBusSend(ThisDeviceAddress, 6, highByte(TargetDeviceAddress),lowByte(TargetDeviceAddress),byte('S'),0x04,highByte(999999),lowByte(999999),0x00,0x00);
      break;
    case 8:
      Serial.println("Pacing Query");
      CanBusSend(ThisDeviceAddress, 4, highByte(TargetDeviceAddress),lowByte(TargetDeviceAddress),byte('?'),0x04,0x00,0x00,0x00,0x00);
      break;
    case 9:
      Serial.println("Unit System Set I");
      CanBusSend(ThisDeviceAddress, 5, highByte(TargetDeviceAddress),lowByte(TargetDeviceAddress),byte('S'),0x05,byte('I'),0x00,0x00,0x00);
      break;
    case 10:
      Serial.println("Unit System Set M");
      CanBusSend(ThisDeviceAddress, 5, highByte(TargetDeviceAddress),lowByte(TargetDeviceAddress),byte('S'),0x05,byte('M'),0x00,0x00,0x00);
      break;
    case 11:
      Serial.println("Unit System Query");
      CanBusSend(ThisDeviceAddress, 4, highByte(TargetDeviceAddress),lowByte(TargetDeviceAddress),byte('?'),0x05,0x00,0x00,0x00,0x00);
      break;
    case 12:
      Serial.println("Units");
      CanBusSend(ThisDeviceAddress, 4, highByte(TargetDeviceAddress),lowByte(TargetDeviceAddress),byte('?'),0x08,0x00,0x00,0x00,0x00);
      break;
    case 13:
      Serial.println("IO Query");
      CanBusSend(ThisDeviceAddress, 4, highByte(TargetDeviceAddress),lowByte(TargetDeviceAddress),byte('?'),0x06,0x00,0x00,0x00,0x00);
      break;
    case 14:
      Serial.println("IO Set Out1 to 1");
      CanBusSend(ThisDeviceAddress, 8, highByte(TargetDeviceAddress),lowByte(TargetDeviceAddress),byte('S'),0x06,0b00000001,0x00,0x00,0x00);
      break;
    case 15:
      Serial.println("IO Set Out1 to 0");
      CanBusSend(ThisDeviceAddress, 8, highByte(TargetDeviceAddress),lowByte(TargetDeviceAddress),byte('S'),0x06,0b00000000,0x00,0x00,0x00);
      break;
    case 16:
      Serial.println("Error State");
      CanBusSend(ThisDeviceAddress, 8, highByte(TargetDeviceAddress),lowByte(TargetDeviceAddress),byte('?'),0x07,0x00,0x00,0x00,0x00);
      break;
    case 17:
      Serial.println("Reset Error State");
      CanBusSend(ThisDeviceAddress, 8, highByte(TargetDeviceAddress),lowByte(TargetDeviceAddress),byte('S'),0x07,0x00,0xFF,0xFF,0x00);
      break;
    case 18:
      Serial.println("Reboot Device");
      CanBusSend(ThisDeviceAddress, 8, highByte(TargetDeviceAddress),lowByte(TargetDeviceAddress),byte('S'),0x10,0x00,0x00,0x00,0x00);
      break;
    default:
      Serial.print("Commmand: ");
      Serial.print(Command);
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
    Serial.print(DataPacket[i],HEX);
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
    Serial.print(cdata[i],HEX);
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
