#include <EEPROM.h>
#include <Adafruit_MCP23X08.h>
#include <Adafruit_MCP23XXX.h>
#include <CAN.h>

bool Streaming;
char Units;
long PacingTimer;
long PacingTime;
int DeviceType = 0;

#define ErrorLEDpin 3
int ErrorNumber = 0;

Adafruit_MCP23X08 AddressGPIO;
int PacketIdentifer = 128;

void setup() {
  Serial.begin(9600);
  pinMode(ErrorLEDpin, OUTPUT);
  digitalWrite(ErrorLEDpin, HIGH);
  delay(250);
  digitalWrite(ErrorLEDpin, LOW);

  // Setup GPIO expander
  if (!AddressGPIO.begin_I2C()) {
    Serial.println("Can Not Communicate with MCP23008");
    ErrorNumber = 1;
  }

  if (ErrorNumber != 0) {
    //Setup Address pins
    AddressGPIO.pinMode(0, INPUT);
    AddressGPIO.pinMode(1, INPUT);
    AddressGPIO.pinMode(2, INPUT);
    AddressGPIO.pinMode(3, INPUT);
    AddressGPIO.pinMode(4, INPUT);
    AddressGPIO.pinMode(5, INPUT);
    AddressGPIO.pinMode(6, INPUT);
    AddressGPIO.pinMode(7, INPUT);
    // Read MCP23008 for Packet Identifier
    byte AddressByte;
    bitWrite(AddressByte, 0, AddressGPIO.digitalRead(0));
    bitWrite(AddressByte, 1, AddressGPIO.digitalRead(1));
    bitWrite(AddressByte, 2, AddressGPIO.digitalRead(2));
    bitWrite(AddressByte, 3, AddressGPIO.digitalRead(3));
    bitWrite(AddressByte, 4, AddressGPIO.digitalRead(4));
    bitWrite(AddressByte, 5, AddressGPIO.digitalRead(5));
    bitWrite(AddressByte, 6, AddressGPIO.digitalRead(6));
    bitWrite(AddressByte, 7, AddressGPIO.digitalRead(7));

    PacketIdentifer = PacketIdentifer + AddressByte;
  }

  Serial.print("CAN Address: ");
  Serial.println(PacketIdentifer);

  // Setup CAN Bus
  if (!CAN.begin(500E3)) {
    Serial.println("Starting CAN failed!");
    ErrorNumber = 2;
  }

  // register the receive callback
  CAN.onReceive(onReceive);

  int TempValue;
  //Read Units out of EEPROM
  TempValue = EEPROM.read(0);
  if (TempValue == "I" || TempValue == "M") {
    Units = TempValue;
  }
  else {
    Units = "I";
    EEPROM.update(0, "I");
  }

  //Read Stream Value out of EEPROM
  TempValue = EEPROM.read(1);
  if (TempValue == 0 || TempValue == 1) {
    Streaming = TempValue;
  }
  else {
    Streaming = 1;
    EEPROM.update(1, 1);
  }

  //Read Pacing value out of EEPROM
  TempValue = EEPROM.read(2) << 8 || EEPROM.read(3);
  if (TempValue >= 0 && TempValue <= 65535) {
    PacingTime = TempValue;
  }
  else {
    PacingTime = 0;
    EEPROM.update(2, 0);
    EEPROM.update(3, 0);
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

  if (RXData[1] == "?" && RXData[2] == "W") { // Handle the Discovery Packet
    DiscoveryResponse(RXData[0]);
  }


  if (RXData[1] == "?" || RXData[1] == "S" || RXData[1] == "W") { // check if the packet is a Query or Set
    if (RXData[0] ==  PacketIdentifer) { //check if this is the target device
      //Status
      if (RXData[2] == "S" && RXData[3] == "T" && RXData[4] == "A" && RXData[5] == "T" && RXData[6] == "U" && RXData[7] == "S") {
        StatusResponse();
      }

      //Streaming
      if (RXData[2] == "S" && RXData[3] == "T" && RXData[4] == "A" && RXData[5] == "T" && RXData[6] == "U" && RXData[7] == "S") {
        if (RXData[1] == "?") {
          StreamingModeResponse();
        }
        if (RXData[1] == "S") {
          StreamingModeSet(RXData[4]);
        }
      }

      //Pacing
      if (RXData[2] == "P" && RXData[3] == "A" && RXData[4] == "C") {
        if (RXData[1] == "?") {
          PacingResponse();
        }
        if (RXData[1] == "S") {
          int TempValue = RXData[5] << 8 || RXData[6];
          PacingSet(TempValue);
        }
      }

      //Units
      if (RXData[2] == "U" && RXData[3] == "N" && RXData[4] == "T") {
        if (RXData[1] == "?") {
          UnitsResponse();
        }
        if (RXData[1] == "S") {
          UnitsSet(RXData[5 ]);
        }
      }

      //Unit ABR
      if (RXData[2] == "U" && RXData[3] == "A") {
        UnitsABRResponse();
      }

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

void StatusResponse() {
  int ChannelNumber = 0;
  byte DataPacket[] = {byte("R"), byte("C"), byte("S"), byte(ChannelNumber), byte(Units), highByte(255), lowByte(63)};
  CANBusSend(7, false, DataPacket);

}

void DiscoveryResponse(int ReplyToAddress) {
  byte DataPacket[] = {byte("R"), byte("I"), byte("A"), byte("M"), byte(DeviceType)};
  CANBusSend(5, false, DataPacket);
}

void StreamingModeResponse() {
  byte DataPacket[] = {byte("R"), byte("C"), byte("S"), byte("T"), byte("R"), byte(Streaming)};
  CANBusSend(6, false, DataPacket);
}

void StreamingModeSet(bool Data) {
  if (Data == true || Data == false) {
    EEPROM.update(1, Data);
    Streaming = Data;
  }
  StreamingModeResponse();
}

void PacingResponse() {
  byte DataPacket[] = {byte("P"), byte("A"), byte("C"), highByte(PacingTime), lowByte(PacingTime)};
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
  byte DataPacket[] = {byte("R"), byte("U"), byte("N"), byte("T"), byte(Units)};
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
