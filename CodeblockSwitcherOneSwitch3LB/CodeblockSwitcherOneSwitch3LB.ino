/*
 * FIS Sonderzeichen:
 * Ä = "_"
 * Ö = "`"
 * Ü = "a"
 */


#include "VAGFISWriter.h"
//#define DEBUG

#ifdef DEBUG
#define DEBUG_PRINT(x)    Serial.print (x)
#define DEBUG_PRINTHEX(x) Serial.print (x, HEX)
#define DEBUG_PRINTln(x)  Serial.println (x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTHEX(x)
#define DEBUG_PRINTln(x)
#endif

#include <Wire.h>
#include "NewSoftwareSerial.h"



#define LED1 9
#define LED2 10
#define LED3 11

#define pinKLineTX 12
#define pinKLineRX 13 //Fix: #define pinKLineRX PB0

#define FIS_Data A0
#define FIS_CLK A1
#define FIS_Switch A2
#define FIS_Enable 2

#define Switch1 5
#define Switch2 6
#define Switch3 7

NewSoftwareSerial obd(pinKLineRX, pinKLineTX, false); // RX, TX, inverse logic

VAGFISWriter fisWriter( FIS_CLK, FIS_Data, FIS_Enable, 0);

uint8_t blockCounter = 0;

int Coding = 0;
int inputCoding = 0;

int WSC = 0;
int newCoding = 0;
int newWSC = 0;


bool CodingPending = true;
bool CodingError = false;

bool Switch1_old = false;
bool Switch2_old = false;
bool Switch3_old = false;

bool ECUfault = false;
int ECUtries = 0;


void obdWrite(uint8_t data) {
  obd.write(data);
}

uint8_t obdRead() {
  unsigned long timeout = millis() + 1000;
  while (!obd.available()) {
    if (millis() >= timeout) {
      DEBUG_PRINTln(F("ERROR: obdRead timeout"));
      return 0;
    }
  }
  uint8_t data = obd.read();
  return data;
}

void send5baud(uint8_t data) {
  // // 1 start bit, 7 data bits, 1 parity, 1 stop bit
#define bitcount 10
  byte bits[bitcount];
  byte even = 1;
  byte bit;
  for (int i = 0; i < bitcount; i++) {
    bit = 0;
    if (i == 0)  bit = 0;
    else if (i == 8) bit = even; // computes parity bit
    else if (i == 9) bit = 1;
    else {
      bit = (byte) ((data & (1 << (i - 1))) != 0);
      even = even ^ bit;
    }
    DEBUG_PRINT(F("bit"));
    DEBUG_PRINT(i);
    DEBUG_PRINT(F("="));
    DEBUG_PRINT(bit);
    if (i == 0) DEBUG_PRINT(F(" startbit"));
    else if (i == 8) DEBUG_PRINT(F(" parity"));
    else if (i == 9) DEBUG_PRINT(F(" stopbit"));
    DEBUG_PRINTln();
    bits[i] = bit;
  }
  // now send bit stream
  for (int i = 0; i < bitcount + 1; i++) {
    if (i != 0) {
      // wait 200 ms (=5 baud), adjusted by latency correction
      delay(200);
      if (i == bitcount) break;
    }
    if (bits[i] == 1) {
      // high
      digitalWrite(pinKLineTX, HIGH);
    } else {
      // low
      digitalWrite(pinKLineTX, LOW);
    }
  }
  obd.flush();
}

bool KWP5BaudInit(uint8_t addr) {
  DEBUG_PRINTln(F("---KWP 5 baud init"));
  send5baud(addr);
  return true;
}

bool KWPSendBlock(char *s, int size) {
  DEBUG_PRINT(F("---KWPSend sz="));
  DEBUG_PRINT(size);
  DEBUG_PRINT(F(" blockCounter="));
  DEBUG_PRINTln(blockCounter);
  // show data
  DEBUG_PRINT(F("OUT:"));
  for (int i = 0; i < size; i++) {
    uint8_t data = s[i];
    DEBUG_PRINTHEX(data);
    DEBUG_PRINT(" ");
  }
  DEBUG_PRINTln();
  for (int i = 0; i < size; i++) {
    uint8_t data = s[i];
    obdWrite(data);
    if (i < size - 1) {
      uint8_t complement = obdRead();
      if (complement != (data ^ 0xFF)) {
        DEBUG_PRINTln(F("ERROR: invalid complement"));
        return false;
      }
    }
  }
  blockCounter++;
  return true;
}

