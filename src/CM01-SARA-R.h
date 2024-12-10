#ifndef MODEM_HANDLER_H
#define MODEM_HANDLER_H

#include <Arduino.h>
#include "driver/uart.h"
#include <vector>
#include <functional>

class ModemHandler {
public:
    using AsyncCallback = std::function<void(const String&)>;

    ModemHandler(HardwareSerial& serialPort, int responseQueueSize = 10, int asyncQueueSize = 10);

    void begin();
    void setPins(int powerPin = 5, int pwrOnPin = 4, int rxPin = 16, int txPin = 17,
                 int rtsPin = 18, int ctsPin = 19, bool useFlowControl = true);

    void sendATCommand(const String& command);
    void sendStringData(const String& data);
    bool getResponse(String& response, int timeoutMs = 5000);
    bool getAsyncEvent(String& event, int timeoutMs = 5000);

    bool getResponses(std::vector<String>* responses, int timeoutMs = 5000);
    void setResponseEndCriteria(const std::vector<String>& criteria);

    void setAsyncResponsePrefixes(const std::vector<String>& prefixes);
    void setAsyncCallback(AsyncCallback callback);

    void setEnablePrompt(char chr = '>');
    void setDisablePrompt();

private:
    HardwareSerial* serial;
    String buffer;
    QueueHandle_t responseQueue;
    QueueHandle_t asyncEventQueue;

    int powerPin;
    int pwrOnPin;
    int rxPin;
    int txPin;
    int rtsPin;
    int ctsPin;
    bool useFlowControl;

    bool enablePrompt;
    char promptCharacter;

    std::vector<String> asyncResponsePrefixes;
    std::vector<String> responseEndCriteria;

    AsyncCallback asyncCallback;

    void powerOnModem();
    void initSerial();
    static void readFromModemTask(void* param);
    void processLine(const String& line);
    bool isEndOfResponse(const String& line);
};

#endif // MODEM_HANDLER_H
