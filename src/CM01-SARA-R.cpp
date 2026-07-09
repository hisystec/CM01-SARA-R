#include "CM01-SARA-R.h"

ModemHandler::ModemHandler(HardwareSerial& serialPort, int responseQueueSize, int asyncQueueSize)
    : serial(&serialPort), buffer(""), readTaskHandle(nullptr),
      powerPin(5), pwrOnPin(4), rxPin(16), txPin(17), rtsPin(18), ctsPin(19),
      useFlowControl(true), enablePrompt(false), promptCharacter('>'),
      debugMode(false), asyncCallback(nullptr) {
    responseQueue = xQueueCreate(responseQueueSize, sizeof(String*));
    asyncEventQueue = xQueueCreate(asyncQueueSize, sizeof(String*));
}

ModemHandler::~ModemHandler() {
    if (readTaskHandle != nullptr) {
        vTaskDelete(readTaskHandle);
        readTaskHandle = nullptr;
    }
    drainQueue(responseQueue);
    drainQueue(asyncEventQueue);
    if (responseQueue != nullptr) {
        vQueueDelete(responseQueue);
        responseQueue = nullptr;
    }
    if (asyncEventQueue != nullptr) {
        vQueueDelete(asyncEventQueue);
        asyncEventQueue = nullptr;
    }
}

void ModemHandler::drainQueue(QueueHandle_t queue) {
    if (queue == nullptr) return;
    String* ptr = nullptr;
    while (xQueueReceive(queue, &ptr, 0) == pdTRUE) {
        delete ptr;
    }
}

void ModemHandler::begin() {
    powerOnModem();
    initSerial();
    setDisablePrompt();
    // begin()が複数回呼ばれても読み取りタスクを増殖させない
    if (readTaskHandle == nullptr) {
        xTaskCreatePinnedToCore(readFromModemTask, "ReadModemTask", 4096, this, 1, &readTaskHandle, 1);
    }
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
    if (responseQueue == nullptr) return false;
    String* responsePtr = nullptr;
    if (xQueueReceive(responseQueue, &responsePtr, pdMS_TO_TICKS(timeoutMs)) == pdTRUE) {
        response = *responsePtr;
        delete responsePtr;
        return true;
    }
    return false;
}

bool ModemHandler::getAsyncEvent(String& event, int timeoutMs) {
    if (asyncEventQueue == nullptr) return false;
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
            // xQueueSendはタイムアウト0(非ブロッキング)のため、キューが満杯だと送信できず
            // pdFALSEが返る。戻り値を確認せず捨てるとlinePtrがどこからも参照されなくなり、
            // 通信エラー時等にasyncEventが溜まりやすい状況でヒープリークにつながっていた。
            if (asyncEventQueue == nullptr || xQueueSend(asyncEventQueue, &linePtr, 0) != pdTRUE) {
                delete linePtr;
            }
            return;
        }
    }

    // 上記と同様、responseQueueが満杯の場合の取りこぼしでリークしないようにする。
    if (responseQueue == nullptr || xQueueSend(responseQueue, &linePtr, 0) != pdTRUE) {
        delete linePtr;
    }
}

bool ModemHandler::sendATCommandWithResponse(const String& command, std::vector<String>* responses, int timeoutMs) {
    if (!responses) return false;

    sendATCommand(command);

    responses->clear();
    unsigned long startTime = millis();
    String response;

    // getResponseへは常に「残り時間」を渡す。フルのtimeoutMsを毎回渡すと、
    // 複数行応答の受信中に指定タイムアウトの数倍待ち得る。
    while (true) {
        unsigned long elapsed = millis() - startTime;
        if (elapsed >= static_cast<unsigned long>(timeoutMs)) break;
        if (getResponse(response, timeoutMs - static_cast<int>(elapsed))) {
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

    // sendATCommandWithResponseと同様、残り時間を渡してトータルの待ち時間を守る
    while (true) {
        unsigned long elapsed = millis() - startTime;
        if (elapsed >= static_cast<unsigned long>(timeoutMs)) break;
        if (getResponse(response, timeoutMs - static_cast<int>(elapsed))) {
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