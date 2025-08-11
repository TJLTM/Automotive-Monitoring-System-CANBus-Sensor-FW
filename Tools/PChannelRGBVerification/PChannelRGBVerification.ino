#define InputPin 4
int OutputPins[] = { 3, 5, 6, 10 };
int brightness = 0;  // how bright the LED is
int fadeAmount = 5;  // how many points to fade the LED by

void setup() {
  Serial.begin(115200);
  Serial.println("Starting up PCHannelRGB Verficiation");
  pinMode(InputPin, INPUT);
  for (int i = 0; i <= 3; i++) {
    pinMode(OutputPins[i], OUTPUT);
    digitalWrite(OutputPins[i], LOW);
    Serial.println("PIN::"+String(OutputPins[i])+" set low");
  }
  
}

void loop() {
  //Pick one of the following
  //OutputTesting();
  //InputTesting();
}

void InputTesting() {
  int InputState = digitalRead(InputPin);
  Serial.println("Input reads:" + String(InputState));
  for (int i = 0; i <= 3; i++) {
    digitalWrite(OutputPins[i], InputState);
    Serial.println("InputTesting PIN:"+String(OutputPins[i])+" to"+String(InputState));
  }
}

void OutputTesting(){
    int Pin;
    int delayTime = 250;
    int step = 5;
    for (int i = 0; i <= 3; i++) {
      Pin = OutputPins[i];
      for (int brightness = 0; brightness < 255; brightness=brightness+step) {
        analogWrite(Pin, brightness);
        Serial.println("Setting PIN:"+String(Pin)+" to"+String(brightness));
        delay(delayTime);
      }
      for (int brightness = 255; brightness >= 0; brightness=brightness-step) {
        analogWrite(Pin, brightness);
        Serial.println("Setting PIN:"+String(Pin)+" to"+String(brightness));
        delay(delayTime);
     }
   }
}
