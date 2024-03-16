#include <CAN.h>

String inputString = "";      // a String to hold incoming data
bool stringComplete = false;  // whether the string is complete

int TargetDeviceAddress = 1;
int ThisDeviceAddress = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  while (!Serial)
    ;
  inputString.reserve(200);

  Serial.println("CAN Receiver");
  CAN.setPins(9, 2);
  // start the CAN bus at 500 kbps
  if (!CAN.begin(500E3)) {
    Serial.println("Starting CAN failed!");
    while (1)
      ;
  }

  CAN.onReceive(onReceive);
}

void loop() {
  // try to parse packet

  if (stringComplete) {

    SendSomething(inputString);
    // clear the string:
    inputString = "";
    stringComplete = false;
  }
}

void SendSomething(String Command) {
  byte DataPacket[8];
  switch (Command.toInt()) {
    case 0:
      Serial.println("Discovery");
      DataPacket[0] = highByte(ThisDeviceAddress);
      DataPacket[1] = lowByte(ThisDeviceAddress);
      DataPacket[2] = byte('?');
      DataPacket[3] = 0x01;
      DataPacket[4] = 0x00;
      DataPacket[5] = 0xFF;
      DataPacket[6] = 0xFF;
      DataPacket[7] = 0x00;
      CANBusSend(7, false, DataPacket);
      break;
    case 1:
      Serial.println("Status");
      DataPacket[0] = highByte(ThisDeviceAddress);
      DataPacket[1] = lowByte(ThisDeviceAddress);
      DataPacket[2] = byte('?');
      DataPacket[3] = 0x02;
      CANBusSend(4, false, DataPacket);
      break;
    case 2:
      Serial.println("Streaming Set 0");
      DataPacket[0] = highByte(ThisDeviceAddress);
      DataPacket[1] = lowByte(ThisDeviceAddress);
      DataPacket[2] = byte('S');
      DataPacket[3] = 0x03;
      DataPacket[4] = 0x00;
      CANBusSend(4, false, DataPacket);
      break;
    case 3:
      Serial.println("Streaming Set 1");
      DataPacket[0] = highByte(ThisDeviceAddress);
      DataPacket[1] = lowByte(ThisDeviceAddress);
      DataPacket[2] = byte('?');
      DataPacket[3] = 0x03;
      DataPacket[4] = 0xFF;
      CANBusSend(4, false, DataPacket);
      break;
    case 4:
      Serial.println("Streaming Query");
      DataPacket[0] = highByte(ThisDeviceAddress);
      DataPacket[1] = lowByte(ThisDeviceAddress);
      DataPacket[2] = byte('?');
      DataPacket[3] = 0x03;
      CANBusSend(3, false, DataPacket);
      break;
    case 5:
      Serial.println("Pacing Set 0");
      DataPacket[0] = highByte(ThisDeviceAddress);
      DataPacket[1] = lowByte(ThisDeviceAddress);
      DataPacket[2] = byte('S');
      DataPacket[3] = 0x04;
      DataPacket[4] = 0x00;
      DataPacket[5] = 0x00;
      CANBusSend(5, false, DataPacket);
      break;
    case 6:
      Serial.println("Pacing Set 1000");
      DataPacket[0] = highByte(ThisDeviceAddress);
      DataPacket[1] = lowByte(ThisDeviceAddress);
      DataPacket[2] = byte('S');
      DataPacket[3] = 0x04;
      DataPacket[4] = highByte(1000);
      DataPacket[5] = lowByte(1000);
      CANBusSend(5, false, DataPacket);
      break;
    case 7:
      Serial.println("Pacing Set 999999");
      DataPacket[0] = highByte(ThisDeviceAddress);
      DataPacket[1] = lowByte(ThisDeviceAddress);
      DataPacket[2] = byte('S');
      DataPacket[3] = 0x04;
      DataPacket[4] = highByte(999999);
      DataPacket[5] = lowByte(999999);
      CANBusSend(5, false, DataPacket);
      break;
    case 8:
      Serial.println("Pacing Query");
      DataPacket[0] = highByte(ThisDeviceAddress);
      DataPacket[1] = lowByte(ThisDeviceAddress);
      DataPacket[2] = byte('?');
      DataPacket[3] = 0x01;
      DataPacket[4] = 0x00;
      DataPacket[5] = 0xFF;
      DataPacket[6] = 0xFF;
      DataPacket[7] = 0x00;
      CANBusSend(4, false, DataPacket);
      break;
    case 9:
      Serial.println("Unit System Set I");
      DataPacket[0] = highByte(ThisDeviceAddress);
      DataPacket[1] = lowByte(ThisDeviceAddress);
      DataPacket[2] = byte('S');
      DataPacket[3] = 0x05;
      DataPacket[4] = byte('I');
      CANBusSend(5, false, DataPacket);
      break;
    case 10:
      Serial.println("Unit System Set M");
      DataPacket[0] = highByte(ThisDeviceAddress);
      DataPacket[1] = lowByte(ThisDeviceAddress);
      DataPacket[2] = byte('S');
      DataPacket[3] = 0x05;
      DataPacket[4] = byte('M');
      CANBusSend(5, false, DataPacket);
      break;
    case 11:
      Serial.println("Unit System Query");
      DataPacket[0] = highByte(ThisDeviceAddress);
      DataPacket[1] = lowByte(ThisDeviceAddress);
      DataPacket[2] = byte('?');
      DataPacket[3] = 0x05;
      CANBusSend(4, false, DataPacket);
      break;
    case 12:
      Serial.println("Units");
      DataPacket[0] = highByte(ThisDeviceAddress);
      DataPacket[1] = lowByte(ThisDeviceAddress);
      DataPacket[2] = byte('?');
      DataPacket[3] = 0x08;
      CANBusSend(4, false, DataPacket);
      break;
    case 13:
      Serial.println("IO Query");
      DataPacket[0] = highByte(ThisDeviceAddress);
      DataPacket[1] = lowByte(ThisDeviceAddress);
      DataPacket[2] = byte('?');
      DataPacket[3] = 0x06;
      CANBusSend(4, false, DataPacket);
      break;
    case 14:
      Serial.println("IO Set Out1 to 1");
      DataPacket[0] = highByte(ThisDeviceAddress);
      DataPacket[1] = lowByte(ThisDeviceAddress);
      DataPacket[2] = byte('S');
      DataPacket[3] = 0x06;
      DataPacket[4] = 0x00;
      DataPacket[5] = 0b00000001;
      CANBusSend(6, false, DataPacket);
      break;
    case 15:
      Serial.println("IO Set Out1 to 0");
      DataPacket[0] = highByte(ThisDeviceAddress);
      DataPacket[1] = lowByte(ThisDeviceAddress);
      DataPacket[2] = byte('?');
      DataPacket[3] = 0x06;
      DataPacket[4] = 0x00;
      DataPacket[5] = 0b00000001;
      CANBusSend(6, false, DataPacket);
      break;
    case 16:
      Serial.println("Error State");
      DataPacket[0] = highByte(ThisDeviceAddress);
      DataPacket[1] = lowByte(ThisDeviceAddress);
      DataPacket[2] = byte('?');
      DataPacket[3] = 0x07;
      CANBusSend(4, false, DataPacket);
      break;
    case 17:
      Serial.println("Reset Error State");
      DataPacket[0] = highByte(ThisDeviceAddress);
      DataPacket[1] = lowByte(ThisDeviceAddress);
      DataPacket[2] = byte('S');
      DataPacket[3] = 0x07;
      CANBusSend(4, false, DataPacket);
      break;
    case 18:
      Serial.println("Reboot Device");
      DataPacket[0] = highByte(ThisDeviceAddress);
      DataPacket[1] = lowByte(ThisDeviceAddress);
      DataPacket[2] = byte('S');
      DataPacket[3] = 0x10;
      CANBusSend(4, false, DataPacket);
      break;
    default:
      Serial.print("Commmand: ");
      Serial.print(Command);
      Serial.println(" is not supported");
      break;
  }
}


