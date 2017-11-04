

// MQTT paths
#define H801_MQTT_PING   "/ping"
#define H801_MQTT_SET    "/set"
#define H801_MQTT_CHANGE "/updated"

// Interval between each ping
#define H801_MQTT_PING_INTERVAL (60*1000)


/**
 * H801 MQTT handling
 */
class H801_MQTT {
  private:
    bool m_bValidConfig;

    PubSubClient m_mqttClient;
    H801_CallbackSetConfig m_setConfig;
    H801_CallbackGetConfig m_getConfig;
    char *m_chipID;

    char m_mqttTopicChange[strlen(H801_MQTT_CHANGE) + 16];
    char m_mqttTopicPing[strlen(H801_MQTT_PING) + 16];
    char m_mqttTopicSet[strlen(H801_MQTT_SET) + 16];

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
      if (!strcmp(mqttTopic, m_mqttTopicSet)) {        
        // Copy mqtt payload to buffer and null terminate it
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
        m_setConfig(json);
        return;
      }
    }

  public:
    H801_MQTT(WiFiClient &wifiClient, H801_CallbackSetConfig setConfig, H801_CallbackGetConfig getConfig) {
      m_mqttClient = PubSubClient(wifiClient);

      H801_MQTT *THIS = this;

      m_mqttClient.setCallback([&](char* mqttTopic, byte* mqttPayload, unsigned int mqttLength) {
        this->callback(mqttTopic, mqttPayload, mqttLength);
      });

      m_setConfig = setConfig;
      m_getConfig = getConfig;

      m_lastPost = 0;
      m_lastReconnect = 0;
      m_mqttTopicChange[0] = '\0';
      m_mqttTopicPing[0]   = '\0';
      m_mqttTopicSet[0]    = '\0';

      m_bValidConfig = false;
    }

    /**
     * Set current MQTT config
     * @param chipID Chip id used for unique topic path
     * @param server Server address
     * @param port   Server port
     */
    void setup(char *chipID, char *server, uint16_t port) {
      if (!server) {
        m_bValidConfig = false;
        return;
      }

      m_chipID = chipID;
      m_mqttClient.setServer(server, port);

      size_t len = strlen(m_chipID);
      strlcpy(m_mqttTopicChange, m_chipID, countof(m_mqttTopicChange));
      strlcpy(m_mqttTopicSet,    m_chipID, countof(m_mqttTopicSet));
      strlcpy(m_mqttTopicPing,   m_chipID, countof(m_mqttTopicPing));

      strlcpy(m_mqttTopicChange + len, H801_MQTT_CHANGE, countof(m_mqttTopicChange) - len);
      strlcpy(m_mqttTopicSet + len,    H801_MQTT_SET,    countof(m_mqttTopicSet) - len);
      strlcpy(m_mqttTopicPing + len,   H801_MQTT_PING,   countof(m_mqttTopicPing) - len);

      m_mqttClient.subscribe(m_mqttTopicSet);

      Serial1.println(m_mqttTopicChange);
      Serial1.println(m_mqttTopicSet);
      Serial1.println(m_mqttTopicPing);
      m_bValidConfig = true;
    }

    /**
     * Publish config changes to MQTT
     * @param buffer New configuration
     */
    void publishConfigUpdate(const char *buffer) {
      if (!m_bValidConfig || !m_mqttClient.connected())
        return;

      m_mqttClient.publish(m_mqttTopicChange, buffer, false);
    }


    /**
     * MQTT loop
     * @param time current millis time
     */
    void loop(unsigned long time) {
      if (!m_bValidConfig)
        return;

      // Check if we are connected
      if (m_mqttClient.connected())
        ; // Ignore

      // Waiting period for next re-connect
      else if (time > m_lastReconnect && time < m_lastReconnect + 5000) {
        return;
      }

      // Attempt to re-connect
      else if (!m_mqttClient.connect(m_chipID)) {
        Serial1.print("MQTT: failed, state(");
        Serial1.print(m_mqttClient.state());
        Serial1.println(") trying again in 5s");
        
        m_lastReconnect = time;
          return;
      }

      // Connected
      else {
        Serial1.println("MQTT: Connected");
        m_mqttClient.subscribe(m_mqttTopicSet);
      }

      m_mqttClient.loop();

      // Trigger if we have wrapped or we had a timeout
      if (time < m_lastPost || time > m_lastPost + (H801_MQTT_PING_INTERVAL)) {
        m_lastPost = time;

        const char *buffer = m_getConfig();
        if (buffer)
          m_mqttClient.publish(m_mqttTopicPing, buffer, false);
      }

    }

};
