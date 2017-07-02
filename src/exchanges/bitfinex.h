#ifndef BITFINEX_H
#define BITFINEX_H

#include "AbstractExchange.h"

class Bitfinex : public AbstractExchange {
  public:
    Bitfinex();
    quote_t getQuote(Parameters &params);
};

#endif