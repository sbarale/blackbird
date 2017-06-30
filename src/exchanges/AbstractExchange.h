#ifndef BLACKBIRD_ABSTRACTEXCHANGE_H
#define BLACKBIRD_ABSTRACTEXCHANGE_H

#include <string>
#include <parameters.h>
#include <jansson.h>
#include <utils/restapi.h>
#include "quote_t.h"

class AbstractExchange {
  public:
    std::string api_url;
    std::string exchange_name;

    AbstractExchange();
    ~AbstractExchange();

    virtual RestApi &queryHandle(Parameters &params);
    virtual json_t *checkResponse(std::ostream &logFile, json_t *root);
    virtual json_t *authRequest(Parameters &params, std::__1::string request, std::__1::string options);
    virtual double getActivePos(Parameters &params);
    virtual double getAvail(Parameters &params, std::__1::string currency);
    virtual double getLimitPrice(Parameters &params, double volume, bool isBid);
    virtual quote_t getQuote(Parameters &params);
    virtual bool isOrderComplete(Parameters &params, std::__1::string orderId);
    virtual std::__1::string
    sendLongOrder(Parameters &params, std::__1::string direction, double quantity, double price);
    virtual std::__1::string
    sendShortOrder(Parameters &params, std::__1::string direction, double quantity, double price);
    virtual std::__1::string sendOrder(Parameters &params, std::__1::string direction, double quantity, double price);

};

#endif //BLACKBIRD_ABSTRACTEXCHANGE_H
