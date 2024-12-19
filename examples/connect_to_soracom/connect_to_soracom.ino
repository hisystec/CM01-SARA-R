/**
 * @file connect_to_soracom.ino
 * @brief Example program for CM01-SARA-R modem.
 * 
 * @author Hideshi Matsufuji
 * @date 2024-12-10
 * 
 * licesence: MIT
 */

#include <Arduino.h>
#include <CM01-SARA-R.h>

ModemHandler* modem;

#define MODEM_POWER 5                   // CM01-SARA-R (EN) Power enable pin
#define MODEM_PWR_ON 4                  // CM01-SARA-R (PWR_ON) Power on pin
#define MODEM_RX_PIN 16                 // CM01-SARA-R (RXD) RxD pin
#define MODEM_TX_PIN 17                 // CM01-SARA-R (RXD) TxD pin
#define MODEM_RTS_PIN 18                // CM01-SARA-R (RTS) RTS pin
#define MODEM_CTS_PIN 19                // CM01-SARA-R (CTS) CTS pin
#define USE_HARDWARE_FLOW_CONTROL true  // Enable hardware flow control
const int BAUD_RATE = 115200;           // baud rate
const String APN = "soracom.io";        // APN

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

void setup() {
 
  // Initializing serial monitor
  Serial.begin(115200);
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
  const long timeout = 5000;
  std::vector<String> responses;
  String asyncResponse;
  modem->enableDebugMode();
  modem->sendATCommandWithResponse("AT+CFUN=0", &responses, timeout);
  modem->sendATCommandWithResponse("AT+CGDCONT=1,\""IPV4V6\",\"" + APN + "\"", &responses, timeout);
  modem->sendATCommandWithResponse("AT+CFUN=1", &responses, timeout);
  modem->sendATCommandWithResponse("AT+UPSD=0,0,0", &responses, timeout);
  modem->sendATCommandWithResponse("AT+UPSD=0,1,\"" + APN + "\"", &responses, timeout);
  modem->sendATCommandWithResponse("AT+UPSDA=0,3", &responses, timeout);
  waitAsyncEvent("+UUPSDA:", timeout);
  if (responses.back() == "OK") {
    modem->sendATCommandWithResponse("AT+CMEE=2", &responses, timeout);
    modem->sendATCommandWithResponse("AT+UHTTP=0", &responses, timeout);
    modem->sendATCommandWithResponse("AT+UHTTP=0,1,\"hi-corp.net\"", &responses, timeout);
    modem->sendATCommandWithResponse("AT+UHTTP=0,6,1", &responses, timeout);
    modem->sendATCommandWithResponse("AT+UHTTPC=0,1,\"/\",\"get.ffs\"", &responses, timeout);
    asyncResponse = waitAsyncEvent("+UUHTTPCR:", timeout);
    if (asyncResponse == "+UUHTTPCR: 0,1,1") {
      modem->sendATCommandWithResponse("AT+URDFILE=\"get.ffs\"", &responses, timeout);
      modem->sendATCommandWithResponse("AT+UDELFILE=\"get.ffs\"", &responses, timeout);
      Serial.println("Completed.");
    } else {
      modem->sendATCommandWithResponse("AT+UHTTPER=0", &responses, timeout);
      if (responses[1] == "+UHTTPER: 0,3,73") {
        Serial.println("The CA file required to access https://hi-corp.net/ is missing.");
        Serial.println("Please obtain the ISRG Root X1 CA file from a trusted source and install it on this modem.");
        Serial.println("You can download it from https://letsencrypt.org/certs/isrgrootx1.pem");
      } else {
        Serial.println("Unknown error.");
      }
    }  
  }

  while (true) ;
}
