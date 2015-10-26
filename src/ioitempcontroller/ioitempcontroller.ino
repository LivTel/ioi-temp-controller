// LM134 datasheet (http://www.ti.com/lit/ds/symlink/lm234.pdf)
// MCP41100 datasheet (http://docs-europe.electrocomponents.com/webdocs/1380/0900766b81380ca2.pdf)
// DT471 datasheet (http://www.lakeshore.com/Documents/LSTC_DT400_l.pdf)

#include <SPI.h>
#include <Ethernet.h>

// NETWORK
// -------
// ARI LOCAL NETWORK
//byte mac[]     = { 0x90, 0xA2, 0xDA, 0x0E, 0x50, 0x9E };
//byte ip[]      = { 150, 204, 241, 253 }; 
//byte gateway[] = { 150, 204 ,241 ,254 };
//byte subnet[]  = { 255, 255 ,255 ,0 };

// TELESCOPE NETWORK
// ioitempcontroller
//byte mac[]     = { 0x90, 0xA2, 0xDA, 0x0E, 0x50, 0x9E };
byte mac[]     = { 0x90, 0xA2, 0xDA, 0x0D, 0xCB, 0x08 };
byte ip[]      = { 192, 168, 1, 75 };
byte gateway[] = { 192, 168 ,1 ,254 };
byte subnet[]  = { 255, 255 ,255 ,0 };

int SERVER_PORT = 8888;

// I/O
// ---
#define REF_pin 7                   // pin to pull high for voltage divided reference.
#define R1_CS_pin  9                // clock select for R1
#define R2_CS_pin  8                // clock select for R2
#define T1_ANALOG_PIN_VPLUS 5       // analog pin for T1 V+
#define T1_ANALOG_PIN_VMINUS 4      // analog pin for T1 V-
#define T2_ANALOG_PIN_VPLUS 3       // analog pin for T2 V+
#define T2_ANALOG_PIN_VMINUS 2      // analog pin for T2 V-

// CURRENT SOURCE
// --------------
// the following section works out what resistances are required to keep a particular set current
// n.b. you can't have a resistance value > the limit of the digital pots (~90K), this corresponds
// to a lower limit of ~13uA.
float SET_CURRENT             = 9*pow(10, -6);   // A
float DIODE_FORWARD_VOLTAGE   = 0.5;              // V
float DIODE_TEMPERATURE_COEFF = -2.5;             // mV/degC

// TEMPERATURE SENSOR
// ------------------
// AS OF 19/10/15, for DT471.
int DIODE_TEMPERATURE_ORDER = 5;
float DIODE_TEMPERATURE_COEFFS[6] = {-906.15255839, 3184.68704355, -4521.75268022, 3189.42006723, -1526.98411765, 668.57235871};
float REFERENCE_VOLTAGE = 2.427;

//float DIODE_FORWARD_VOLTAGE_TEMPERATURE_SENSOR   = 0.54294;  // V (DT471)
//float DIODE_TEMPERATURE_COEFF_TEMPERATURE_SENSOR = -2.0; // mV/degC (DT471)

// PROGRAM PARAMETERS
// --------------
#define NUMREADINGS     10      // number of readings to take to define average analog read
#define NUMREADINGSOMIT 10      // number of analog reads to take before taking actual reading


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

float readSensor(int sensor, boolean returnT)
{
  int pin_vplus;
  int pin_vminus;
  switch(sensor)
  {
    case 1:
      pin_vplus = T1_ANALOG_PIN_VPLUS;
      pin_vminus = T1_ANALOG_PIN_VMINUS;
      break;
    case 2:
      pin_vplus = T2_ANALOG_PIN_VPLUS;
      pin_vminus = T2_ANALOG_PIN_VMINUS;
      break;      
    default:
      return false;      
  } 
  for (int i=0; i<NUMREADINGSOMIT; i++)
  {      
    analogRead(pin_vplus);
    analogRead(pin_vminus);
  } 
  float sensor_readings[NUMREADINGS];
  float this_pin_vplus_voltage, this_pin_vminus_voltage;
  for (int i=0; i<NUMREADINGS; i++)
  {      
    this_pin_vplus_voltage = (analogRead(pin_vplus)/(pow(2, 10)))*REFERENCE_VOLTAGE; // 10 bit ADC
    this_pin_vminus_voltage = (analogRead(pin_vminus)/(pow(2, 10)))*REFERENCE_VOLTAGE; // 10 bit ADC
    sensor_readings[i] = this_pin_vplus_voltage-this_pin_vminus_voltage; 
  } 
  
  BubbleSort(sensor_readings);    
  float avV = NUMREADINGS % 2 ? sensor_readings[NUMREADINGS/2] : (sensor_readings[NUMREADINGS/2 - 1] + sensor_readings[NUMREADINGS/2])/2;    
  
  float avT = 0;
  for (int i=0; i<=DIODE_TEMPERATURE_ORDER; i++) 
  {
     avT += DIODE_TEMPERATURE_COEFFS[i]*pow(avV, DIODE_TEMPERATURE_ORDER-i) ;
  }
   
  if (returnT) 
  {
    return avT;
  } else {
    return avV;
  }
}

EthernetServer server(SERVER_PORT);

void setup()
{
  SPI.begin();
  Serial.begin(9600);
  Ethernet.begin(mac, ip, gateway, subnet); // start the ethernet connection
  server.begin();                           // and begin listening for TCP connections

  analogReference(EXTERNAL);  // 2.5V with 1.5k, 1.5k divider

  pinMode (R1_CS_pin, OUTPUT);
  pinMode (R2_CS_pin, OUTPUT);
  
  pinMode (REF_pin, OUTPUT);
  digitalWrite(REF_pin, HIGH);

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
            float rtn = readSensor(1, true);
            client.println(rtn);
          } else if (clientMsg.indexOf("KRDG?B") >= 0) {
            float rtn = readSensor(2, true);
            client.println(rtn);
          } if (clientMsg.indexOf("VRDG?A") >= 0) {
            float rtn = readSensor(1, false);
            client.println(rtn);
          } else if (clientMsg.indexOf("VRDG?B") >= 0) {
            float rtn = readSensor(2, false);
            client.println(rtn);
          }
          clientMsg = "";
          client.flush();
        } 
      }
    }
  }  
}




