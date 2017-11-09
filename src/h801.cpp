
#include <FS.h> //this needs to be first, or it all crashes and burns...

#include <string>

#include <ESP8266WiFi.h>
#include <WiFiManager.h>

#include <PubSubClient.h>

#include <ArduinoJson.h>

#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

extern "C"{
  #include "pwm.h"
}

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

typedef const char* (*H801_FunctionSet)(JsonObject&);
typedef const char* (*H801_FunctionGet)(void);


// Forward declaration
bool stringToUnsignedLong(const char *psz, unsigned long *dest);
const char * statusToJSONString(unsigned long fadeTime);
bool jsonToLight(JsonObject& json, unsigned long fadeTime);

const char *funcSetStatus(JsonObject&);
const char *funcGetStatus(void);

const char *funcSetConfig(JsonObject&);
const char *funcGetConfig(void);
bool  funcSaveConfig(void);
bool  funcReadConfig(void);


typedef struct tagH801_Functions {
  // Led status
  H801_FunctionSet set_Status;
  H801_FunctionGet get_Status;

  // Configuration
  H801_FunctionSet set_Config;
  H801_FunctionGet get_Config;

} H801_Functions, *PH801_Functions;


// H801 configuration
typedef struct tagH801_CONFIG {
  struct {
    char hostname[16];
  } HTTP;

  struct {
    char server[128];
    char port[6];

    char alias[128];

    char login[128];
    char passw[128];
  } MQTT;

} H801_Config, *PH801_Config;


#include "h801_led.h"
#include "h801_mqtt.h"
#include "h801_http.h"


// Led pins
#define H801_LED_PIN_G  1
#define H801_LED_PIN_R  5

// Input pins
#define H801_GPIO_PIN0  0

// Configuration file
#define H801_CONFIG_FILE "/config.json"

// Number of steps to to fade each second
#define H801_DURATION_FADE_STEPS 10



// Struct containing function from the by the classes
H801_Functions callbackFunctions = {
  .set_Status = funcSetStatus,
  .get_Status = funcGetStatus,

  .set_Config = funcSetConfig,
  .get_Config = funcGetConfig
};



// Global variables
static bool s_isFading = false;
static bool s_shouldSaveConfig = false;

static WiFiClient s_wifiClient;
static H801_MQTT s_mqttClient(s_wifiClient, &callbackFunctions);
static H801_HTTP s_httpServer(              &callbackFunctions);


// Initialization helper
#define INITIALIZE_PIN(_id_, _pin_) H801_Led(#_id_, _num_)


H801_Led LedStatus[] = {
  H801_Led("R"),
  H801_Led("G"),
  H801_Led("B"),
  H801_Led("W1"),
  H801_Led("W2")
};

static H801_Config s_config = {0};


/**
 * Setup H801 and connect to the WiFi
 */
