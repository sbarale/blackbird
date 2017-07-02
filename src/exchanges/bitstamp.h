#ifndef BITSTAMP_H
#define BITSTAMP_H

#include "exchanges/abstracts/AbstractExchange.h"

class Bitstamp : public AbstractExchange {
  public:
    Bitstamp();
    json_t *authRequest(Parameters &params, std::__1::string request, std::__1::string options);
    double getActivePos(Parameters &params);
    double getAvail(Parameters &params, std::__1::string currency);
    double getLimitPrice(Parameters &params, double volume, bool isBid);
    bool isOrderComplete(Parameters &params, std::__1::string orderId);
    std::__1::string
    sendLongOrder(Parameters &params, std::__1::string direction, double quantity, double price);
};

#endif //BITSTAMP_H