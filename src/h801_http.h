

class H801_HTTP {
  private:
    ESP8266WebServer m_httpServer;
    ESP8266HTTPUpdateServer m_httpUpdater;
    H801_Config &m_config;
    PH801_Functions m_functions;

    /**
     * Determine content type by file extension
     * @param  filename  File to get conent type for
     * @return Content type string
     */
    String httpContentType(String filename){
           if(filename.endsWith(".htm")) return "text/html";
      else if(filename.endsWith(".html")) return "text/html";
      else if(filename.endsWith(".css")) return "text/css";
      else if(filename.endsWith(".js"))  return "application/javascript";
      else if(filename.endsWith(".png")) return "image/png";
      else if(filename.endsWith(".gif")) return "image/gif";
      else if(filename.endsWith(".jpg")) return "image/jpeg";
      else if(filename.endsWith(".ico")) return "image/x-icon";
      else if(filename.endsWith(".xml")) return "text/xml";
      else if(filename.endsWith(".pdf")) return "application/x-pdf";
      else if(filename.endsWith(".zip")) return "application/x-zip";
      else if(filename.endsWith(".gz")) return "application/x-gzip";
      return "text/plain";
    }

    /**
     * Stream file from SPIFFS to HTTP client
     * @param  path   File to serve
     * @return false  If unable to find file
     *         true   If able to send file to client
     */
    bool handleFileRead(String path){
      Serial1.printf("handleFileRead: %s\n", path.c_str());
      if(path.endsWith("/")) path += "index.html";
      String contentType = httpContentType(path);
      
      if (!SPIFFS.begin()) {
        return false;
      }

      if(SPIFFS.exists(path)){
        File file = SPIFFS.open(path, "r");
        m_httpServer.streamFile(file, contentType);
        file.close();
        SPIFFS.end();
        return true;
      }
      SPIFFS.end();
      return false;
    }


    /**
     * HTTP GET root/index page
     */
    void get_Root() {
      if(!handleFileRead("/index.html"))
        m_httpServer.send(404, "text/plain", "FileNotFound");
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
        this->get_Root();
      });
      m_httpServer.on("/index.html", HTTP_GET, [&]() {
        this->get_Root();
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



