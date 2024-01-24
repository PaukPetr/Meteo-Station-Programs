/*
  openSTRATOkit Basic Tracker

  @DevOK1RAJ Team, code by OK1CDJ 3/2021

  low ("space") frequency:     434.69 MHz
  frequency shift:             610 Hz
  baud rate:                   300 baud
  encoding:                    ASCII (7-bit)
  stop bits:                   2

  NOTE: RTTY frequency shift will be rounded
        to the nearest multiple of frequency step size.
        The exact value depends on the module:
        RF69 - 61 Hz steps

  MegaCoreX documentation: https://github.com/MCUdude/MegaCoreX

  Compile and upload instructions:
  - if you're using Windows, you can skip all but the last step
  and download a portable version of Arduino IDE with everything
  preinstalled: https://files.dotknisevesmiru.cz/arduino_openSTRATOkit.zip
  - In your Arduino IDE, go to File > Preferences and enter
  https://mcudude.github.io/MegaCoreX/package_MCUdude_MegaCoreX_index.json
  into the “Additional Board Manager URLs” field
  - Go to Tools > Board > Boards Manager…, search for MegaCoreX and install
    MegaCoreX by MCUdude
  - In the Tools > Board menu select MegaCoreX > ATmega4809
  - In the Tools menu, make sure you select:
    Clock: "Internal 16MHz"
    BOD: "BOD 2.6V"
    Pinout: "48 pin standard"
    Reset pin: "Reset"
    Bootloader: "Optiboot (UART3 alternative pins)"
    Programmer: "JTAG2UPDI"
  - Download the following libraries from the Arduino Library Manager
  (Project > Add library > Manage libraries...):
    Adafruit BME280 Library ba Adafruit - select "Install All" in the pop-up window
    RadioLib by Jan Gromes
  - Download and import TinyGPSPlus library from here:
  https://github.com/mikalhart/TinyGPSPlus/archive/refs/heads/master.zip
  (Project > Add library > Add .ZIP Library)
  - Connect your openSTRATOkit and select it in Tools > Port
  (make sure you use a short, quality cable, like the one provided with the kit)
*/

// libraries
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <RadioLib.h>
#include <util/crc16.h>
#include <SPI.h>
#include <SD.h>

#include"Meteo.h"
#include"Geiger.h"

// Radio Settings
#define FREQ 434.690
#define SHIFT 610
#define BAUD 300
#define ENC ASCII
#define STOPB 2

String call = "TTS7"; // CHANGE THIS!
long pkt_num = 1; // packet number
float batt_voltage;

File myFile;

RF69 radio = new Module(33, 9, RADIOLIB_NC);

// create RTTY client instance using the FSK module
RTTYClient rtty(&radio);

Adafruit_BME280 bme;

void(* resetFunc) (void) = 0; //declare reset function @ address 0

void setup() {

  // init serial comm
  Serial3.pins(12, 13);
  Serial3.begin(115200);
  Serial1.pins(18, 19);
  Serial1.begin(9600);
  Serial3.println("openSTRATOtracker");
  Serial3.println();

  // init and check for BME280
  if(!bme.begin(0x76)){
    Serial3.print("[BME280], no BME280 detected...");
  } else Serial3.println("[BME280] found...");
  
  // init radio
  Serial3.print(F("[RF69] Initializing ... "));
  SPI.pins(30, 31, 32, 33);
  int state = radio.begin();

  // check for errors
  if (state == ERR_NONE) {
    Serial3.println(F("success!"));
  } else {
    Serial3.print(F("failed, code "));
    Serial3.println(state);
    resetFunc();  
    while (true);
  }

  // radio output power
  Serial3.print(F("[RF69] Setting high power module ... "));
    state = radio.setOutputPower(20, true);
    if (state == ERR_NONE) {
      Serial3.println(F("success!"));
    } else {
      Serial3.print(F("failed, code "));
      Serial3.println(state);
      resetFunc();  
      while (true);
    }

  // set-up rtty comm
  Serial3.print(F("[RTTY] Initializing ... "));
  state = rtty.begin(FREQ, SHIFT, BAUD, ENC, STOPB);
  if (state == ERR_NONE) {
    Serial3.println(F("success!"));
  } else {
    Serial3.print(F("failed, code "));
    resetFunc();      
    Serial3.println(state);
    while (true);
  }

  // init SD card
  Serial3.print("[SD] Initializing SD card...");
  if (!SD.begin(10)) {
    Serial3.println("initialization failed!");
  } else Serial3.println(F("success!"));

  Geiger::Setup();
  Meteo::Setup();
}
  

