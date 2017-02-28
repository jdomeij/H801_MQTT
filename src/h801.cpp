
#include <FS.h> //this needs to be first, or it all crashes and burns...

#include <string>

#include <ESP8266WiFi.h>
#include <WiFiManager.h>

#include <PubSubClient.h>

#include <ArduinoJson.h>

#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

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

// Led gamma table
// https://learn.adafruit.com/led-tricks-gamma-correction/the-longer-fix
const uint16_t gammaTable[] = {
     0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
     0,    1,    1,    1,    1,    1,    1,    1,    1,    2,    2,    2,    2,    2,    3,    3,
     3,    3,    4,    4,    4,    5,    5,    5,    6,    6,    7,    7,    7,    8,    8,    9,
    10,   10,   11,   11,   12,   13,   13,   14,   15,   15,   16,   17,   18,   19,   20,   20,
    21,   22,   23,   24,   25,   26,   27,   29,   30,   31,   32,   33,   35,   36,   37,   38,
    40,   41,   43,   44,   46,   47,   49,   50,   52,   54,   55,   57,   59,   61,   63,   64,
    66,   68,   70,   72,   74,   77,   79,   81,   83,   85,   88,   90,   92,   95,   97,  100,
   102,  105,  107,  110,  113,  115,  118,  121,  124,  127,  130,  133,  136,  139,  142,  145,
   149,  152,  155,  158,  162,  165,  169,  172,  176,  180,  183,  187,  191,  195,  199,  203,
   207,  211,  215,  219,  223,  227,  232,  236,  240,  245,  249,  254,  258,  263,  268,  273,
   277,  282,  287,  292,  297,  302,  308,  313,  318,  323,  329,  334,  340,  345,  351,  357,
   362,  368,  374,  380,  386,  392,  398,  404,  410,  417,  423,  429,  436,  442,  449,  455,
   462,  469,  476,  483,  490,  497,  504,  511,  518,  525,  533,  540,  548,  555,  563,  571,
   578,  586,  594,  602,  610,  618,  626,  634,  643,  651,  660,  668,  677,  685,  694,  703,
   712,  721,  730,  739,  748,  757,  766,  776,  785,  795,  804,  814,  824,  833,  843,  853,
   863,  873,  884,  894,  904,  915,  925,  936,  946,  957,  968,  979,  990, 1001, 1012, 1023
};

// MQTT paths
#define H801_MQTT_PING   "/ping"
#define H801_MQTT_SET    "/set"
#define H801_MQTT_CHANGE "/updated"

// Interval between each ping
#define H801_MQTT_PING_INTERVAL (5*1000)

// Number of steps to to fade each second
#define H801_DURATION_FADE_STEPS 10

// Forward declarations
void mqttCallback(char* mqttTopic, byte* mqttPayload, unsigned int mqttLength);
void saveConfigCallback();
void setLightPWM();
void publishMQTT(bool changeEvent);
void mqttReconnect();


// Global variables
bool durationFading = false;
bool H801StatusOn = false;


WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

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


// Initialization helper
#define INITIALIZE_PIN(_X_) { id: #_X_, pin: H801_PWM_PIN_ ##_X_, bri: 0, fadeBri: 0, fadeStep: 0, fadeLen: 0}

// Struct defining one pin
typedef struct {
  const char* id;
  const uint8_t pin;
  uint8_t bri;
  
  float     fadeBri;
  float     fadeStep;
  uint32_t  fadeLen;

} H801LedStatus, *PH801LedStatus;

// Create led definitions
H801LedStatus LedStatusR  = INITIALIZE_PIN(R);
H801LedStatus LedStatusG  = INITIALIZE_PIN(G);
H801LedStatus LedStatusB  = INITIALIZE_PIN(B);
H801LedStatus LedStatusW1 = INITIALIZE_PIN(W1);
H801LedStatus LedStatusW2 = INITIALIZE_PIN(W2);

// Array with all leds
const PH801LedStatus LedStatus[] = {
  &LedStatusR,
  &LedStatusG,
  &LedStatusB,
  &LedStatusW1,
  &LedStatusW2,
};

// H801 configuration
struct {
  uint16_t chipIDLen;
  char chipID[16];
  char hostname[16];

  char mqttTopicChange[strlen(H801_MQTT_CHANGE) + 16];
  char mqttTopicPing[strlen(H801_MQTT_PING) + 16];
  char mqttTopicSet[strlen(H801_MQTT_SET) + 16];

  bool shouldSaveConfig;
  char mqttServer[40];
  char mqttPort[6];
  uint16_t mqttPortNum;

} H801Config;

/**
 * Callback indicating that we need to save the configuration
 */
void saveConfigCallback () {
  Serial1.println("Config: Should save config");
  H801Config.shouldSaveConfig = true;
}

