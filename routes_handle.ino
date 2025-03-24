


// Serve index.html with dynamic values
void handleRoot() {
  deviceIPaddress = WiFi.localIP().toString();

  if (!LittleFS.exists("/index.html")) {
    server.send(404, "text/plain", "404: Not Found");
    return;
  }

  File file = LittleFS.open("/index.html", "r");
  if (!file) {
    server.send(500, "text/plain", "Internal Server Error: Could not open index.html");
    return;
  }

  String html = file.readString();
  file.close();

  // Replace placeholders with configuration values
  html.replace("{{serverURL}}", serverURL);
  html.replace("{{SerialNumber}}", serialNumber);
  html.replace("{{wifiSSID}}", wifiSSID);
  html.replace("{{wifiPassword}}", wifiPassword);
  html.replace("{{ipAddress}}", deviceIPaddress);
  html.replace("{{intervalHeartbeat}}", String(intervalHeartbeat));
  html.replace("{{gmtTimeZone}}", gmtTimeZone);
  html.replace("{{server_ip}}", server_ip);
  html.replace("{{server_port}}", server_port);


  server.send(200, "text/html", html);
}


void handleLogoImage() {
  File file = LittleFS.open("/logo.png", "r");  // Open the file from SPIFFS
  if (!file) {
    server.send(404, "text/plain", "Image not found");
    return;
  }

  server.streamFile(file, "image/jpeg");  // Send the file over HTTP
  file.close();
}
void handleConfigJson() {
  if (!LittleFS.exists("/config.json")) {
    server.send(404, "application/json", "{\"error\": \"Config file not found\"}");
    return;
  }

  File file = LittleFS.open("/config.json", "r");
  String jsonContent = file.readString();
  file.close();

  server.send(200, "application/json", jsonContent);
}
// Save configuration handler
void handleSaveConfig() {
  if (server.hasArg("serverURL") && server.hasArg("wifiSSID") && server.hasArg("wifiPassword")) {
    serverURL = server.arg("serverURL");
    serialNumber = server.arg("SerialNumber");
    wifiSSID = server.arg("wifiSSID");
    wifiPassword = server.arg("wifiPassword");

    intervalHeartbeat = server.arg("intervalHeartbeat").toInt();

    if (intervalHeartbeat <= 10) intervalHeartbeat = 10;
    intervalHeartbeat = intervalHeartbeat ;
    server_ip = server.arg("server_ip");
    server_port = server.arg("server_port");
    gmtTimeZone = server.arg("gmtTimeZone");

    if (saveConfig()) {
      server.send(200, "text/html", "<html><body><h1>Configuration Saved! Restarting...</h1></body></html>");
      delay(2000);

    } else {
      server.send(500, "text/html", "<html><body><h1>Failed to Save Configuration!</h1></body></html>");
    }
  } else {
    server.send(400, "text/plain", "Bad Request: Missing parameters");
  }
}