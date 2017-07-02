#ifndef BLACKBIRD_EXCHANGEPARAMETERS_H
#define BLACKBIRD_EXCHANGEPARAMETERS_H
#include "ApiParameters.h"

struct Fees {
    double transaction;
    double withdraw;
    double deposit;
};

class ExchangeParameters {
  public:
    Fees fees;
    ApiParameters api;
};

#endif //BLACKBIRD_EXCHANGEPARAMETERS_H