/**
 * Update PWM light value
 * @param led Led configuration
 */
void setLightPWM(PH801LedStatus led) {
  uint8_t bri = led->bri;
  analogWrite(led->pin, gammaTable[bri]);
}

/**
 * Setup H801 and connect to the WiFi
 */
void setup() {
  // Set pins as output and update value
  for (uint i = 0; i < countof(LedStatus); i++) {
    pinMode(LedStatus[i]->pin, OUTPUT);
    setLightPWM(LedStatus[i]);
  }

  // red, green led as output
  pinMode(H801_LED_PIN_R, OUTPUT);
  pinMode(H801_LED_PIN_G, OUTPUT);

  // red: off, green: on
  digitalWrite(H801_LED_PIN_R, 0);
  digitalWrite(H801_LED_PIN_G, 1);

  // Generate hostname and chip id
  sprintf(H801Config.chipID, "%08X", ESP.getChipId());
  sprintf(H801Config.hostname, "esp%08X", ESP.getChipId());

  H801Config.chipIDLen = strlen(H801Config.chipID);

  // Setup console
  Serial1.begin(115200);
  Serial1.println("--------------------------");
  Serial1.println();

  delay(10000);

  // Default config values
  strlcpy(H801Config.mqttServer, "10.8.0.100", countof(H801Config.mqttServer));
  strlcpy(H801Config.mqttPort,   "1883",       countof(H801Config.mqttPort));

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
  WiFiManagerParameter customMQTTServer("server", "mqtt server", H801Config.mqttServer, 40);
  WiFiManagerParameter customMQTTPort(  "port",   "mqtt port",   H801Config.mqttPort, 6);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  
  //add all your parameters here
  wifiManager.addParameter(&customMQTTServer);
  wifiManager.addParameter(&customMQTTPort);

  //reset settings - for testing
  //wifiManager.resetSettings();

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality();
  
  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  wifiManager.setTimeout(3600);


  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect(H801Config.hostname, "password")) {
    Serial1.println("WiFi: failed to connect, timeout detected");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }


  //if you get here you have connected to the WiFi
  Serial1.println("WiFi: connected");

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


  {
    size_t len = strlen(H801Config.chipID);
    strlcpy(H801Config.mqttTopicChange, H801Config.chipID, countof(H801Config.mqttTopicChange));
    strlcpy(H801Config.mqttTopicSet,    H801Config.chipID, countof(H801Config.mqttTopicSet));
    strlcpy(H801Config.mqttTopicPing,   H801Config.chipID, countof(H801Config.mqttTopicPing));

    strlcpy(H801Config.mqttTopicChange + len, H801_MQTT_CHANGE, countof(H801Config.mqttTopicChange) - len);
    strlcpy(H801Config.mqttTopicSet + len,    H801_MQTT_SET,    countof(H801Config.mqttTopicSet) - len);
    strlcpy(H801Config.mqttTopicPing + len,   H801_MQTT_PING,   countof(H801Config.mqttTopicPing) - len);

  }


  // Convert mqtt_port to int, default to 1883
  if (!(*H801Config.mqttPort && 
        (H801Config.mqttPortNum = (uint16_t)strtol(H801Config.mqttPort, NULL, 10)) != 0)) {
    H801Config.mqttPortNum = 1883;
  }

  // Print MQTT
  Serial1.print("  MQTT:    \"");
  Serial1.print(H801Config.mqttServer);
  Serial1.print("\" ");
  Serial1.println(H801Config.mqttPortNum);

  // init the MQTT connection
  mqttClient.setServer(H801Config.mqttServer, H801Config.mqttPortNum);
  mqttClient.setCallback(mqttCallback);



  MDNS.begin(H801Config.hostname);

  httpUpdater.setup(&httpServer);
  httpServer.begin();

  MDNS.addService("http", "tcp", 80);


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
}


/**
 * Publish current state to MQTT
 * @param changeEvent Is it a change event or ping event
 */
void publishMQTT(bool changeEvent) {
  static char buffer[1024];

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();

  // Global on/off state
  root["on"] = H801StatusOn;

  // Convert each led state
  for (uint i = 0; i < countof(LedStatus); i++) {
    PH801LedStatus led = LedStatus[i];
    root[led->id] = led->bri;
  }

  // Serialize JSON
  root.printTo(buffer, sizeof(buffer));

  // Print to serial
  Serial1.printf("%s: ", changeEvent ? H801Config.mqttTopicChange : H801Config.mqttTopicPing);
  Serial1.println(buffer);

  // Publish
  mqttClient.publish(changeEvent ? H801Config.mqttTopicChange : H801Config.mqttTopicPing, buffer, false);
}



/**
 * MQTT callback
 * @param mqttTopic   MQTT topic
 * @param mqttPayload MQTT payload
 * @param mqttLength  MQTT payload length
 */