bool KWPReceiveBlock(char s[], int maxsize, int &size) {
  bool ackeachbyte = false;
  uint8_t data = 0;
  int recvcount = 0;
  if (size == 0) ackeachbyte = true;
  DEBUG_PRINT(F("---KWPReceive sz="));
  DEBUG_PRINT(size);
  DEBUG_PRINT(F(" blockCounter="));
  DEBUG_PRINTln(blockCounter);
  if (size > maxsize) {
    DEBUG_PRINTln("ERROR: invalid maxsize");
    return false;
  }
  unsigned long timeout = millis() + 1000;
  while ((recvcount == 0) || (recvcount != size)) {
    while (obd.available()) {
      data = obdRead();
      s[recvcount] = data;
      recvcount++;
      if ((size == 0) && (recvcount == 1)) {
        size = data + 1;
        if (size > maxsize) {
          DEBUG_PRINTln("ERROR: invalid maxsize");
          return false;
        }
      }
      if ((ackeachbyte) && (recvcount == 2)) {
        if (data != blockCounter) {
          DEBUG_PRINTln(F("ERROR: invalid blockCounter"));
          return false;
        }
      }
      if ( ((!ackeachbyte) && (recvcount == size)) ||  ((ackeachbyte) && (recvcount < size)) ) {
        obdWrite(data ^ 0xFF);  // send complement ack
      }
      timeout = millis() + 1000;
    }
    if (millis() >= timeout) {
      DEBUG_PRINTln(F("ERROR: timeout"));
      ECUtries++;
      if (ECUtries <= 4) {
        connect(0x01, 9600);
      } else {
        ECUfault = true;
      }
      return false;
    }
  }
  // show data
  DEBUG_PRINT(F("IN: sz="));
  DEBUG_PRINT(size);
  DEBUG_PRINT(F(" data="));
  for (int i = 0; i < size; i++) {
    uint8_t data = s[i];
    DEBUG_PRINTHEX(data);
    DEBUG_PRINT(F(" "));
  }
  DEBUG_PRINTln();
  blockCounter++;
  return true;
}

bool KWPSendAckBlock() {
  DEBUG_PRINT(F("---KWPSendAckBlock blockCounter="));
  DEBUG_PRINTln(blockCounter);
  char buf[32];
  sprintf(buf, "\x03%c\x09\x03", blockCounter);
  return (KWPSendBlock(buf, 4));
}

bool connect(uint8_t addr, int baudrate) {
  DEBUG_PRINT(F("------connect addr="));
  DEBUG_PRINT(addr);
  DEBUG_PRINT(F(" baud="));
  DEBUG_PRINTln(baudrate);

  blockCounter = 0;
  obd.begin(baudrate);
  KWP5BaudInit(addr);
  // answer: 0x55, 0x01, 0x8A
  char s[3];
  int size = 3;
  if (!KWPReceiveBlock(s, 3, size)) return false;
  if (    (((uint8_t)s[0]) != 0x55)
          ||   (((uint8_t)s[1]) != 0x01)
          ||   (((uint8_t)s[2]) != 0x8A)   ) {
    DEBUG_PRINTln(F("ERROR: invalid magic"));
    return false;
  }
  if (!readConnectBlocks()) return false;
  return true;
}

bool readConnectBlocks() {
  // read connect blocks
  DEBUG_PRINTln(F("------readconnectblocks"));
  String info;
  while (true) {
    int size = 0;
    char s[64];
    if (!(KWPReceiveBlock(s, 64, size))) return false;
    if (size == 0) return false;
    if (s[2] == '\x09') {
      break;
    }
    if (s[2] == '\xF6' && s[3] == '\x00') {
      byte MSB_Coding = s[4];
      byte LSB_Coding = s[5];
      Coding = (MSB_Coding * 0x100 + LSB_Coding) / 2;
      byte MSB_WSC = s[6];
      byte LSB_WSC = s[7];
      WSC = MSB_WSC * 0x100 + LSB_WSC;
    }
    if (s[2] != '\xF6') {
      Serial.println(F("ERROR: unexpected answer"));
      return false;
    }
    String text = String(s);
    info += text.substring(3, size - 1);
    if (!KWPSendAckBlock()) return false;
  }
  Serial.println(info);
  Serial.print("Codierung: ");
  Serial.print(Coding);
  Serial.print(" WSC: ");
  Serial.println(WSC);
  return true;
}

bool sendCoding(int _Coding, int _WSC) {
  newCoding = _Coding;
  newWSC = _WSC;

  DEBUG_PRINT(F("---KWPSendCodingBlock blockCounter="));
  DEBUG_PRINTln(blockCounter);


  char buf[32];

  byte msb_coding = (_Coding * 2) & 0xFF;
  byte lsb_coding = (_Coding * 2) >> 8;
  byte msb_wsc = _WSC & 0xFF;
  byte lsb_wsc = _WSC >> 8;
  sprintf(buf, "\x07%c\x10%c%c%c%c\x03", blockCounter, lsb_coding, msb_coding, lsb_wsc, msb_wsc);

  if (!KWPSendBlock(buf, 8)) return false;
  if (!readConnectBlocks()) return false;




  return true;

}

