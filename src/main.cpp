#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <Button2.h>
#include <mbedtls/md.h>
#include "Base64.h"

//Define WiFi settings
const char* ssid = "SSIDGOESHERE";
const char* password = "SSIDPASSGOESHERE";

//Define OBS information
const char* obsIP = "OBS IP ADDRESS GOES HERE";         // OBS computer
const int obsPort = 4455;                   // OBS Websocket port
const String obsPassword = "OBS WEBSOCKET PASSWORD GOES HERE"; // OBS Websocket Password

//Define Buttons
#define BUTTON1_PIN 4
#define BUTTON2_PIN 22
Button2 button_1, button_2;

unsigned long previousMillis = 0;
const long interval = 5000;

JsonDocument doc;

WebSocketsClient webSocket;

String concatenateAndHashAndEncode(const String& str1, const String& str2) {
    // Thanks ChatGPT!
    // Concatenate two strings
    String concatenatedStr = str1 + str2;

    // Initialize SHA256 context
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);

    // Select the SHA256 digest
    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_setup(&ctx, md_info, 0);

    // Calculate the SHA256 hash
    mbedtls_md_starts(&ctx);
    mbedtls_md_update(&ctx, (const unsigned char*)concatenatedStr.c_str(), concatenatedStr.length());
    unsigned char hash[MBEDTLS_MD_MAX_SIZE];
    mbedtls_md_finish(&ctx, hash);
    mbedtls_md_free(&ctx);

    // Encode the binary hash to base64
    String base64Encoded = base64::encode(hash, mbedtls_md_get_size(md_info));

    return base64Encoded;
}

void ParseOBSResponse(char *payload)
{
  DeserializationError error = deserializeJson(doc, payload);

  // Test if parsing succeeds.
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }

  int op = doc["op"];
  JsonVariant auth;
  JsonDocument responsedoc;
  std::string output;
  std::string secret_string;
  const char* secret_char;
  unsigned char shaResult[32];



  switch (op) {

    case 0:
      //Hello
      auth = doc["d"]["authentication"];
      String authenticationString;
      if (!auth.isNull()) {
        Serial.println("authentication required");
        //Concat password and salt 
        //Generate an SHA256 binary hash of the result and base64 encode it, known as a base64 secret.
        String base64Secret = concatenateAndHashAndEncode(obsPassword, doc["d"]["authentication"]["salt"]);
        Serial.print("base64 Secret: ");
        Serial.println(base64Secret);
        authenticationString = concatenateAndHashAndEncode(base64Secret, doc["d"]["authentication"]["challenge"]);
        Serial.print("Authentication string: ");
        Serial.println(authenticationString);
      }
      responsedoc["op"] = 1;
      responsedoc["d"]["rpcVersion"] = 1;
      responsedoc["d"]["authentication"] = authenticationString;
      responsedoc["d"]["eventSubscriptions"] = 2;
      serializeJson(responsedoc, output);
      Serial.println(output.c_str());
      webSocket.sendTXT(output.c_str());
      break;
    /*
    case 1:
      //Identify
      //we should never recieve this, opcode for sending identify
    break;
    case 2:
      //we were authenticated
      // change this to maybe set some flag saying that we're all good?
      responsedoc["op"] = 6;
      responsedoc["d"]["requestType"] = "GetCurrentProgramScene";
      responsedoc["d"]["requestId"] = "";
      serializeJson(responsedoc, output);
      Serial.println(output);
      webSocket.sendTXT(output);
    break;
    case 3:
      //ReIdentify
      //we should never recieve this, opcode for sending reidentify
    break;
    case 5:
      //Event
    break;
    case 6:
      //Request
      //we should never recieve this, opcode for sending request
      //got the last scene, now change it
      responsedoc["op"] = 6;
      //JsonObject d2 = scenedoc.createNestedObject("d");
      responsedoc["d"]["requestType"] = "SetCurrentProgramScene";
      responsedoc["d"]["requestId"] = "";
      responsedoc["d"]["requestData"]["sceneName"] = "Scene 2";
      serializeJson(responsedoc, output);
      Serial.println(output);
      webSocket.sendTXT(output);
    break;
    case 7:
    //RequestResponse
    break;
    case 8:
      //RequestBatch
      //we should never recieve this, opcode for sending request batches
    break;
    case 9:
    //RequestBatchResponse
    break;
    */       
  }
}


void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {

	switch(type) {
		case WStype_DISCONNECTED:
			Serial.println("[WSc] Disconnected!\n");
			break;
		case WStype_CONNECTED:
			Serial.printf("[WSc] Connected to url: %s\n", payload);
			break;
		case WStype_TEXT:
			Serial.printf("[WSc] get text: %s\n", payload);
      ParseOBSResponse((char *)payload);
			break;
		case WStype_BIN:
		case WStype_ERROR:
      Serial.printf("[WSc] got error: %s\n", payload);
      break;		
		case WStype_FRAGMENT_TEXT_START:
		case WStype_FRAGMENT_BIN_START:
		case WStype_FRAGMENT:
		case WStype_FRAGMENT_FIN:
		break;
	}

}

void handleTap(Button2& b) {
  //Deal with buttonpresses
  Serial.println("button pressed");
  Serial.print("click ");
  Serial.print("on button #");
  Serial.print((b == button_1) ? "1" : "2");
  Serial.println();
  String scene = ((b == button_1) ? "Scene" : "Scene 2");
  //Change the Scene
  JsonDocument responsedoc;
  String output;
  responsedoc["op"] = 6;
  responsedoc["d"]["requestType"] = "SetCurrentProgramScene";
  responsedoc["d"]["requestId"] = "";
  responsedoc["d"]["requestData"]["sceneName"] = scene;
  serializeJson(responsedoc, output);
  Serial.println(output);
  webSocket.sendTXT(output);
}

void setup() {
  Serial.begin(9600);

  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while(WiFi.status() != WL_CONNECTED) { 
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());

	// server address, port and URL
  webSocket.begin(obsIP, obsPort, "/");

	// event handler
	webSocket.onEvent(webSocketEvent);

	// try every 5000 again if connection has failed
	webSocket.setReconnectInterval(5000);

  button_1.begin(BUTTON1_PIN);
  button_2.begin(BUTTON2_PIN);
  button_1.setTapHandler(handleTap);
  button_2.setTapHandler(handleTap);

}

void loop() {
  unsigned long currentMillis = millis();
  button_1.loop();
  button_2.loop();
  if(currentMillis - previousMillis >= interval) {
     // Check WiFi connection status
    if(WiFi.status()== WL_CONNECTED ){
      webSocket.loop();
    }
    else {
      Serial.println("WiFi Disconnected");
      // Try reconnecting
      WiFi.begin(ssid, password);
      Serial.println("Connecting");
      while(WiFi.status() != WL_CONNECTED) { 
        delay(500);
        Serial.print(".");
      }
      Serial.println("");
      Serial.print("Connected to WiFi network with IP Address: ");
      Serial.println(WiFi.localIP());
    }
  }
}