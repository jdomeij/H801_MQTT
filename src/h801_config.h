
// Configuration file
#define H801_CONFIG_FILE "/config.json"


/**
 * H801 Configuration
 */
class H801_Config {
  public:
    StaticJsonBuffer<1024> m_jsonBuffer;

    // MQTT config
    struct {
      char server[128];
      char port[6];

      char alias[128];

      char login[128];
      char passw[128];
    } m_MQTT;

    struct {
      bool R;
      bool G;
      bool B;
      bool W1;
      bool W2;
    } m_ButtonFade;

  private:
    /**
     * Copy json variable to string buffer if valid
     * @param  json     JSON value
     * @param  dest     dest buffer
     * @param  destSize dest buffer len
     * @return false if failure or unchanged
     */
    bool jsonToStringProp(const JsonVariant &json, char *dest, size_t destSize) {
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

    /**
     * Copy json variable to string buffer if valid and not hidden password
     * @param  json     JSON value
     * @param  dest     dest buffer
     * @param  destSize dest buffer len
     * @return false if failure or unchanged
     */
    bool jsonToPasswProp(const JsonVariant &json, char *dest, size_t destSize) {
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

      // Dummy password
      if (!strcmp(tmp, "*********"))
        return false;

      // And update the value
      strlcpy(dest, tmp, destSize);
      return true;
    }

    /**
     * Copy json variable to bool value if valid
     * @param  json     JSON value
     * @param  dest     dest variable
     * @return false if failure or unchanged
     */
    bool jsonToBoolProp(const JsonVariant &json, bool *dest) {
      // Check for valid json object
      if (!json.success()) {
        return false;
      }

      // Remember old value
      bool oldValue = *dest;

      // Number
      if (json.is<signed long>())
        *dest = (json.as<signed long>() != 0);
      
      // Boolean
      else if (json.is<bool>())
        *dest = json.as<bool>();

      // String (true|false|1|0)
      else if (json.is<const char*>()) {
        const char *val = json.as<const char*>();

        if (!strcmp(val, "true"))
          *dest = true;
        else if (!strcmp(val, "1"))
          *dest = true;
        else if (!strcmp(val, "false"))
          *dest = false;
        else if (!strcmp(val, "0"))
          *dest = false;
        else
          return false;
      }
      else
        return false;
      
      return *dest == oldValue;
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

      m_ButtonFade.R = true;
      m_ButtonFade.G = true;
      m_ButtonFade.B = true;
      m_ButtonFade.W1 = true;
      m_ButtonFade.W2 = true;
    }


    /**
     * Converts configuration to JSON stirng
     * @param  hidePassword Hides password fields
     * @return JSON string
     */
    char * toJSONString(bool hidePassword) {
      static char buffer[1024];

      m_jsonBuffer.clear();
      JsonObject& json = m_jsonBuffer.createObject();

      json["mqtt_server"] = m_MQTT.server;
      json["mqtt_port"]   = m_MQTT.port;

      json["mqtt_alias"]  = m_MQTT.alias;

      json["mqtt_login"]  = m_MQTT.login;
      
      // Output empty string if password is empty
      if (!hidePassword || !*m_MQTT.passw) {
        json["mqtt_passw"]  = m_MQTT.passw;
      }
      else {
        json["mqtt_passw"]  = "*********";
      }


      // Button fading
      JsonObject& buttonFade = json.createNestedObject("button_fade");
      buttonFade["R"]  = m_ButtonFade.R;
      buttonFade["G"]  = m_ButtonFade.G;
      buttonFade["B"]  = m_ButtonFade.B;
      buttonFade["W1"] = m_ButtonFade.W1;
      buttonFade["W2"] = m_ButtonFade.W2;

      // Serialize JSON
      json.printTo(buffer, sizeof(buffer));
      return buffer;
    }


  /**
   * TODO: Set current configuration
   * @param  json New configuration
   * @return Was any value modified
   */
  bool set(JsonObject& json) {

    bool isModified = false;

    isModified = jsonToStringProp(json["mqtt_server"], m_MQTT.server,  countof(m_MQTT.server)) || isModified;
    isModified = jsonToStringProp(json["mqtt_port"],   m_MQTT.port,    countof(m_MQTT.port))   || isModified;
    isModified = jsonToStringProp(json["mqtt_alias"],  m_MQTT.alias,   countof(m_MQTT.alias))  || isModified;
    isModified = jsonToStringProp(json["mqtt_login"],  m_MQTT.login,   countof(m_MQTT.login))  || isModified;
    isModified = jsonToStringProp(json["mqtt_passw"],  m_MQTT.passw,   countof(m_MQTT.passw))  || isModified;


    JsonObject& buttonFade = json["button_fade"];
    if (buttonFade.success()) {
      isModified = jsonToBoolProp(buttonFade["R"],  &m_ButtonFade.R)  || isModified;
      isModified = jsonToBoolProp(buttonFade["G"],  &m_ButtonFade.G)  || isModified;
      isModified = jsonToBoolProp(buttonFade["B"],  &m_ButtonFade.B)  || isModified;
      isModified = jsonToBoolProp(buttonFade["W1"], &m_ButtonFade.W1) || isModified;
      isModified = jsonToBoolProp(buttonFade["W2"], &m_ButtonFade.W2) || isModified;
    }
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

    m_jsonBuffer.clear();
    JsonObject& json = m_jsonBuffer.parseObject(buf.get());

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

    this->set(json);

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
  }
};