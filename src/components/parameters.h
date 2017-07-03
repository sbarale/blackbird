#ifndef PARAMETERS_H
#define PARAMETERS_H

#include "utils/unique_sqlite.hpp"

#include <fstream>
#include <string>
#include <vector>
#include <curl/curl.h>

struct tokens : std::ctype<char> {
    tokens() : std::ctype<char>(get_table()) {
    }

    static std::ctype_base::mask const *get_table() {
        typedef std::ctype<char>  cctype;
        static const cctype::mask *const_rc = cctype::classic_table();
        static cctype::mask       rc[cctype::table_size];
        std::memcpy(rc, const_rc, cctype::table_size * sizeof(cctype::mask));

        rc[','] = std::ctype_base::space;
        rc[' '] = std::ctype_base::space;
        return &rc[0];
    }
};

// Stores all the parameters defined
// in the configuration file.
struct Parameters {

    std::vector<std::string> exchName;
    std::vector<double>      fees;
    std::vector<bool>        canShort;
    std::vector<bool>        isImplemented;
    std::vector<std::string> tickerUrl;
    CURL                     *curl;
    double                   spreadEntry;
    double                   spreadTarget;
    unsigned                 maxLength;
    double                   priceDeltaLim;
    double                   trailingLim;
    unsigned                 trailingCount;
    double                   orderBookFactor;
    bool                     demoMode;
    std::string              leg1;
    std::string              leg2;
    bool                     verbose;
    std::ofstream            *logFile;
    unsigned                 interval;
    unsigned                 debugMaxIteration;
    bool                     useFullExposure;
    double                   testedExposure;
    double                   maxExposure;
    bool                     useVolatility;
    unsigned                 volatilityPeriod;
    std::string              cacert;

    std::vector<std::string> exchanges;
    bool                     sendEmail;
    std::string              senderAddress;
    std::string              senderUsername;
    std::string              senderPassword;
    std::string              smtpServerAddress;
    std::string              receiverAddress;
    std::string              dbFile;
    unique_sqlite            dbConn;
    Parameters(std::string fileName);
    void addExchange(std::string n, double f, bool h, bool m);
    int nbExch() const;
    std::string tradedPair() const;
};

// Copies the parameters from the configuration file
// to the Parameter structure.
std::string getParameter(std::string parameter, std::ifstream &configFile);
bool getBool(std::string value);
double getDouble(std::string value);
unsigned getUnsigned(std::string value);
std::vector<std::string> makeVector(std::string values);

#endif
