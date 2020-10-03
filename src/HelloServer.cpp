#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <stdio.h>
#include <FS.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include "ringstream.h"
#include "wifipw.h" //Contains my password and ssid so is not on the repository :)
#include <time.h>
#include "AM2320.h"
#include "Wire.h"
#include "PID_v1.h"
#define BUFFERSIZE 0xFFF
#define TZ "CET+1CEST,M3.5.0,M10.5.0/3"
#define PWMMAX 1024
//const char* ssid = "";
//const char* password = "";
//const char* otapw = "";

/*
We want a web interface that:
-Controls lighting hours
-For which we need to control 2 GPIO pins
-maybe have a temperature/humidity sensor, which needs another 2 pins, SCL and SDA (I2C)

-maybe time frames as different configurations and calendar settings, configuring periods for which to use which configurations of hours

-Write timing stuff to memory. So parsing wil be necessary.


-reconfigure TX and RX to GPIO pins
-4bit  2d array, hour x minute -  


-Adjust the website obv
-rest function for updating time frames




TODO:
-clean up:
  -serial to stream2
  -remove unncessary prints

*/
AM2320 sensor;
ESP8266WebServer server(80);
LoopbackStream stream2(BUFFERSIZE); //Serial incoming + prints
const int FANPIN= 13;
const int PWMPIN = 12;
time_t lastTimestamp=0;
double pwmstatus = 0;
int pwmfreq =1000;
bool fanStatus = false;
double temptar = 60;
const String configFileName = "/config.ini";
bool enableHeating = false;
float lastHum = -1;
double lastTemp = -1;

//Define Variables we'll be connecting to
double Setpoint, Input, Output;

//Specify the links and initial tuning parameters
double Kp=2, Ki=5, Kd=1;
PID myPID(&lastTemp, &pwmstatus, &temptar, Kp, Ki, Kd, DIRECT);

void OTASetup();

template<typename ... Args>
String string_format( const String& format, Args ... args )
{
    size_t size = snprintf( nullptr, 0, format.c_str(), args ... ) + 1; // Extra space for '\0'
    char* buf =  new char[ size ] ; 
    snprintf( buf, size, format.c_str(), args ... );
    String out = String( buf );
    free(buf);
    return out; // We don't want the '\0' inside
}

time_t getLocalTime()
{
  auto newstamp = (time_t)time(nullptr);
  if(newstamp - lastTimestamp > 30*60)
  {
      configTime(2*3600, 0, "0.de.pool.ntp.org", "1.de.pool.ntp.org");
  }
  lastTimestamp=  newstamp;
  return lastTimestamp;// - 6*3600;
}

tm getLocalTM()
{
  time_t now = getLocalTime();
  tm split = *localtime(&now);
  split.tm_mon+=1;

  split.tm_year+=1900;
  return split;
}

String getFormattedLocalTime()
{
  tm split = getLocalTM();
  return String(string_format("%d-%02d-%02d %02d:%02d:%02d", split.tm_year, split.tm_mon, split.tm_mday, split.tm_hour, split.tm_min, split.tm_sec));
}

void startMessageLine()
{
 stream2.print(getFormattedLocalTime()+": ");
}
template <class T>
void StatusPrintln(T&& line)
{
  startMessageLine();
  stream2.println(line);
}

String readFile(String path) { // send the right file to the client (if it exists)
  startMessageLine();
  stream2.print("handleFileRead: " + path);
  //if (path.endsWith("/")) path += "index.html";         // If a folder is requested, send the index file
  String contentType = "text/html";            // Get the MIME type
  if (SPIFFS.exists(path)) {                            // If the file exists
    File file = SPIFFS.open(path, "r");                 // Open it
    auto text = file.readString();
    file.close();
    //size_t sent = server.streamFile(file, contentType); // And send it to the client
    //file.close();                                       // Then close the file again
    return text;
  }
  stream2.println("\tFile Not Found");
  return "";                                         // If the file doesn't exist, return false
}

