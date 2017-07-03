#ifndef BTCE_H
#define BTCE_H

#include "components/quote_t.h"
#include <string>
#include <exchanges/abstracts/AbstractExchange.h>

struct Parameters;

class BTCe : public AbstractExchange {
  public:
    BTCe();
    quote_t getQuote(Parameters &params);
    double getAvail(Parameters &params, std::string currency);
    bool isOrderComplete(Parameters &params, std::string orderId);
    double getActivePos(Parameters &params);
    json_t *checkResponse(std::ostream &logFile, json_t *root);
    double getLimitPrice(Parameters &params, double volume, bool isBid);
    json_t *authRequest(Parameters &params, const char *request, const std::string &options);
    json_t *adjustResponse(json_t *root);
};

#endif
