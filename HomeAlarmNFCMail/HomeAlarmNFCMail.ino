/*
PINOUT (TO COMPLETE):
RC522 MODULE    Uno/Nano     MEGA
SDA             D10          D12
SCK             D13          D52
MOSI            D11          D51
MISO            D12          D50
IRQ             N/A          N/A
GND             GND          GND
RST             D9           D11
3.3V            3.3V         3.3V
*/

// Library includes
#include <SPI.h>
#include <SD.h>
#include <Ethernet.h>
#include <RFID.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Defines SDA and RST from the RFID-RC522 (suppress ETH SHIELD conflict).
#define SDA_DIO 12
#define RESET_DIO 11

// Defines buffer size for HTTP clients.
#define REQ_BUF_SZ 60

// Define OneWire DS18B20
#define ONE_WIRE_BUS 2

// Defines ETH things 
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192,168,1,102);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);
EthernetServer server(84);
EthernetClient client;
File webFile;
char HTTP_req[REQ_BUF_SZ] = {0};
char req_index = 0;

// Defines LEDs 
const byte alarmLed = 9;              // Turns ON when the alarm is trigerred 
const byte warningLed = 8;            // Turns ON when a motion is detected 
const byte idleLed = 7;               // Is ON when the alarm is enabled 
const byte alarmStateLed = 6;         // GREEN is ON when the alarm is enabled / RED is ON when the alarm is disabled
const byte NFCLed = 13;               // Turns ON when a RFID device is detected

// Defines I/O
const byte PIRSensor = 3;             // INPUT - PIR sensor
const byte buzzer = 45;               // OUTPUT - buzzer 

// Defines LEDs states 
volatile byte alarm = LOW;
volatile byte warning = LOW;
volatile byte idle = LOW;
volatile byte alarmState = LOW;
volatile byte PIRState = LOW;

// Defines counters 
unsigned long startTime;
unsigned long stopTime;

// Defines alarm states 
int lock = 0;

// Defines RFID things 
RFID RC522(SDA_DIO, RESET_DIO); 
int rfid1[5];
 
boolean webState = 1;

// Setup a oneWire instance to communicate with any OneWire devices 
OneWire oneWire(ONE_WIRE_BUS);
 
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

