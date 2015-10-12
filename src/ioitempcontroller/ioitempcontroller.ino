// LM134 datasheet (http://www.ti.com/lit/ds/symlink/lm234.pdf)
// MCP41100 datasheet (http://docs-europe.electrocomponents.com/webdocs/1380/0900766b81380ca2.pdf)
// DT471 datasheet (http://www.lakeshore.com/Documents/LSTC_DT400_l.pdf)

#include <SPI.h>
#include <Ethernet.h>

// NETWORK
// -------
// ARI LOCAL NETWORK
byte mac[]     = { 0x90, 0xA2, 0xDA, 0x0E, 0x50, 0x9E };
byte ip[]      = { 150, 204, 241, 253 }; 
byte gateway[] = { 150, 204 ,241 ,254 };
byte subnet[]  = { 255, 255 ,255 ,0 };

// TELESCOPE NETWORK
// ioitempcontroller
//byte mac[]     = { 0x90, 0xA2, 0xDA, 0x0D, 0xCB, 0x08 };
//byte ip[]      = { 192, 168, 1, 75 };
//byte gateway[] = { 192, 168 ,1 ,254 };
//byte subnet[]  = { 255, 255 ,255 ,0 };

int SERVER_PORT = 8888;

// I/O
// ---
#define R1_CS_pin  9          // clock select for R1
#define R2_CS_pin  10         // clock select for R2
#define T1_ANALOG_PIN 5       // analog pin for T1
#define T2_ANALOG_PIN 4       // analog pin for T2

// CURRENT SOURCE
// --------------
// the following section works out what resistances are required to keep a particular set current
// n.b. you can't have a resistance value > the limit of the digital pots (~90K), this corresponds
// to a lower limit of ~13uA.
float SET_CURRENT             = 10*pow(10, -6);   // A
float DIODE_FORWARD_VOLTAGE   = 0.5;              // V
float DIODE_TEMPERATURE_COEFF = -2.5;             // mV/degC

// TEMPERATURE SENSOR
// ------------------
float DIODE_TEMPERATURE_REFERENCE                = 293;  // K
float DIODE_FORWARD_VOLTAGE_TEMPERATURE_SENSOR   = 0.43; // V
float DIODE_TEMPERATURE_COEFF_TEMPERATURE_SENSOR = -2.5; // mV/degC

//float DIODE_FORWARD_VOLTAGE_TEMPERATURE_SENSOR   = 0.54294;  // V (DT471)
//float DIODE_TEMPERATURE_COEFF_TEMPERATURE_SENSOR = -2.0; // mV/degC (DT471)

// PROGRAM PARAMETERS
// --------------
#define NUMREADINGS     10      // number of readings to take to define average analog read
#define NUMREADINGSOMIT 5       // number of analog reads to take before taking actual reading



float get_R1_for_given_set_current(float i_set, float resistor_ratio) {
  return ((resistor_ratio*67.7*pow(10, -3)) + ((67.7*pow(10, -3)) + DIODE_FORWARD_VOLTAGE))/(resistor_ratio*i_set);  // this is taken from p9 of the lm134 datasheet
}

int digitalPotWrite(int value, int address, int pin)
{
  digitalWrite(pin, LOW);
  SPI.transfer(address);
  SPI.transfer(value);
  digitalWrite(pin, HIGH);
}

void BubbleSort(float analogValues[])
{  
  int i, j;
  for (i=0; i<NUMREADINGS; i++)
  {
    for (j=0; j<i; j++)
    {
      if (analogValues[i] > analogValues[j]) // out of order?
      {
        // swap
        float temp = analogValues[i];
        analogValues[i] = analogValues[j];
        analogValues[j] = temp;
      }
    }
  }
}

float readSensor(int pin)
{
  for (int i=0; i<NUMREADINGSOMIT; i++)
  {      
    analogRead(pin);
  } 
  float analogValues[NUMREADINGS];
  for (int i=0; i<NUMREADINGS; i++)
  {      
    float voltage = (analogRead(pin)/(pow(2, 10)))*1.1; // 10 bit ADC
    analogValues[i] = voltage; 
  } 
  BubbleSort(analogValues);    
  float avV = NUMREADINGS % 2 ? analogValues[NUMREADINGS/2] : (analogValues[NUMREADINGS/2 - 1] + analogValues[NUMREADINGS/2])/2;    
  float avT = DIODE_TEMPERATURE_REFERENCE + (DIODE_FORWARD_VOLTAGE_TEMPERATURE_SENSOR-avV)*(1.0/abs(DIODE_TEMPERATURE_COEFF_TEMPERATURE_SENSOR*pow(10, -3)));
  return avT;
}

EthernetServer server(SERVER_PORT);

void setup()
{
  SPI.begin();
  Serial.begin(9600);
  Ethernet.begin(mac, ip, gateway, subnet); // start the ethernet connection
  server.begin();                           // and begin listening for TCP connections

  analogReference(INTERNAL);  // 1.1V

  pinMode (9, OUTPUT);
  pinMode (10, OUTPUT);

  float resistor_ratio   = ((abs(DIODE_TEMPERATURE_COEFF)*pow(10, -3)) - (227*pow(10, -6)))/(227*pow(10, -6));   // this is taken from p8 of the lm134 datasheet, assuming -2mV/C for the temperature coeff of the diode
  float R1               = get_R1_for_given_set_current(SET_CURRENT, resistor_ratio);
  float R2               = resistor_ratio*R1;

  int R1_byte = ceil(R1/((90000.-100.)/255.));
  int R2_byte = ceil(R2/((90000.-100.)/255.));
  
  if (R1_byte > 254) {
    Serial.println("R1 is out of range. Setting to highest value."); 
    R1_byte = 254;
  } else if (R1_byte < 0) {
    Serial.println("R1 is out of range. Setting to lowest value."); 
    R1_byte = 0;
  }
  
  if (R2_byte > 254) {
    Serial.println("R2 is out of range. Setting to highest value."); 
    R2_byte = 254;
  } else if (R2_byte < 0) {
    Serial.println("R2 is out of range. Setting to lowest value."); 
    R2_byte = 0;
  }
  
  Serial.print("R1 = ");
  Serial.print(R1);
  Serial.print(" = ");
  Serial.println(R1_byte);
  Serial.print("R2 = ");;
  Serial.print(R2);
  Serial.print(" = ");
  Serial.println(R2_byte);
  
  digitalPotWrite(R1_byte, 0x11, R1_CS_pin);
  digitalPotWrite(R2_byte, 0x11, R2_CS_pin); 
}

void loop()
{
  EthernetClient client = server.available();
  if (client) {
    String clientMsg = "";
    while (client.connected()) {
      // transmit
      while (client.available()) {
        char c = client.read();
        clientMsg+=c;
        if (c == '\n') {
          if (clientMsg.indexOf("KRDG?A") >= 0) {
            float T = readSensor(T1_ANALOG_PIN);
            client.println(T);
          } else if (clientMsg.indexOf("KRDG?B") >= 0) {
            float T = readSensor(T2_ANALOG_PIN);
            client.println(T);
          }
          clientMsg = "";
          client.flush();
        } 
      }
    }
  }       
}




