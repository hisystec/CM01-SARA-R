/**
 * @file CM01-SARA-R.cpp
 * @brief Implementation of ModemHandler class for CM01-SARA-R modem.
 * 
 * @author Hideshi Matsufuji
 * @date 2024-12-10
 * 
 * licesence: MIT
 *
 */

#include <CM01-SARA-R.h>

/**
 * @brief Constructs a ModemHandler object.
 * 
 * This constructor initializes the ModemHandler with the specified
 * serial port and queue sizes for response and asynchronous events.
 * It sets up the serial communication and prepares the necessary
 * queues for handling modem responses and events.
 * 
 * @param serialPort Reference to the HardwareSerial object for communication.
 * @param responseQueueSize The size of the queue for storing response strings.
 * @param asyncQueueSize The size of the queue for storing asynchronous event strings.
 */
ModemHandler::ModemHandler(HardwareSerial& serialPort, int responseQueueSize, int asyncQueueSize)
    : serial(&serialPort), buffer(""), asyncCallback(nullptr) {
    responseQueue = xQueueCreate(responseQueueSize, sizeof(String*));
    asyncEventQueue = xQueueCreate(asyncQueueSize, sizeof(String*));
}

/**
 * @brief Initializes the modem and starts the task for reading from the modem.
 *
 * This function initializes the modem by calling `powerOnModem` and
 * `initSerial`, then disables the prompt character and starts the task that
 * reads from the modem. The task is created with a stack size of 4096 bytes
 * and a priority of 1, and is pinned to core 1. The function then waits for
 * 6 seconds before returning.
 */
void ModemHandler::begin() {
    powerOnModem();
    initSerial();
    setDisablePrompt();
    xTaskCreatePinnedToCore(readFromModemTask, "ReadModemTask", 4096, this, 1, NULL, 1);
    delay(6000);
}

/**
 * @brief Enables the prompt character.
 *
 * This function sets the prompt character and enables prompt mode. In
 * prompt mode, the modem will send the prompt character at the end of
 * each response. The task that reads from the modem will then detect
 * the prompt character and consider it as the end of the response.
 *
 * @param chr The prompt character.
 */
void ModemHandler::setEnablePrompt(char chr) {
  this->promptCharacter = chr;
  this->enablePrompt = true;
}

/**
 * @brief Disables the prompt character.
 *
 * This function disables prompt mode by setting the prompt character to 0.
 * When prompt mode is disabled, the task that reads from the modem will not
 * detect the prompt character and will not consider it as the end of the
 * response.
 */
void ModemHandler::setDisablePrompt() {
  this->enablePrompt = false;
}

/**
 * @brief Sets the pins for communication with the modem.
 *
 * This function sets the pins used for communication with the modem.
 * The pins are:
 *  - powerPin: The pin used to power on the modem.
 *  - pwrOnPin: The pin used to turn on the modem.
 *  - rxPin: The pin used for receiving data from the modem.
 *  - txPin: The pin used for sending data to the modem.
 *  - rtsPin: The pin used for RTS (Request to Send) control.
 *  - ctsPin: The pin used for CTS (Clear to Send) control.
 *  - useFlowControl: A boolean indicating whether to use hardware flow
 *    control. If true, the RTS and CTS pins are used to control the flow
 *    of data.
 */
void ModemHandler::setPins(int powerPin, int pwrOnPin, int rxPin, int txPin,
                           int rtsPin, int ctsPin, bool useFlowControl) {
    this->powerPin = powerPin;
    this->pwrOnPin = pwrOnPin;
    this->rxPin = rxPin;
    this->txPin = txPin;
    this->rtsPin = rtsPin;
    this->ctsPin = ctsPin;
    this->useFlowControl = useFlowControl;
}

/**
 * @brief Sends an AT command to the modem.
 *
 * This function sends an AT command to the modem. The command is sent as a
 * string followed by a newline character. The function does not wait for a
 * response from the modem.
 *
 * @param command The AT command to send to the modem.
 */
void ModemHandler::sendATCommand(const String& command) {
    serial->println(command);
}

/**
 * @brief Sends string data to the modem.
 *
 * This function sends the provided string data to the modem using
 * the serial connection. Unlike AT commands, this data is sent
 * without appending a newline character at the end.
 *
 * @param data The string data to send to the modem.
 */
void ModemHandler::sendStringData(const String& data) {
    serial->print(data);
}

/**
 * @brief Waits for a response from the modem.
 *
 * This function waits for a response from the modem. The response is stored
 * in the provided string reference. The function will wait for the specified
 * timeout period before returning. If a response is received within the
 * timeout period, the function returns true. If no response is received
 * or the timeout period is reached, the function returns false.
 *
 * @param response The string reference to store the response in.
 * @param timeoutMs The timeout period in milliseconds.
 * @return true if a response is received within the timeout period, false
 *    otherwise.
 */
