#include <EEPROM.h>
#include <CAN.h>

bool Streaming;
char Units;
long PacingTimer;
long PacingTime;
int DeviceType = 0;

#define ErrorLEDpin 3
int ErrorNumber = 0;

int PacketIdentifer = EEPROM.read(3) << 24 + EEPROM.read(2) << 16 + EEPROM.read(1) << 8 +  EEPROM.read(0);

void setup() {
  Serial.begin(9600);
  pinMode(ErrorLEDpin, OUTPUT);
  digitalWrite(ErrorLEDpin, HIGH);
  delay(250);
  digitalWrite(ErrorLEDpin, LOW);
  

  Serial.print("CAN Address: ");
  Serial.println(PacketIdentifer);

  // Setup CAN Bus
  CAN.setPins(9, 2);
  if (!CAN.begin(500E3)) {
    Serial.println("Starting CAN failed!");
    ErrorNumber = 2;
  }

  // register the receive callback
  CAN.onReceive(onReceive);

  int TempValue;
  //Read Units out of EEPROM
  TempValue = EEPROM.read(4);
  if (TempValue == "I" || TempValue == "M") {
    Units = TempValue;
  }
  else {
    Units = "I";
    EEPROM.update(0, "I");
  }

  //Read Stream Value out of EEPROM
  TempValue = EEPROM.read(5);
  if (TempValue == 0 || TempValue == 1) {
    Streaming = TempValue;
  }
  else {
    Streaming = 1;
    EEPROM.update(1, 1);
  }

  //Read Pacing value out of EEPROM
  TempValue = EEPROM.read(6) << 8 || EEPROM.read(7);
  if (TempValue >= 0 && TempValue <= 65535) {
    PacingTime = TempValue;
  }
  else {
    PacingTime = 0;
    EEPROM.update(6, 0);
    EEPROM.update(7, 0);
  }

  Serial.print("Units:");
  Serial.println(Units);

  Serial.print("Streaming:");
  Serial.println(Streaming);

  Serial.print("Pacing:");
  Serial.println(PacingTime);

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
  }
  else {
    //if no error run the main program
    long CurrentTime = millis();
    if (Streaming == true) {
      if (PacingTime != 0) {
        if (abs(PacingTimer - CurrentTime) > PacingTime) {
          StatusResponse();
          PacingTimer = CurrentTime;
        }
      }
      else {
        StatusResponse();
      }
    }
  }

}

void onReceive(int packetSize) {
  bool RTR = CAN.packetRtr();
  int ID = CAN.packetId();
  byte RXData[packetSize];

  if (CAN.available()) {
    for (uint8_t Index = 0; Index < packetSize; Index++)
      RXData[Index] = CAN.read();
  }

  //Discovery

  if (RXData[2] == "?" && RXData[2] == 0x01) { // Handle the Discovery Packet
    DiscoveryResponse(RXData[0]);
  }

  //Check Device Address
  byte TargetDeviceAddress =  RXData[0] << 8 + RXData[1];

  if (TargetDeviceAddress == PacketIdentifer) {

    if (RXData[2] == "?") { // check if the packet is a Query

      //Streaming
      //Pacing
      //Units
      //Unit ABR

    } else if (RXData[2] == "S") {// check if the packet is a Set
      //Streaming
      //Pacing
      //Units
      //Unit ABR
    }
  }
}




void CANBusSend(int NumberOfBytes, bool RequestResponse, byte Data[]) {
  CAN.beginPacket(PacketIdentifer, NumberOfBytes, RequestResponse);

  for (uint8_t Index = 0; Index < NumberOfBytes; Index++) {
    CAN.write(Data[Index]);
  }

  CAN.endPacket();
}

float ReadSensors() {
  float Value = 0;


  return Value;
}

void StatusResponse(int ReplyToAddress) {
  int ChannelNumber = 0;
  float ReturnedValue = ReadSensors();
  byte DataPacket[] = {highByte(ReplyToAddress), lowByte(ReplyToAddress), byte("R"), 0x02, byte(ChannelNumber), highByte(ReturnedValue), lowByte(ReturnedValue), byte(DeviceType)};
  CANBusSend(7, false, DataPacket);
}

void DiscoveryResponse(int ReplyToAddress) {
  byte DataPacket[] = {highByte(ReplyToAddress), lowByte(ReplyToAddress), byte("R"), 0x01, 0x01, byte(DeviceType),byte(DeviceType),byte(DeviceType)};
  CANBusSend(7, false, DataPacket);
}

void StreamingModeResponse() {
  byte DataPacket[] = {highByte(ReplyToAddress), lowByte(ReplyToAddress), byte("R"), 0x03, byte(Streaming)};
  CANBusSend(5, false, DataPacket);
}

void StreamingModeSet(bool Data) {
  if (Data == true || Data == false) {
    EEPROM.update(1, Data);
    Streaming = Data;
  }
  StreamingModeResponse();
}

void PacingResponse() {
  byte DataPacket[] = {highByte(ReplyToAddress), lowByte(ReplyToAddress), byte("C"), highByte(PacingTime), lowByte(PacingTime)};
  CANBusSend(6, false, DataPacket);
}

void PacingSet(int Data) {
  if (Data >= 0 && Data <= 65535) {
    EEPROM.update(2, highByte(Data));
    EEPROM.update(3, lowByte(Data));
    PacingTime = Data;
  }
  PacingResponse();
}

void UnitsResponse() {
  byte DataPacket[] = {highByte(ReplyToAddress), lowByte(ReplyToAddress), byte("N"), byte("T"), byte(Units)};
  CANBusSend(5, false, DataPacket);
}

void UnitsSet(char Data) {
  if (Data == "I" || Data == "M") {
    EEPROM.update(0, Data);
  }
  UnitsResponse();
}

void UnitsABRResponse() {
  byte ABR[] = {0, 0, 0, 0};
  switch (DeviceType) {
    case 1: // Current
      ABR[3] = byte("A");
      break;
    case 2: // Temp
      if (Units == "I") {
        ABR[3] = byte("F");
      }
      else {
        ABR[3] = byte("C");
      }
      break;
    case 3: // Voltage
      ABR[3] = byte("V");
      break;
    case 4: // Pressure
      if (Units == "I") {
        ABR[1] = byte("P");
        ABR[2] = byte("S");
        ABR[3] = byte("I");
      }
      else {
        ABR[1] = byte("K");
        ABR[2] = byte("P");
        ABR[3] = byte("A");
      }
      break;
    case 5: // Vacuum
      if (Units == "I") {
        ABR[0] = byte("I");
        ABR[1] = byte("N");
        ABR[2] = byte("H");
        ABR[3] = byte("G");
      }
      else {
        ABR[1] = byte("K");
        ABR[2] = byte("P");
        ABR[3] = byte("A");
      }
      break;
    case 6: // I/O
      ABR[0] = byte("B");
      ABR[1] = byte("O");
      ABR[2] = byte("O");
      ABR[3] = byte("L");
      break;
    case 7: // RPM
      ABR[1] = byte("R");
      ABR[2] = byte("P");
      ABR[3] =  byte("A");
      break;
  }

  CANBusSend(4, false, ABR);
}
