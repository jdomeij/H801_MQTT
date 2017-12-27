
class H801_HTTP {
  private:
    ESP8266WebServer m_httpServer;
    ESP8266HTTPUpdateServer m_httpUpdater;
    H801_Config &m_config;
    PH801_Functions m_functions;
    StaticJsonBuffer<1024> m_jsonBuffer;

    bool send_File(const char* contentType, const char *fileName) {
      Serial1.printf("send_File: %s\n", fileName);
      
      if (!SPIFFS.begin()) {
        return false;
      }

      Serial1.printf("GET: %s\r\n", fileName);

      // Check if exact match exists
      if (SPIFFS.exists(fileName)) {
        File file = SPIFFS.open(fileName, "r");
        m_httpServer.sendHeader("Cache-Control", "max-age=86400");
        m_httpServer.streamFile(file, contentType);
        file.close();
        SPIFFS.end();
        return true;
      }

      // No file
      SPIFFS.end();
      m_httpServer.send(404, "text/plain", "FileNotFound");
      return false;
    }


    /**
     * HTTP GET root/index page
     */
    void send_Data(const char* type, const char *data) {
      //m_httpServer.sendHeader("Cache-Control", "max-age=86400");
      m_httpServer.send(200, type, data);
    }


    /**
     * HTTP POST status
     */
    void post_Status() {
      m_jsonBuffer.clear();
      
      // Check if body received
      if (m_httpServer.hasArg("plain")== false) { 
        m_httpServer.send(406, "application/json", "{ \"message\": \"Body not received\"}");
        return;
      }

      Serial1.print("POST Input: ");
      //Serial1.println(m_httpServer.arg("plain"));

      m_jsonBuffer.clear();
      // Parse the json
      JsonObject& json = m_jsonBuffer.parseObject(m_httpServer.arg("plain"));

      // Failed to parse json
      if (!json.success()) {
        m_httpServer.send(406, "application/json", "{ \"message\": \"invalid JSON\"}");
        return;
      }

      Serial1.print("HTTP POST: ");
      json.printTo(Serial1);
      Serial1.println();

      const char *jsonString = m_functions->set_Status("HTTP", json);
      m_httpServer.send(200, "application/json", jsonString);
      return;
    }


    /**
     * HTTP GET status
     */
    void get_Status() {
      int numArgs = 0;

      // Check if we have arguments
      if ((numArgs = m_httpServer.args()) == 0) {
        m_httpServer.send(200, "application/json", m_functions->get_Status());
        return;
      }


      m_jsonBuffer.clear();
      JsonObject& json = m_jsonBuffer.createObject();

      // Convert to JSON
      for (int i = 0; i < numArgs; i++) {
        json[m_httpServer.argName(i)] = m_httpServer.arg(i); 
      }

      Serial1.print("HTTP GET: ");
      json.printTo(Serial1);
      Serial1.println();

      const char *jsonString = m_functions->set_Status("HTTP", json);
      if (!jsonString) {
        m_httpServer.send(406, "application/json", "{ \"message\": \"Unable to update lights\"}");
      }

      m_httpServer.send(200, "application/json", jsonString);
    }


    /**
     * HTTP GET config
     */
    void get_Config() {
      m_httpServer.send(200, "application/json", m_functions->get_Config());
    }


    /**
     * HTTP POST config
     */
    void post_Config() {
      // Check if body received
      if (m_httpServer.hasArg("plain")== false) { 
        m_httpServer.send(406, "application/json", "{ \"message\": \"Body not received\"}");
        return;
      }

      m_jsonBuffer.clear();

      // Parse the json
      JsonObject& json = m_jsonBuffer.parseObject(m_httpServer.arg("plain"));

      // Failed to parse json
      if (!json.success()) {
        m_httpServer.send(406, "application/json", "{ \"message\": \"invalid JSON\"}");
        return;
      }

      Serial1.print("HTTP POST: ");
      json.printTo(Serial1);
      Serial1.println();

      const char *jsonString = m_functions->set_Config("HTTP", json);
      m_httpServer.send(200, "application/json", jsonString);
      return;
    }

    /**
     * HTTP DELETE config
     */
    void delete_Config() {
      m_functions->reset_Config();
      m_httpServer.send(200, "application/json", "{}");
    }


  public:
    H801_HTTP(H801_Config &config, PH801_Functions functions):
        m_config(config),
        m_httpServer(80),
        m_httpUpdater() {

      m_functions = functions;
    }


    /**
     * Setup HTTP server
     * @param config Configuration
     */
    void setup() {
      MDNS.begin(getHostname());

      m_httpUpdater.setup(&m_httpServer);
      

      // Static contentent
      m_httpServer.on("/",           HTTP_GET, [&]() {
        this->send_File("text/html", "/index.html");
      });
      m_httpServer.on("/index.html", HTTP_GET, [&]() {
        this->send_File("text/html", "/index.html");
      });
      m_httpServer.on("/iro.min.js", HTTP_GET, [&]() {
        this->send_File("text/javascript", "/iro.min.js");
      });
      m_httpServer.on("/bundle.js", HTTP_GET, [&]() {
        this->send_File("text/javascript", "/bundle.js.gz");
      });
      m_httpServer.on("/icons.woff", HTTP_GET, [&]() {
        this->send_File("application/font-woff", "/icons.woff");
      });


      // Data content
      m_httpServer.on("/status", HTTP_GET,  [&]() {
        this->get_Status();
      });
      m_httpServer.on("/status", HTTP_POST, [&]() {
        this->post_Status();
      });
      m_httpServer.on("/config", HTTP_GET, [&]() {
        this->get_Config();
      });      
      m_httpServer.on("/config", HTTP_POST, [&]() {
        this->post_Config();
      });
      m_httpServer.on("/config", HTTP_DELETE, [&]() {
        this->delete_Config();
      });

      m_httpServer.begin();

      MDNS.addService("http", "tcp", 80);
    }


    void loop() {
      m_httpServer.handleClient();
    }
};