// One switch
int readInput() {
  // Use only Codeblocks 2 and 3
  if (digitalRead(Switch1)) {
    Switch1_old = true;
    return 2;
  }
  else {
    Switch1_old = false;
    return 3;
  }
}

void toggle109() {
  digitalWrite(LED3, LOW);
  delay(50);
  digitalWrite(LED3, HIGH);
}


// three switches to +12V (Rotary)
/*int readInput() {
  if(digitalRead(Switch1)) return 1;
  else if(digitalRead(Switch2)) return 2;
  else if(digitalRead(Switch3)) return 3;

  // if no input is HIGH, return actual coding
  else return Coding;
  }
*/

// three switches to GND (Rotary)
/*int readInput() {
  if(!digitalRead(Switch1)) return 1;
  else if(!digitalRead(Switch2)) return 2;
  else if(!digitalRead(Switch3)) return 3;

  // if no input is LOW, return actual coding
  else return Coding;
  }
*/


void setup() {
  // Configure I/O
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);

  pinMode(FIS_Switch, OUTPUT);

  // Switch to Ground
  pinMode(Switch1, INPUT_PULLUP);
  pinMode(Switch2, INPUT_PULLUP);
  pinMode(Switch3, INPUT_PULLUP);

  digitalWrite(LED3, HIGH);

  /* For Switch to +12V add R_OPT (external Pull-Downs)
    pinMode(Switch1, INPUT);
    pinMode(Switch2, INPUT);
    pinMode(Switch3, INPUT);
  */

  // Init K-Line interface
  pinMode(pinKLineTX, OUTPUT);
  digitalWrite(pinKLineTX, HIGH);

  // Init serial line - for debugging
  Serial.begin(19200);


  changeCodeblock(true);
 
}