void loop() {
  sendData2();
  Geiger::VoltageControl ();
}

// compile sensor information and transmit it
void sendData2() {

    String datastring;
	String geigerdata = "Geiger DiffVoltage, ControlVoltage:"; //saves geiger data to print to Serial3 later

    datastring = "$$$$" + call + ",";    // call
    datastring += String(pkt_num) + ","; // packet number

	datastring += "01:01:01,";		// fake time
  datastring += "10.0,";        // fake lat/long

    datastring += String(bme.readTemperature(), 1) + ",";  // temperature external
    datastring += String(bme.readPressure(), 0) + ",";     // pressure
    datastring += String(bme.readHumidity()) + ",";         // relative humidity
	datastring += String(Meteo::winddir()) + ",";          // wind direction
	datastring += String(Meteo::windspeed()) + ",";          // wind speed  

    datastring += String(Geiger::GetCount());

    datastring += "," + String(Geiger::DiffVoltage);
    datastring += "," + String(Geiger::ControlVoltage);
	geigerdata += String(Geiger::DiffVoltage) + "," + String(Geiger::ControlVoltage) + ",";
  

  // checksum
    unsigned int CHECKSUM = gps_CRC16_checksum(datastring.c_str());
    char checksum_str[6];
    sprintf(checksum_str, "*%04X", CHECKSUM);
    datastring += String(checksum_str);

    // transmit the data
    Serial3.println(F("[RTTY] Sending RTTY data ... "));

    // send out idle condition for 500 ms
    rtty.idle();
    delay(1000);
  
    Serial3.println(datastring);
	Serial3.println(geigerdata);
    rtty.println(datastring);

    Serial3.println(F("[RTTY] Done!"));

    // write a copy to the SD card; advance packet number
	writeData(datastring);
    pkt_num++;
}

// write data to the SD card
void writeData(String Str) {
   myFile = SD.open("data.txt", FILE_WRITE);
   if (myFile) {
    Serial3.print("[SD] Writing to data.txt...");
    myFile.println(Str);
    myFile.close();
    Serial3.println("done.");
  } else {
    // if the file didn't open, print an error:
    Serial3.println("error opening data.txt");
  }
}

// calculate a CRC16 checksum
uint16_t gps_CRC16_checksum(char *string) {
  size_t i;
  uint16_t crc;
  uint8_t c;

  crc = 0xFFFF;

  // Calculate checksum ignoring the first four $s
  for (i = 4; i < strlen(string); i++) {
    c = string[i];
    crc = _crc_xmodem_update(crc, c);
  }
  return crc;
}


void sendUBX(uint8_t *MSG, uint8_t len) {
  Serial1.flush();
  Serial1.write(0xFF); //
  delay(100);
  for(int i=0; i<len; i++) {
    Serial1.write(MSG[i]);
  }
}


boolean getUBX_ACK(uint8_t *MSG) {
  uint8_t b;
  uint8_t ackByteID = 0;
  uint8_t ackPacket[10];
  unsigned long startTime = millis();

  // Construct the expected ACK packet    
  ackPacket[0] = 0xB5;  // header
  ackPacket[1] = 0x62;  // header
  ackPacket[2] = 0x05;  // class
  ackPacket[3] = 0x01;  // id
  ackPacket[4] = 0x02;  // length
  ackPacket[5] = 0x00;
  ackPacket[6] = MSG[2];  // ACK class
  ackPacket[7] = MSG[3];  // ACK id
  ackPacket[8] = 0;   // CK_A
  ackPacket[9] = 0;   // CK_B

  // Calculate the checksums
  for (uint8_t ubxi=2; ubxi<8; ubxi++) {
    ackPacket[8] = ackPacket[8] + ackPacket[ubxi];
    ackPacket[9] = ackPacket[9] + ackPacket[8];
  }

  while (1) {

    // Test for success
    if (ackByteID > 9) {
      // All packets in order!
      return true;
    }

    // Timeout if no valid response in 3 seconds
    if (millis() - startTime > 3000) { 
      return false;
    }

    // Make sure data is available to read
    if (Serial1.available()) {
      b = Serial1.read();

      // Check that bytes arrive in sequence as per expected ACK packet
      if (b == ackPacket[ackByteID]) { 
        ackByteID++;
      } 
      else {
        ackByteID = 0;  // Reset and look again, invalid order
      }

    }
  }
}
