#ifndef BITSTAMP_H
#define BITSTAMP_H

#include "exchanges/abstracts/AbstractExchange.h"

class Bitstamp : public AbstractExchange {
  public:
    Bitstamp();
    json_t *authRequest(Parameters &params, std::string request, std::string options);
    double getActivePos(Parameters &params);
    double getAvail(Parameters &params, std::string currency);
    double getLimitPrice(Parameters &params, double volume, bool isBid);
    bool isOrderComplete(Parameters &params, std::string orderId);
    std::string sendLongOrder(Parameters &params, std::string direction, double quantity, double price);
};

#endif //BITSTAMP_H