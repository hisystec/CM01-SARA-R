/**
 * @file remove_CA.ino
 * @brief Example code for removing a CA file from the modem.
 * 
 * @author Hideshi Matsufuji
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
 * @brief Lists and selects CA files for deletion.
 *
 * This function retrieves a list of registered CA files from the modem
 * by sending an AT command. It parses the response to extract file names,
 * descriptions, and expiry dates, and displays them to the user. The user
 * can select a CA file to delete, and the function will prompt for 
 * confirmation before deletion. If no CA files are found, it notifies the user.
 *
 * @return true if a CA file is successfully selected and deleted, false otherwise.
 */
bool listAndSelectCAFiles() {
  std::vector<String> responses;
  if (modem->sendATCommandWithResponse("AT+USECMNG=3", &responses, 5000)) {
    if (responses.back().indexOf("OK") == -1) {
       Serial.println("Failed to retrieve CA file list.");
      return false;    
    }
  }

  Serial.println("Registered CA Files:");
  int index = 1;
  struct CAEntry {
      String fileName;
      String description;
      String expiry;
  };
  std::vector<CAEntry> caList;

  for (const auto& response : responses) {
    if ((response.indexOf("\"CA\",")) != -1) {

      int fileNameStart = response.indexOf("\",\"") + 3;
      int descStart = response.indexOf("\",\"", fileNameStart) + 3;
      int expiryStart = response.indexOf("\",\"", descStart) + 3;
      int expiryEnd = response.indexOf("\"", expiryStart);

      if (fileNameStart == 2 || descStart == 2 || expiryStart == 2 || expiryEnd == -1) {
        Serial.println("Error parsing line: " + response);
        continue;
      }

      String fileName = response.substring(fileNameStart, descStart - 3);
      String description = response.substring(descStart, expiryStart - 3);
      String expiry = response.substring(expiryStart, expiryEnd);

      caList.push_back({fileName, description, expiry});
      Serial.printf("%d. %s (%s) - Expiry: %s\n", index++, description.c_str(), fileName.c_str(), expiry.c_str());
    }
  }
  
  if (caList.empty()) {
    Serial.println("No CA files found.");
    delay(30000);
    return false;
  }

  Serial.println("Enter the number of the CA file to delete (or 0 to exit):");
  while (true) {
    if (Serial.available()) {
      String input = Serial.readStringUntil('\n');
      input.trim();
      int choice = input.toInt();

      if (choice == 0) {
        Serial.println("Exiting...");
        return false;
      } else if (choice > 0 && choice <= caList.size()) {
        const CAEntry &selectedCA = caList[choice - 1];

        Serial.printf("Are you sure you want to delete \"%s\" (%s)? (y/n): ", 
                      selectedCA.fileName.c_str(), 
                      selectedCA.description.c_str());
        while (true) {
          if (Serial.available()) {
            String confirmation = Serial.readStringUntil('\n');
            confirmation.trim();
            if (confirmation.equalsIgnoreCase("y")) {
              Serial.println("");
              deleteCAFile(selectedCA.fileName);
              return true;
            } else if (confirmation.equalsIgnoreCase("n")) {
              Serial.println("Deletion canceled.");
              return false;
            } else {
              Serial.println("Invalid input. Please enter 'y' or 'n':");
            }
          }
      }
      } else {
        Serial.println("Invalid choice. Please try again.");
      }
    }
  }
  return true;
}

/**
 * @brief Deletes a CA file from the modem.
 *
 * This function sends an AT command to the modem to delete a CA file with the
 * specified name. It sends the command and waits for a response to confirm
 * successful deletion.
 *
 * @param filename The name of the CA file to delete.
 * @return true if the file is deleted successfully, false otherwise.
 */
bool deleteCAFile(const String &filename) {
  std::vector<String> responses;
  
  if (filename.startsWith("ubx_")) {
    Serial.println("Cannot delete pre-installed file: " + filename);
    return false;
  }

  String command = "AT+USECMNG=2,0,\"" + filename + "\"";

  if (modem->sendATCommandWithResponse(command, &responses, 5000)) {
    if (responses.back().indexOf("OK") != -1) {
      Serial.println("File deleted successfully: " + filename);
      return true;
    } else {
      Serial.println("Failed to delete file: " + filename);
      return false;    
    }
  } else {
    Serial.println("Error: Expected timeout.");
    return false;
  }
}

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
    Serial.println("Error: Modem is not responding.");
    return false;
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
 * @brief Interactively adds a new CA file to the modem.
 *
 * This function interacts with the user to obtain the name of the CA file and
 * the PEM data for the CA. It then sends the AT command to register the CA
 * file with the specified name and PEM data. It waits for a response to
 * confirm successful registration.
 *
 * @return true if the CA file is registered successfully, false otherwise.
 */
bool addCAFile() {
  Serial.println("Enter the name for the CA file:");
  while (!Serial.available());
  String certName = Serial.readStringUntil('\n');
  certName.trim();

  Serial.println("Enter the PEM data for the CA file (end with an empty line):");
  String pemData;
  while (true) {
    while (!Serial.available());
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.isEmpty()) break;
    Serial.println("Receipt: " + line);
    pemData += line + "\n";
  }

  if (registerCaCertificate(certName, pemData)) {
    Serial.println("CA file registered successfully.");
    return true;
  } else {
    Serial.println("Failed to register CA file.");
    return false;
  }
}

/**
 * @brief Provides an interactive menu for managing CA files.
 *
 * This function displays an interactive menu to the user, allowing them to
 * choose between different options for managing CA files. The user can list
 * and delete existing CA files, add a new CA file, or exit the program. The
 * function continuously loops until the user chooses to exit.
 */
void interactiveMenu() {
  while (true) {
    Serial.println("\nChoose an option:");
    Serial.println("1. List and delete CA files");
    Serial.println("2. Add a new CA file");
    Serial.println("0. Exit");

    while (!Serial.available());
    String input = Serial.readStringUntil('\n');
    input.trim();
    int choice = input.toInt();

    switch (choice) {
      case 1:
        listAndSelectCAFiles();
        break;
      case 2:
        addCAFile();
        break;
      case 0:
        Serial.println("Exiting...");
        return;
      default:
        Serial.println("Invalid option. Please try again.");
    }
  }
}

/**
 * @brief The main loop of the program.
 *
 * This loop continuously displays the interactive menu to the user and
 * waits for them to choose an option. The loop runs indefinitely until the
 * user chooses to exit the program.
 */
void loop() {
  interactiveMenu();
  delay(1000);
}
