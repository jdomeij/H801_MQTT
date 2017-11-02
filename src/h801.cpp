
#include <FS.h> //this needs to be first, or it all crashes and burns...

#include <string>

#include <ESP8266WiFi.h>
#include <WiFiManager.h>

#include <PubSubClient.h>

#include <ArduinoJson.h>

#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>


// Any compiler claiming C++11 supports, Visual C++ 2015
#if __cplusplus >= 201103L || _MSC_VER >= 1900 // C++ 11 implementation
  template <typename T, std::size_t N>
  constexpr std::size_t countof(T const (&)[N]) noexcept {
    return N;
  }
#else
  template <typename T, std::size_t N>
  char(&COUNTOF_REQUIRES_ARRAY_ARGUMENT(T(&)[N]))[N];
  #define countof(x) sizeof(COUNTOF_REQUIRES_ARRAY_ARGUMENT(x))
#endif



#include "h801_led.h"
#include "h801_mqtt.h"


// Define PWM pins
#define H801_PWM_PIN_R   15
#define H801_PWM_PIN_G   13
#define H801_PWM_PIN_B   12
#define H801_PWM_PIN_W1  14
#define H801_PWM_PIN_W2   4

// Led pins
#define H801_LED_PIN_G  1
#define H801_LED_PIN_R  5

// Configuration file
#define H801_CONFIG_FILE "/config.json"

// Number of steps to to fade each second
#define H801_DURATION_FADE_STEPS 10

// Forward declarations
void saveConfigCallback();

void httpHandleRoot();
void httpHandleGET();
void httpHandlePOST();

char *mqttHandleSet(char*);
char *mqttHandleGet(void);


// Global variables
bool isFading = false;
bool H801StatusOn = false;


WiFiClient wifiClient;
H801_MQTT mqttClient(wifiClient, mqttHandleSet, mqttHandleGet);
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;


// Initialization helper
#define INITIALIZE_PIN(_X_) H801_Led(#_X_, H801_PWM_PIN_ ##_X_)


H801_Led LedStatus[] = {
  INITIALIZE_PIN(R),
  INITIALIZE_PIN(G),
  INITIALIZE_PIN(B),
  INITIALIZE_PIN(W1),
  INITIALIZE_PIN(W2)
};


// H801 configuration
struct {
  uint16_t chipIDLen;
  char chipID[16];
  char hostname[16];

  bool shouldSaveConfig;
  char mqttServer[40];
  char mqttPort[6];
  uint16_t mqttPortNum;

} H801Config;


/**
 * Callback indicating that we need to save the configuration
 */
void configSaveCallback () {
  Serial1.println("Config: Should save config");
  H801Config.shouldSaveConfig = true;
}

bool stringToInt(const char *psz, int *dest) {
  int result = 0;
  bool negative = false;

  if (!psz) {
    *dest = 0;
    return false;
  }

  // Check if negative
  if (*psz == '-') {
    negative = true;
    psz++;
  }

  // Parse one char at a time
  while(*psz && *psz >= '0' && *psz <= '9') {
    result = (10 * result) + (*psz - '0');
    psz++;
  }

  // Negate the value
  if (negative)
    result *= -1;

  // Update return value
  *dest = result;

  // Did we manage to parse the whole string
  return (*psz == '\0');

}


/**
 * Setup H801 and connect to the WiFi
 */
