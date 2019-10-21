//Modified by experimentool March 2019 for I2C INA219 power module, SPI oled 128x64 and one wire DS18B20 temperature probe
//Using STM32 maple mini microcontroller
// I2C pins SDA PB6, SCL PB7
//Uses onboard INA219 100 milliohm shunt resistor good for 3200 mA.
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include ".\Adafruit_GFX.h"
#include ".\Adafruit_SSD1306.h"
#include <OneWireSTM.h>
Adafruit_INA219 ina219;
OneWire  ds(10);  // on pin 10 aka PA1 on maple mini for DS18B20 probe(a 4.7K pullup resistor to 3.3V is necessary)

// These pin #'s are for Maple Mini
//     __Signal__Maple_//__OLED 128x64___
#define OLED_CS   14   //   ---   x Not Connected
#define OLED_DC   22   //   D/C   pin# 6
#define OLED_RST  21   //   RST   pin# 5
#define OLED_MOSI 20   //   SDA   pin# 4
#define OLED_CLK  19   //   SCL   pin# 3

Adafruit_SSD1306 OLED(OLED_MOSI, OLED_CLK, OLED_DC, OLED_RST, OLED_CS);  // OLED object
void setup(void)
{
  uint32_t currentFrequency;

  pinMode (17, OUTPUT); digitalWrite(17, LOW);       // option oled "Gnd"
  pinMode (18, OUTPUT); digitalWrite(18, HIGH);      // option oled "Vcc"

  
  OLED.begin(SSD1306_SWITCHCAPVCC);                  // generate OLED HV on module!
  OLED.display();                                    // Splash screen == all blanks or Logo if present
  OLED.clearDisplay();                               // Clear the buffer, blank screen
  OLED.setTextColor(WHITE);                          // defaults to BLACK background
  OLED.setTextSize(2);
  OLED.setCursor(0, 0);
  OLED.print("experimentool");                          // .print writes into SRAM buffer
  OLED.display();       

  //OLED.setRotation(0); // rotate 0 degrees
  //OLED.setRotation(1); // rotate 90 degrees
  OLED.setRotation(2); // rotate 180 degrees
  //OLED.setRotation(3); // rotate 270 degree
  //OLED.dim(true); //display.dim(false); puts oled back to full brilliance mode
  OLED.display();

  Serial.begin(9600);
  Serial.println("Hello!");

  Serial.println("Measuring voltage and current with INA219 ...");
  ina219.begin();
}

void loop(void)
{
  float shuntvoltage = 0;
  float busvoltage = 0;
  float current_mA = 0;
  float loadvoltage = 0;
  float power = 0;

  shuntvoltage = ina219.getShuntVoltage_mV();
  busvoltage = ina219.getBusVoltage_V();
  current_mA = ina219.getCurrent_mA();
  loadvoltage = busvoltage + (shuntvoltage / 1000);
  power = busvoltage * (current_mA / 1000);

  Serial.print("Bus Voltage:   "); Serial.print(busvoltage); Serial.println(" V");
  Serial.print("Shunt Voltage: "); Serial.print(shuntvoltage); Serial.println(" mV");
  Serial.print("Load Voltage:  "); Serial.print(loadvoltage); Serial.println(" V");
  Serial.print("Current:       "); Serial.print(current_mA); Serial.println(" mA");
  
  
  //Serial.println("");

  byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];
  float celsius, fahrenheit;

  if ( !ds.search(addr)) {
    //  Serial.println("No more addresses.");
    //   Serial.println();
    ds.reset_search();
    delay(250);
    //  return;
  }

  // Serial.print("ROM =");
  // for( i = 0; i < 8; i++) {
  //  Serial.write(' ');
  //  Serial.print(addr[i], HEX);
  // }

  if (OneWire::crc8(addr, 7) != addr[7]) {
    Serial.println("CRC is not valid!");
    return;
  }
  // Serial.println();

  // the first ROM byte indicates which chip
  switch (addr[0]) {
    case 0x10:
      //    Serial.println("  Chip = DS18S20");  // or old DS1820
      type_s = 1;
      break;
    case 0x28:
      //   Serial.println("  Chip = DS18B20");
      type_s = 0;
      break;
    case 0x22:
      //   Serial.println("  Chip = DS1822");
      type_s = 0;
      break;
    default:
      //   Serial.println("Device is not a DS18x20 family device.");
      return;
  }

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);        // start conversion, with parasite power on at the end

  delay(1000);     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.

  present = ds.reset();
  ds.select(addr);
  ds.write(0xBE);         // Read Scratchpad

  //  Serial.print("  Data = ");
  // Serial.print(present, HEX);
  // Serial.print(" ");
  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
    //    Serial.print(data[i], HEX);
    //   Serial.print(" ");
  }
  // Serial.print(" CRC=");
  //  Serial.print(OneWire::crc8(data, 8), HEX);
  // Serial.println();

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
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
  fahrenheit = celsius * 1.8 + 32.0;
  Serial.print("Temperature:   ");
  Serial.print(celsius);
  Serial.print(" Celsius, ");
  Serial.print(fahrenheit);
  Serial.println(" Fahrenheit");
  Serial.println("");

  OLED.clearDisplay();                               // Clear the buffer.
  OLED.setTextSize(1);
  OLED.setTextColor(WHITE);                          // only WHITE and BLACK available
  OLED.setCursor(0, 0);
  OLED.print("BUS VOLTS   ");
  OLED.println(busvoltage);
  OLED.setCursor(0, 10);                             // 15 is blank-line separator
 // OLED.setTextSize(2);
  OLED.print("SHUNT mV    ");
  OLED.println(shuntvoltage);
  OLED.setCursor(0 ,21);
  OLED.print("LOAD VOLTS  ");
  OLED.println(loadvoltage);
  OLED.setCursor(0 ,32);
  OLED.print("CURRENT mA  ");
  OLED.println(current_mA);
  OLED.setCursor(0 ,43);
  OLED.print("WATTS       ");
  OLED.println(power);
  OLED.setCursor(0 ,54);
  OLED.print("TEMPERATURE ");
  OLED.println(fahrenheit);
  OLED.display();
 

  delay(3000);
}
