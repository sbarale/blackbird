#include "coin.h"
#include <cmath>

Coin::Coin(unsigned i, std::string n, double f, bool h, bool m) {
    id            = i;
    exchName      = n;
    fees          = f;
    hasShort      = h;
    isImplemented = m;
    bid           = 0.0;
    ask           = 0.0;
}

void Coin::updateData(quote_t quote) {
    bid = quote.bid();
    ask = quote.ask();
}

unsigned Coin::getId() const {
    return id;
}

double Coin::getBid() const {
    return bid;
}

double Coin::getAsk() const {
    return ask;
}

double Coin::getMidPrice() const {
    if (bid > 0.0 && ask > 0.0) {
        return (bid + ask) / 2.0;
    } else {
        return 0.0;
    }
}

std::string Coin::getExchName() const {
    return exchName;
}

double Coin::getFees() const {
    return fees;
}

bool Coin::getHasShort() const {
    return hasShort;
}

bool Coin::getIsImplemented() const {
    return isImplemented;
}