void setup() {
  // Setup console
  Serial1.begin(115200);
  Serial1.println("--------------------------");
  Serial1.println();

  // PWM resolution, take max value of gamma table 1000
  const uint32_t h801_pwm_period = s_gammaTable[countof(s_gammaTable) - 1]; // * 200ns ^= 200 Mhz

  uint32_t h801_pwm_initval[countof(LedStatus)] = {0};

  // PWM setup
  uint32 h801_pwm_io_info[countof(LedStatus)][3] = {
    // MUX, FUNC, PIN
    {PERIPHS_IO_MUX_MTDO_U,  FUNC_GPIO15, 15}, // R
    {PERIPHS_IO_MUX_MTCK_U,  FUNC_GPIO13, 13}, // G
    {PERIPHS_IO_MUX_MTDI_U,  FUNC_GPIO12, 12}, // B
    {PERIPHS_IO_MUX_MTMS_U,  FUNC_GPIO14, 14}, // W1
    {PERIPHS_IO_MUX_GPIO4_U, FUNC_GPIO4,   4}, // W2
  };

  for (size_t i = 0; i < countof(LedStatus); i++) {
    LedStatus[i].setup(h801_pwm_io_info[i][2], i);
  }

  // Init lights
  pwm_init(h801_pwm_period, h801_pwm_initval, countof(LedStatus), h801_pwm_io_info);
  pwm_start();


  // red, green led as output
  pinMode(H801_LED_PIN_R, OUTPUT);
  pinMode(H801_LED_PIN_G, OUTPUT);

  // red: off, green: on
  digitalWrite(H801_LED_PIN_R, 0);
  digitalWrite(H801_LED_PIN_G, 1);

  // GPIO0 as input
  pinMode(H801_GPIO_PIN0, INPUT);


  // Generate hostname
  snprintf(s_config.HTTP.hostname, countof(s_config.HTTP.hostname), "H801 %08X", ESP.getChipId());
  s_config.HTTP.hostname[countof(s_config.HTTP.hostname)-1] = '\0';


  bool resetWifiSettings = false;
  Serial1.println("Config: Loading config");


  if (!SPIFFS.begin()) {
    Serial1.println("Config: failed to mount FS, settings was cleared");
    resetWifiSettings = true;
  }
  // Try to read the configuration file 
  else if (!funcReadConfig()) {
    Serial1.println("Config: failed to read config, settings was cleared");
    resetWifiSettings = true;
  }
  else {
    Serial1.print("Config: ");
    Serial1.println(funcGetConfig());
  }

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  // Failed to read settings so we should reset wifi settings
  if (resetWifiSettings)
    wifiManager.resetSettings();

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter customMQTTTitle("<br><b>MQTT Config</B> (optional)");
  WiFiManagerParameter customMQTTServer("mqttServer", "mqtt server",    s_config.MQTT.server, countof(s_config.MQTT.server));
  WiFiManagerParameter customMQTTPort(  "mqttPort",   "mqtt port",      s_config.MQTT.port,   countof(s_config.MQTT.port));

  WiFiManagerParameter customMQTTAliasBR("<br>");
  WiFiManagerParameter customMQTTAlias("mqttAlias",   "mqtt alias",     s_config.MQTT.alias, countof(s_config.MQTT.alias));
  
  WiFiManagerParameter customMQTTLoginBR("<br>");
  WiFiManagerParameter customMQTTLogin("mqttSLogin",  "mqtt login",     s_config.MQTT.login, countof(s_config.MQTT.login));
  WiFiManagerParameter customMQTTPassw("mqttPassw",   "mqtt password",  s_config.MQTT.passw, countof(s_config.MQTT.passw));


  //set config save notify callback
  wifiManager.setSaveConfigCallback([]() {
    Serial1.println("Config: Should save config");
    s_shouldSaveConfig = true;
  });

  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  
  //add all your parameters here
  wifiManager.addParameter(&customMQTTTitle);
  wifiManager.addParameter(&customMQTTServer);
  wifiManager.addParameter(&customMQTTPort);
  
  wifiManager.addParameter(&customMQTTAliasBR);
  wifiManager.addParameter(&customMQTTAlias);

  wifiManager.addParameter(&customMQTTLoginBR);
  wifiManager.addParameter(&customMQTTLogin);
  wifiManager.addParameter(&customMQTTPassw);

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
  if (!wifiManager.autoConnect(s_config.HTTP.hostname)) {
    Serial1.println("WiFi: failed to connect, timeout detected");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }


  //if you get here you have connected to the WiFi
  Serial1.println("\nWiFi: Connected");

  Serial1.printf("  %15s %s\n", "IP:",      WiFi.localIP().toString().c_str());
  Serial1.printf("  %15s %s\n", "Subnet:",  WiFi.subnetMask().toString().c_str());
  Serial1.printf("  %15s %s\n", "Gateway:", WiFi.gatewayIP().toString().c_str());
  Serial1.printf("  %15s %s\n", "MAC:",     WiFi.macAddress().c_str());
  Serial1.printf("  %15s %s\n", "SSID:",    WiFi.SSID().c_str());

  if (s_shouldSaveConfig) {
    //read updated parameters
    strlcpy(s_config.MQTT.server, customMQTTServer.getValue(),  countof(s_config.MQTT.server));
    strlcpy(s_config.MQTT.port,   customMQTTPort.getValue(),    countof(s_config.MQTT.port));
    strlcpy(s_config.MQTT.alias,  customMQTTAlias.getValue(),   countof(s_config.MQTT.alias));
    strlcpy(s_config.MQTT.alias,  customMQTTAlias.getValue(),   countof(s_config.MQTT.alias));
    strlcpy(s_config.MQTT.login,  customMQTTLogin.getValue(),   countof(s_config.MQTT.login));
    strlcpy(s_config.MQTT.passw,  customMQTTPassw.getValue(),   countof(s_config.MQTT.passw));    
  }


  // Setup MQTT client if we have configured one
  if (*s_config.MQTT.server) {
    // Print MQTT
    Serial1.printf("  %15s \"%s\":%s\n", "MQTT:", s_config.MQTT.server, s_config.MQTT.port);

    // init the MQTT connection
    if (!s_mqttClient.setup(&s_config)) {
      Serial1.println("MQTT: Failed to connect to server");
    }

  }

  // Setup HTTP server
  s_httpServer.setup(&s_config);

  //save the custom parameters to FS
  if (s_shouldSaveConfig) {
    funcSaveConfig();
  }

  Serial1.println("\nSystem: Information");
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

  Serial1.println("\nSystem: Running");
}


