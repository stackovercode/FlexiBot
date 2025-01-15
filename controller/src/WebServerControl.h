#ifndef WEBSERVERCONTROL_H
#define WEBSERVERCONTROL_H

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>

class WebServerControl {
public:
    WebServerControl(int port = 80);

    void begin(const char* ssid, const char* password);
    void handleClient();
    void setControlModeCallback(void (*callback)(const char* mode));
    void setCommandCallback(void (*callback)(const char* command));

private:

    WiFiServer server;

    void (*controlModeCallback)(const char* mode);
    void (*commandCallback)(const char* command);

    String currentMode;

    void sendWebPage(WiFiClient& client);
    void parseRequest(WiFiClient& client, const String& request);
};

#endif // WEBSERVERCONTROL_H