String getContentType(String filename){
  if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path){  // send the right file to the client (if it exists)
  //startMessageLine();
  // server.client().setNoDelay(1);

  stream2.print("handleFileRead: " + path);
  if(path.endsWith("/")) path += "status.html";           // If a folder is requested, send the index file
  String contentType = getContentType(path);             // Get the MIME type
  if(SPIFFS.exists(path)){  // If the file exists, either as a compressed archive, or normal                                       // Use the compressed version
    File file = SPIFFS.open(path, "r"); 
    auto s = file.size();
    stream2.print("file exists....\n")  ;             // Open the file
    server.send_P(200, contentType.c_str(), file.readString().c_str(), s);
    //size_t sent = server.streamFile(file, contentType);    // Send it to the client
    //Serial.print(string_format("wrote %d bytes", sent));
    file.close();                                          // Close the file again
    stream2.println(String("\tSent file: ") + path);
    return true;
  }
  stream2.println(String("\tFile Not Found: ") + path);
  return false;                                          // If the file doesn't exist, return false
}

bool writeFile(String text, String path)
{
  StatusPrintln("Trying to write file...");
  int bufsize= text.length()*sizeof(char);
  StatusPrintln(string_format("file size is: %i", bufsize)); 
  FSInfo info;
  SPIFFS.info(info);

  unsigned int freebytes = info.totalBytes-info.usedBytes;
  StatusPrintln(string_format("free bytes on flash: %i", freebytes)); 
  return true;
}

/*
target temp and pwm freq setting

*/
void saveConfig()
{
  File f = SPIFFS.open(configFileName, "w");
  if(!f)
  {
    stream2.println("Failed to open file while saving configuration!");
  }
  String thing = string_format("%f:%d\n%d\n", pwmfreq, temptar, enableHeating);
  auto written = f.printf(thing.c_str());
  stream2.println(thing);
  stream2.printf("Wrote %d bytes | Error: %d\n", written,f.getWriteError());
  
  f.flush();
  f.close();
  
}

void readConfig()
{
  File f = SPIFFS.open(configFileName, "r");
  
  if(!f)
  {
    startMessageLine();
    stream2.println("Failed to open file while reading configuration! IMPORTANT. CHECK LIGHTS AND CONFIG PLEASE");
    return;
  }
  String curline = f.readStringUntil('\n');
  stream2.println(curline);
  if(curline.length()>0)
  {
    auto split = curline.indexOf(':');
    String s1 = curline.substring(0, split);
    String s2 = curline.substring(split+1);
    stream2.printf("Loaded settings: %s:%s\n", s1.c_str(), s2.c_str());
    pwmfreq = s1.toInt();
    temptar = s2.toFloat();
  }
  curline = f.readStringUntil('\n');
  stream2.println(curline);
  if(curline.length()>0)
  {
    enableHeating = curline.toInt();
  }
}

