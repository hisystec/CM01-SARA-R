/**
 * @file AT_Debug.ino
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

/**
 * @brief Callback function for asynchronous response from the modem.
 * 
 * This function is called when an asynchronous response is received from the
 * modem. The response is printed to the serial console.
 * 
 * @param response The asynchronous response received from the modem.
 */
void onAsyncResponse(const String& response) {
    Serial.print("Async Response Received: ");
    Serial.println(response);
}

/**
 * @brief Sets up the serial communication and initializes the modem.
 * 
 * This function configures the serial monitor for communication and initializes 
 * the modem by setting the necessary pins, asynchronous response prefixes, and 
 * response end criteria. It also registers a callback function for handling 
 * asynchronous responses and starts the modem. Finally, it sends an AT command 
 * to confirm the modem connection status.
 */
void setup() {
 
  // Initializing serial monitor
  Serial.begin(115200);
  delay(1000);

  // Initializing modem
  Serial.println("Initializing modem...");
  modem = new ModemHandler(Serial2);
  modem->disableDebugMode();
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
      Serial.println("You can send AT commands now...");
  } else {
      Serial.println("Failed to open modem.");
  }
}

/**
 * @brief The main loop of the program.
 * 
 * In this loop, the program will wait for user input from the serial console.
 * The user can enter any valid AT command to send to the modem. The program
 * will then send the command to the modem and wait for the response. The
 * response from the modem is stored in a vector and then printed to the
 * serial console.
 */
void loop() {
  std::vector<String> responses;
  String command = Serial.readStringUntil('\n');
  command.trim();
  if (command != "") {
    if (modem->sendATCommandWithResponse(command, &responses, 5000)) {
        for (const auto& response : responses) {
            Serial.println(response);
        }
    } else {
        Serial.println("Failed to receive full response.");
    }
  }
  delay(0);
}