void mqttCallback(char* mqttTopic, byte* mqttPayload, unsigned int mqttLength) {
  static char payload[200];

  // Ignore to large payloads
  if (mqttLength >= countof(payload))
    return;

  // Set topic
  if (!strcmp(mqttTopic, H801Config.mqttTopicSet)) {
    // Copy mqtt paylod to buffer and null terminate it
    strncpy(payload, (char*)mqttPayload, min(mqttLength, countof(payload)));
    payload[min(mqttLength, countof(payload) - 1)] = '\0';

    // Par se the json
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& json = jsonBuffer.parseObject(payload);

    // Failed to parse json
    if (!json.success()) {
      return;
    }

    json.printTo(Serial);

    // Check if duration is specified
    long duration = 0;
    if (json.containsKey("duration"))
      duration = json["duration"].as<long>();

    // Calculate number of steps for this duration
    uint32_t durationSteps = 0;
    if (duration > 0)
      durationSteps = (duration/H801_DURATION_FADE_STEPS);

    // Has any value changed
    bool isChanged = false;

    // Check all PWM leds
    for (uint i = 0; i < countof(LedStatus); i++) {
      PH801LedStatus led = LedStatus[i];
      
      // Skip led if we don't have any value
      if (!json.containsKey(led->id))
        continue;

      // Get value and ensure that it in the valid range
      long newValue = json[led->id].as<long>();
      newValue = constrain(newValue, 0x00, 0xFF);

      // Clear any current fading
      led->fadeLen = 0;

      // No change
      if ((uint8_t)newValue == led->bri)
        continue;

      // Indicate that we have changed the light
      isChanged = true;

      // No duration update pwm directly
      if (!durationSteps) {
        led->bri = (uint8_t)newValue;
        setLightPWM(led);
      }
      // Calculate step size and set fading flag
      // Set the bri to the target value so the change event contains the target values
      else {
        led->fadeStep = ((int)newValue - (int)led->bri)/(float)durationSteps;
        led->fadeBri = led->bri;
        led->fadeLen = durationSteps;
        durationFading = true;
        
        Serial1.printf("Led %s: bri:%u diff:%d dur:%ld step:%lu\n", 
                       led->id, led->bri, ((int)newValue - (int)led->bri), duration, durationSteps);

        // Set bri to target value so we publish the target value to mqtt  
        led->bri = (uint8_t)newValue;
      }
    }

    // Values was updated, publish the update
    if (isChanged)
      publishMQTT(true);
  }
}

void mqttReconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial1.println("MQTT: Connecting...");
    // Attempt to connect
    if (mqttClient.connect(H801Config.chipID)) {
      Serial1.println("MQTT: Connected");

      mqttClient.subscribe(H801Config.mqttTopicSet);
    }
    else {
      Serial1.print("MQTT: failed, state(");
      Serial1.print(mqttClient.state());
      Serial1.println(") trying again in 5s");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


/**
 * Main loop
 */
void loop() {
  static unsigned long lastPost = 0;
  static unsigned long lastFade = 0;
  static bool fadingLedOn = false;

  // process OTA updates
  httpServer.handleClient();
  
  if (!mqttClient.connected()) {
    mqttReconnect();
  }
  mqttClient.loop();

  unsigned long time = millis();

  // Are we fading light
  if (durationFading && (time >= lastFade + (100/H801_DURATION_FADE_STEPS) || time < lastFade)) {
    lastFade = time;
    durationFading = false;
    uint i;

    // For each led
    for (i=0; i < countof(LedStatus); i++) {
      PH801LedStatus led = LedStatus[i];
      
      // Fading light?
      if (!led->fadeLen)
        continue;

      // Decrease fading counter
      led->fadeLen--;

      // Multiply 
      led->fadeBri += led->fadeStep;
      led->fadeBri = constrain(led->fadeBri, 0x00, 0xFF);

      // Update brghtness 
      led->bri=(uint8_t)(led->fadeBri + 0.5);
      setLightPWM(led);

      // Still fading
      if (led->fadeLen) {
        durationFading = true;
      }
      // Last fade event, clear values
      else {
        led->fadeBri = 0;
        led->fadeStep = 0;
      }
    }

    // Blink blink led during fading, ensure led is green when done
    fadingLedOn = (durationFading ? (!fadingLedOn) : true);
    
    digitalWrite(H801_LED_PIN_G, fadingLedOn?0:1);
    digitalWrite(H801_LED_PIN_R, fadingLedOn?1:0);
  }

  // Trigger if we have wrapped or we had a timeout
  if (time > lastPost + (H801_MQTT_PING_INTERVAL) || time < lastPost) {
    lastPost = time;
    publishMQTT(false);
  }

  delay(1);
}

