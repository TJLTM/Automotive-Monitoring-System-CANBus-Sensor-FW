#include <CAN.h>

String inputString = "";      // a String to hold incoming data
bool stringComplete = false;  // whether the string is complete

int TargetDeviceAddress = 1; 
int ThisDeviceAddress = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  while (!Serial);
  inputString.reserve(200);

  Serial.println("CAN Receiver");

  // start the CAN bus at 500 kbps
  if (!CAN.begin(500E3)) {
    Serial.println("Starting CAN failed!");
    while (1);
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

void SendSomething(String Command){
  switch(Command.toInt()){
    case 0:
      //Discovery 
      CANBusSend();
      break;
    case 1: 
      //Status
      break;
    case 2: 
      //Streaming Set
      break;
    case 3: 
      //Streaming Query
      break;
    case 4:
      //Pacing Set
      break;
    case 5: 
      //Pacing Query
      break;
    case 6: 
      //Unit System Set
      break;
    case 7: 
      //Unit System Query
      break;
    case 8:
      //Units 
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
  CAN.beginPacket(PacketIdentifer, NumberOfBytes, RequestResponse);

  for (uint8_t Index = 0; Index < NumberOfBytes; Index++) {
    CAN.write(Data[Index]);
  }

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
    if (inChar == '\n') {
      stringComplete = true;
    }
  }
}