void CANBusSend(int NumberOfBytes, bool RequestResponse, byte Data[]) {
  /*

  :param ReplyToAddress: reply address to put into the CAN packet header, defaults to -1
  :type ReplyToAddress: int
  :return: None
  :rtype: None
    */
  CAN.beginPacket(ThisDeviceAddress, NumberOfBytes, RequestResponse);
  Serial.print("CAN Packet:");
  for (uint8_t Index = 0; Index < (NumberOfBytes + 1); Index++) {
    CAN.write(Data[Index]);
    Serial.print(Data[Index], HEX);
  }
  Serial.println();
  CAN.endPacket();
}


void onReceive(int packetSize) {
  // received a packet
  Serial.print("Received ");

  if (CAN.packetExtended()) {
    Serial.print("extended ");
  }

  if (CAN.packetRtr()) {
    // Remote transmission request, packet contains no data
    Serial.print("RTR ");
  }

  Serial.print("packet with id 0x");
  Serial.print(CAN.packetId(), HEX);

  if (CAN.packetRtr()) {
    Serial.print(" and requested length ");
    Serial.println(CAN.packetDlc());
  } else {
    Serial.print(" and length ");
    Serial.println(packetSize);

    // only print packet data for non-RTR packets
    while (CAN.available()) {
      Serial.print((char)CAN.read());
    }
    Serial.println();
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
