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
 * @brief Initializes the serial communication and the modem.
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
  modem->setPins(
    MODEM_POWER,
    MODEM_PWR_ON,
    MODEM_RX_PIN,
    MODEM_TX_PIN,
    MODEM_RTS_PIN,
    MODEM_CTS_PIN,
    USE_HARDWARE_FLOW_CONTROL);
  modem->setAsyncResponsePrefixes({"+UFOTASTAT:", "+ULWM2MSTAT:", "+UUPSDA:", "+UUSIMSTAT:"});
  modem->setResponseEndCriteria({"OK", "ERROR", "+CME ERROR:*", "+CMS ERROR:*"});  
  modem->begin();

  modem->sendATCommand("AT");
  std::vector<String> responses;
  if (modem->getResponses(&responses, 5000)) {
    Serial.println("Modem initialized successfully!");
  } else {
    Serial.println("Modem initialization failed.");
    delay(30000L);
    return;
  }
}

/**
 * @brief Deletes a CA file stored in the modem's storage.
 * 
 * This function sends the AT+USECMNG command to the modem to delete a CA file
 * from the modem's storage. The function will not delete pre-installed files
 * (i.e., files starting with "ubx_"). If the file is deleted successfully, the
 * function returns true. Otherwise, the function returns false and prints an
 * error message to the serial monitor.
 * 
 * @param filename The filename of the CA file to delete.
 * @return true if the file is deleted successfully, false otherwise.
 */
bool deleteCAFile(const String &filename) {
  std::vector<String> responses;
    
  if (filename.startsWith("ubx_")) {
    Serial.println("Cannot delete pre-installed file: " + filename);
    return false;
  }

  String command = "AT+USECMNG=2,0,\"" + filename + "\"";
  modem->sendATCommand(command);

  if (modem->getResponses(&responses, 5000)) {
    if (responses.back().indexOf("OK") != -1) {
      Serial.println("File deleted successfully: " + filename);
      for (const auto& response : responses) {
        Serial.println(response);
      }
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
 * @brief Retrieves a list of registered CA files from the modem and allows the user to select one for deletion.
 * 
 * This function sends the AT+USECMNG command to the modem to retrieve a list of CA files. The function then parses the
 * list and displays it to the user. The user is prompted to select a CA file to delete by entering the corresponding
 * number. If the user selects a CA file, the function will send the AT+USECMNG command to delete the selected CA file.
 * If the file is deleted successfully, the function returns true. Otherwise, the function returns false and prints an
 * error message to the serial monitor.
 * 
 * @return true if a CA file is deleted successfully, false otherwise.
 */
bool listAndSelectCAFiles() {
  std::vector<String> responses;
  modem->sendATCommand("AT+USECMNG=3");
  if (modem->getResponses(&responses, 5000)) {
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
 * @brief The main loop of the program.
 * 
 * This function is an infinite loop that calls listAndSelectCAFiles every second to
 * display the list of registered CA files and allow the user to select one for deletion.
 * The function then waits 1 second before repeating the process.
 */
void loop() {
  listAndSelectCAFiles();
  delay(1000);
}