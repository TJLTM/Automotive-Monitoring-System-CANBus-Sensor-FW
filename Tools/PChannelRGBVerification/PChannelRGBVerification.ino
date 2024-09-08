#define InputPin 4
#define Output1Pin 3
#define Output2Pin 5
#define Output3Pin 6
#define Output4Pin 10

int OutputPins[] = { 3, 5, 6, 10 };

int brightness = 0;  // how bright the LED is
int fadeAmount = 5;  // how many points to fade the LED by

void setup() {
  Serial.begin(115200);
  pinMode(InputPin, INPUT);
  for (int i = 0; i <= 3; i++) {
    pinMode(OutputPins[i], OUTPUT);
  }
  Serial.println("Starting up");
}

void loop() {
  OutputTesting();
  InputTesting();
}

void InputTesting() {
  int InputState = digitalRead(InputPin);
  Serial.println("Input reads:" + String(InputState));
  for (int i = 0; i <= 3; i++) {
    digitalWrite(OutputPins[i], InputState);
  }
}


void OutputTesting() {
  for (int i = 0; i <= 3; i++) {
    Serial.println("Cycling Output:" + String(i));
    bool Hit255, ReturnedTo0 = 0;
    while (Hit255 != false && ReturnedTo0 != false) {
      analogWrite(i, brightness);

      // change the brightness for next time through the loop:
      brightness = brightness + fadeAmount;

      // reverse the direction of the fading at the ends of the fade:
      if (brightness <= 0 || brightness >= 255) {
        fadeAmount = -fadeAmount;
      }
      // wait for 30 milliseconds to see the dimming effect
      delay(30);


      if (brightness == 255) { Hit255 = true; }
      if (Hit255 == true && brightness == 0) { ReturnedTo0 = true; }
    }
    Serial.println("Finished Cycling Output:" + String(i));
    brightness = 0;
  }
}