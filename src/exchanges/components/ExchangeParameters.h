#ifndef BLACKBIRD_EXCHANGEPARAMETERS_H
#define BLACKBIRD_EXCHANGEPARAMETERS_H

#include "ApiParameters.h"
#include <iostream>

struct Fees {
    double transaction;
    double withdraw;
    double deposit;
};

class ExchangeParameters {
  public:
    Fees          fees;
    ApiParameters api;
    bool          enabled;

    void load(std::string filename = "");

  protected:
    std::string getParameter(std::string parameter, std::ifstream &configFile);
    bool getBool(std::string value);
    double getDouble(std::string value);
    unsigned getUnsigned(std::string value);

};

#endif //BLACKBIRD_EXCHANGEPARAMETERS_H
