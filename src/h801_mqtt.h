

// MQTT paths
#define H801_MQTT_PING   "/ping"
#define H801_MQTT_SET    "/set"
#define H801_MQTT_UPDATE "/updated"
#define H801_MQTT_BUTTON "/button"


// Interval between each ping
#define H801_MQTT_PING_INTERVAL (60*1000)


/**
 * H801 MQTT handling
 */
class H801_MQTT {
  private:
    bool m_validConfig;

    PubSubClient m_mqttClient;
    H801_Config& m_config;
    PH801_Functions m_functions;

    char m_chipID[10];
    char m_topicButton[128];
    char m_topicPing[128];
    char m_topicSet[128];
    char m_topicUpdate[128];
    char m_topicSetNoAlias[128];

    unsigned long m_lastPost;
    unsigned long m_lastReconnect;

    /**
     * MQTT callback
     * @param mqttTopic   Topic
     * @param mqttPayload Data
     * @param mqttLength  Data length
     */
    void callback(char* mqttTopic, byte* mqttPayload, unsigned int mqttLength) {
      static char payload[200];
      if (mqttLength >= countof(payload))
        return;
      
      // Set topic
      if (!strcmp(mqttTopic, m_topicSet) ||
          (*m_topicSetNoAlias && !strcmp(mqttTopic, m_topicSetNoAlias))) {        
        // Copy mqtt payload to buffer and NULL terminate it
        strncpy(payload, (char*)mqttPayload, min(mqttLength, countof(payload)));
        payload[min(mqttLength, countof(payload) - 1)] = '\0';

        StaticJsonBuffer<200> jsonBuffer;

        // Parse the json
        JsonObject& json = jsonBuffer.parseObject(payload);

        if (!json.success()) {
          return;
        }

        Serial1.print("MQTT /set: ");
        json.printTo(Serial1);
        Serial1.println("");


        // Change event is automatically triggered so ignore response
        m_functions->set_Status("MQTT", json);
        return;
      }
    }


    /**
     * Concat two string into a third one
     * @param  dst    Destination
     * @param  dstLen Dest length
     * @param  strA   First string
     * @param  strB   Second string
     * @return Number of characters written
     */
    size_t concatTopic(char *dst, size_t dstLen, const char *strA, const char *strB) {
      size_t offset = strlcpy(dst, strA, dstLen);
      if (offset + 1 >= dstLen)
        return offset;

      return offset + strlcpy(dst + offset, strB, dstLen - offset);
    }


    /**
     * Updates topic variables with new topic prefix
     * @param newTopic New topic prefix
     */
    void updateTopic(const char *newTopic) {
      concatTopic(m_topicButton, countof(m_topicButton), newTopic, H801_MQTT_BUTTON);
      concatTopic(m_topicPing,   countof(m_topicPing),   newTopic, H801_MQTT_PING);
      concatTopic(m_topicUpdate, countof(m_topicUpdate), newTopic, H801_MQTT_UPDATE);
      concatTopic(m_topicSet,    countof(m_topicSet),    newTopic, H801_MQTT_SET);
    }


  public:
    /**
     * H801 MQTT constructor
     * @param wifiClient WiFi client
     * @param config     Configuration
     * @param functions  External functions
     */
    H801_MQTT(WiFiClient &wifiClient, H801_Config &config, PH801_Functions functions):
        m_config(config),
        m_functions(functions),
        m_lastPost(0),
        m_lastReconnect(0),
        m_validConfig(false) {
      m_mqttClient = PubSubClient(wifiClient);

      m_mqttClient.setCallback([&](char* mqttTopic, byte* mqttPayload, unsigned int mqttLength) {
        this->callback(mqttTopic, mqttPayload, mqttLength);
      });

      // Create base topics
      snprintf(m_chipID, countof(m_chipID), "%08X", ESP.getChipId());
      m_chipID[countof(m_chipID)-1] = '\0';

      // Used when we have alias
      m_topicSetNoAlias[0] = '\0';

      updateTopic(m_chipID);
    }


