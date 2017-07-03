#ifndef BLACKBIRD_EXCHANGEFACTORY_H
#define BLACKBIRD_EXCHANGEFACTORY_H

#include "../exchanges/abstracts/AbstractExchange.h"
#include "../exchanges/bitfinex.h"
#include "../exchanges/bitstamp.h"
#include "../exchanges/btce.h"

class ExchangeFactory {
  public:
    static AbstractExchange *make(std::string of_type) {
        if (of_type == "Bitfinex") {
            return new Bitfinex;
        } else if (of_type == "Bitstamp") {
            return new Bitstamp;
        } else if (of_type == "BTCe") {
            return new BTCe;
        } else {
            std::cout << "Exchange " << of_type << " not defined!" << std::endl;
            exit(1);
        }
    }
};

#endif //BLACKBIRD_EXCHANGEFACTORY_H
