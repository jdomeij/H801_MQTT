
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

typedef const char* (*H801_CallbackSetConfig)(JsonObject&);
typedef const char* (*H801_CallbackGetConfig)(void);

#include "h801_led.h"
#include "h801_mqtt.h"
#include "h801_http.h"


// Define PWM pins
#define H801_PWM_PIN_R   15
#define H801_PWM_PIN_G   13
#define H801_PWM_PIN_B   12
#define H801_PWM_PIN_W1  14
#define H801_PWM_PIN_W2   4

// Led pins
#define H801_LED_PIN_G  1
#define H801_LED_PIN_R  5

// Input pins
#define H801_GPIO_PIN0  0

// Configuration file
#define H801_CONFIG_FILE "/config.json"

// Number of steps to to fade each second
#define H801_DURATION_FADE_STEPS 10


const char *callbackHandleSet(JsonObject&);
const char *callbackHandleGet(void);


// Global variables
static bool s_isFading = false;

static WiFiClient s_wifiClient;
static H801_MQTT s_mqttClient(s_wifiClient, callbackHandleSet, callbackHandleGet);
static H801_HTTP s_httpServer(              callbackHandleSet, callbackHandleGet);


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
 * Convert string to integer
 * @param psz Input string
 * @param dest Output value
 * @return false if failed to convert string
 */
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

  // GPIO0 as input
  pinMode(H801_GPIO_PIN0, INPUT);


  // Generate hostname and chip id
  sprintf(H801Config.chipID, "%08X", ESP.getChipId());
  sprintf(H801Config.hostname, "H801 %08X", ESP.getChipId());

  H801Config.chipIDLen = strlen(H801Config.chipID);

  // Setup console
  Serial1.begin(115200);
  Serial1.println("--------------------------");
  Serial1.println();

  // Default config values
  strlcpy(H801Config.mqttServer,     "", countof(H801Config.mqttServer));
  strlcpy(H801Config.mqttPort,   "1883", countof(H801Config.mqttPort));

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
      Serial1.print("Config: ");
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
  wifiManager.setSaveConfigCallback([]() {
    Serial1.println("Config: Should save config");
    H801Config.shouldSaveConfig = true;
  });

  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  
  //add all your parameters here
  wifiManager.addParameter(&customMQTTTitle);
  wifiManager.addParameter(&customMQTTServer);
  wifiManager.addParameter(&customMQTTPort);

  //reset settings - for testing
  wifiManager.resetSettings();

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
      json.printTo(configFile);

      // Print config
      Serial1.print("Config Save: ");
      json.printTo(Serial);
      Serial1.println("");

      configFile.close();
    }
    else {
      Serial1.println("Config: Failed to open config file for writing");
    }
    //end save
  }

  // Setup MQTT client if we have configured one
  if (*H801Config.mqttServer != '\0') {
    // Convert mqtt_port to int, default to 1883
    if (!(*H801Config.mqttPort && 
          (H801Config.mqttPortNum = (uint16_t)strtol(H801Config.mqttPort, NULL, 10)) != 0)) {
      H801Config.mqttPortNum = 1883;
    }

    // Print MQTT
    Serial1.printf("  %15s \"%s\":%u\r\n", "MQTT:", H801Config.mqttServer, H801Config.mqttPortNum);

    // init the MQTT connection
    s_mqttClient.setup(H801Config.chipID, H801Config.mqttServer, H801Config.mqttPortNum);
  }

  // Setup HTTP server
  s_httpServer.setup(H801Config.hostname);


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
  for (H801_Led& led : LedStatus) {
    String &id = led.get_ID();
    
    // Skip led if we don't have any value
    if (!json.containsKey(id))
      continue;

    if (!led.set_Bri(json[id], fadeSteps))
      continue;

    // Indicate that we have changed the light
    isChanged = true;
  }

  return isChanged;
}


/**
 * Generate string with current status as JSON string
 * @return Current status as JSON string 
 */
const char * statusToJSONString() {
  static char buffer[1024];

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();

  // Convert each led state
  for (H801_Led& led : LedStatus) {
    root[led.get_ID()] = led.get_Bri();
  }

  // Serialize JSON
  root.printTo(buffer, sizeof(buffer));

  return buffer;
}


/**
 * Callback used to update current LED values
 * @param  json   JSON object with new status
 * @return        New status as JSON stirng
 */
const char *callbackHandleSet(JsonObject& json) {
  // Check for bad JSON
  if (!json.success()) {
    return NULL;
  }

  Serial1.print("Input JSON: ");
  json.printTo(Serial1);
  Serial1.println("");

  // Update light from json string
  if (jsonToLight(json)) {
    s_isFading = true;
  }

  // Get current config
  const char*jsonString = statusToJSONString();

  Serial1.print("State: ");
  Serial1.println(jsonString);

  // If changed, publish state
  if (s_isFading) {
    s_mqttClient.publishConfigUpdate(jsonString);
  }

  return jsonString;
}

/**
 * Retreives current status
 * @return JSON string with current status
 */
const char *callbackHandleGet(void) {
  return statusToJSONString();
}


/**
 * Main loop
 */
void loop() {
  static unsigned long lastFade = 0;
  static unsigned int fadingLedIndex = 0;

  // De-bounce the button on startup
  static bool buttonInit = false;

  // What direction are we fading with the button
  static bool buttonFadeDirUp = false;

  unsigned long time = millis();

  // process HTTP
  s_httpServer.loop();
  
  // process MQTT
  s_mqttClient.loop(time);

  // Check if GPIO i pressed for 500ms
  static unsigned long gpioCount = 0;
  if (!digitalRead(H801_GPIO_PIN0)) {
    gpioCount++;
  }

  // Need to broadcast change
  else if (gpioCount > 500) {
    const char *jsonString = statusToJSONString();

    Serial1.print("State: ");
    Serial1.println(jsonString);

    // Publish change  
    s_mqttClient.publishConfigUpdate(jsonString);

    gpioCount = 0;

    // Green light on
    digitalWrite(H801_LED_PIN_G, false);
  }

  // Reset counter
  else {
    gpioCount = 0;
    buttonInit = true;
  }

  // Button pressed for 500ms
  if (gpioCount >= 500 && buttonInit) {
    // First time we enter
    if (gpioCount == 500) {
      buttonFadeDirUp = !buttonFadeDirUp;
      Serial1.printf("Button pressed: fading %s\r\n", buttonFadeDirUp? "up":"down");
    }

    if ((gpioCount & 0x7) == 0) {
      digitalWrite(H801_LED_PIN_G, (gpioCount & 0xff) != 0x00);

      // For each led
      for (H801_Led& led : LedStatus) {
        led.do_ButtonFade(buttonFadeDirUp);
      }
    }
    // Ensure that gpioCount don't overflow
    gpioCount &= 0x0FFFFFFF;
  }

  // Are we fading light
  else if (s_isFading && (time >= lastFade + (100/H801_DURATION_FADE_STEPS) || time < lastFade)) {
    lastFade = time;
    s_isFading = false;

    // Fade each light
    for (H801_Led& led : LedStatus) {
      if (led.do_Fade())
        s_isFading = true;
    }

    // Blink leds during fading, ensure led is green when done
    fadingLedIndex++;
    if (s_isFading) {    
      digitalWrite(H801_LED_PIN_G, (fadingLedIndex & 0x7) != 0x00);
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

