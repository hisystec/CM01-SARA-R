/**
 * @file http_post.ino
 * @brief HTTP POST example program for CM01-SARA-R modem.
 *
 * @author Hideshi Matsufuji
 * @date 2026-06-04
 *
 * licesence: MIT
 */

#include <Arduino.h>
#include <CM01-SARA-R.h>

ModemHandler* modem;

#define MODEM_POWER 5                   // CM01-SARA-R (EN) Power enable pin
#define MODEM_PWR_ON 4                  // CM01-SARA-R (PWR_ON) Power on pin
#define MODEM_RX_PIN 16                 // CM01-SARA-R (RXD) RxD pin
#define MODEM_TX_PIN 17                 // CM01-SARA-R (TXD) TxD pin
#define MODEM_RTS_PIN 18                // CM01-SARA-R (RTS) RTS pin
#define MODEM_CTS_PIN 19                // CM01-SARA-R (CTS) CTS pin
#define USE_HARDWARE_FLOW_CONTROL true  // Enable hardware flow control

const int BAUD_RATE = 115200;           // baud rate
const String APN = "soracom.io";        // APN

// HTTPS endpoint. Install the required CA certificate on the modem before
// running this example.
const String HTTP_SERVER = "hi-corp.net";
const String HTTP_PATH = "/";
const String HTTP_POST_FILE = "postfile";
const String HTTP_RESPONSE_FILE = "response";
const int HTTP_PORT = 443;
const bool HTTP_USE_TLS = true;
const int HTTP_CONTENT_TYPE_JSON = 4;

void onAsyncResponse(const String& response) {
  Serial.print("Async Response Received: ");
  Serial.println(response);
}

String waitAsyncEvent(const String& asyncEvent, int timeoutMs) {
  String asyncResponse;
  while (modem->getAsyncEvent(asyncResponse, timeoutMs)) {
    asyncResponse.trim();
    if (asyncResponse.indexOf(asyncEvent) != -1) {
      Serial.println("[Match Found]: " + asyncResponse);
      return asyncResponse;
    } else {
      Serial.println("[Skip Unrelated Response]: " + asyncResponse);
    }
  }
  return "";
}

bool sendCommandOk(const String& command, int timeoutMs) {
  std::vector<String> responses;
  if (!modem->sendATCommandWithResponse(command, &responses, timeoutMs)) {
    Serial.println("Command timeout: " + command);
    return false;
  }

  if (responses.empty() || responses.back() != "OK") {
    Serial.println("Command failed: " + command);
    for (const auto& response : responses) {
      Serial.println(response);
    }
    return false;
  }

  return true;
}

bool activatePdpContext(int timeoutMs) {
  if (!sendCommandOk("AT+CFUN=0", timeoutMs)) return false;
  if (!sendCommandOk("AT+CGDCONT=1,\"IPV4V6\",\"" + APN + "\"", timeoutMs)) return false;
  if (!sendCommandOk("AT+CFUN=1", timeoutMs)) return false;
  if (!sendCommandOk("AT+UPSD=0,0,0", timeoutMs)) return false;
  if (!sendCommandOk("AT+UPSD=0,1,\"" + APN + "\"", timeoutMs)) return false;
  if (!sendCommandOk("AT+UPSDA=0,3", timeoutMs)) return false;

  String asyncResponse = waitAsyncEvent("+UUPSDA:", timeoutMs);
  return asyncResponse.indexOf("+UUPSDA: 0,\"") != -1;
}

bool uploadPostFile(const String& payload, int timeoutMs) {
  std::vector<String> responses;
  String uploadCommand = "AT+UDWNFILE=\"" + HTTP_POST_FILE + "\"," + String(payload.length());

  modem->setEnablePrompt('>');
  if (!modem->sendATCommandWithResponse(uploadCommand, &responses, timeoutMs)) {
    Serial.println("AT+UDWNFILE failed.");
    modem->setDisablePrompt();
    return false;
  }

  bool promptSeen = false;
  for (const auto& response : responses) {
    if (response.indexOf('>') != -1) {
      promptSeen = true;
      break;
    }
  }

  if (!promptSeen) {
    Serial.println("Upload prompt not received.");
    modem->setDisablePrompt();
    return false;
  }

  modem->sendStringData(payload);
  modem->setDisablePrompt();

  if (!modem->getResponses(&responses, timeoutMs)) {
    Serial.println("Timed out waiting for upload confirmation.");
    return false;
  }

  for (const auto& response : responses) {
    if (response.indexOf("ERROR") != -1) {
      Serial.println("Upload failed: " + response);
      return false;
    }
    if (response == "OK") {
      return true;
    }
  }

  Serial.println("Upload not acknowledged.");
  return false;
}