    /**
     * Set current MQTT config
     */
    bool setup() {
      // Ensure that we are not connected
      if (m_mqttClient.connected()) {
        m_mqttClient.disconnect();
      }

      // Is mqtt enabled
      if (!*m_config.m_MQTT.server) {
        m_validConfig = false;
        return true;
      }

      // Default mqtt port
      uint16_t port = 1883;

      // Convert mqtt_port to int, default to 1883
      if (*m_config.m_MQTT.port) {
        if ((port = (uint16_t)strtol(m_config.m_MQTT.port, NULL, 10)) == 0) {
          port = 1883;
        }
      }

      // Do we have custom topic
      if (*m_config.m_MQTT.alias) {
        updateTopic(m_config.m_MQTT.alias);
        concatTopic(m_topicSetNoAlias, countof(m_topicSetNoAlias), m_chipID, H801_MQTT_SET);
      }
      // Use chip ID as topic
      else {
        updateTopic(m_chipID);
        m_topicSetNoAlias[0] = '\0';
      }


      Serial1.println("\nMQTT: Registered topics");
      Serial1.printf("   %s\n", m_topicButton);
      Serial1.printf("   %s\n", m_topicPing);
      Serial1.printf("   %s\n", m_topicUpdate);
      Serial1.printf("   %s\n", m_topicSet);
      if (*m_topicSetNoAlias) {
        Serial1.printf("   %s\n", m_topicSetNoAlias);
      }

      // Configure server
      m_mqttClient.setServer(m_config.m_MQTT.server, port);

      // Try to connect
      if (this->connect()) {
        // Always listen to the <id>/set topic
        m_mqttClient.subscribe(m_topicSet);

        // Register non-alias version
        if (*m_topicSetNoAlias) {
          m_mqttClient.subscribe(m_topicSetNoAlias);
        }
      }
      else {
        Serial1.println("MQTT: Failed to connect to server");
      }

      m_validConfig = true;
      return true;
    }


    /**
     * Publish config changes to MQTT
     * @param buffer New configuration
     */
    void publishConfigUpdate(const char *buffer) {
      if (!m_validConfig || !m_mqttClient.connected())
        return;

      m_mqttClient.publish(m_topicUpdate, buffer, false);
    }


    /**
     * Publish button press to MQTT
     */
    void publishButtonPress() {
      if (!m_validConfig || !m_mqttClient.connected())
        return;

      m_mqttClient.publish(m_topicButton, "{\"button\":true}", false);
    }

    /**
     * Convert MQTT client state to string
     * @param  state Numeric state
     * @return String representation of state
     */
    const char * getConnectStateInfo(int state) {
      switch(state) {
        case MQTT_CONNECTION_TIMEOUT:
          return "the server didn't respond within the keepalive time";
        case MQTT_CONNECTION_LOST:
          return "the network connection was broken";
        case MQTT_CONNECT_FAILED:
          return "the network connection failed";
        case MQTT_DISCONNECTED:
          return "the client is disconnected cleanly";
        case MQTT_CONNECTED:
          return "the cient is connected";
        case MQTT_CONNECT_BAD_PROTOCOL:
          return "the server doesn't support the requested version of MQTT";
        case MQTT_CONNECT_BAD_CLIENT_ID:
          return "the server rejected the client identifier";
        case MQTT_CONNECT_UNAVAILABLE:
          return "the server was unable to accept the connection";
        case MQTT_CONNECT_BAD_CREDENTIALS:
          return "the username/password were rejected";
        case MQTT_CONNECT_UNAUTHORIZED:
          return "the client was not authorized to connect";
        default:
          break;
      }
      return "Unknown";
    }


    /**
     * Handles connection to MQTT server
     * @return false if failed to connect to server
     */
    bool connect() {
      // No password
      if (!*m_config.m_MQTT.login) {
        if (m_mqttClient.connect(m_chipID))
          return true;
      }

      // Password authentication
      else {
        if (m_mqttClient.connect(m_chipID, m_config.m_MQTT.login, m_config.m_MQTT.passw))
          return true;
      }

      // Failed to connect
      Serial1.printf("MQTT: connect failed, state: %d (%s)", m_mqttClient.state(), this->getConnectStateInfo(m_mqttClient.state()));
      return false;
    }


    /**
     * MQTT loop
     * @param time current millis time
     */
    void loop(unsigned long time) {
      if (!m_validConfig)
        return;

      // Check if we are connected
      if (m_mqttClient.connected())
        ; // Ignore

      // Waiting period for next re-connect
      else if (time > m_lastReconnect && time < m_lastReconnect + 5000) {
        return;
      }

      // Attempt to re-connect
      else if (!this->connect()) {
        Serial1.println("MQTT: re-trying again in 5s");        
        m_lastReconnect = time;
        return;
      }

      // Connected
      else {
        Serial1.println("MQTT: Connected");
        m_mqttClient.subscribe(m_topicSet);
        if (*m_topicSetNoAlias) {
          m_mqttClient.subscribe(m_topicSetNoAlias);
        }
      }

      m_mqttClient.loop();

      // Trigger if we have wrapped or we had a timeout
      if (time < m_lastPost || time > m_lastPost + (H801_MQTT_PING_INTERVAL)) {
        m_lastPost = time;

        const char *buffer = m_functions->get_Status();
        if (buffer)
          m_mqttClient.publish(m_topicPing, buffer, false);
      }
    }
};
