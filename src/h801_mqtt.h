

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
    bool m_bValidConfig;

    PubSubClient m_mqttClient;
    PH801_Functions m_functions;

    String m_chipID;
    String m_topicButton;
    String m_topicPing;
    String m_topicSet;
    String m_topicUpdate;

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
      if (!strcmp(mqttTopic, m_topicSet.c_str())) {        
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
        m_functions->set_Status(json);
        return;
      }
    }

  public:
    H801_MQTT(WiFiClient &wifiClient, PH801_Functions functions) {
      m_mqttClient = PubSubClient(wifiClient);

      H801_MQTT *THIS = this;

      m_mqttClient.setCallback([&](char* mqttTopic, byte* mqttPayload, unsigned int mqttLength) {
        this->callback(mqttTopic, mqttPayload, mqttLength);
      });

      m_functions = functions;

      m_chipID = String(ESP.getChipId(), HEX);
      m_lastPost = 0;
      m_lastReconnect = 0;

      m_topicButton = String(m_chipID + H801_MQTT_BUTTON);
      m_topicPing   = String(m_chipID + H801_MQTT_PING);
      m_topicSet    = String(m_chipID + H801_MQTT_SET);
      m_topicUpdate = String(m_chipID + H801_MQTT_UPDATE);

      m_bValidConfig = false;
    }

    /**
     * Set current MQTT config
     * @param server Server address
     * @param port   Server port
     */
    void setup(char *server, uint16_t port) {
      if (!server) {
        m_bValidConfig = false;
        return;
      }

      m_mqttClient.setServer(server, port);

      m_mqttClient.subscribe(m_topicSet.c_str());

      Serial1.println("MQTT Topics");
      Serial1.println(m_topicButton);
      Serial1.println(m_topicPing);
      Serial1.println(m_topicSet);
      Serial1.println(m_topicUpdate);

      m_bValidConfig = true;
    }

    /**
     * Publish config changes to MQTT
     * @param buffer New configuration
     */
    void publishConfigUpdate(const char *buffer) {
      if (!m_bValidConfig || !m_mqttClient.connected())
        return;

      m_mqttClient.publish(m_topicUpdate.c_str(), buffer, false);
    }

    /**
     * Publish button press to MQTT
     */
    void publishButtonPress() {
      if (!m_bValidConfig || !m_mqttClient.connected())
        return;

      m_mqttClient.publish(m_topicButton.c_str(), "{\"button\":true}", false);
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
      else if (!m_mqttClient.connect(m_chipID.c_str())) {
        Serial1.print("MQTT: failed, state(");
        Serial1.print(m_mqttClient.state());
        Serial1.println(") trying again in 5s");
        
        m_lastReconnect = time;
          return;
      }

      // Connected
      else {
        Serial1.println("MQTT: Connected");
        m_mqttClient.subscribe(m_topicSet.c_str());
      }

      m_mqttClient.loop();

      // Trigger if we have wrapped or we had a timeout
      if (time < m_lastPost || time > m_lastPost + (H801_MQTT_PING_INTERVAL)) {
        m_lastPost = time;

        const char *buffer = m_functions->get_Status();
        if (buffer)
          m_mqttClient.publish(m_topicPing.c_str(), buffer, false);
      }

    }

};
