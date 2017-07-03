#ifndef BLACKBIRD_EXCHANGEPARAMETERS_H
#define BLACKBIRD_EXCHANGEPARAMETERS_H

#include "ApiParameters.h"
#include <iostream>

struct Margin {
    bool _long;
    bool _short;
};
struct Capabilities {
    bool   spot;
    Margin margin;
};
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
    Capabilities  capabilities;
    void load(std::string filename = "");

  protected:
    std::string getParameter(std::string parameter, std::ifstream &configFile);
    bool getBool(std::string value);
    double getDouble(std::string value);
    unsigned getUnsigned(std::string value);

};

#endif //BLACKBIRD_EXCHANGEPARAMETERS_H