void setup(){
  
  // Start serial for debuging
  Serial.begin(9600);

  // Set pinModes
  pinMode(alarmLed, OUTPUT);
  pinMode(warningLed, OUTPUT);
  pinMode(idleLed, OUTPUT);
  pinMode(alarmStateLed, OUTPUT);
  pinMode(NFCLed, OUTPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(PIRSensor, INPUT);

  // Calibrates PIR sensor (between 30 and 60 sec)
  Serial.print("Calibrating sensor ");
  for(int i = 0; i < 60; i++){
    Serial.print(".");
    delay(1000);
  }
  Buzz();
  Serial.println();
  
  if (!SD.begin(4)) {
    Serial.println("Can't init SD");
    return;
  }else{
    Serial.println("SD init");
  }
  
  if (!SD.exists("index.htm")) {
    Serial.println("Can't find file");
    return;
  }else{
    Serial.println("File loc");
  }
  
  Ethernet.begin(mac, ip, gateway, gateway, subnet);
  Serial.println("Ethernet begin");

  server.begin();
  Serial.println("Server begin");
  Serial.print("    Server is at IP ");
  Serial.println(Ethernet.localIP());
  Serial.print("    Server is at MAC ");
  Serial.print(mac[0],HEX);
  Serial.print(":");
  Serial.print(mac[1],HEX);
  Serial.print(":");
  Serial.print(mac[2],HEX);
  Serial.print(":");
  Serial.print(mac[3],HEX);
  Serial.print(":");
  Serial.print(mac[4],HEX);
  Serial.print(":");
  Serial.println(mac[5],HEX);
  
  SPI.begin();
  Serial.println("SPI");
  
  RC522.init();
  Serial.println("RFIF init");
  
  sensors.begin();
}

void loop(){

  sensors.requestTemperatures();

  // Print LEDs states
  digitalWrite(alarmLed, alarm);
  digitalWrite(warningLed, warning);
  digitalWrite(idleLed, idle);
  digitalWrite(alarmStateLed, alarmState);

  // Check PIR sensor
  if(digitalRead(PIRSensor) == HIGH){
    if (PIRState == LOW) {
      Serial.println("Motion detected !");
      PIRState = HIGH;
      
      if(alarmState == HIGH){
        warning = HIGH;
      } else {
        warning = LOW;
      }
    }
  }else{
    if (PIRState == HIGH){
      Serial.println("Motion ended !");
      PIRState = LOW;

      if(alarmState == HIGH){
        warning = HIGH;
      } else {
        warning = LOW;
      }
    }
  }

  // Check RFID 
  if(RC522.isCard()){
    RC522.readCardSerial();
    digitalWrite(NFCLed, HIGH); 
    delay(100);
    digitalWrite(NFCLed, LOW); 

    // Save device ID
    for(int i=0;i<5;i++){
      //Serial.print(RC522.serNum[i],HEX);
      rfid1[i] = RC522.serNum[i];
    }

    // Check if device is known (it can be a badge or a card)
    if((rfid1[0] == 0x16 && rfid1[1] == 0x1F && rfid1[2] == 0xF1 && rfid1[3] == 0x30 && rfid1[4] == 0xC8 ) || (rfid1[0] == 0xB6 && rfid1[1] == 0x4B && rfid1[2] == 0xEF && rfid1[3] == 0xC5 && rfid1[4] == 0xD7)){
      if(rfid1[0] == 0xB6){
        Serial.print(" Card  ");
      }else if(rfid1[0] == 0x16){
        Serial.print(" Badge ");
      }
      Serial.print(rfid1[0],HEX);
      Serial.print(".");
      Serial.print(rfid1[1],HEX);
      Serial.println(".xx.xx.xx.xx");

      // Change alarm state if RFID is known
      if(alarmState == HIGH) {
        lock = -1;
      }else{
        lock = -2;
      }
    delay(1000);
    }
  }

  // Check alarm state
  switch(lock){
    case -1:// Alarm disabled
      if(alarmState == HIGH){
        webState = 0;
        Serial.println();
        Serial.println((" -!- Alarm disabled -!-"));
        Serial.println();
        Buzz();
      }
      warning = LOW;
      alarmState = LOW;
      idle = LOW;
      break;
    case -2:// Alarm beeing enabled (start sequence, 20 seconds to go out)
      startTime = millis();
      stopTime = 0.0;
      lock = -3;
      break;
    case -3:// Alarm enabled (finish sequence)
      stopTime = millis();
      if(stopTime - startTime >= 20000) {
        webState = 1;
        Serial.println();
        Serial.println((" -!- Alarm enabled -!-"));
        Serial.println();
        Buzz();
        Buzz();
        idle = HIGH;
        alarmState = HIGH;
        lock = 0;
        startTime = 0.0;
        stopTime = 0.0;
      }
      break;
    case 0:// Alarm wait PIR sensor
      if(alarmState == HIGH && warning == HIGH) {
        startTime = millis();
        lock = 1;
      }
      break;
    case 1:// Detection state 1
      if(alarmState == HIGH && warning == HIGH){
   
        stopTime = millis();
        if(stopTime - startTime >= 2500) {
          lock = 2;
          startTime = millis();
        }
        Buzz();
        digitalWrite(alarmLed, HIGH);
        delay(100);  
        digitalWrite(alarmLed, LOW);
        delay(100);
      }
      break;
    case 2:// Detection state 2
      if(alarmState == HIGH && warning == HIGH){
        stopTime = millis();
        if(stopTime - startTime >= 5000) {
          lock = 3;
        }
        Buzz();
        digitalWrite(alarmLed, HIGH);
        delay(25);  
        digitalWrite(alarmLed, LOW);
        delay(25);
      }
      break;
    case 3:// Detection state 3, send mail 
      if(alarmState == HIGH && warning == HIGH){
        digitalWrite(alarmLed, HIGH);
        if(sendEmail()) { 
           Serial.println(F("Email sent"));
          lock = 4;
        } else { 
           Serial.println(F("Email failed"));
          lock = 5;
        }
      }
      break;
    case 4:// Detection state 4, mail sent
      warning = LOW;
      alarmState == HIGH;
      lock = 0;
      break;
    case 5:// Detection state 3, mail send
      warning = LOW;
      alarmState == HIGH;
      lock = 0;
      break;
    default: 
      digitalWrite(alarmLed, HIGH);
    break;
  }

  // Web client AJAX
  EthernetClient client = server.available();
  if (client){
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        if (req_index < (REQ_BUF_SZ - 1)){
          HTTP_req[req_index] = c;
          req_index++;
        }
        //Serial.println(HTTP_req);
        if (c == '\n' && currentLineIsBlank) {
          client.println("HTTP/1.1 200 OK");
          if (StrContains(HTTP_req, "ajax_inputs")) {
            client.println("Content-Type: text/xml");
            client.println("Connection: keep-alive");
            client.println();
            SetState();
            XML_response(client);
          } else {
            client.println("Content-Type: text/html");
            client.println("Connection: keep-alive");
            client.println();
            webFile = SD.open("index.htm");
            if (webFile) {
              while(webFile.available()) {
                client.write(webFile.read());
              }
              webFile.close();
            }
          }
        req_index = 0;
        StrClear(HTTP_req, REQ_BUF_SZ);
        break;
        } 
        if (c == '\n'){
          currentLineIsBlank = true;
        }else if (c != '\r'){
          currentLineIsBlank = false;
        }
      }
    }
    delay(1);
    client.stop();
  }
 // delay(10);
}

