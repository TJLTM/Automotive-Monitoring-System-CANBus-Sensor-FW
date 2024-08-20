#include <Adafruit_MAX31865.h>

// use hardware SPI, just pass in the CS pin
Adafruit_MAX31865 thermo0 = Adafruit_MAX31865(10);
Adafruit_MAX31865 thermo1 = Adafruit_MAX31865(10);
Adafruit_MAX31865 thermo2 = Adafruit_MAX31865(10);

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
}



void loop() {
  delay(1000);
  Serial.print("Channel 0:");
  uint16_t rtd0 = thermo0.readRTD();
  Serial.print("RTD0 value: "); Serial.println(rtd0);
  float ratio0 = rtd0;
  ratio0 /= 32768;
  Serial.print("Ratio0 = "); Serial.println(ratio0,8);
  Serial.print("Resistance0 = "); Serial.println(RREF*ratio0,8);
  Serial.print("Temperature0 = "); Serial.println(thermo0.temperature(RNOMINAL, RREF));

  delay(1000);
  Serial.print("Channel 1:");
  uint16_t rtd1 = thermo1.readRTD();
  Serial.print("RTD1 value: "); Serial.println(rtd1);
  float ratio1 = rtd1;
  ratio1 /= 32768;
  Serial.print("Ratio1 = "); Serial.println(ratio1,8);
  Serial.print("Resistance1 = "); Serial.println(RREF*ratio1,8);
  Serial.print("Temperature1 = "); Serial.println(thermo1.temperature(RNOMINAL, RREF));

  delay(1000);
  Serial.print("Channel 2:");
  uint16_t rtd2 = thermo2.readRTD();
  Serial.print("RTD2 value: "); Serial.println(rtd2);
  float ratio2 = rtd2;
  ratio2 /= 32768;
  Serial.print("Ratio2 = "); Serial.println(ratio2,8);
  Serial.print("Resistance2 = "); Serial.println(RREF*ratio2,8);
  Serial.print("Temperature2 = "); Serial.println(thermo2.temperature(RNOMINAL, RREF));

}
