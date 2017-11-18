
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

// countof
template <typename T, std::size_t N>
constexpr std::size_t countof(T const (&)[N]) noexcept {
  return N;
}



// Forward declaration
bool stringToUnsignedLong(const char *psz, unsigned long *dest);
const char * statusToJSONString(const char *eventSource, unsigned long fadeTime);
bool jsonToLight(JsonObject& json, unsigned long fadeTime);
void startWifiManager(bool resetWifiSettings);
void printSystemInfo(void);
const char* getHostname(void);

// Set/Get Status
const char *funcSetStatus(const char *eventSource, JsonObject&);
const char *funcGetStatus(void);

// Set/Get Config
const char *funcSetConfig(const char *eventSource, JsonObject&);
const char *funcGetConfig(void);

// Reset Config
void funcResetConfig(void);
void funcResetConfirmTimeout(void);


typedef const char* (*H801_FunctionSet)(const char *eventSource, JsonObject& json);
typedef const char* (*H801_FunctionGet)(void);

typedef struct tagH801_Functions {
  // Led status
  H801_FunctionSet set_Status;
  H801_FunctionGet get_Status;

  // Configuration
  H801_FunctionSet set_Config;
  H801_FunctionGet get_Config;

  void (*reset_Config)(void);

} H801_Functions, *PH801_Functions;


// Struct containing function from the by the classes
H801_Functions callbackFunctions = {
  .set_Status = funcSetStatus,
  .get_Status = funcGetStatus,

  .set_Config = funcSetConfig,
  .get_Config = funcGetConfig,

  .reset_Config = funcResetConfirmTimeout,
};


#include "h801_config.h"
#include "h801_led.h"
#include "h801_mqtt.h"
#include "h801_http.h"

// Led pins
#define H801_LED_PIN_G  1
#define H801_LED_PIN_R  5

// Input pins
#define H801_GPIO_PIN0  0


// Number of steps to to fade each second
#define H801_DURATION_FADE_STEPS 10

// Global variables
static bool s_isFading = false;
static bool s_shouldSaveConfig = false;

static H801_Config s_config;
static WiFiClient s_wifiClient;
static H801_MQTT s_mqttClient(s_wifiClient, s_config, &callbackFunctions);
static H801_HTTP s_httpServer(              s_config, &callbackFunctions);

// List of all LEDs
H801_Led LedStatus[] = {
  H801_Led("R",   0, PERIPHS_IO_MUX_MTDO_U,  FUNC_GPIO15, 15),
  H801_Led("G",   1, PERIPHS_IO_MUX_MTCK_U,  FUNC_GPIO13, 13),
  H801_Led("B",   2, PERIPHS_IO_MUX_MTDI_U,  FUNC_GPIO12, 12),
  H801_Led("W1",  3, PERIPHS_IO_MUX_MTMS_U,  FUNC_GPIO14, 14),
  H801_Led("W2",  4, PERIPHS_IO_MUX_GPIO4_U, FUNC_GPIO4,   4)
};


/**
 * Setup H801 and connect to the WiFi
 */
