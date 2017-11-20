
// Configuration file
#define H801_CONFIG_FILE "/config.json"


/**
 * H801 Configuration
 */
class H801_Config {
  public:

    // MQTT config
    struct {
      char server[128];
      char port[6];

      char alias[128];

      char login[128];
      char passw[128];
    } m_MQTT;

  private:
    /**
     * Copy json variable to string buffer if valid
     * @param  json     JSON value
     * @param  dest     dest buffer
     * @param  destSize dest buffer len
     * @return false if failure
     */
    bool jsonToProp(const JsonVariant &json, char *dest, size_t destSize) {
      // Check for valid json object
      if (!json.success()) {
        return false;
      }

      // Check type
      if (!json.is<char *>())
        return false;

      // Get value
      const char *tmp = json.as<const char *>();
      if (!tmp)
        return false;

      // Ignore same value
      if (!strcmp(dest, tmp))
        return false;

      // And update the value
      strlcpy(dest, tmp, destSize);
      return true;
    }

  public:
    /**
     * H801 Configuration object
     */
    H801_Config() {
      this->clear();
    }


    /**
     * Reset configuration
     */
    void clear(void) {
      memset(&m_MQTT, 0, sizeof(m_MQTT));
    }


    /**
     * Converts configuration to JSON stirng
     * @param  hidePassword Hides password fields
     * @return JSON string
     */
    char * toJSONString(bool hidePassword) {
      static char buffer[1024];

      StaticJsonBuffer<200> jsonBuffer;
      JsonObject& root = jsonBuffer.createObject();

      root["mqtt_server"] = m_MQTT.server;
      root["mqtt_port"]   = m_MQTT.port;

      root["mqtt_alias"]  = m_MQTT.alias;

      root["mqtt_login"]  = m_MQTT.login;
      
      // Output empty string if password is empty
      if (!hidePassword || !*m_MQTT.passw) {
        root["mqtt_passw"]  = m_MQTT.passw;
      }
      else {
        root["mqtt_passw"]  = "*********";
      }

      // Serialize JSON
      root.printTo(buffer, sizeof(buffer));
      return buffer;
    }


  /**
   * TODO: Set current configuration
   * @param  json New configuration
   * @return Was any value modified
   */
  bool set(JsonObject& json) {

    bool isModified = false;

    isModified = jsonToProp(json["mqtt_server"], m_MQTT.server,  countof(m_MQTT.server)) || isModified;
    isModified = jsonToProp(json["mqtt_port"],   m_MQTT.port,    countof(m_MQTT.port)) || isModified;
    isModified = jsonToProp(json["mqtt_alias"],  m_MQTT.alias,   countof(m_MQTT.alias)) || isModified;
    isModified = jsonToProp(json["mqtt_login"],  m_MQTT.login,   countof(m_MQTT.login)) || isModified;
    isModified = jsonToProp(json["mqtt_passw"],  m_MQTT.passw,   countof(m_MQTT.passw)) || isModified;

    return isModified;
  }


  /**
   * Write current configuration to SPIFFS
   * @return false if failure
   */
  bool save(void) {
    if (!SPIFFS.begin()) {
      Serial1.println("Config: failed to mount FS");
      return false;
    }

    const char *config = this->toJSONString(false);

    Serial1.println("Config: Saving config");

    File configFile = SPIFFS.open(H801_CONFIG_FILE, "w");
    
    if (!configFile) {
      Serial1.println("Config: Failed to open config file for writing");
      configFile.close();
      SPIFFS.end();
      return false;
    }

    // Print config
    Serial1.print("Config Save: ");
    Serial1.println(config);

    if (!configFile.print(config)) {
      Serial1.println("Config: Failed to write config file");
      configFile.close();
      SPIFFS.end();
      return false;
    }

    configFile.close();
    SPIFFS.end();
    return true;
  }


  /**
   * Read current configuration from SPIFFS
   * @return false if failed to read config
   */
  bool load() {
    File configFile;

    if (!SPIFFS.begin()) {
      Serial1.println("Config: failed to mount FS");
      return false;
    }

    if (!SPIFFS.exists(H801_CONFIG_FILE)) {
      Serial1.println("Config: No config file");
      this->clear();
      SPIFFS.end();
      return true;
    }

    if ((configFile = SPIFFS.open(H801_CONFIG_FILE, "r")) == 0) {
      Serial1.println("Config: Unable to open " H801_CONFIG_FILE);
      SPIFFS.end();
      return false;
    }

    size_t size = configFile.size();
    // Allocate a buffer to store contents of the file.
    std::unique_ptr<char[]> buf(new char[size]);

    configFile.readBytes(buf.get(), size);
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.parseObject(buf.get());

    // Close file and filesystem
    configFile.close();
    SPIFFS.end();

    if (!json.success()) {
      Serial1.println("Config: Failed to parse json config");
      Serial1.println(buf.get());
      return false;
    }

    Serial1.print("Config: Input JSON ");
    json.printTo(Serial1);
    Serial1.println("");

    jsonToProp(json["mqtt_server"], m_MQTT.server,  countof(m_MQTT.server));
    jsonToProp(json["mqtt_port"],   m_MQTT.port,    countof(m_MQTT.port));
    jsonToProp(json["mqtt_alias"],  m_MQTT.alias,   countof(m_MQTT.alias));
    jsonToProp(json["mqtt_login"],  m_MQTT.login,   countof(m_MQTT.login));
    jsonToProp(json["mqtt_passw"],  m_MQTT.passw,   countof(m_MQTT.passw));

    return true;
  }

  /**
   * Removes configuration from disk and WifiManager
   */
  void remove() {
    if (SPIFFS.begin()) {
      // Remove JSON configuration
      SPIFFS.remove(H801_CONFIG_FILE);
      SPIFFS.end();
    }
    else {
      Serial1.println("Config: failed to mount FS");
    }

    // Create new wifimanager and tell it to clear config
    WiFiManager wifiManager;
    wifiManager.resetSettings();
  }
};