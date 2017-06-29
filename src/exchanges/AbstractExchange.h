//
// Created by Macbook on 6/29/17.
//

#ifndef BLACKBIRD_ABSTRACTEXCHANGE_H
#define BLACKBIRD_ABSTRACTEXCHANGE_H

#include <string>
#include <parameters.h>
#include <jansson.h>
#include <utils/restapi.h>
#include "quote_t.h"

class AbstractExchange {
  public:
    static RestApi &queryHandle(Parameters &params) {
        static RestApi query(api_url,
                             params.cacert.c_str(),
                             *params.logFile
        );
        return query;
    }

    static json_t *checkResponse(std::ostream &logFile, json_t *root) {
        auto msg = json_object_get(root, "message");
        if (msg) {
            logFile << "<Bitfinex> Error with response: "
                    << json_string_value(msg) << '\n';
        }
        return root;
    }

    static std::__1::string sendOrder(Parameters &params, std::__1::string direction, double quantity, double price);
    static json_t *authRequest(Parameters &params, std::__1::string request, std::__1::string options);
    static double getActivePos(Parameters &params);
    static double getAvail(Parameters &params, std::__1::string currency);
    static double getLimitPrice(Parameters &params, double volume, bool isBid);
    static quote_t getQuote(Parameters &params);
    static bool isOrderComplete(Parameters &params, std::__1::string orderId);
    static std::__1::string
    sendLongOrder(Parameters &params, std::__1::string direction, double quantity, double price);
    static std::__1::string
    sendShortOrder(Parameters &params, std::__1::string direction, double quantity, double price);
    static std::string api_url;
};

#endif //BLACKBIRD_ABSTRACTEXCHANGE_H
