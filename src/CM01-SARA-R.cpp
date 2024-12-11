#include <CM01-SARA-R.h>

ModemHandler::ModemHandler(HardwareSerial& serialPort, int responseQueueSize, int asyncQueueSize)
    : serial(&serialPort), buffer(""), asyncCallback(nullptr), debugMode(false) {
    responseQueue = xQueueCreate(responseQueueSize, sizeof(String*));
    asyncEventQueue = xQueueCreate(asyncQueueSize, sizeof(String*));
}

void ModemHandler::begin() {
    powerOnModem();
    initSerial();
    setDisablePrompt();
    xTaskCreatePinnedToCore(readFromModemTask, "ReadModemTask", 4096, this, 1, NULL, 1);
    delay(6000);
}

void ModemHandler::setEnablePrompt(char chr) {
    this->promptCharacter = chr;
    this->enablePrompt = true;
}

void ModemHandler::setDisablePrompt() {
    this->enablePrompt = false;
}

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

void ModemHandler::sendATCommand(const String& command) {
    if (debugMode) debugPrint("TX", command);
    serial->println(command);
}

void ModemHandler::sendStringData(const String& data) {
    if (debugMode) debugPrint("TX", data);
    serial->print(data);
}

bool ModemHandler::getResponse(String& response, int timeoutMs) {
    String* responsePtr = nullptr;
    if (xQueueReceive(responseQueue, &responsePtr, pdMS_TO_TICKS(timeoutMs)) == pdTRUE) {
        response = *responsePtr;
        delete responsePtr;
        return true;
    }
    return false;
}

bool ModemHandler::getAsyncEvent(String& event, int timeoutMs) {
    String* eventPtr = nullptr;
    if (xQueueReceive(asyncEventQueue, &eventPtr, pdMS_TO_TICKS(timeoutMs)) == pdTRUE) {
        event = *eventPtr;
        delete eventPtr;
        return true;
    }
    return false;
}

void ModemHandler::setAsyncResponsePrefixes(const std::vector<String>& prefixes) {
    asyncResponsePrefixes = prefixes;
}

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

void ModemHandler::readFromModemTask(void* param) {
    ModemHandler* handler = static_cast<ModemHandler*>(param);
    while (true) {
        while (handler->serial->available()) {
            char c = handler->serial->read();
            if (c == '\r' || c == '\n') {
                if (!handler->buffer.isEmpty()) {
                    handler->processLine(handler->buffer);
                    if (handler->debugMode) handler->debugPrint("RX", handler->buffer);
                    handler->buffer = "";
                }
            } else if (c == handler->promptCharacter && handler->enablePrompt) {
                handler->buffer += c;
                handler->processLine(handler->buffer);
                if (handler->debugMode) handler->debugPrint("RX", handler->buffer);
                handler->buffer = "";
            } else {
                handler->buffer += c;
            }
        }
        delay(10);
    }
}

void ModemHandler::setAsyncCallback(AsyncCallback callback) {
    asyncCallback = callback;
}

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

void ModemHandler::setResponseEndCriteria(const std::vector<String>& criteria) {
    responseEndCriteria = criteria;
}

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

void ModemHandler::enableDebugMode() {
    debugMode = true;
}

void ModemHandler::disableDebugMode() {
    debugMode = false;
}

void ModemHandler::debugPrint(const String& direction, const String& data) {
    String timestamp = String(millis());
    Serial.println("[" + timestamp + "] " + direction + ": " + data);
}