void Buzz(void){
  analogWrite(buzzer, 100);
  delay(50);
  analogWrite(buzzer ,0);
  delay(50);
}

void SetState(void) {
    // alarmState
    if (StrContains(HTTP_req, "state=1")) {
       webState = 1;
       lock = -2;
       Serial.println(" WEB HIGH");
    }
    else if (StrContains(HTTP_req, "state=0")) {
        webState = 0;
        lock = -1;
        Serial.println(" WEB LOW");
    }
}

// AJAX things
void XML_response(EthernetClient cl){
    
    cl.print("<?xml version = \"1.0\" ?>");
    cl.print("<inputs>");
    // read switches
    cl.print("<switch>");
    if (digitalRead(3)) {
        cl.print("ON");
    } else {
        cl.print("OFF");
    }
    cl.println("</switch>");
    cl.print("<switch>");
    if (digitalRead(2)) {
        cl.print("ON");
    } else {
        cl.print("OFF");
    }
    cl.println("</switch>");
    cl.print("<switch>");
    cl.print(sensors.getTempCByIndex(0)-2);
    cl.print("</switch>");
    
    // button alarmStates
    cl.print("<button>");
    if (webState == 1) {
        cl.print("ON");
    } else {
        cl.print("OFF");
    }
    cl.println("</button>");
    cl.print("</inputs>");
}

// Mail function
byte sendEmail(){
  if (client.connect("mail.smtp2go.com", 2525)) {
    client.println("EHLO 192.168.1.102");
    if(!eRcv()) return 0;
    client.println("auth login");
    if(!eRcv()) return 0;
    client.println("cC5rb2VuaWcuZ3VpbGxhdW1lQGdtYWlsLmNvbQ==");
    if(!eRcv()) return 0;
    client.println("VzNoV2lDSjhJVThi");
    if(!eRcv()) return 0;
    client.println("MAIL From: <p.koenig.guillaume@gmail.com>");
    if(!eRcv()) return 0;
    client.println("RCPT To: <p.koenig.guillaume@gmail.com>");
    client.println("RCPT To: <guillaume.koenig57@orange.fr>");
    if(!eRcv()) return 0;
    client.println("DATA");
    if(!eRcv()) return 0;
    client.println("To: You <p.koenig.guillaume@gmail.com>");
    client.println("From: Me <p.koenig.guillaume@gmail.com>");
    client.println("Subject: Arduino HomeAlarm\r\n");
    client.println("ALARM !\r\n");
    client.println("ALARM ! The HomeAlarm systme has detected a problem.\r\n");
    client.println("Contact 911 immediatly !");
    client.println(".");
    if(!eRcv()) return 0;
    client.println("QUIT");
    if(!eRcv()) return 0;
    client.stop();
    return 1;
  }
}

void StrClear(char *str, char length){
  for (int i = 0; i < length; i++) {
    str[i] = 0;
  }
}

char StrContains(char *str, const char *sfind){
  char found = 0;
  char index = 0;
  char len;
  len = strlen(str);

  if (strlen(sfind) > len) {
    return 0;
  }
  while (index < len) {
    if (str[index] == sfind[found]) {
      found++;
      if (strlen(sfind) == found) {
        return 1;
      }
    }else{
      found = 0;
    }
    index++;
  }
  return 0;
}

byte eRcv(){
  byte respCode;
  byte thisByte;
  int loopCount = 0;
 
  while(!client.available()){
    delay(1);
    loopCount++;
    if(loopCount > 10000){
      client.stop();
      Serial.println(F("\r\nTimeout"));
      return 0;
    }
  }
  respCode = client.peek();
  while(client.available()){  
    thisByte = client.read();    
    Serial.write(thisByte);
  }  
  if(respCode >= '4'){
    efail();
    return 0;  
  }
  return 1;
}
 
void efail(){
  byte thisByte = 0;
  int loopCount = 0;
 
  client.println(F("QUIT"));
  while(!client.available()) {
    delay(1);
    loopCount++;
    if(loopCount > 10000) {
      client.stop();
      Serial.println(F("\r\nTimeout"));
      return;
    }
  }
  while(client.available()){  
    thisByte = client.read();    
    Serial.write(thisByte);
  }
  client.stop();
  Serial.println(F("disconnected"));
}

