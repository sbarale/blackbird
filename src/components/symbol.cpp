#include "symbol.h"
#include <cmath>

Symbol::Symbol(unsigned i, std::string n, double f, bool h, bool m) {
    id            = i;
    exchName      = n;
    fees          = f;
    hasShort      = h;
    isImplemented = m;
    bid           = 0.0;
    ask           = 0.0;
}

void Symbol::updateData(quote_t quote) {
    bid = quote.bid();
    ask = quote.ask();
}

unsigned Symbol::getId() const {
    return id;
}

double Symbol::getBid() const {
    return bid;
}

double Symbol::getAsk() const {
    return ask;
}

double Symbol::getMidPrice() const {
    if (bid > 0.0 && ask > 0.0) {
        return (bid + ask) / 2.0;
    } else {
        return 0.0;
    }
}

std::string Symbol::getExchName() const {
    return exchName;
}

double Symbol::getFees() const {
    return fees;
}

bool Symbol::getHasShort() const {
    return hasShort;
}

bool Symbol::getIsImplemented() const {
    return isImplemented;
}
