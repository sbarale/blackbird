#ifndef BLACKBIRD_ABSTRACTEXCHANGE_H
#define BLACKBIRD_ABSTRACTEXCHANGE_H

#include <string>
#include <components/parameters.h>
#include <jansson.h>
#include <utils/restapi.h>
#include "components/quote_t.h"
#include "exchanges/components/ApiParameters.h"
#include "exchanges/components/ExchangeParameters.h"

class AbstractExchange {
  public:
    std::string        exchange_name;
    ExchangeParameters config;
    AbstractExchange();
    ~AbstractExchange();

    std::string getName() {
        return exchange_name;
    }

    virtual RestApi queryHandle(Parameters &params);
    virtual json_t *checkResponse(std::ostream &logFile, json_t *root);
    virtual json_t *authRequest(Parameters &params, std::string request, std::string options);
    virtual double getActivePos(Parameters &params);
    virtual double getAvail(Parameters &params, std::string currency);
    virtual double getLimitPrice(Parameters &params, double volume, bool isBid);
    virtual quote_t getQuote(Parameters &params);
    virtual bool isOrderComplete(Parameters &params, std::string orderId);
    virtual std::string sendLongOrder(Parameters &params, std::string direction, double quantity, double price);
    virtual std::string sendShortOrder(Parameters &params, std::string direction, double quantity, double price);
    virtual std::string sendOrder(Parameters &params, std::string direction, double quantity, double price);
    virtual void loadConfig();
};

#endif //BLACKBIRD_ABSTRACTEXCHANGE_H
