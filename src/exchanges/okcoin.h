#ifndef OKCOIN_H
#define OKCOIN_H

#include "components/quote_t.h"
#include <string>
#include <exchanges/abstracts/AbstractExchange.h>

struct json_t;
struct Parameters;

class OKCoin : public AbstractExchange {
  public:
    OKCoin();
    quote_t getQuote(Parameters &params);
    double getAvail(Parameters &params, std::string currency);
    std::string sendLongOrder(Parameters &params, std::string direction, double quantity, double price);
    std::string sendShortOrder(Parameters &params, std::string direction, double quantity, double price);
    bool isOrderComplete(Parameters &params, std::string orderId);
    double getActivePos(Parameters &params);
    double getLimitPrice(Parameters &params, double volume, bool isBid);
    json_t *authRequest(Parameters &params, std::string url, std::string signature, std::string content);
  protected:
    void getBorrowInfo(Parameters &params);
    int borrowBtc(Parameters &params, double amount);
    void repayBtc(Parameters &params, int borrowId);

};

#endif
