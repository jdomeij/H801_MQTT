

class H801_HTTP {
  private:
    ESP8266WebServer m_httpServer;
    ESP8266HTTPUpdateServer m_httpUpdater;
    PH801_Config m_config;
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
      if(SPIFFS.exists(path)){
        File file = SPIFFS.open(path, "r");
        m_httpServer.streamFile(file, contentType);
        file.close();
        return true;
      }
      return false;
    }


    /**
     * HTTP handle for root/index page
     */
    void httpHandleRoot() {
      if(!handleFileRead("/index.html"))
        m_httpServer.send(404, "text/plain", "FileNotFound");
    }


    /**
     * HTTP handle for POST
     */
    void httpHandlePOST() {
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

      const char *jsonString = m_functions->set_Status(json);
      m_httpServer.send(200, "application/json", jsonString);
      return;
    }


    /**
     * HTTP handle for GET
     */
    void httpHandleGET() {
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

      const char *jsonString = m_functions->set_Status(json);
      if (!jsonString) {
        m_httpServer.send(406, "application/json", "{ \"message\": \"Unable to update lights\"}");
      }

      m_httpServer.send(200, "application/json", jsonString);
    }

  public:
    H801_HTTP(PH801_Functions functions) {
      m_httpServer  = ESP8266WebServer(80);
      m_httpUpdater = ESP8266HTTPUpdateServer();

      m_functions = functions;
    }


    /**
     * Setup HTTP server
     * @param config Configuration
     */
    void setup(PH801_Config config) {
      MDNS.begin(config->HTTP.hostname);

      m_httpUpdater.setup(&m_httpServer);
      
      m_httpServer.on("/",           HTTP_GET, [&]() {
        this->httpHandleRoot();
      });
      m_httpServer.on("/index.html", HTTP_GET, [&]() {
        this->httpHandleRoot();
      });
      m_httpServer.on("/status", HTTP_GET,  [&]() {
        this->httpHandleGET();
      });
      m_httpServer.on("/status", HTTP_POST, [&]() {
        this->httpHandlePOST();
      });
      m_httpServer.begin();

      MDNS.addService("http", "tcp", 80);
    }


    void loop() {
      m_httpServer.handleClient();
    }
};