bool ModemHandler::getResponse(String& response, int timeoutMs) {
    String* responsePtr = nullptr;
    if (xQueueReceive(responseQueue, &responsePtr, pdMS_TO_TICKS(timeoutMs)) == pdTRUE) {
        response = *responsePtr;
        delete responsePtr;
        return true;
    }
    return false;
}

/**
 * @brief Waits for an asynchronous event from the modem.
 *
 * This function waits for an asynchronous event from the modem. The event is
 * stored in the provided string reference. The function will wait for the
 * specified timeout period before returning. If an event is received within
 * the timeout period, the function returns true. If no event is received or
 * the timeout period is reached, the function returns false.
 *
 * @param event The string reference to store the event in.
 * @param timeoutMs The timeout period in milliseconds.
 * @return true if an event is received within the timeout period, false
 *    otherwise.
 */
bool ModemHandler::getAsyncEvent(String& event, int timeoutMs) {
    String* eventPtr = nullptr;
    if (xQueueReceive(asyncEventQueue, &eventPtr, pdMS_TO_TICKS(timeoutMs)) == pdTRUE) {
        event = *eventPtr;
        delete eventPtr;
        return true;
    }
    return false;
}

/**
 * @brief Sets the prefixes for asynchronous responses.
 *
 * This function sets the prefixes for asynchronous responses from the modem.
 * When an asynchronous response is received, the task that reads from the
 * modem will check if the response starts with any of the prefixes
 * specified in this function. If a match is found, the response is
 * considered an asynchronous event and is stored in the asynchronous
 * event queue. Otherwise, the response is considered a synchronous
 * response and is stored in the response queue.
 *
 * @param prefixes The vector of prefixes to match asynchronous responses
 *    against.
 */
void ModemHandler::setAsyncResponsePrefixes(const std::vector<String>& prefixes) {
    asyncResponsePrefixes = prefixes;
}

/**
 * @brief Powers on the modem.
 *
 * This function configures the necessary pins for powering on the modem.
 * It sets the power and power-on pins to the appropriate modes and states
 * to initiate the modem power sequence. A series of digital writes and delays
 * are used to ensure the modem is powered on correctly.
 */
void ModemHandler::powerOnModem() {
    pinMode(powerPin, OUTPUT);
    digitalWrite(powerPin, HIGH);
    pinMode(pwrOnPin, OUTPUT_OPEN_DRAIN);
    digitalWrite(pwrOnPin, HIGH);
    delay(500);
    digitalWrite(pwrOnPin, LOW);
    delay(500);
    digitalWrite(pwrOnPin, HIGH);
}

/**
 * @brief Initializes the serial communication for the modem.
 *
 * This function initializes the serial communication with the modem using
 * the provided pins and baud rate. If hardware flow control is enabled, the
 * function configures the UART to use hardware flow control with the RTS and
 * CTS pins. Otherwise, the RTS pin is set to output mode and set low.
 */
void ModemHandler::initSerial() {
    serial->begin(115200, SERIAL_8N1, rxPin, txPin);
    if (useFlowControl) {
        uart_config_t uart_config = {
            .baud_rate = 115200,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
            .rx_flow_ctrl_thresh = 122,
        };
        uart_param_config(UART_NUM_2, &uart_config);
        uart_set_pin(UART_NUM_2, txPin, rxPin, rtsPin, ctsPin);
    } else {
        pinMode(rtsPin, OUTPUT);
        digitalWrite(rtsPin, LOW);
    }
}


/**
 * @brief Reads from the modem and processes lines.
 *
 * This function is a task that runs indefinitely and reads from the
 * modem. It processes each line of input from the modem by checking
 * for the prompt character and then calling the processLine function
 * to handle the line. The processLine function can then process the
 * line as an asynchronous event or a synchronous response.
 *
 * @param param The ModemHandler object to read from and process lines for.
 */
void ModemHandler::readFromModemTask(void* param) {
    ModemHandler* handler = static_cast<ModemHandler*>(param);
    while (true) {
        while (handler->serial->available()) {
            char c = handler->serial->read();
            if (c == '\r' || c == '\n') {
                if (!handler->buffer.isEmpty()) {
                    handler->processLine(handler->buffer);
                    handler->buffer = "";
                }
            } else if (c == handler->promptCharacter && handler->enablePrompt) {
                handler->buffer += c;
                handler->processLine(handler->buffer);
                handler->buffer = "";
            } else {
                handler->buffer += c;
            }
        }
    }
}

/**
 * @brief Sets the callback function for asynchronous responses.
 *
 * This function assigns a user-defined callback function to handle
 * asynchronous responses from the modem. The callback is invoked
 * whenever an asynchronous response is received and processed.
 *
 * @param callback A function to be called with the asynchronous
 * response string as its argument.
 */