void setup() {
  // Setup console
  Serial1.begin(115200);
  Serial1.println("--------------------------");
  Serial1.println();

  // red, green led as output
  pinMode(H801_LED_PIN_R, OUTPUT);
  pinMode(H801_LED_PIN_G, OUTPUT);

  // PWM resolution, take max value of gamma table 1000
  const uint32_t h801_pwm_period = s_gammaTable[countof(s_gammaTable) - 1]; // * 200ns ^= 200 Mhz

  // Initial pwm values
  uint32_t h801_pwm_initval[countof(LedStatus)] = {0};

  // PWM setup
  uint32_t h801_pwm_io_info[countof(LedStatus)][3] = {0};
  for (size_t i = 0; i < countof(LedStatus); i++) {
    LedStatus[i].get_IO(h801_pwm_io_info[i]);
  }

  // Init pwm and start it
  pwm_init(h801_pwm_period, h801_pwm_initval, countof(h801_pwm_io_info), h801_pwm_io_info);
  pwm_start();

  // red: off, green: on
  digitalWrite(H801_LED_PIN_R, 1);
  digitalWrite(H801_LED_PIN_G, 1);

  // GPIO0 as input
  pinMode(H801_GPIO_PIN0, INPUT);

  // Delay to allow starting serial monitor
  delay(1000);

  // Display system information
  printSystemInfo();

  // Set hostname of device
  WiFi.hostname(getHostname());

  // Load configuration
  bool resetWifiSettings = false;
  Serial1.println("Config: Loading config");

  // Try to read the configuration file 
  if (!s_config.load()) {
    Serial1.println("Config: failed to read config, settings was cleared");
    resetWifiSettings = true;
  }
  else {
    Serial1.print("Config: ");
    Serial1.println(funcGetConfig());
  }

  // Starts the wifi manager
  startWifiManager(resetWifiSettings);

  // Setup MQTT client if we have configured one
  if (*s_config.m_MQTT.server) {
    // Print MQTT
    Serial1.printf("  %15s \"%s\":%s\n", "MQTT:", s_config.m_MQTT.server, s_config.m_MQTT.port);

    // init the MQTT connection
    s_mqttClient.setup();

  }

  // Setup HTTP server
  s_httpServer.setup();

  //save the custom parameters to FS
  if (s_shouldSaveConfig) {
    s_config.save();
  }

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

    const char *jsonString = statusToJSONString("button", 0);

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
      digitalWrite(H801_LED_PIN_G, (fadingLedIndex & 0x7) != 0x7);

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
 * Start WifiManager and let it login to Wifi
 * @param resetWifiSettings Should we reset setting before starting
 */
void startWifiManager(bool resetWifiSettings) {
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
  WiFiManagerParameter customMQTTServer("mqttServer", "mqtt server",    s_config.m_MQTT.server, countof(s_config.m_MQTT.server));
  WiFiManagerParameter customMQTTPort(  "mqttPort",   "mqtt port",      s_config.m_MQTT.port,   countof(s_config.m_MQTT.port));

  WiFiManagerParameter customMQTTAliasBR("<br>");
  WiFiManagerParameter customMQTTAlias("mqttAlias",   "mqtt alias",     s_config.m_MQTT.alias, countof(s_config.m_MQTT.alias));
  
  WiFiManagerParameter customMQTTLoginBR("<br>");
  WiFiManagerParameter customMQTTLogin("mqttSLogin",  "mqtt login",     s_config.m_MQTT.login, countof(s_config.m_MQTT.login));
  WiFiManagerParameter customMQTTPassw("mqttPassw",   "mqtt password",  s_config.m_MQTT.passw, countof(s_config.m_MQTT.passw));


  //
  wifiManager.setAPCallback([](WiFiManager *myWiFiManager) {
    Serial1.println("Wifi: Entered config mode");
    Serial1.println(WiFi.softAPIP());

    Serial1.println(myWiFiManager->getConfigPortalSSID());
  });


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
  if (!wifiManager.autoConnect(getHostname())) {
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
    strlcpy(s_config.m_MQTT.server, customMQTTServer.getValue(),  countof(s_config.m_MQTT.server));
    strlcpy(s_config.m_MQTT.port,   customMQTTPort.getValue(),    countof(s_config.m_MQTT.port));
    strlcpy(s_config.m_MQTT.alias,  customMQTTAlias.getValue(),   countof(s_config.m_MQTT.alias));
    strlcpy(s_config.m_MQTT.alias,  customMQTTAlias.getValue(),   countof(s_config.m_MQTT.alias));
    strlcpy(s_config.m_MQTT.login,  customMQTTLogin.getValue(),   countof(s_config.m_MQTT.login));
    strlcpy(s_config.m_MQTT.passw,  customMQTTPassw.getValue(),   countof(s_config.m_MQTT.passw));    
  }
}

/**
 * Prints system and SPIFFS information
 */
void printSystemInfo(void) {
  Serial1.println("\nSystem: Information");
  Serial1.printf("%20s: %08X\n", "ChipId",         ESP.getChipId());
  Serial1.printf("%20s: %08X\n", "FlashChipId",    ESP.getFlashChipId());
  Serial1.printf("%20s: %u\n",   "BootMode",       ESP.getBootMode());
  Serial1.printf("%20s: %s\n",   "SdkVersion",     ESP.getSdkVersion());
  Serial1.printf("%20s: %u\n",   "BootVersion",    ESP.getBootVersion());
  Serial1.printf("%20s: %u\n",   "FlashChipSize",  ESP.getFlashChipSize());
  Serial1.printf("%20s: %u\n",   "FlashChipRealSize",      ESP.getFlashChipRealSize());
  Serial1.printf("%20s: %u\n",   "FlashChipSizeByChipId",  ESP.getFlashChipSizeByChipId());
  Serial1.printf("%20s: %u\n",   "FreeHeap",       ESP.getFreeHeap());
  Serial1.printf("%20s: \"%s\"\n", "ResetReason",  ESP.getResetReason().c_str());
  Serial1.printf("%20s: \"%s\"\n", "SketchMD5",    ESP.getSketchMD5().c_str());
  
  if (SPIFFS.begin()) {
    Serial1.println("\nSPIFFS: Information");
    FSInfo fs_info;
    if (SPIFFS.info(fs_info)) {
      Serial1.printf("%20s: %ukb (%u)\n", "Used", fs_info.usedBytes>>10, fs_info.usedBytes);
      Serial1.printf("%20s: %ukb (%u)\n", "Total", fs_info.totalBytes>>10, fs_info.totalBytes);
    }

    Dir dir = SPIFFS.openDir("");
    while (dir.next()) {
      File f = dir.openFile("r");
      if (f) {
        Serial1.printf("%20s: %ukb (%u)\n", dir.fileName().c_str(), f.size()>>10, f.size());
        f.close();
      }
    }

    SPIFFS.end();
  }
  Serial1.println("");
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
 * @param  json     JSON object
 * @param  fadeTime Time to use to reach new state
 * @return Did any LED values change
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
 * @param eventSource  Source event for status change
 * @param fadeTime     Number of ms to reach the new state
 * @return Current status as JSON string 
 */
const char * statusToJSONString(const char *eventSource, unsigned long fadeTime) {
  static char buffer[1024];

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();

  if (fadeTime)
    root["duration"] = fadeTime;

  // Convert each led state
  for (H801_Led& led : LedStatus) {
    root[led.get_ID()] = led.get_Bri();
  }

  if (eventSource && *eventSource)
    root["event"] = eventSource;

  // Serialize JSON
  root.printTo(buffer, sizeof(buffer));

  return buffer;
}


/**
 * Return device hostname
 * @return hostname
 */
const char* getHostname(void) {
  static char hostname[32];
  if (*hostname)
    return hostname;

  // Generate hostname
  snprintf(hostname, countof(hostname), "H801 %08X", ESP.getChipId());
  hostname[countof(hostname)-1] = '\0';
  return hostname;
}


/**
 * Retreives current LED status
 * @return JSON string with current status
 */
const char *funcGetStatus(void) {
  return statusToJSONString(NULL, 0);
}


/**
 * Callback used to update current LED status
 * @param  eventSource  Label of which system updated status
 * @param  json         JSON object with new status
 * @return New status as JSON stirng
 */
const char *funcSetStatus(const char* eventSource, JsonObject& json) {
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
  const char*jsonString = statusToJSONString(eventSource, fadeTime);

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
  return s_config.toJSONString(true);
}


/**
 * Set current configuration
 * @param  eventSource  Origin of update request
 * @param  json         New configuration
 * @return Current configuration
 */
const char *funcSetConfig(const char* eventSource, JsonObject& json) {
  // Print event source
  Serial1.printf("Config: Update from %s\n", eventSource);

  s_config.set(json);

  // Re-setup mqtt client with new info
  s_mqttClient.setup();

  // Return config with no password
  return s_config.toJSONString(true);
}


/**
 * Resets the configuration if two requests are made during 5s
 * After reset the unit will reboot
 */
void funcResetConfirmTimeout() {
  static unsigned long lastRequest = 0;



  // First time called, snapshot
  if (!lastRequest) {
    Serial1.println("Config: Reset timer first call");
    lastRequest = millis();
    return;
  }

  // Longer than 5s between requests
  if (millis() - lastRequest > 5000) {
    Serial1.println("Config: Reset timer updated");
    lastRequest = millis();
    return;
  }

  Serial1.println("Config: Resetting device");

  // Clear config
  s_config.remove();

  delay(3000);

  // Reboot system
  ESP.restart();

  delay(5000);
}
