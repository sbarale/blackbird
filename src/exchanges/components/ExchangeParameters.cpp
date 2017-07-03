#include "ExchangeParameters.h"
#include <fstream>

void ExchangeParameters::load(string filename) {
    std::ifstream configFile(filename);

    if (!configFile.is_open()) {
        std::cout << "ERROR: " << filename << " cannot be open.\n";
        exit(EXIT_FAILURE);
    }

    api.auth.client_id         = getParameter("api.client_id", configFile);
    api.auth.key               = getParameter("api.key", configFile);
    api.auth.secret            = getParameter("api.secret", configFile);

    fees.transaction           = getDouble(getParameter("fee.transaction", configFile));
    capabilities.spot          = getBool(getParameter("capabilities.spot", configFile));
    capabilities.margin._short = getBool(getParameter("capabilities.margin.short", configFile));
    capabilities.margin._long  = getBool(getParameter("capabilities.margin.long", configFile));

    enabled = getBool(getParameter("enabled", configFile));

}

std::string ExchangeParameters::getParameter(std::string parameter, std::ifstream &configFile) {
    std::string line;
    configFile.clear();
    configFile.seekg(0);

    while (getline(configFile, line)) {
        if (line.length() > 0 && line.at(0) != '#') {
            std::string key   = line.substr(0, line.find('='));
            std::string value = line.substr(line.find('=') + 1, line.length());
            if (key == parameter) {
                return value;
            }
        }
    }
    std::cout << "ERROR: parameter '" << parameter << "' not found. Your configuration file might be too old.\n"
              << std::endl;
    exit(EXIT_FAILURE);
}

bool ExchangeParameters::getBool(std::string value) {
    return value == "true";
}

double ExchangeParameters::getDouble(std::string value) {
    return atof(value.c_str());
}

unsigned ExchangeParameters::getUnsigned(std::string value) {
    return (unsigned int) atoi(value.c_str());
}