void setup() {
  // red, green led as output
  pinMode(H801_LED_PIN_R, OUTPUT);
  pinMode(H801_LED_PIN_G, OUTPUT);

  // red: off, green: on
  digitalWrite(H801_LED_PIN_R, 0);
  digitalWrite(H801_LED_PIN_G, 1);

  // Generate hostname and chip id
  sprintf(H801Config.chipID, "%08X", ESP.getChipId());
  sprintf(H801Config.hostname, "H801 %08X", ESP.getChipId());

  H801Config.chipIDLen = strlen(H801Config.chipID);

  // Setup console
  Serial1.begin(115200);
  Serial1.println("--------------------------");
  Serial1.println();

  delay(10000);

  // Default config values
  strlcpy(H801Config.mqttServer, "", countof(H801Config.mqttServer));
  strlcpy(H801Config.mqttPort,   "1883",       countof(H801Config.mqttPort));

  Serial1.println("Config: Loading config");

  // Try to read the configuration file 
  File configFile;
  if (!SPIFFS.begin()) {
    Serial1.println("Config: failed to mount FS");
  }
  else if (!SPIFFS.exists(H801_CONFIG_FILE)) {
    Serial1.println("Config: No config file");
  }
  else if ((configFile = SPIFFS.open(H801_CONFIG_FILE, "r")) == 0) {
    Serial1.println("Config: Unable to open " H801_CONFIG_FILE);
  }
  else {
    size_t size = configFile.size();
    // Allocate a buffer to store contents of the file.
    std::unique_ptr<char[]> buf(new char[size]);

    configFile.readBytes(buf.get(), size);
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.parseObject(buf.get());

    if (json.success()) {
      json.printTo(Serial1);
      Serial1.println("\nparsed json");

      strcpy(H801Config.mqttServer, json["mqtt_server"]);
      strcpy(H801Config.mqttPort, json["mqtt_port"]);
    }
    else {
      Serial1.println("Config: Failed to load json config");
    }
  }

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter customMQTTTitle("<br><b>MQTT Config</B> (optional)");
  WiFiManagerParameter customMQTTServer("server", "mqtt server", H801Config.mqttServer, 40);
  WiFiManagerParameter customMQTTPort(  "port",   "mqtt port",   H801Config.mqttPort, 6);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(configSaveCallback);

  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  
  //add all your parameters here
  wifiManager.addParameter(&customMQTTTitle);
  wifiManager.addParameter(&customMQTTServer);
  wifiManager.addParameter(&customMQTTPort);

  //reset settings - for testing
  //wifiManager.resetSettings();

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  wifiManager.setTimeout(3600);

  Serial1.println("WiFi: Starting mangager");

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect(H801Config.hostname)) {
    Serial1.println("WiFi: failed to connect, timeout detected");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }


  //if you get here you have connected to the WiFi
  Serial1.println("\r\nWiFi: Connected");

  Serial1.printf("  %15s %s\n", "IP:",      WiFi.localIP().toString().c_str());
  Serial1.printf("  %15s %s\n", "Subnet:",  WiFi.subnetMask().toString().c_str());
  Serial1.printf("  %15s %s\n", "Gateway:", WiFi.gatewayIP().toString().c_str());
  Serial1.printf("  %15s %s\n", "MAC:",     WiFi.macAddress().c_str());
  Serial1.printf("  %15s %s\n", "SSID:",    WiFi.SSID().c_str());

  //read updated parameters
  strcpy(H801Config.mqttServer, customMQTTServer.getValue());
  strcpy(H801Config.mqttPort,   customMQTTPort.getValue());

  //save the custom parameters to FS
  if (H801Config.shouldSaveConfig) {
    Serial1.println("Config: Saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = H801Config.mqttServer;
    json["mqtt_port"]   = H801Config.mqttPort;

    File configFile = SPIFFS.open(H801_CONFIG_FILE, "w");
    if (configFile) {
      json.printTo(Serial);
      json.printTo(configFile);
      configFile.close();
    }
    else {
      Serial1.println("Config: Failed to open config file for writing");
    }
    //end save
  }


  // Setup MQTT server if we have configured one
  if (*H801Config.mqttServer != '\0') {
    // Convert mqtt_port to int, default to 1883
    if (!(*H801Config.mqttPort && 
          (H801Config.mqttPortNum = (uint16_t)strtol(H801Config.mqttPort, NULL, 10)) != 0)) {
      H801Config.mqttPortNum = 1883;
    }

    // Print MQTT
    Serial1.printf("  %15s \"%s\":%u\r\n", "MQTT:", H801Config.mqttServer, H801Config.mqttPortNum);

    // init the MQTT connection
    mqttClient.set_Config(H801Config.chipID, H801Config.mqttServer, H801Config.mqttPortNum);
  }


  MDNS.begin(H801Config.hostname);

  httpUpdater.setup(&httpServer);
  
  httpServer.on("/",           HTTP_GET, httpHandleRoot);
  httpServer.on("/index.html", HTTP_GET, httpHandleRoot);

  httpServer.on("/status", HTTP_GET,  httpHandleGET);
  httpServer.on("/status", HTTP_POST, httpHandlePOST);
  httpServer.begin();

  MDNS.addService("http", "tcp", 80);

  Serial1.println("\r\nSystem: Information");
  Serial1.printf("%20s: ", "getBootMode");
  Serial1.println(ESP.getBootMode());
  Serial1.printf("%20s: ", "getSdkVersion");
  Serial1.println(ESP.getSdkVersion());
  Serial1.printf("%20s: ", "getBootVersion");
  Serial1.println(ESP.getBootVersion());
  Serial1.printf("%20s: ", "getChipId");
  Serial1.println(ESP.getChipId());
  Serial1.printf("%20s: ", "getFlashChipSize");
  Serial1.println(ESP.getFlashChipSize());
  Serial1.printf("%20s: ", "getFlashChipRealSize");
  Serial1.println(ESP.getFlashChipRealSize());
  Serial1.printf("%20s: ", "getFlashChipSizeByChipId");
  Serial1.println(ESP.getFlashChipSizeByChipId());
  Serial1.printf("%20s: ", "getFlashChipId");
  Serial1.println(ESP.getFlashChipId());
  Serial1.printf("%20s: ", "getFreeHeap");
  Serial1.println(ESP.getFreeHeap());

  // Green light on
  digitalWrite(H801_LED_PIN_G, false);

  Serial1.println("\r\nSystem: Running");
}