void ModemHandler::setAsyncCallback(AsyncCallback callback) {
    asyncCallback = callback;
}

/**
 * @brief Processes a line of input from the modem.
 *
 * This function analyzes a line of input from the modem to determine if it matches
 * any of the predefined asynchronous response prefixes. If a match is found, it invokes
 * the asynchronous callback, if set, and queues the line as an asynchronous event. If no
 * match is found, the line is queued as a synchronous response.
 *
 * @param line The line of input from the modem to process.
 */
void ModemHandler::processLine(const String& line) {
    String* linePtr = new String(line);

    for (const auto& prefix : asyncResponsePrefixes) {
        if (line.startsWith(prefix)) {
            if (asyncCallback) {
                asyncCallback(line);
            }
            xQueueSend(asyncEventQueue, &linePtr, 0);
            return;
        }
    }

    xQueueSend(responseQueue, &linePtr, 0);
}

/**
 * @brief Sends an AT command to the modem and waits for responses.
 *
 * This function sends an AT command to the modem and waits for responses.
 * The function will wait for the specified timeout period before returning.
 * If a response is received within the timeout period, the function will
 * store the response in the provided vector and return true. If the function
 * times out or no response is received, the function will return false.
 *
 * @param command The AT command to send to the modem.
 * @param responses The vector to store the responses in.
 * @param timeoutMs The timeout period in milliseconds.
 * @return true if a response is received within the timeout period, false
 *    otherwise.
 */
bool ModemHandler::sendATCommandWithResponse(const String& command, std::vector<String>* responses, int timeoutMs) {
    if (!responses) return false;

    sendATCommand(command);

    responses->clear();
    unsigned long startTime = millis();
    String response;

    while (millis() - startTime < timeoutMs) {
        if (getResponse(response, timeoutMs)) {
            responses->push_back(response);

            if (isEndOfResponse(response)) {
                return true;
            }
        } else {
            break;
        }
    }
    return !responses->empty();
}


/**
 * @brief Sets the criteria for determining the end of a response.
 *
 * This function sets the criteria for determining the end of a response from
 * the modem. The criteria is a vector of strings that may contain a wildcard
 * character '*'. If the wildcard character is present, the line must start
 * with the string before the wildcard. If no wildcard character is present,
 * the line must match the string exactly.
 *
 * @param criteria The vector of strings to set as the response end criteria.
 */
void ModemHandler::setResponseEndCriteria(const std::vector<String>& criteria) {
    responseEndCriteria = criteria;
}

/**
 * @brief Waits for responses from the modem and stores them in a vector.
 *
 * This function waits for responses from the modem and stores them in the
 * provided vector. The responses are stored in the order they are received.
 * The function will wait for the specified timeout period before returning.
 * If a response is received within the timeout period that matches the
 * criteria set by setResponseEndCriteria, the function returns true.
 * If no response is received or the timeout period is reached, the function
 * returns false.
 *
 * @param responses Pointer to a vector to store the responses in.
 * @param timeoutMs The timeout period in milliseconds.
 * @return true if a response is received within the timeout period, false
 *    otherwise.
 */
bool ModemHandler::getResponses(std::vector<String>* responses, int timeoutMs) {
    if (!responses) return false;
    responses->clear();
    String response;
    unsigned long startTime = millis();

    while (millis() - startTime < timeoutMs) {
        if (getResponse(response, timeoutMs)) {
            responses->push_back(response);
            if (isEndOfResponse(response)) {
                return true;
            }
        } else {
            break;
        }
    }
    return false;
}

/**
 * @brief Determines if a line marks the end of a response from the modem.
 *
 * This function checks whether a given line meets the criteria for the end
 * of a response. It first checks if prompt mode is enabled and if the line
 * contains the prompt character, which would indicate the end of the response.
 * It then evaluates the line against a set of predefined end criteria, which
 * may include wildcard patterns. If the criteria contain a wildcard '*', the
 * line is checked for a prefix match. If no wildcard is present, the line 
 * must match the criteria exactly.
 *
 * @param line The line of input from the modem to evaluate.
 * @return true if the line signifies the end of a response, false otherwise.
 */
bool ModemHandler::isEndOfResponse(const String& line) {
    if (enablePrompt && line.indexOf(promptCharacter) != -1) {
        return true;
    }

    for (const auto& criteria : responseEndCriteria) {
        if (criteria.indexOf('*') != -1) {
            String prefix = criteria.substring(0, criteria.indexOf('*'));
            if (line.startsWith(prefix)) {
                return true;
            }
        } else {
            if (line.equals(criteria)) {
                return true;
            }
        }
    }
    return false;
}