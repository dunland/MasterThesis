const int noiseFloor = 473; // TODO: create function to assess noiseFloor upon startup
const int max_val = 1023;
const int globalThreshold = 20;
int pin = A3;

void setup() {
  Serial.begin(115200);
  delay(1000);
  while (!Serial);

  pinMode(LED_BUILTIN, OUTPUT);
  //pinMode(A0, INPUT);

}

void loop() {

  //Serial.println(analogRead(pin));

  int diff = noiseFloor - analogRead(pin);
  diff = abs(diff); // positive values only
  //diff = map(diff, 0, max_val, 0, 255);  // 8-bit conversion
  Serial.println(diff);
  //Serial.write(diff);

  if (diff > globalThreshold)
    //if (diff > 100)
  {
    digitalWrite(LED_BUILTIN, HIGH);
  }
  else
  {
    digitalWrite(LED_BUILTIN, LOW);
  }

  delay(10);

}
