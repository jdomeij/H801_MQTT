
const PROGMEM char * s_htmlStylesheet = 
  #include "http/stylesheet.css"
;

const PROGMEM char * s_htmlIndex =
  #include "http/index.html"
;

const PROGMEM char * s_htmlConfig = 
  #include "http/config.html"
;


class H801_HTTP {
  private:
    ESP8266WebServer m_httpServer;
    ESP8266HTTPUpdateServer m_httpUpdater;
    H801_Config &m_config;
    PH801_Functions m_functions;


    /**
     * HTTP GET root/index page
     */
    void html_Index() {
      m_httpServer.sendHeader("Cache-Control", "max-age=86400");
      m_httpServer.send(200, "text/html", s_htmlIndex);
    }

    void html_Config() {
      m_httpServer.sendHeader("Cache-Control", "max-age=86400");
      m_httpServer.send(200, "text/html", s_htmlConfig);
    }

    void html_Stylesheet() {
      m_httpServer.sendHeader("Cache-Control", "max-age=86400");
      m_httpServer.send(200, "text/css", s_htmlStylesheet);
    }


    /**
     * HTTP POST status
     */
    void post_Status() {
      StaticJsonBuffer<200> jsonBuffer;
      
      // Check if body received
      if (m_httpServer.hasArg("plain")== false) { 
        m_httpServer.send(200, "application/json", "{ \"message\": \"Body not received\"}");
        return;
      }

      //Serial1.print("POST Input: ");
      //Serial1.println(m_httpServer.arg("plain"));

      // Parse the json
      JsonObject& json = jsonBuffer.parseObject(m_httpServer.arg("plain"));

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


      StaticJsonBuffer<200> jsonBuffer;
      JsonObject& json = jsonBuffer.createObject();

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
      StaticJsonBuffer<200> jsonBuffer;
      
      // Check if body received
      if (m_httpServer.hasArg("plain")== false) { 
        m_httpServer.send(200, "application/json", "{ \"message\": \"Body not received\"}");
        return;
      }

      // Parse the json
      JsonObject& json = jsonBuffer.parseObject(m_httpServer.arg("plain"));

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
    H801_HTTP(H801_Config &config, PH801_Functions functions):m_config(config) {
      m_httpServer  = ESP8266WebServer(80);
      m_httpUpdater = ESP8266HTTPUpdateServer();

      m_functions = functions;
    }


    /**
     * Setup HTTP server
     * @param config Configuration
     */
    void setup() {
      MDNS.begin(getHostname());

      m_httpUpdater.setup(&m_httpServer);
      
      m_httpServer.on("/",           HTTP_GET, [&]() {
        this->html_Index();
      });
      m_httpServer.on("/index.html", HTTP_GET, [&]() {
        this->html_Index();
      });
      m_httpServer.on("/config.html", HTTP_GET, [&]() {
        this->html_Config();
      });
      m_httpServer.on("/stylesheet.css", HTTP_GET, [&]() {
        this->html_Stylesheet();
      });
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



