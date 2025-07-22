
#define Channel0 A1
#define Channel1 A2
#define Channel2 A3
#define Channel3 A4
float ConversionFactor = 5.0/1023;

void setup() {
  Serial.begin(9600);
  while (!Serial)
    ;
  Serial.println("Starting up");
}



void loop() {
  delay(1000);
  Serial.print("Channel 0:");
  Serial.println(analogRead(Channel0)*ConversionFactor);

  delay(1000);
  Serial.print("Channel 1:");
  Serial.println(analogRead(Channel1)*ConversionFactor);

  delay(1000);
  Serial.print("Channel 2:");
  Serial.println(analogRead(Channel2)*ConversionFactor);

  delay(1000);
  Serial.print("Channel 3:");
  Serial.println(analogRead(Channel3)*ConversionFactor);  
}