/**
 * Main loop
 */
void loop() {
  static unsigned long lastTime = 0;
  static unsigned long lastFade = 0;
  static unsigned int fadingLedIndex = 0;

  // De-bounce the button on startup
  static bool buttonInit = false;

  // What direction are we fading with the button
  static bool buttonFadeDirUp = false;

  // Gpio 0 counter
  static unsigned long gpioCount = 0;

  //
  unsigned long time = millis();

  // rate limit to once each ms
  if (lastTime == time) {
    return;
  }
  lastTime = time;

  // process HTTP
  s_httpServer.loop();
  
  // process MQTT
  s_mqttClient.loop(time);

  // Check if GPIO i pressed
  if (!digitalRead(H801_GPIO_PIN0)) {
    gpioCount++;
  }

  // de-bounce
  else if (gpioCount < 100) {
    // Clear counter
    gpioCount = 0;
    buttonInit = true;
  }

  // Button click
  else if (gpioCount < 700) {
    // Clear counter
    gpioCount = 0;
    s_mqttClient.publishButtonPress();
  }

  // Button fading done, publish new value
  else {
    // Clear counter
    gpioCount = 0;

    const char *jsonString = statusToJSONString(0);

    Serial1.print("State: ");
    Serial1.println(jsonString);

    // Publish change  
    s_mqttClient.publishConfigUpdate(jsonString);

    // Green light on
    digitalWrite(H801_LED_PIN_G, false);
  }



  // Button pressed for 500ms
  if (gpioCount >= 700 && buttonInit) {
    // First time we enter
    if (gpioCount == 700) {
      buttonFadeDirUp = !buttonFadeDirUp;
      Serial1.printf("Button pressed: fading %s\n", buttonFadeDirUp? "up":"down");
    }

    if (time >= lastFade + (100/H801_DURATION_FADE_STEPS) || time < lastFade) {
      lastFade = time;
      fadingLedIndex++;
      digitalWrite(H801_LED_PIN_G, (fadingLedIndex & 0x3) != 0x00);

      // For each led
      for (H801_Led& led : LedStatus) {
        led.do_ButtonFade(buttonFadeDirUp);
      }
      pwm_start();
    }
    // Ensure that gpioCount don't overflow
    gpioCount = 0x0FFFFFFF;
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
      pwm_start();    
      digitalWrite(H801_LED_PIN_G, (fadingLedIndex & 0x7) != 0x00);
      //digitalWrite(H801_LED_PIN_R, (fadingLedIndex&(0x04)) != );
    }
    else {
      // Inverted values
      digitalWrite(H801_LED_PIN_G, false); 
      //digitalWrite(H801_LED_PIN_R, true);
    }
  }
}


/**
 * Convert string to unsigned long
 * @param psz Input string
 * @param dest Output value
 * @return false if failed to convert string
 */
bool stringToUnsignedLong(const char *psz, unsigned long *dest) {
  int result = 0;

  if (!psz) {
    *dest = 0;
    return false;
  }

  // Parse one char at a time
  while(*psz && *psz >= '0' && *psz <= '9') {
    result = (10 * result) + (*psz - '0');
    psz++;
  }

  // Update return value
  *dest = result;

  // Did we manage to parse the whole string
  return (*psz == '\0');

}


/**
 * Update light values using JSON values
 * @param  json JSON object
 * @return Did any led value change
 */
