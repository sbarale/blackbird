#ifndef BITFINEX_H
#define BITFINEX_H

#include "AbstractExchange.h"

struct json_t;
struct Parameters;

class Bitfinex : public AbstractExchange {
  public:
    static std::string api_url;
};

#endif
