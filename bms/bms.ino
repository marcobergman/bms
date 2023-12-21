/**
   bms.ino
    Marco Bergman
    Created on: 10-DEC-2023

*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "ESP8266TimerInterrupt.h"             //https://github.com/khoih-prog/ESP8266TimerInterrupt
#include <Adafruit_ADS1X15.h>
#include <Adafruit_INA228.h>

// User configuration starts here
String wifiSsid        =  "openplotter";
String wifiPassword    =  "12345678";
String signalkIpString =  "10.10.10.1";
int    signalkUdpPort  =  30330;
String signalkSource   =  "DIY BMS";

// Calibration for the voltage dividers
bool calibrationTime = false;
float referenceVoltage = 10.00;
float value0V[4] = {2, 1, 1, 1};
float valueRef[4] = {14503, 14348, 14676, 14365};

// BMS parameters
float chargeDisconnectVoltage = 3.6;
float chargeDisconnectSoc = 90;
float chargeDisconnectTemp = 3;
float chargeDisconnectCurrent = 30;
float chargeReconnectVoltage = 3.5;
float chargeReconnectSoc = 80;
float chargeReconnectTemp = 4;
String chargeReconnectCurrent = "MANUAL";
float chargeAlarmVoltage = 3.4;
float chargeAlarmSoc = 85;
float chargeAlarmTemp = 5;
float chargeAlarmCurrent = 25;
float dischargeAlarmVoltage = 3.25;
float dischargeAlarmSoc = 25;
float dischargeAlarmTemp = 50;
float dischargeAlarmCurrent = 50;
float dischargeReconnectVoltage = 3.2;
float dischargeReconnectSoc = 30;
float dischargeReconnectTemp = 60;
String dischargeReconnectCurrent = "MANUAL";
float dischargeDisconnectVoltage = 3.1;
float dischargeDisconnectSoc = 20;
float dischargeDisconnectTemp = 70;
float dischargeDisconnectCurrent = 60;

float calibrationVoltageMax = 3.70;
float calibrationVoltageMin = 3.20;
float calibrationHysteresisVoltage = 0.1;
float calibrationSocMax = 100;
float calibrationSocMin = 17;
float packCapacity = 280; // Ah
float actualDischarge = 0; // Ah
float shuntResistance = 0.00605; // Ohm
// User configuration ends here

// For setRelais():
const int CHARGERELAIS = 13; // D7 = GPIO13
const int DISCHARGERELAIS = 15; // D8 = GPI15 'LOAD'
const int CONNECT = 1;
const int DISCONNECT = 0;

const int LED = 2; // GPIO2 =  Wemos onboard LED
const unsigned int MAX_MESSAGE_LENGTH = 12;

String bmsStatus = "OK";
String chargeStatus = "";
String dischargeStatus = "";

float voltage0 = 0; // defined here because needs to be set in setup()
float voltage1 = 0;
float voltage2 = 0;
float voltage3 = 0;
float packDischargeCurrent = 0;

float maxCellVoltage = 0;
float minCellVoltage = 0;

WiFiClient client;
WiFiUDP udp;
const String wifiStatus[8] = {"WL_IDLE_STATUS", "WL_NO_SSID_AVAIL", "unknown", "WL_CONNECTED", "WL_CONNECT_FAILED", "", "WL_CONNECT_WRONG_PASSWORD", "WL_DISCONNECTED"};

Adafruit_ADS1115 ads;
Adafruit_INA228 ina228 = Adafruit_INA228();

IPAddress signalkIp;
bool x = signalkIp.fromString(signalkIpString);

int i = 1; // metadata counter

bool mustSendConfig = false;
bool mustTestWifi = false;
bool mustWakeWifi = false;
bool wifiAsleep = false;

bool capacitySet = false;

ESP8266Timer timer;
#define TIMER_INTERVAL_MS 10000  // Call TimerHandler every 10 seconds
int bmsClock = 0;


void IRAM_ATTR TimerHandler() {
  bmsClock += 1;

  if (bmsClock % 6 == 0) {
    mustTestWifi = true;
  }
  if ((bmsClock + 4) % 5 == 0) {
    mustSendConfig = true;
  }
  if (bmsClock % 6 == 0 && wifiAsleep) {
    mustWakeWifi = true;
  }
}


void setup() {
//TEMPORARY: TEST WITH 18650 cell pack
chargeDisconnectVoltage = 4.2;
chargeReconnectVoltage = 4.1;
chargeAlarmVoltage = 4.05;
dischargeAlarmVoltage = 3.75;
dischargeReconnectVoltage = 3.7;
dischargeDisconnectVoltage = 3.6;
calibrationVoltageMax = 4.17;
calibrationVoltageMin = 3.50;
calibrationSocMin = 10;
packCapacity = 2.6; // Ah


  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  startWifi();
  udp.begin(33333);
  pinMode(DISCHARGERELAIS, OUTPUT);
  pinMode(CHARGERELAIS, OUTPUT);
  pinMode(LED, OUTPUT); 
  pinMode(13, INPUT_PULLUP); // pulling down D7 resets CHARGE
  ads.setGain(GAIN_TWO);
  ads.begin();
  delay(100);
  voltage0 = readVoltage(0);
  voltage1 = readVoltage(1);
  voltage2 = readVoltage(2);
  voltage3 = readVoltage(3);

  if (!ina228.begin()) {
    Serial.println("Couldn't find INA228 chip");
    while (1)
      ;
  }
  ina228.setShunt(shuntResistance, 10.0);

  bool x = timer.attachInterruptInterval(TIMER_INTERVAL_MS * 1000, TimerHandler);
  Serial.println("Setup completed");
}


void startWifi() {
  Serial.println("\nWifi connecting...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid, wifiPassword);
}


void testWifi() {
  if (wifiAsleep) {
    return;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print ("Wifi connected with IP address ");
    Serial.println (WiFi.localIP());
  }
  else if (WiFi.status() == WL_NO_SSID_AVAIL) {
    Serial.println ("Wifi base station not found. Wifi going to sleep to preserve energy.");
    WiFi.setSleepMode (WIFI_MODEM_SLEEP);
    WiFi.forceSleepBegin ();
    wifiAsleep = true;
  }
  else {
    Serial.printf ("WIFI Connection status: %d: ", WiFi.status());
    Serial.println (wifiStatus[WiFi.status()]);
    WiFi.printDiag(Serial);
  }
}


void wakeWifi() {
  Serial.println ("Wifi waking up.");
  WiFi.forceSleepWake();
  wifiAsleep = false;
  bmsClock = 5;
}


void sendSignalkMessage (String message) {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  //Serial.println(message);
  udp.beginPacket(signalkIp, signalkUdpPort);
  udp.write(message.c_str());
  udp.endPacket();  
}


void sendBmsConfig () {
  // send configuration parameters to SignalK
  Serial.println ("Sending BMS config parameters.");
  String value = "{";
  value = value  + "\"chargeDisconnectVoltage\": " + String(chargeDisconnectVoltage) +", ";
  value = value  + "\"chargeDisconnectSoc\": " + String(chargeDisconnectSoc) +", ";
  value = value  + "\"chargeDisconnectTemp\": " + String(chargeDisconnectTemp) +", ";
  value = value  + "\"chargeDisconnectCurrent\": " + String(chargeDisconnectCurrent) +", ";
  value = value  + "\"chargeReconnectVoltage\": " + String(chargeReconnectVoltage) +", ";
  value = value  + "\"chargeReconnectSoc\": " + String(chargeReconnectSoc) +", ";
  value = value  + "\"chargeReconnectTemp\": " + String(chargeReconnectTemp) +", ";
  value = value  + "\"chargeReconnectCurrent\": \"" + String(chargeReconnectCurrent) +"\", ";
  value = value  + "\"chargeAlarmVoltage\": " + String(chargeAlarmVoltage) +", ";
  value = value  + "\"chargeAlarmSoc\": " + String(chargeAlarmSoc) +", ";
  value = value  + "\"chargeAlarmTemp\": " + String(chargeAlarmTemp) +", ";
  value = value  + "\"chargeAlarmCurrent\": " + String(chargeAlarmCurrent) +", ";
  value = value  + "\"dischargeAlarmVoltage\": " + String(dischargeAlarmVoltage) +", ";
  value = value  + "\"dischargeAlarmSoc\": " + String(dischargeAlarmSoc) +", ";
  value = value  + "\"dischargeAlarmTemp\": " + String(dischargeAlarmTemp) +", ";
  value = value  + "\"dischargeAlarmCurrent\": " + String(dischargeAlarmCurrent) +", ";
  value = value  + "\"dischargeReconnectVoltage\": " + String(dischargeReconnectVoltage) +", ";
  value = value  + "\"dischargeReconnectSoc\": " + String(dischargeReconnectSoc) +", ";
  value = value  + "\"dischargeReconnectTemp\": " + String(dischargeReconnectTemp) +", ";
  value = value  + "\"dischargeReconnectCurrent\": \"" + String(dischargeReconnectCurrent) +"\", ";
  value = value  + "\"dischargeDisconnectVoltage\": " + String(dischargeDisconnectVoltage) +", ";
  value = value  + "\"dischargeDisconnectSoc\": " + String(dischargeDisconnectSoc) +", ";
  value = value  + "\"dischargeDisconnectTemp\": " + String(dischargeDisconnectTemp) +", ";
  value = value  + "\"dischargeDisconnectCurrent\": " + String(dischargeDisconnectCurrent);
  value = value + "}";

  String message = "{\"updates\":[{\"$source\": \""+ signalkSource + "\", \"values\":[{\"path\":\"bms.config\",\"value\":" + value + "}]}]}";

  sendSignalkMessage (message);
}


void sendBmsState(float packSoc, float packDischargeCurrent, String bmsStatus, float packTemp) {
  // send bsm status to SignalK
  String value = "{";
  value = value  + "\"packSoc\": " + String(packSoc) +", ";
  value = value  + "\"packDischargeCurrent\": " + String(packDischargeCurrent) +", ";
  value = value  + "\"packTemp\": " + String(packTemp) +", ";
  value = value  + "\"bmsStatus\": \"" + String(bmsStatus) + "\"";
  value = value + "}";

  String message = "{\"updates\":[{\"$source\": \""+ signalkSource + "\", \"values\":[{\"path\":\"bms.state\",\"value\":" + value + "}]}]}";

  sendSignalkMessage (message);
}


void signalkSendValue (String path, String value, String units) {
  // send one particular value to signalk. Every now and then, set the 'units' metadata.
  String message = "{\"updates\":[{\"$source\": \""+ signalkSource + "\", \"values\":[{\"path\":\"" + path + "\",\"value\":" + value + "}]}]}";

  i -= 1; if (i == 0) {  // to start, then periodically, update the 'units' metadata:
    message = "{\"updates\":[{\"meta\":[{\"path\":\"" + path + "\",\"value\":{\"units\":\"" + units + "\"}}]}]}";
    i = 99;
  }
  sendSignalkMessage (message);
}


float readPackSoc() {
  float soc;
  actualDischarge = ina228.readCharge();
  soc = (packCapacity - actualDischarge) / packCapacity * 100;
  return soc;
}


float readPackDischargeCurrent() {
  return ina228.readCurrent()/1000; // Convert milli amps to amps
}


void checkCalibration() {
  if (maxCellVoltage > calibrationVoltageMax && packDischargeCurrent > 0) { // not charging
    ina228.resetAcc(); // Reset accumulator registers on the INA228: "reset the on-chip coulomb counter"
    Serial.println("Defining pack to be at " + String(calibrationSocMax) + " %.");
  }
  if (minCellVoltage < calibrationVoltageMin && packDischargeCurrent > 0 && !capacitySet) { // not charging
    packCapacity = actualDischarge / (calibrationSocMax - calibrationSocMin) * 100;
    capacitySet = true; 
    Serial.println("Setting packCapacity to " + String (packCapacity, 1) + "Ah.");
  }
  if (minCellVoltage > calibrationVoltageMin + calibrationHysteresisVoltage && packDischargeCurrent > 0 && capacitySet) {
    capacitySet = false;
  }
}


float readPackTemp() {
  // temporary, awaiting temp sensors
  return ina228.readDieTemp();
}

float readVoltage(int index) {
  // Read voltage from a particular cell and apply calibration
  int16_t value = ads.readADC_SingleEnded(index);
  float voltage = (value - value0V[index])/(valueRef[index] - value0V[index]) * referenceVoltage;
  if (calibrationTime) {
      Serial.println("adc[" + String(index) + "]: " + String(value) + ": " + String(voltage, 3) + "V");
  }
  return voltage;
}

float maxVoltage (float a, float b, float c, float d) {
  float m = max(a, b);
  m = max (m, c);
  m = max (m, d);
  return m;
}

float minVoltage (float a, float b, float c, float d) {
  float m = min(a, b);
  m = min (m, c);
  m = min (m, d);
  return m;
}

void sendCellVoltages(float cell0Voltage, float cell1Voltage, float cell2Voltage, float cell3Voltage) {
  signalkSendValue("cell0.voltage", String(cell0Voltage, 3), "V");
  signalkSendValue("cell1.voltage", String(cell1Voltage, 3), "V");
  signalkSendValue("cell2.voltage", String(cell2Voltage, 3), "V");
  signalkSendValue("cell3.voltage", String(cell3Voltage, 3), "V");
}


void setRelais (int gpio, int value) {
  // Connect or disconnect charge or load relais
    if (value == CONNECT) {
      digitalWrite(gpio, HIGH);
    }
    else {
      digitalWrite(gpio, LOW);
    }
}


void blink() {
  // 2 blinks = connected to Wifi, 1 blink = power saving mode
  const int blinkMs = 2;
  digitalWrite(LED, LOW);
  delay (blinkMs);
  digitalWrite(LED, HIGH);
  if (!wifiAsleep){
    delay (100);
    digitalWrite(LED, LOW);
    delay (blinkMs);
    digitalWrite(LED, HIGH);
  }
}


void printCurrentValues() {
  // For CLI purposes:
  //
  Serial.println("cdv: chargeDisconnectVoltage: " + String(chargeDisconnectVoltage, 3));
  Serial.println("cds: chargeDisconnectSoc: " + String(chargeDisconnectSoc));
  Serial.println("cdt: chargeDisconnectTemp: " + String(chargeDisconnectTemp));
  Serial.println("cdc: chargeDisconnectCurrent: " + String(chargeDisconnectCurrent));
  Serial.println("crv: chargeReconnectVoltage: " + String(chargeReconnectVoltage, 3));
  Serial.println("crs: chargeReconnectSoc: " + String(chargeReconnectSoc));
  Serial.println("crt: chargeReconnectTemp: " + String(chargeReconnectTemp));
  Serial.println("cav: chargeAlarmVoltage: " + String(chargeAlarmVoltage, 3));
  Serial.println("cas: chargeAlarmSoc: " + String(chargeAlarmSoc));
  Serial.println("cat: chargeAlarmTemp: " + String(chargeAlarmTemp));
  Serial.println("cac: chargeAlarmCurrent: " + String(chargeAlarmCurrent));
  Serial.println("dav: dischargeAlarmVoltage: " + String(dischargeAlarmVoltage, 3));
  Serial.println("das: dischargeAlarmSoc: " + String(dischargeAlarmSoc));
  Serial.println("dat: dischargeAlarmTemp: " + String(dischargeAlarmTemp));
  Serial.println("dac: dischargeAlarmCurrent: " + String(dischargeAlarmCurrent));
  Serial.println("drv: dischargeReconnectVoltage: " + String(dischargeReconnectVoltage, 3));
  Serial.println("drs: dischargeReconnectSoc: " + String(dischargeReconnectSoc));
  Serial.println("drt: dischargeReconnectTemp: " + String(dischargeReconnectTemp));
  Serial.println("ddv: dischargeDisconnectVoltage: " + String(dischargeDisconnectVoltage, 3));
  Serial.println("dds: dischargeDisconnectSoc: " + String(dischargeDisconnectSoc));
  Serial.println("ddt: dischargeDisconnectTemp: " + String(dischargeDisconnectTemp));
  Serial.println("ddc: dischargeDisconnectCurrent: " + String(dischargeDisconnectCurrent));
  Serial.println("cvm: calibrationVoltageMax: " + String(calibrationVoltageMax, 3));
  Serial.println("cvn: calibrationVoltageMin: " + String(calibrationVoltageMin, 3));	
  Serial.println("csm: calibrationHysteresisVoltage: " + String(calibrationHysteresisVoltage));
  Serial.println("csm: calibrationSocMax: " + String(calibrationSocMax));
  Serial.println("csn: calibrationSocMin: " + String(calibrationSocMin));	
  Serial.println("pc: packCapacity: " + String(packCapacity));
  Serial.println("ad: actualDischarge: " + String(actualDischarge));	
  Serial.println("sr: shuntResistance: " + String(shuntResistance, 8));
}


bool processMessage(String message) {
  // process message from the cli provider
  if (message == "") {
    return false;
  }

  if (message == "q") {
    // quit cli
    return true;
  }

  if (message == "v") {
    printCurrentValues();
    return false;
  }

  int delimiterPos = message.indexOf ("=");
  if (delimiterPos == -1) {
    Serial.println("Format: parameter=value");
    return false;
  }

  // split message into parameter and value
  String parameter = message.substring(0, delimiterPos);
  parameter.toLowerCase();
  float value = message.substring(delimiterPos+1).toFloat();
  bool parameterUnknown = false;
	
  if      (parameter == "cdv") chargeDisconnectVoltage = value;
  else if (parameter == "cds") chargeDisconnectSoc = value;
  else if (parameter == "cdt") chargeDisconnectTemp = value;
  else if (parameter == "crv") chargeReconnectVoltage = value;
  else if (parameter == "crs") chargeReconnectSoc = value;
  else if (parameter == "crt") chargeReconnectTemp = value;
  else if (parameter == "crc") chargeReconnectCurrent = value;
  else if (parameter == "cav") chargeAlarmVoltage = value;
  else if (parameter == "cas") chargeAlarmSoc = value;
  else if (parameter == "cat") chargeAlarmTemp = value;
  else if (parameter == "cac") chargeAlarmCurrent = value;
  else if (parameter == "dav") dischargeAlarmVoltage = value;
  else if (parameter == "das") dischargeAlarmSoc = value;
  else if (parameter == "dat") dischargeAlarmTemp = value;
  else if (parameter == "dac") dischargeAlarmCurrent = value;
  else if (parameter == "drv") dischargeReconnectVoltage = value;
  else if (parameter == "drs") dischargeReconnectSoc = value;
  else if (parameter == "drt") dischargeReconnectTemp = value;
  else if (parameter == "ddv") dischargeDisconnectVoltage = value;
  else if (parameter == "dds") dischargeDisconnectSoc = value;
  else if (parameter == "ddt") dischargeDisconnectTemp = value;
  else if (parameter == "ddc") dischargeDisconnectCurrent = value;
  else if (parameter == "cvm") calibrationVoltageMax = value;
  else if (parameter == "cvn") calibrationVoltageMin = value;
  else if (parameter == "chv") calibrationHysteresisVoltage = value;
  else if (parameter == "csm") calibrationSocMax = value;
  else if (parameter == "csn") calibrationSocMin = value;
  else if (parameter == "pc") packCapacity = value;
  else if (parameter == "ad") actualDischarge = value;
  else if (parameter == "sr") shuntResistance = value;
  else parameterUnknown = true;

  if (parameterUnknown) {
    Serial.println("Unknown parameter " + parameter + "; type ? for parameters.");
    return false;
  }

  Serial.println ("parameter " + parameter + " set to " + String(value, 6));
  mustSendConfig = true;
  return true;
}


void provideCli() {
  // Stop processing for a while (60 sec timeout) and provide a command line interface to change parameters
  // Activate: any character
  // Arduino IDE Serial Monitor "Carriage Return"
  // Putty Serial: "Local echo: Force on", "Serial Flow Control: None"

  static char message[MAX_MESSAGE_LENGTH];
  static unsigned int message_pos = 0;
  bool finished = false;
  int cliTimer = 0;

  Serial.print("SimpleBMS Serial CLI. {parameter=value | [q]uit | [v]alues} timeout 60s");
  while (!finished) {
    while (Serial.available() > 0) { 
      cliTimer = 0;
      char inByte = Serial.read();

      if ( inByte != '\r' && (message_pos < MAX_MESSAGE_LENGTH - 1) ) {
        message[message_pos] = inByte;
        message_pos++;
      }
      else {
        message[message_pos] = '\0';
        Serial.println("");
        message_pos = 0;
        finished = processMessage(message);
        if (!finished) {
          Serial.print ("> ");
        }
      }
    }
    while (Serial.available() == 0 && ! finished) { 
      cliTimer += 1;
      delay(100);
      if (cliTimer > 600) {
        return;
      }
    }
  }
}


int timestamp = millis();

void T(String comment){
  Serial.print (millis() - timestamp);
  Serial.println (" ms; now at " + comment);
  timestamp = millis();
}

void testIna228() {
  Serial.print("Discharge current: ");
  Serial.print(ina228.readCurrent());
  Serial.println(" mA");

  Serial.print("Shunt Voltage: ");
  Serial.print(ina228.readShuntVoltage());
  Serial.println(" mV");

  Serial.print("Charge: ");
  Serial.print(String(ina228.readCharge(), 8));
  Serial.println(" Ah");
}


void loop() {
  const float dampingFactor = 0.8;

  // each measurement takes 32ms:
  voltage0 = readVoltage(0);
  voltage1 = readVoltage(1);
  voltage2 = readVoltage(2);
  voltage3 = readVoltage(3);

  float cell0Voltage = voltage0;
  float cell1Voltage = voltage1 - voltage0;
  float cell2Voltage = voltage2 - voltage1;
  float cell3Voltage = voltage3 - voltage2;

  maxCellVoltage = maxVoltage(cell0Voltage, cell1Voltage, cell2Voltage, cell3Voltage);
  minCellVoltage = minVoltage(cell0Voltage, cell1Voltage, cell2Voltage, cell3Voltage);

  float packSoc = readPackSoc();
  float packTemp = readPackTemp();
  packDischargeCurrent = readPackDischargeCurrent();

  checkCalibration();

  // Charge reconnect
  if (chargeStatus == "chargeDisconnectVoltage" && maxCellVoltage < chargeReconnectVoltage ) {
    chargeStatus = "";
  }
  if (chargeStatus == "chargeDisconnectSoc" && packSoc < chargeReconnectSoc) {
    chargeStatus = "";
  }
  if (chargeStatus == "chargeDisconnectTemp" && packTemp > chargeReconnectTemp) {
    chargeStatus = "";
  }

  // Disable charge alarms
  if (chargeStatus == "chargeAlarmVoltage" && maxCellVoltage < chargeAlarmVoltage) {
    chargeStatus = "";
  }
  if (chargeStatus == "chargeAlarmSoc" && packSoc < chargeAlarmSoc) {
    chargeStatus = "";
  }
  if (chargeStatus == "chargeAlarmTemp" && packTemp > chargeAlarmTemp) {
    chargeStatus = "";
  }
  if (chargeStatus == "chargeAlarmCurrent" && packDischargeCurrent < chargeAlarmCurrent) {
    chargeStatus = "";
  }

  // Charge alarms
  if (chargeStatus == "" && maxCellVoltage > chargeAlarmVoltage) {
    chargeStatus = "chargeAlarmVoltage";
  }
  if (chargeStatus == "" && packSoc > chargeAlarmSoc) {
    chargeStatus = "chargeAlarmSoc";
  }
  if (chargeStatus == "" && packTemp < chargeAlarmTemp) {
    chargeStatus = "chargeAlarmTemp";
  }
  if (chargeStatus == "" && packDischargeCurrent > chargeAlarmCurrent) {
    chargeStatus = "chargeAlarmCurrent";
  }

  // Charge disconnect
  if (maxCellVoltage > chargeDisconnectVoltage) {
    chargeStatus = "chargeDisconnectVoltage";
    setRelais (CHARGERELAIS, DISCONNECT);
  }
  if (packSoc > chargeDisconnectSoc) {
    chargeStatus = "chargeDisconnectSoc";
    setRelais (CHARGERELAIS, DISCONNECT);
  }
  if (packTemp < chargeDisconnectTemp) {
    chargeStatus = "chargeDisconnectTemp";
    setRelais (CHARGERELAIS, DISCONNECT);
  }
  if (packDischargeCurrent > chargeDisconnectCurrent) {
    chargeStatus = "chargeDisconnectCurrent";
    setRelais (CHARGERELAIS, DISCONNECT);
  }


  // Discharge reconnect
  if (dischargeStatus == "dischargeDisconnectVoltage" && minCellVoltage > dischargeReconnectVoltage ) {
    dischargeStatus = "";
  }
  if (dischargeStatus == "dischargeDisconnectSoc" && packSoc > dischargeReconnectSoc) {
    dischargeStatus = "";
  }
  if (dischargeStatus == "dischargeDisconnectTemp" && packTemp < dischargeReconnectTemp) {
    dischargeStatus = "";
  }

  // Disable discharge  alarms
  if (dischargeStatus == "dischargeAlarmVoltage" && maxCellVoltage > dischargeAlarmVoltage) {
    dischargeStatus = "";
  }
  if (dischargeStatus == "dischargeAlarmSoc" && packSoc > dischargeAlarmSoc) {
    dischargeStatus = "";
  }
  if (dischargeStatus == "dischargeAlarmTemp" && packTemp < dischargeAlarmTemp) {
    dischargeStatus = "";
  }
  if (dischargeStatus == "dischargeAlarmCurrent" && packDischargeCurrent > - dischargeAlarmCurrent) {
    dischargeStatus = "";
  }

  // Discharge alarms
  if (dischargeStatus == "" && maxCellVoltage < dischargeAlarmVoltage) {
    dischargeStatus = "dischargeAlarmVoltage";
  }
  if (dischargeStatus == "" && packSoc < dischargeAlarmSoc) {
    dischargeStatus = "dischargeAlarmSoc";
  }
  if (dischargeStatus == "" && packTemp > dischargeAlarmTemp) {
    dischargeStatus = "dischargeAlarmTemp";
  }
  if (dischargeStatus == "" && packDischargeCurrent < - dischargeAlarmCurrent) {
    dischargeStatus = "dischargeAlarmCurrent";
  }

  // discharge disconnect
  if (minCellVoltage < dischargeDisconnectVoltage) {
    dischargeStatus = "dischargeDisconnectVoltage";
    setRelais (DISCHARGERELAIS, DISCONNECT);
  }
  if (packSoc < dischargeDisconnectSoc) {
    dischargeStatus = "dischargeDisconnectSoc";
    setRelais (DISCHARGERELAIS, DISCONNECT);
  }
  if (packTemp > dischargeDisconnectTemp) {
    dischargeStatus = "dischargeDisconnectTemp";
    setRelais (DISCHARGERELAIS, DISCONNECT);
  }
  if (packDischargeCurrent < - dischargeDisconnectCurrent) {
    dischargeStatus = "dischargeDisconnectCurrent";
    setRelais (DISCHARGERELAIS, DISCONNECT);
  }

  if (chargeStatus.substring(0,16) != "chargeDisconnect") {
    setRelais(CHARGERELAIS, CONNECT);
  }

  if (dischargeStatus.substring(0,19) != "dischargeDisconnect") {
    setRelais(DISCHARGERELAIS, CONNECT);
  }

  bmsStatus = chargeStatus + dischargeStatus;

  Serial.println ("adc[0] = " + String(cell0Voltage, 6) + "; bmsStatus = " + bmsStatus); // + "\r"

  sendCellVoltages(cell0Voltage, cell1Voltage, cell2Voltage, cell3Voltage);

  sendBmsState(packSoc, packDischargeCurrent, bmsStatus, packTemp);

  if (mustSendConfig) {
    sendBmsConfig();
    mustSendConfig = false;
  }
  if (mustTestWifi) {
    testWifi();
    mustTestWifi = false;
  }
  if (mustWakeWifi) {
    wakeWifi();
    mustWakeWifi = false;
  }

  // testIna228();

  blink();

  if (! digitalRead(13)) {
    ina228.resetAcc();
    Serial.println("Resetting accumulators");
  }

  if (Serial.available() > 0) {
    provideCli();
  }
  
  delay(1000);
}
