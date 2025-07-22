
#define TEMPPin A0
float ConversionFactor = 5.0/1023;

void setup() {
  Serial.begin(9600);
  while (!Serial)
    ;
  Serial.println("Starting up");
}



void loop() {
  delay(1000);
  Serial.print("TEMPPin:");
  Serial.println(analogRead(TEMPPin)*ConversionFactor);


}