void changeCodeblock(bool firstRun) {
   // Enable 3LB lines to cluster
  digitalWrite(FIS_Switch, HIGH);
  // Enable 3LB
  fisWriter.begin();
  // Clear screen
  fisWriter.reset();
  fisWriter.initFullScreen();

  // Light up output led
  digitalWrite(LED1, HIGH);

  if (firstRun) {
  // Write Welcome Message
  fisWriter.sendMsgFS(0, 40, 0x25, 10, "WILLKOMMEN");


  

  // Wait 500ms to settle down Immo <-> ECU
  delay(500);
  }


  // Write Connect Message
  Serial.println("Verbinde mit MSG...");
  fisWriter.sendMsgFS(0, 72, 0x25, 8, "VERBINDE");
  fisWriter.sendMsgFS(0, 80, 0x25, 7, "MIT MSG");

  // Connect to ECU
  connect(0x01, 9600);

  if (!ECUfault) {


    // Read input
    inputCoding = readInput();


    // Clear screen and write current Coding
    fisWriter.initFullScreen();
    fisWriter.sendMsgFS(0, 0, 0x25, 8, "AKTUELLE");
    fisWriter.sendMsgFS(0, 8, 0x25, 9, "CODIERUNG");
    char b[2];
    String str;
    str = String(Coding);
    str.toCharArray(b, 2);
    fisWriter.sendMsgFS(0, 16, 0x25, 1, b);

    // Write new Coding

    fisWriter.sendMsgFS(0, 32, 0x25, 8, "GEW_HLTE");
    fisWriter.sendMsgFS(0, 40, 0x25, 9, "CODIERUNG");
    str = String(inputCoding);
    str.toCharArray(b, 2);
    fisWriter.sendMsgFS(0, 48, 0x25, 1, b);

    // Check if new Coding is different to actual coding

    if (inputCoding != Coding) {
      // New Coding is different
      // Recode ECU

      // Clear screen and write Send Coding
      fisWriter.initFullScreen();

      fisWriter.sendMsgFS(0, 0, 0x25, 8, "AKTUELLE");
      fisWriter.sendMsgFS(0, 8, 0x25, 9, "CODIERUNG");
      str = String(Coding);
      str.toCharArray(b, 2);
      fisWriter.sendMsgFS(0, 16, 0x25, 1, b);


      fisWriter.sendMsgFS(0, 32, 0x25, 5, "SENDE");
      fisWriter.sendMsgFS(0, 40, 0x25, 4, "NEUE");
      fisWriter.sendMsgFS(0, 48, 0x25, 9, "CODIERUNG");
      char b[2];
      String str;
      str = String(inputCoding);
      str.toCharArray(b, 2);
      fisWriter.sendMsgFS(0, 56, 0x25, 1, b);


      // Send recode command
      sendCoding(inputCoding, WSC);

      // Wait 500ms (for readability)
      delay(500);

      // Check if new Coding is accepted
      if (Coding == newCoding) {
        // Recode was successful

        // Clear screen and write success message
        fisWriter.initFullScreen();
        fisWriter.sendMsgFS(0, 0, 0x25, 9, "CODIERUNG");
        fisWriter.sendMsgFS(0, 8, 0x25, 10, "ERFOLREICH");
        fisWriter.sendMsgFS(0, 16, 0x25, 10, "aBERNOMMEN");

        // Write message for ignition cycle
        fisWriter.sendMsgFS(0, 56, 0x25, 7, "ZaNDUNG");
        fisWriter.sendMsgFS(0, 64, 0x25, 14, "AUS UND WIEDER");
        fisWriter.sendMsgFS(0, 72, 0x25, 11, "EINSCHALTEN");

        Serial.println("Codierung erfolgreich übernommen. Zündung aus- und wieder einschalten!");

        CodingPending = true;
        // Optional: Disconnect ECU to force reboot
        //toggle109();
        
      }
      else if (Coding != newCoding) {
        // Recode was not successful (e.g. ECU doesn't support coding, or codeblock is empty
        CodingError = true;

        // Clear screeen and write fault message
        fisWriter.initFullScreen();
        fisWriter.sendMsgFS(0, 16, 0x25, 9, "CODIERUNG");
        fisWriter.sendMsgFS(0, 24, 0x25, 6, "KONNTE");
        fisWriter.sendMsgFS(0, 32, 0x25, 5, "NICHT");
        fisWriter.sendMsgFS(0, 40, 0x25, 10, "aBERNOMMEN");
        fisWriter.sendMsgFS(0, 48, 0x25, 7, "WERDEN!");
         ECUtries = 0;

        Serial.println("Codierung konnte nicht übernommen werden");
      }

      // Debug: print elapsed time
      Serial.print("Dauer des Vorgangs: ");
      Serial.print(millis());
      Serial.println(" ms");

    }
    else {
      // New Coding and actual coding are identical
      // no need for Recode

      // Disable LED blinking
      CodingPending = false;

      // Debug: print elapsed time
      Serial.print("Dauer des Vorgangs: ");
      Serial.print(millis());
      Serial.println(" ms");

      // Create "countdown"
      /*fisWriter.sendMsgFS(0, 64, 0x25, 3, "...");
        delay(1000);
        fisWriter.sendMsgFS(0, 72, 0x25, 2, "..");
        delay(1000);
        fisWriter.sendMsgFS(0, 80, 0x25, 1, ".");
        delay(1000);

      */
      fisWriter.sendMsgFS(0, 64, 0x25, 9, "CODIERUNG");
      fisWriter.sendMsgFS(0, 72, 0x25, 9, "IDENTISCH");
      delay(3000);
      // and exit 3LB Mode
      fisWriter.reset();
      digitalWrite(FIS_Switch, LOW);

    }

    // Disable Status LED
    digitalWrite(LED1, LOW);



    // Toggle status LED every 250 ms
    int i = 0;
    while (CodingPending) {
      i++;
      if (!CodingError) {

        // and exit 3LB after 5 seconds
        if (i == 5) {
          fisWriter.reset();
          digitalWrite(FIS_Switch, LOW);
          digitalWrite(LED1, LOW);
          break;
        }
        digitalWrite(LED1, LOW);
        delay(500);
        digitalWrite(LED1, HIGH);
        delay(500);
      }
      else {
        // and exit 3LB after 5 seconds
        if (i == 30) {
          fisWriter.reset();
          digitalWrite(FIS_Switch, LOW);
          CodingPending = false;
          digitalWrite(LED1, LOW);
          break;
        }
        digitalWrite(LED1, LOW);
        delay(125);
        digitalWrite(LED1, HIGH);
        delay(125);

      }
    }
  }
  else {
    Serial.println("Keine Verbindung zum MSG möglich!");
    
    fisWriter.reset();
    fisWriter.initFullScreen();
    fisWriter.sendMsgFS(0, 8, 0x25, 7, "FEHLER!");
    fisWriter.sendMsgFS(0, 32, 0x25, 5, "KEINE");
    fisWriter.sendMsgFS(0, 40, 0x25, 10, "VERBINDUNG");
    fisWriter.sendMsgFS(0, 48, 0x25, 7, "M`GLICH");
    delay(5000);
    fisWriter.reset();
    ECUtries = 0;
    digitalWrite(FIS_Switch, LOW); 
    digitalWrite(LED1, LOW);

  }


  // We're finished
  // nothing else to do
}

void loop() {
  // If one switch changes state
  if (digitalRead(Switch1) != Switch1_old) {
    // wait 250 ms (debouncing)
    delay(250);
    // Check again
    if (digitalRead(Switch1) != Switch1_old) {
      changeCodeblock(false);
    }
  }
}