/**
 * Update light values using JSON values
 * @param  json JSON object
 * @return Did any led value change
 */
bool jsonToLight(JsonObject& json) {
  int tmp;

  // Check if duration is specified
  unsigned long fadeTime = 0;
  if (json.containsKey("duration")) {
    const JsonVariant &fadeValue = json["duration"];

    // Number value
    if (fadeValue.is<long>())
      fadeTime = (unsigned long)constrain(fadeValue.as<long>(), 0, 100000000);
    //String value
    else if (fadeValue.is<char*>() && stringToInt(fadeValue.as<char*>(), &tmp))
      fadeTime = (unsigned long)constrain(tmp, 0, 100000000);
  }

  // Calculate number of steps for this duration
  uint32_t fadeSteps = 0;
  if (fadeTime > 0)
    fadeSteps = (fadeTime/H801_DURATION_FADE_STEPS);

  // Has any value changed
  bool isChanged = false;

  // Check all PWM leds
  for (uint i = 0; i < countof(LedStatus); i++) {
    String id = LedStatus[i].get_ID();
    
    // Skip led if we don't have any value
    if (!json.containsKey(id))
      continue;

    if (!LedStatus[i].set_Bri(json[id], fadeSteps))
      continue;

    // Indicate that we have changed the light
    isChanged = true;
  }

  return isChanged;
}


/**
 * Generate string with current status as JSON string
 * @return status string
 */
char * statusToJSONString() {
  static char buffer[1024];

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();

  // Global on/off state
  root["on"] = H801StatusOn;

  // Convert each led state
  for (uint i = 0; i < countof(LedStatus); i++) {
    root[LedStatus[i].get_ID()] = LedStatus[i].get_Bri();
  }

  // Serialize JSON
  root.printTo(buffer, sizeof(buffer));

  return buffer;
}


/**
 * Determine content type by file extension
 * @param  filename  File to get conent type for
 * @return Content type string
 */