bool httpPostJson(const String& jsonPayload, int timeoutMs) {
  String asyncResponse;
  String command;

  if (!sendCommandOk("AT+CMEE=2", timeoutMs)) return false;
  if (!sendCommandOk("AT+UHTTP=0", timeoutMs)) return false;
  if (!sendCommandOk("AT+UHTTP=0,1,\"" + HTTP_SERVER + "\"", timeoutMs)) return false;
  if (!sendCommandOk("AT+UHTTP=0,5," + String(HTTP_PORT), timeoutMs)) return false;
  if (!sendCommandOk("AT+UHTTP=0,6," + String(HTTP_USE_TLS ? 1 : 0), timeoutMs)) return false;
  if (!uploadPostFile(jsonPayload, timeoutMs)) return false;

  command = "AT+UHTTPC=0,4,\"" + HTTP_PATH + "\",\"" + HTTP_RESPONSE_FILE + "\",\"" +
            HTTP_POST_FILE + "\"," + String(HTTP_CONTENT_TYPE_JSON);
  if (!sendCommandOk(command, timeoutMs)) return false;

  asyncResponse = waitAsyncEvent("+UUHTTPCR:", timeoutMs);
  if (asyncResponse.indexOf("+UUHTTPCR: 0,4,1") != -1) {
    sendCommandOk("AT+URDFILE=\"" + HTTP_RESPONSE_FILE + "\"", timeoutMs);
    sendCommandOk("AT+UDELFILE=\"" + HTTP_POST_FILE + "\"", timeoutMs);
    sendCommandOk("AT+UDELFILE=\"" + HTTP_RESPONSE_FILE + "\"", timeoutMs);
    return true;
  }

  Serial.println("HTTP POST failed.");
  std::vector<String> responses;
  if (modem->sendATCommandWithResponse("AT+UHTTPER=0", &responses, timeoutMs)) {
    for (const auto& response : responses) {
      Serial.println(response);
    }
    if (responses.size() > 1 && responses[1] == "+UHTTPER: 0,3,73") {
      Serial.println("The CA file required to access https://hi-corp.net/ is missing.");
      Serial.println("Please obtain the ISRG Root X1 CA file from a trusted source and install it on this modem.");
      Serial.println("You can download it from https://letsencrypt.org/certs/isrgrootx1.pem");
    }
  }
  sendCommandOk("AT+UDELFILE=\"" + HTTP_POST_FILE + "\"", timeoutMs);
  sendCommandOk("AT+UDELFILE=\"" + HTTP_RESPONSE_FILE + "\"", timeoutMs);
  return false;
}

void setup() {
  // Initializing serial monitor
  Serial.begin(BAUD_RATE);
  delay(1000);

  // Initializing modem
  Serial.println("Initializing modem...");
  modem = new ModemHandler(Serial2);
  modem->setPins(
    MODEM_POWER,
    MODEM_PWR_ON,
    MODEM_RX_PIN,
    MODEM_TX_PIN,
    MODEM_RTS_PIN,
    MODEM_CTS_PIN,
    USE_HARDWARE_FLOW_CONTROL);
  modem->setAsyncResponsePrefixes({"+UFOTASTAT:", "+ULWM2MSTAT:", "+UUPSDA:", "+UUSIMSTAT:", "+UUHTTPCR:"});
  modem->setResponseEndCriteria({"OK", "ERROR", "+CME ERROR:*", "+CMS ERROR:*"});
  modem->setAsyncCallback(onAsyncResponse);
  modem->begin();

  std::vector<String> responses;
  if (modem->sendATCommandWithResponse("AT", &responses, 5000)) {
    Serial.println("Modem initialized successfully!");
  } else {
    Serial.println("Modem initialization failed.");
    delay(30000L);
    return;
  }
}

void loop() {
  const long timeout = 30000;
  String payload = "{\"uptime_ms\":" + String(millis()) + ",\"message\":\"CM01-SARA-R HTTP POST\"}";

  modem->enableDebugMode();

  if (!activatePdpContext(timeout)) {
    Serial.println("Failed to activate PDP context.");
    while (true);
  }

  if (httpPostJson(payload, timeout)) {
    Serial.println("HTTP POST completed.");
  } else {
    Serial.println("HTTP POST was not completed.");
  }

  while (true);
}
