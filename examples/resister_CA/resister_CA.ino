/**
 * @brief CM01-SARA-R (EN) AT Command Example
 * @file resister_CA.ino
 * @author Firstname Lastname
 * @date 2024-12-10
 * 
 * licesence: MIT
 */

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
 * @brief Registers a CA certificate to the modem.
 *
 * This function sends an AT command to the modem to register a CA certificate
 * with the specified name and PEM data. It sends the PEM data line by line
 * and waits for a response to confirm successful registration.
 *
 * @param certName The name of the certificate to be registered.
 * @param pemData The PEM formatted certificate data.
 * @param delayMs The delay in milliseconds between sending each line of PEM data.
 * @return true if the certificate is registered successfully, false otherwise.
 */
bool registerCaCertificate(const String &certName, const String &pemData, unsigned int delayMs = 0) {
  const char prompt = '>';
  String command = "AT+USECMNG=0,0,\"" + certName + "\"," + String(pemData.length());
  std::vector<String> responses;

  modem->setEnablePrompt(prompt);  
  if (modem->sendATCommandWithResponse(command, &responses, 5000)) {
    if (responses.back().indexOf(prompt) == -1) {
      Serial.println("Error: Expected '>' prompt not received.");
      return false;
    }
  } else {
    Serial.println("Error: Expected modem is not responding.");
  }
  modem->setDisablePrompt();

  Serial.println("Sending PEM data line by line...");

  int startIdx = 0;
  while (startIdx < pemData.length()) {
    int endIdx = pemData.indexOf('\n', startIdx);
    if (endIdx == -1) {
      endIdx = pemData.length();
    }
    String line = pemData.substring(startIdx, endIdx + 1);
    modem->sendStringData(line);
    Serial.print("Sent: ");
    Serial.print(line);

    delay(delayMs);

    startIdx = endIdx + 1;
  }

  Serial.println("PEM data sent. Awaiting response...");

  if (modem->getResponses(&responses, 5000)) {
    if (responses.back().indexOf("OK") != -1) {
     for (const auto& response : responses) {
          Serial.println(response);
      }
      return true;
    } else {
      return false;    
    }
  } else {
    return false;
  }
}

/**
 * @brief Setup function
 * 
 * Initializes the serial monitor and the modem. The modem is configured
 * with the specified pins and settings. The function sends an AT command
 * to the modem to verify that it is responding.
 */
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

/**
 * @brief Registers a sample CA certificate to the modem.
 *
 * This function sends an AT command to the modem to register a CA certificate
 * with the specified name and PEM data. It sends the PEM data line by line
 * and waits for a response to confirm successful registration.
 *
 * Please note that this is a sample code and should not be used in production
 * without proper security considerations. The certificate data is for sample
 * purposes only and should be replaced with appropriate CA certificate in
 * production environments.
 */
void loop() {
  
  // CA Certificate registration example
  // This certificate data is for sample purposes only. 
  // Please use an appropriate CA certificate in production environments.

  String certName = "sample_root_ca";
  String pemData = R"(-----BEGIN CERTIFICATE-----
MIIDfTCCAmWgAwIBAgIUHzPpdfKtqX/50iBK3BwAG2V3QRAwDQYJKoZIhvcNAQEL
BQAwTjELMAkGA1UEBhMCSlAxETAPBgNVBAgMCEt1bWFtb3RvMRIwEAYDVQQHDAlL
b3NoaS1zaGkxGDAWBgNVBAoMD0guSSBTeXN0ZWMgSW5jLjAeFw0yNDEyMTAwNjQ4
MzNaFw0yNTEyMTAwNjQ4MzNaME4xCzAJBgNVBAYTAkpQMREwDwYDVQQIDAhLdW1h
bW90bzESMBAGA1UEBwwJS29zaGktc2hpMRgwFgYDVQQKDA9ILkkgU3lzdGVjIElu
Yy4wggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDabMZmBcgGWgpP86Wj
P3dTsqkf2aKRaEHrZGKZ3fS0UqudUgAeGlIOvtyoKhDFneyON/KjH20GoSDJoT3d
AXgrJFMVoOfwOj1QSaWcTkpAtzfO+90bdm7X/NvYnQSdBwpKSBQcKs3g9ogm31m1
Wygm16Iz7nI0agZkvEXUG+7+/uF4ZJmVNsGqdMBAkdHv+HOfL1bDEWmtDKoZ474q
pWy5X6xXMN3KPkmm369NtSAASh0N9gbj5yRHnblAX5VyBgmCJPdeRRL2jHDGh1mB
rTTPF9hZ2AePOv+OdU1oECRRkp2uH710ljRENUQmTK4iUTLDO8W1qxeP6OiACST2
dreHAgMBAAGjUzBRMB0GA1UdDgQWBBTg30MeFl065OKY8pkikozbIXmMVDAfBgNV
HSMEGDAWgBTg30MeFl065OKY8pkikozbIXmMVDAPBgNVHRMBAf8EBTADAQH/MA0G
CSqGSIb3DQEBCwUAA4IBAQCRlQfsrLsP5d9clG43BqJz8q8r4qwUINnF9BVDSozQ
522bVpMeGB0QXh5QAFNsrsy7WzcChFnYoGC6WeMCmn//V5nyRq8BB4YtEVvcSI+L
U4JmTL+tOSAOIHeG0ZdozbMAeIE3NjueP07ZCiFJyh6BEBo8rgOxVsMv3sNHKfsE
+l86WqIcAFxOpXVrDJPkRkmdMdtHrK/w7qkwBBc8H80k6/QE/M20PDzgMxh6tqjb
dhTDq3nydW/hkXX7dvj2VLiS4Iwft+0seae/2if7/T5f8LaBIbgj5koHKxz6dhqV
wqKdNe51+h9piLpoBsFm37GraPev/mYQ4fAaR9S64PMG
-----END CERTIFICATE-----
)";

  bool success = registerCaCertificate(certName, pemData);
  if (success) {
    Serial.println("Certificate registered successfully!");
  } else {
    Serial.println("Certificate registration failed.");
  }

  while (true);
}