String getFlashData()
{
  uint32_t realSize = ESP.getFlashChipRealSize();
  uint32_t ideSize = ESP.getFlashChipSize();
  FlashMode_t ideMode = ESP.getFlashChipMode();
  String mess = "";
  mess+=string_format("Flash real id:   %08X\n", ESP.getFlashChipId());
  mess+=string_format("Flash real size: %u bytes\n\n", realSize);

  mess+=string_format("Flash ide  size: %u bytes\n", ideSize);
  mess+=string_format("Flash ide speed: %u Hz\n", ESP.getFlashChipSpeed());
  mess+=string_format("Flash ide mode:  %s\n", (ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT" : ideMode == FM_DIO ? "DIO" : ideMode == FM_DOUT ? "DOUT" : "UNKNOWN"));
  if (ideSize != realSize) {
    mess+=string_format("Flash Chip configuration wrong!\n");
  } else {
    mess+=string_format("Flash Chip configuration ok.\n");
  }
  FSInfo info;
  SPIFFS.info(info);
  mess+=string_format("total bytes on flash: %i\n", info.totalBytes);
  mess+=string_format("used bytes on flash: %i\n", info.usedBytes);
  return mess;
}

void handleFlash()
{
  String mess =getFlashData();
  server.send(200, "text/plain", mess);
}

void handleNotFound(){
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void handleStatusData()
{
  StaticJsonDocument<0x5FF> jsonBuffer; //TODO SOMETHING + STATUS BUFFER SIZE
  char JSONmessageBuffer[0x1FF];
  jsonBuffer["tval"] = lastTemp;
  jsonBuffer["tartval"] = temptar;
  jsonBuffer["hval"] = lastHum;
  jsonBuffer["pwmval"] = (pwmstatus/PWMMAX)*100;
  jsonBuffer["fan"] = fanStatus;
  jsonBuffer["time"] = getFormattedLocalTime();
  jsonBuffer["status"] = stream2.available()?stream2.readString():""; //TODO move time thing to read buffers part on condition of encountering a \n :)
  serializeJsonPretty(jsonBuffer,JSONmessageBuffer);
  server.send(200, "application/json", JSONmessageBuffer);
}

// void handleUpdateTarget()
// {
//   int c = server.args();
//   for(int i =0; i<c;++i)
//   {
//     StatusPrintln(string_format("Arg %i: %s", i, server.arg(i).c_str())); 
//   }
//   StatusPrintln("Target update successful."); 
//   stream.print("$");stream.print((char)5); stream.print((char)1);stream.println("~");
//   //stream.println(string_format("$%c%c&%c~", (char)START_FLASH_LED, (char)1, char(95)));
//   blinking = ! blinking;
//   server.send(202);
// }

void handleSerialInput()
{
  bool nextcharisnumber=false;
  if(server.args() > 0&& server.hasArg("input"))
  {
    StatusPrintln("Got command! : "+ server.arg("input"));

    char da = server.arg("input")[0];
    char rw = server.arg("input")[1];
    auto split = server.arg("input").indexOf('-');
    if(rw=='r')
    {
      int pinnr = server.arg("input").substring(2).toInt();
      stream2.printf("Reading pin %d in %s: %d", pinnr, da=='d'?"digital":"analog",  da=='d'?digitalRead(pinnr):analogRead(pinnr));
    }
    else
    {
      int pinnr = server.arg("input").substring(2, split).toInt();
      int val = server.arg("input").substring(split+1).toInt();
      stream2.printf("Writing pin %d in %s: %d", pinnr, da=='d'?"digital":"analog",  val);

      if(da=='d')
      {
        digitalWrite(pinnr, val==0?LOW:HIGH);
      }
      else if(da=='a')
      {
        analogWrite(pinnr, val);
      }
    }
    //stream.println("");
  }
  server.send(200);
}

void handleSetTemp()
{
  if(server.args() > 0&& server.hasArg("temp") && server.hasArg("pwmfr"))
  {
    float newtemp = server.arg("temp").toFloat();
    int newFr = server.arg("pwmfr").toInt();
    if(newFr!=pwmfreq && newFr!=0)
    {
      analogWriteFreq(newFr);
      pwmfreq=newFr;
    }
    temptar = newtemp;
    stream2.printf("setting new target temp: %f : %d", temptar, pwmfreq);
  }
  server.send(200);
}

void handleSetHeat()
{
  if(server.args() > 0&& server.hasArg("heat"))
  {
    enableHeating = server.arg("heat").toInt();
  }
  server.send(200);
}

void handleSetFan()
{
  fanStatus = !fanStatus;
  digitalWrite(FANPIN, fanStatus?HIGH:LOW);
  stream2.printf("Set fan to: %d\n", fanStatus);
  server.send(200);
}

void addRESTSources()
{
  server.on("/get/status", HTTP_GET, handleStatusData);
  server.on("/update/serialinput", HTTP_PUT, handleSerialInput);
  server.on("/set/temp", HTTP_PUT, handleSetTemp);
  server.on("/set/heat", HTTP_PUT, handleSetHeat);  
  server.on("/set/fan", HTTP_PUT, handleSetFan);  
}





void setup(void){
  stream2.clear();

  pinMode(PWMPIN, OUTPUT);
  pinMode(FANPIN, OUTPUT);
  //AM2320 stuff
  Wire.begin();
  sensor.begin();

  Serial.begin(9600);
  Serial.write("TEST!");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  stream2.println("");

  startMessageLine();
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    stream2.print(".");
  }
  stream2.println("");
  startMessageLine();
  stream2.print("Connected to ");
  stream2.println(ssid); 
  startMessageLine();
  stream2.print("IP address: ");
  stream2.println(WiFi.localIP()); 

  if (MDNS.begin("shrubs")) {
    StatusPrintln("MDNS responder started"); 
  }

  server.on("/flash", HTTP_GET, handleFlash);

  addRESTSources();
  // server.on("/inline", [](){
  //   server.send(200, "text/plain", "this works as well");
  // });

  server.onNotFound([]() {                              // If the client requests any URI
      if (!handleFileRead(server.uri()))                  // send it if it exists
        handleNotFound(); // otherwise, respond with a 404 (Not Found) error
    });


  OTASetup();
  auto res = SPIFFS.begin();
  stream2.println(res?"SPIFFS STARTED": "SHITS FUCKED");
  StatusPrintln("HTTP server started");
  startMessageLine();
  stream2.print("free heap=");
  stream2.println(ESP.getFreeHeap());
  startMessageLine();
  stream2.print("free sketch space=");
  stream2.println(ESP.getFreeSketchSpace());
  configTime(2*3600, 0, "0.de.pool.ntp.org", "1.de.pool.ntp.org");
  StatusPrintln("Waiting for time");
  startMessageLine();
  while (!time(nullptr)) {
    stream2.print(".");
    delay(1000);
  }
  startMessageLine();
  StatusPrintln("Reading config data...");
  readConfig();
  server.begin();
  stream2.println("Success.");
  myPID.SetOutputLimits(0, 1024);
  myPID.SetMode(AUTOMATIC);
  //server.client().setDefaultNoDelay(1);
}

void updateExternals()
{
  if(enableHeating)
  {
    analogWrite(PWMPIN, pwmstatus);
  }
  else
  {
    analogWrite(PWMPIN, 0);
  }
}


void loop(void){
  //bufferSerial();

  ArduinoOTA.handle();
  MDNS.update();
// if (delayRunning && ((millis() - delayStart) >= 2000)) {
//      stream2.print(stream2.available());
//   stream2.print(" | ");
//   stream2.println(stream2.pos);
//   delayStart = millis();
//   }
    static int last_result_time = 0;
    if(millis() - last_result_time > 100)
    {
      sensor.measure();
      lastHum= sensor.getHumidity();
      lastTemp = sensor.getTemperature();
      Serial.write(string_format("%f\n", pwmstatus).c_str());
      //startMessageLine();
        // stream2.println(string_format("NTP UPdate: %i",ntpClient.);

      //updateExternals();
      //StatusPrintln(string_format("Time:"));
      last_result_time = millis();
    }
    myPID.Compute();
    updateExternals();
    delay(100);
    server.handleClient();
  //console_send();
  }

void OTASetup()
{
  ArduinoOTA.setHostname("root");
  ArduinoOTA.setPassword(otapw);
  ArduinoOTA.setPort(8266);
  ArduinoOTA.onStart([]() {
    stream2.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    stream2.println("\nEnd");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    stream2.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) stream2.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) stream2.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) stream2.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) stream2.println("Receive Failed");
    else if (error == OTA_END_ERROR) stream2.println("End Failed");
  });
  ArduinoOTA.begin();
  stream2.println("OTA ready"); 

}