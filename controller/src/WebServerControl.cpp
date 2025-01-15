#include "WebServerControl.h"

WebServerControl::WebServerControl(int port)
    : server(port), 
      controlModeCallback(nullptr),
      commandCallback(nullptr),
      currentMode("INDIVIDUAL")
{
}

void WebServerControl::begin(const char* ssid, const char* password) {
    Serial.println("Configuring Access Point...");
    int retryCount = 0;

    // Attempt to create AP
    while (WiFi.beginAP(ssid, password) != WL_AP_LISTENING && retryCount < 5) {
        Serial.println("Retrying AP setup...");
        delay(2000);
        retryCount++;
    }

    if (retryCount >= 5) {
        Serial.println("Failed to create AP. Rebooting...");
        NVIC_SystemReset(); // Reset the board (Portenta)
    } else {
        Serial.println("Access Point Created!");
        Serial.print("SSID: "); Serial.println(ssid);
        Serial.print("Password: "); Serial.println(password);
        Serial.print("IP Address: "); Serial.println(WiFi.localIP());
    }

    server.begin();
}

void WebServerControl::handleClient() {
    WiFiClient client = server.available();
    if (client) {
        Serial.println("New Client Connected");
        String request = "";
        while (client.connected()) {
            if (client.available()) {
                char c = client.read();
                request += c;

                // Check for end of request
                if (c == '\n') {
                    if (request.endsWith("\r\n\r\n")) {
                        break;
                    }
                }
            }
        }
        parseRequest(client, request);
        sendWebPage(client);

        client.stop();
        Serial.println("Client Disconnected");
    }
}

void WebServerControl::sendWebPage(WiFiClient& client) {
    // Basic HTTP headers
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println("Connection: close");
    client.println();

    // HTML content
    client.println("<!DOCTYPE html>");
    client.println("<html><head><title>Robot Control</title>");
    client.println("<style>");
    client.println("button { padding: 10px 20px; margin: 5px; }");
    client.println("#INDIVIDUAL { background-color: lightgreen; }");
    client.println("#GAIT { background-color: lightblue; }");
    client.println("</style>");
    client.println("<script>");
    client.println("function sendCommand(command) {");
    client.println("    var xhr = new XMLHttpRequest();");
    client.println("    xhr.open('GET', '/' + command, true);");
    client.println("    xhr.send();");
    client.println("    highlightButton(command);");
    client.println("}");
    client.println("function highlightButton(command) {");
    client.println("    document.getElementById('INDIVIDUAL').style.backgroundColor = 'lightgrey';");
    client.println("    document.getElementById('GAIT').style.backgroundColor = 'lightgrey';");
    client.println("    if(document.getElementById(command)) {");
    client.println("        document.getElementById(command).style.backgroundColor = 'lightgreen';");
    client.println("    }");
    client.println("}");
    client.println("</script>");
    client.println("</head><body>");
    client.println("<h1>Robot Control</h1>");

    // Mode buttons
    client.println("<button id='INDIVIDUAL' onclick=\"sendCommand('INDIVIDUAL')\">Individual Mode</button>");
    client.println("<button id='GAIT' onclick=\"sendCommand('GAIT')\">Gait Mode</button>");

    // Motor controls
    client.println("<h2>Motor Controls</h2>");
    client.println("<button onclick=\"sendCommand('ROTATE_M1_CW')\">Rotate M1 CW</button>");
    client.println("<button onclick=\"sendCommand('ROTATE_M1_CCW')\">Rotate M1 CCW</button>");

    client.println("<button onclick=\"sendCommand('STOP_MOTORS')\">Stop All Motors</button>");

    // Gait controls
    client.println("<h2>Gait Controls</h2>");
    client.println("<button onclick=\"sendCommand('START_CRAWLING')\">Start Crawling</button>");
    client.println("<button onclick=\"sendCommand('START_WALKING')\">Start Walking</button>");
    client.println("<button onclick=\"sendCommand('START_FASTCRAWL')\">Start Fast Crawl</button>");
    client.println("<button onclick=\"sendCommand('STOP_GAIT')\">Stop Gait</button>");

    client.println("</body></html>");
}

void WebServerControl::parseRequest(WiFiClient& client, const String& request) {
    if (request.indexOf("GET /INDIVIDUAL") >= 0) {
        currentMode = "INDIVIDUAL";
        if (controlModeCallback) controlModeCallback("INDIVIDUAL");
    } 
    else if (request.indexOf("GET /GAIT") >= 0) {
        currentMode = "GAIT";
        if (controlModeCallback) controlModeCallback("GAIT");
    }
    else if (request.startsWith("GET /")) {
        int spaceIdx = request.indexOf(' ', 5);
        if (spaceIdx < 0) return; 
        String command = request.substring(5, spaceIdx);
        if (command.length() > 0 && commandCallback) {
            commandCallback(command.c_str());
        }
    }
}

void WebServerControl::setControlModeCallback(void (*callback)(const char* mode)) {
    controlModeCallback = callback;
}

void WebServerControl::setCommandCallback(void (*callback)(const char* command)) {
    commandCallback = callback;
}