String httpContentType(String filename){
       if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js"))  return "application/javascript";
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

/**
 * Stream file from SPIFFS to HTTP client
 * @param  path   File to serve
 * @return false  If unable to find file
 *         true   If able to send file to client
 */
bool handleFileRead(String path){
  Serial1.printf("handleFileRead: %s\r\n", path.c_str());
  if(path.endsWith("/")) path += "index.html";
  String contentType = httpContentType(path);
  if(SPIFFS.exists(path)){
    File file = SPIFFS.open(path, "r");
    size_t sent = httpServer.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}


/**
 * HTTP handle for root/index page
 */
void httpHandleRoot() {
  if(!handleFileRead("/index.html"))
    httpServer.send(404, "text/plain", "FileNotFound");
}


/**
 * HTTP handle for POST
 */
void httpHandlePOST() {
  StaticJsonBuffer<200> jsonBuffer;
  
  // Check if body received
  if (httpServer.hasArg("plain")== false) { 
    httpServer.send(200, "application/json", "{ \"message\": \"Body not received\"}");
    return;
  }

  //Serial1.print("POST Input: ");
  //Serial1.println(httpServer.arg("plain"));

  // Parse the json
  JsonObject& json = jsonBuffer.parseObject(httpServer.arg("plain"));

  // Failed to parse json
  if (!json.success()) {
    httpServer.send(406, "application/json", "{ \"message\": \"invalid JSON\"}");
    return;
  }

  Serial1.print("POST Parsed: ");
  json.printTo(Serial1);
  Serial1.println();

  // Update light from json string, ensure 
  if (jsonToLight(json))
    isFading = true;

  char *jsonString = statusToJSONString();
  
  // Publish changes
  mqttClient.publishConfigUpdate(jsonString);
  Serial1.println(jsonString);

  httpServer.send(200, "application/json", jsonString);

  return;
}


/**
 * HTTP handle for GET
 */
void httpHandleGET() {
  int tmp;
  bool bGotArgs = false;
  StaticJsonBuffer<200> jsonBuffer;

  JsonObject& json = jsonBuffer.createObject();

  if (httpServer.hasArg("on")) {
    bGotArgs = true;
    json["on"] = httpServer.arg("on");
  }

  if (httpServer.hasArg("duration") && stringToInt(httpServer.arg("duration").c_str(), &tmp)) {
    bGotArgs = true;
    json["duration"] = tmp;
  }

    // Convert each led state
  for (uint i = 0; i < countof(LedStatus); i++) {
    String id = LedStatus[i].get_ID();
    if (!httpServer.hasArg(id))
      continue;

    if (!stringToInt(httpServer.arg(id).c_str(), &tmp))
      continue;

    bGotArgs = true;
    json[id] = tmp;
  }

  // Did we get any arg
  if (bGotArgs) {
    Serial1.print("GET Parsed: ");
    json.printTo(Serial1);
    Serial1.println();

    // Update light from json string
    if (jsonToLight(json))
      isFading = true;
  }


  char *jsonString = statusToJSONString();

  // Only publish if changed  
  if (bGotArgs) {
    mqttClient.publishConfigUpdate(jsonString);
    Serial1.println(jsonString);
  }

  httpServer.send(200, "application/json", jsonString);

  return;
} 


/**
 * Callback used by MQTT to update current LED values
 * @param  jsonString New status as JSON string
 * @return New status as JSON stirng
 */
char *mqttHandleSet(char *jsonString) {
  StaticJsonBuffer<200> jsonBuffer;

  // Parse the json
  JsonObject& json = jsonBuffer.parseObject(jsonString);

  // Failed to parse json
  if (!json.success()) {
    return NULL;
  }

  Serial1.print("MQTT Parsed: ");
  json.printTo(Serial1);

  // Update light from json string
  if (jsonToLight(json))
    isFading = true;

  // Get current config
  jsonString = statusToJSONString();
  Serial1.println(jsonString);

  return jsonString;
}

/**
 * Retreives current status
 * @return JSON string with current status
 */
char *mqttHandleGet(void) {
  return statusToJSONString();
}


/**
 * Main loop
 */
void loop() {
  static unsigned long lastFade = 0;
  static unsigned int fadingLedIndex = 0;

  unsigned long time = millis();

  // process OTA updates
  httpServer.handleClient();
  
  // process MQTT
  mqttClient.loop(time);

  // Are we fading light
  if (isFading && (time >= lastFade + (100/H801_DURATION_FADE_STEPS) || time < lastFade)) {
    lastFade = time;
    isFading = false;
    uint i;

    // For each led
    for (i=0; i < countof(LedStatus); i++) {
      // Fade led,
      if (LedStatus[i].do_Fade())
        isFading = true;
    }

    // Blink leds during fading, ensure led is green when done
    fadingLedIndex++;
    if (isFading) {    
      digitalWrite(H801_LED_PIN_G, (fadingLedIndex&(0x7)) != 0x00);
      //digitalWrite(H801_LED_PIN_R, (fadingLedIndex&(0x04)) != );
    }
    else {
      // Inverted values
      digitalWrite(H801_LED_PIN_G, false); 
      //digitalWrite(H801_LED_PIN_R, true);
    }
  }

  delay(1);
}

