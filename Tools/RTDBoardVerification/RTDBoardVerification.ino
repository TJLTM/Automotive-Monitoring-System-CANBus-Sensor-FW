#include <Adafruit_MAX31865.h>

// use hardware SPI, just pass in the CS pin
Adafruit_MAX31865 thermo0 = Adafruit_MAX31865(3);
Adafruit_MAX31865 thermo1 = Adafruit_MAX31865(4);
Adafruit_MAX31865 thermo2 = Adafruit_MAX31865(5);
bool IsThereAFault = false;

// The value of the Rref resistor. Use 430.0 for PT100 and 4300.0 for PT1000
#define RREF      430.0
// The 'nominal' 0-degrees-C resistance of the sensor
// 100.0 for PT100, 1000.0 for PT1000
#define RNOMINAL  100.0

void setup() {
  Serial.begin(9600);
  while (!Serial)
    ;
  Serial.println("Starting up");

  thermo0.begin(MAX31865_3WIRE);  // set to 2WIRE or 4WIRE as necessary
  thermo1.begin(MAX31865_3WIRE);  // set to 2WIRE or 4WIRE as necessary
  thermo2.begin(MAX31865_3WIRE);  // set to 2WIRE or 4WIRE as necessary
}



void loop() {
  delay(1000);
  Serial.print("Themo0:");
  Serial.println(RTDReadAndFaultCheck(thermo0));
  delay(1000);
  Serial.print("Themo1:");
  Serial.println(RTDReadAndFaultCheck(thermo1));
  delay(1000);
  Serial.print("Themo2:");
  Serial.println(RTDReadAndFaultCheck(thermo2));

}



double RTDReadAndFaultCheck(Adafruit_MAX31865 &thermo) {
  IsThereAFault = false;
  uint16_t rtd = thermo.readRTD();
  float ratio = rtd;
  ratio /= 32768;
  float Resistance = RREF * ratio;
  double Temp = thermo.temperature(RNOMINAL, RREF);
  uint8_t fault = thermo.readFault();

  if (fault) {
    Temp = 0.0;
    Serial.print("Fault 0x"); Serial.println(fault, HEX);
    if (fault & MAX31865_FAULT_HIGHTHRESH) {
      Serial.println("RTD High Threshold");
    }
    if (fault & MAX31865_FAULT_LOWTHRESH) {
      Serial.println("RTD Low Threshold");
    }
    if (fault & MAX31865_FAULT_REFINLOW) {
      Serial.println("REFIN- > 0.85 x Bias");
    }
    if (fault & MAX31865_FAULT_REFINHIGH) {
      Serial.println("REFIN- < 0.85 x Bias - FORCE- open");
    }
    if (fault & MAX31865_FAULT_RTDINLOW) {
      Serial.println("RTDIN- < 0.85 x Bias - FORCE- open");
    }
    if (fault & MAX31865_FAULT_OVUV) {
      Serial.println("Under/Over voltage");
    }
    thermo.clearFault();
  } else {
    if (Resistance == 0 && Temp == -1)
    {
      Temp = 0.0;
      Serial.println("Reading zero resistance can you communicate to the chip?");
    }
  }

  return Temp;
}