bool jsonToLight(JsonObject& json, unsigned long fadeTime) {
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
const char * statusToJSONString(unsigned long fadeTime) {
  static char buffer[1024];

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();

  if (fadeTime)
    root["duration"] = fadeTime;

  // Convert each led state
  for (H801_Led& led : LedStatus) {
    root[led.get_ID()] = led.get_Bri();
  }

  // Serialize JSON
  root.printTo(buffer, sizeof(buffer));

  return buffer;
}


/**
 * Retreives current LED status
 * @return JSON string with current status
 */
const char *funcGetStatus(void) {
  return statusToJSONString(0);
}


/**
 * Callback used to update current LED status
 * @param  json   JSON object with new status
 * @return        New status as JSON stirng
 */
const char *funcSetStatus(JsonObject& json) {
  // Check for bad JSON
  if (!json.success()) {
    return NULL;
  }

  Serial1.print("Input JSON: ");
  json.printTo(Serial1);
  Serial1.println("");


  // Check if duration is specified
  unsigned long fadeTime = 0;
  if (json.containsKey("duration")) {
    const JsonVariant &fadeValue = json["duration"];

    // Number value
    if (fadeValue.is<long>())
      fadeTime = (unsigned long)constrain(fadeValue.as<long>(), 0, 100000000);
    //String value
    else if (fadeValue.is<char*>() && stringToUnsignedLong(fadeValue.as<char*>(), &fadeTime))
      fadeTime = (unsigned long)constrain(fadeTime, 0, 100000000);
    else
      fadeTime = 0;
  }

  // Update light from json string
  if (jsonToLight(json, fadeTime)) {
    s_isFading = true;
    pwm_start();
  }

  // Get current config
  const char*jsonString = statusToJSONString(fadeTime);

  Serial1.print("State: ");
  Serial1.println(jsonString);

  // If changed, publish state
  if (s_isFading) {
    s_mqttClient.publishConfigUpdate(jsonString);
  }

  return jsonString;
}


/**
 * Retreives the current configuration
 * @return Current configuration
 */
const char *funcGetConfig() {
  static char buffer[1024];

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();

  root["mqtt_server"] = s_config.MQTT.server;
  root["mqtt_port"]   = s_config.MQTT.port;

  root["mqtt_alias"]  = s_config.MQTT.alias;

  root["mqtt_login"]  = s_config.MQTT.login;
  root["mqtt_passw"]  = s_config.MQTT.passw;


  // Serialize JSON
  root.printTo(buffer, sizeof(buffer));

  return buffer;
}

/**
 * TODO: Set current configuration
 * @param  json New configuration
 * @return Current configuration
 */
const char *funcSetConfig(JsonObject& json) {
  return funcGetConfig();
}


/**
 * Write current configuration to SPIFFS
 */
bool funcSaveConfig(void) {
  const char *config = funcGetConfig();

  Serial1.println("Config: Saving config");

  File configFile = SPIFFS.open(H801_CONFIG_FILE, "w");
  
  if (!configFile) {
    Serial1.println("Config: Failed to open config file for writing");
    configFile.close();
    return false;
  }

  // Print config
  Serial1.print("Config Save: ");
  Serial1.println(config);

  if (!configFile.print(config)) {
    Serial1.println("Config: Failed to write config file");
    configFile.close();
    return false;
  }

  configFile.close();
  return true;
}

/**
 * Copies single json property to string destination
 * @param dest     Destination
 * @param destSize Size
 * @param json     JSON value
 */
static void funcReadConfigHelper(char *dest, size_t destSize, const JsonVariant &json) {
  *dest = '\0';
  if (!json.success())
    return;

  if (!json.is<char *>())
    return;

  const char *tmp = json.as<const char *>();
  if (!tmp)
    return;

  strlcpy(dest, tmp, destSize);
}

/**
 * Read current configuration from SPIFFS
 * @return false if failed to read config
 */
bool funcReadConfig() {
  File configFile;

  if (!SPIFFS.exists(H801_CONFIG_FILE)) {
    Serial1.println("Config: No config file");
    return false;
  }
  if ((configFile = SPIFFS.open(H801_CONFIG_FILE, "r")) == 0) {
    Serial1.println("Config: Unable to open " H801_CONFIG_FILE);
    return false;
  }

  size_t size = configFile.size();
  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  configFile.readBytes(buf.get(), size);
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if (!json.success()) {
    Serial1.println("Config: Failed to parse json config");
    Serial1.println(buf.get());
    configFile.close();
    return false;
  }

  Serial1.print("Config: Input JSON ");
  json.printTo(Serial1);
  Serial1.println("");

  funcReadConfigHelper(s_config.MQTT.server,  countof(s_config.MQTT.server),  json["mqtt_server"]);
  funcReadConfigHelper(s_config.MQTT.port,    countof(s_config.MQTT.port),    json["mqtt_port"]);
  funcReadConfigHelper(s_config.MQTT.alias,   countof(s_config.MQTT.alias),   json["mqtt_alias"]);
  funcReadConfigHelper(s_config.MQTT.login,   countof(s_config.MQTT.login),   json["mqtt_login"]);
  funcReadConfigHelper(s_config.MQTT.passw,   countof(s_config.MQTT.passw),   json["mqtt_passw"]);

  configFile.close();

  return true;
}

