#include <Wire.h>
#include <OneWire.h>
#include <RTClib.h>
#include <TimeLib.h>
#include <SPI.h>
#include <SD.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

OneWire  ds(4);
Servo myservo;
Servo servo;
RTC_DS1307 rtc;
const int chipSelect = 10;  // Chip select pin for SD card module
const int SensorPin = A0;   // The pH meter Analog output is connected with the Arduino's Analog
const int servoPin = 5;
unsigned long int avgValue;  // Store the average value of the sensor feedback
float b;
int pos= 0;
int buf[10], temp;
const int bluePin = 8;  // blue pin
const int ldrPin1 = A1;
const int ldrPin2 = A2;
const int ldrPin3 = A3;
int blueVal;
int ldrVal1;
int ldrVal2;
int ldrVal3;
int avgLdr;

File dataFile;

LiquidCrystal_I2C lcd(0x27, 16, 2);  // I2C address and dimensions of your LCD display

void setup()
{
  pinMode(bluePin, OUTPUT);
  pinMode(ldrPin1, INPUT);
  pinMode(ldrPin2, INPUT);
  pinMode(ldrPin3, INPUT);
  pinMode(9, OUTPUT);
  
  myservo.attach(7);
  Wire.begin();
  
  servo.attach(servoPin);
  servo.write(0);
  
  Serial.begin(9600);
  Serial.println("Ready");

  lcd.begin(16, 2);  // Initialize the LCD display
  
  // Initialize the SD card
  if (!SD.begin(chipSelect))
  {
    Serial.println("SD card initialization failed!");
    return;
  }
  Serial.println("SD card initialized.");

  // Initialize the RTC
  rtc.begin();
  rtc.adjust(DateTime(__DATE__, __TIME__));

  // Open the data file in append mode
  dataFile = SD.open("data.csv", FILE_WRITE);
if (dataFile) {
    Serial.println("File opened ok");
    // print the headings for our data
    dataFile.println("Date,Time,pH value,Temperature, Intensity");
  }
  dataFile.close();
}
bool food = false;
void loop()
{
  
  byte i;
  byte present = 0;
  byte type_s;
  byte data[9];
  byte addr[8];
  float celsius;

  DateTime now = rtc.now();  // Get the current date and time

  //code for the servo motor of feeder section 
  if (now.hour() == 7 && now.minute() == 0 && food ==false) {
    // Trigger the servo motor
    moveServo();
    food=true;
    delay(1000); // Delay to avoid continuous triggering within the same second
    
  }
  if (now.hour() == 7 && now.minute() == 1 && food ==true) {
    food=false;
  }
  

  //code for pH senosor
  for (int i = 0; i < 10; i++)   // Get 10 sample values from the sensor to smooth the value
  {
    buf[i] = analogRead(SensorPin);
    delay(10);
  }
  for (int i = 0; i < 9; i++)    // Sort the analog values from small to large
  {
    for (int j = i + 1; j < 10; j++)
    {
      if (buf[i] > buf[j])
      {
        temp = buf[i];
        buf[i] = buf[j];
        buf[j] = temp;
      }
    }
  }
  avgValue = 0;
  for (int i = 2; i < 8; i++)   // Take the average value of the 6 center samples
    avgValue += buf[i];
  float phValue = (float)avgValue * 5.0 / 1024 / 6;   // Convert the analog value into millivolts
  phValue = 3.5 * phValue;   // Convert the millivolts into pH value



  //temperature code 
  if ( !ds.search(addr)) {
    ds.reset_search();
    delay(250);
    //return;
  }

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);        // start conversion, with parasite power on at the end
  
  //delay(1000);     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.
  
  present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE);         // Read Scratchpad

  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
  }
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }
  celsius = (float)raw / 16.0;

  //code for light intensity 
  ldrVal1 = analogRead(ldrPin1);
  ldrVal2 = analogRead(ldrPin2);
  ldrVal3 = analogRead(ldrPin3);
  avgLdr = (ldrVal1 + ldrVal2 + ldrVal3 )/3;

  if (hour() >= 6 && hour() < 17) { // 6 am to 5 pm
    // greenVal = ;
    blueVal = 0;
    if (avgLdr > 600) {
      // greenVal = 0;
      blueVal = 127;
    }
  }
  else if (hour() >= 17 && hour() < 20) {
    // greenVal = 127;
    blueVal = 127;
  }
  
  if(hour() >= 20 && hour() < 6) {
    // greenVal = 255;
    blueVal = 255;
    if (avgLdr < 100) {
      // greenVal = 127;
      blueVal = 127;
    }
  }
  analogWrite(bluePin, blueVal);

  // Print pH value with date and time to serial monitor
  Serial.print("Date: ");
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(',');
  Serial.print("Time: ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.print(',');
  Serial.print(" pH: ");  
  Serial.print(phValue, 2);
  Serial.print(',');
  Serial.print("Temperature: ");
  Serial.print(celsius);
  Serial.print("°C");
  Serial.print(',');
  Serial.print("Intesisty: ");
  Serial.print(avgLdr);  

  Serial.println();
 

 // Display pH value on LCD
  lcd.init();
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("pHvalue of water");
  lcd.print("       ");  // Clear the previous pH value
  lcd.setCursor(3, 1);   // Set the LCD cursor position
  lcd.print(phValue); // Display the current pH value on the LCD
  delay(4000);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Temp of water");
  lcd.print("  ");
  lcd.setCursor(3,1);
  lcd.print(celsius);
  delay(4000);
  lcd.clear();

  //code of servo motor and buzzer for pH sensor 
  if(phValue>8 || phValue<6)
  {
    digitalWrite(9, HIGH);   
    delay(1000);                       
    digitalWrite(9, LOW);    
    delay(1);     
    for (pos = 0; pos <= 180; pos += 1) { // goes from 0 degrees to 180 degrees
    // in steps of 1 degree
    myservo.write(pos);              // tell servo to go to position in variable 'pos'
    delay(15);                       // waits 15ms for the servo to reach the position
  }
   for (pos = 180; pos >= 0; pos -= 1) { // goes from 180 degrees to 0 degrees
    myservo.write(pos);              // tell servo to go to position in variable 'pos'
    delay(15);                       // waits 15ms for the servo to reach the position
  }                  
  }

  //code of buzzer for temperature sensor 
  if(celsius>30 || celsius<25)
  {
    digitalWrite(9, HIGH);   
    delay(2000);                       
    digitalWrite(9, LOW);    
    delay(2000); 
  }

  // Log pH value with date and time to SD card in CSV format
  File dataFile = SD.open("data.csv", FILE_WRITE);
  if (dataFile)
  {
    dataFile.print(now.year(), DEC);
    dataFile.print('/');
    dataFile.print(now.month(), DEC);
    dataFile.print('/');
    dataFile.print(now.day(), DEC);
     dataFile.print(',');
    dataFile.print(now.hour(), DEC);
    dataFile.print(':');
    dataFile.print(now.minute(), DEC);
    dataFile.print(':');
    dataFile.print(now.second(), DEC);
    dataFile.print(',');
    dataFile.print(phValue); 
    //dataFile.println();
    dataFile.print(',');
    dataFile.print(celsius);
    dataFile.print("°C"); 
    dataFile.print(',');
    dataFile.print(avgLdr); 
    dataFile.println();
    dataFile.close();
  }
  else
  {
    Serial.println("Error opening data.csv");
  }
  //delay(1000);
}
void moveServo() {
     servo.write(90); // Move the servo to 90 degrees
     delay(1000);     // Wait for the servo to reach the position
     servo.write(0);  // Reset the servo to the initial position
}
   
