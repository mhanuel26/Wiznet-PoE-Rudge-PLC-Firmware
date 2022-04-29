/*
  OpenPLC W5100S-Pico-EVB
 
 */

#include <SPI.h>
#include <Ethernet.h>


#define SPI1_RX   8
#define SPI1_SCK  10
#define SPI1_TX   11
#define DO_CNFG   2
#define DO_EN     7
#define DO_CSB    6
#define DO_FLTR   20   // Need to Move - not connected in this version
#define DO_FAULT  21  // Need to Move - not connected in this version
#define O_CS_PIN  5
#define SPI_GPIO_SETTINGS SPISettings(1000000, MSBFIRST, SPI_MODE0)

void init_o_cs() {
  pinMode(O_CS_PIN, OUTPUT);
}

void set_o_cs(void){
  digitalWrite(O_CS_PIN, LOW);
}

void rst_o_cs(void){
  digitalWrite(O_CS_PIN, HIGH);
} 

void init_csb() {
  pinMode(DO_CSB, OUTPUT);
  digitalWrite(DO_CSB, HIGH);
}

void set_csb(void){
  digitalWrite(DO_CSB, LOW);
}

void rst_csb(void){
  digitalWrite(DO_CSB, HIGH);
} 

void init_io_pins(){

  // Configure IN7 PIN to select O_ outputs using serial interface
  pinMode(DO_CNFG, OUTPUT);
  digitalWrite(DO_CNFG, LOW);
  // Configure EN pin high
  pinMode(DO_EN, OUTPUT);
  digitalWrite(DO_EN, HIGH);
  // Configure Digital Output IC CS pin 
  init_o_cs();
  // Configure Digital Input IC CS pin 
  init_csb();  
  
}

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
IPAddress ip(192, 168, 1, 30);

unsigned char modbus_buffer[100];

int processModbusMessage(unsigned char *buffer, int bufferSize);

#include "modbus.h"

extern bool mb_discrete_input[MAX_DISCRETE_INPUT];
extern bool mb_coils[MAX_COILS];
extern uint16_t mb_input_regs[MAX_INP_REGS];
extern uint16_t mb_holding_regs[MAX_HOLD_REGS];

//Create the modbus server instance
EthernetServer server(502);

void pinConfig()
{

}

void setup() {
  
  Ethernet.init(17);  // WIZnet W5100S-EVB-Pico

  init_io_pins();
  
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  
  // Initialize the GPIO modules
  Serial.println("device SPI1 Initialization ");
  
  uint8_t coils = 0x01;
  SPI1.setRX(SPI1_RX);
  SPI1.setSCK(SPI1_SCK);
  SPI1.setTX(SPI1_TX);
  SPI1.begin();

  delay(50); 
  Serial.println("Ethernet Pico PLC device");

  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip);

  // Check for Ethernet hardware present
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
    while (true) {
      delay(1); // do nothing, no point running without Ethernet hardware
    }
  }
  if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("Ethernet cable is not connected.");
  }

  // start the server
  server.begin();
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());

  memset((void*)mb_coils, 0, sizeof(mb_coils));
  updateIO();
}

void updateIO()
{
  // Update COILS
  uint8_t coils = 0;
  for (int i = 0; i < sizeof(mb_coils); i++)
  {
    coils |= mb_coils[i]<<i;    
  }
  SPI1.beginTransaction(SPI_GPIO_SETTINGS);
  set_o_cs();
  uint16_t val16 = (coils<<8) | 0xFF; 
  uint16_t receivedVal16 = SPI1.transfer16(val16);
  rst_o_cs();
  SPI1.endTransaction();

  // Update Discrete Inputs


  uint8_t val8 = 0;
  SPI1.beginTransaction(SPI_GPIO_SETTINGS);
  set_csb();
  uint8_t inputs_vals = SPI1.transfer(val8);
  rst_csb();
  SPI1.endTransaction();

  for (int i = 0; i < sizeof(mb_discrete_input); i++)
  {
    mb_discrete_input[i] = (bool)((inputs_vals>>i)&0x01);
  }
  
  
}

void loop() {
  // listen for incoming clients
  EthernetClient client = server.available();
  if (!client)
    return;
  Serial.println("new client");

  while (client.connected()) {
    // Wait until the client sends some data
    while(!client.available())
    {
      delay(1);
      if (!client.connected())
        return;
    }
    int i = 0;
    while(client.available())
    {
      modbus_buffer[i] = client.read();
      i++;
      if (i == 100)
        break;
    }

    updateIO();
    unsigned int return_length = processModbusMessage(modbus_buffer, i);
    client.write((const uint8_t *)modbus_buffer, return_length);
    updateIO();
    delay(1);    

  }
  
  Serial.println("client disconnected");
  delay(1);
}
