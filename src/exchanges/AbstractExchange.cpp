#include <cmath>
#include <array>
#include <iomanip>
#include <sstream>
#include <utils/base64.h>
#include <openssl/hmac.h>
#include <hex_str.hpp>
#include <openssl/sha.h>
#include "unique_json.hpp"
#include "utils/restapi.h"
#include "parameters.h"
#include "AbstractExchange.h"

/*
 * TODO: This is NOT PROPER CODE STRUCTURE. This is temporary to go through the
 *       refactoring process w/o that much pain
 */


AbstractExchange::AbstractExchange() {
    exchange_name = "Abstract";
    api_url       = "DAFUK";
    std::cout << "Init " << exchange_name << std::endl;
}

AbstractExchange::~AbstractExchange() {

}

quote_t AbstractExchange::getQuote(Parameters &params) {

    std::cout << "Calling getQuote() in AbstractExchange class for " << exchange_name << std::endl;
    auto             &exchange = queryHandle(params);

    // Added to handle ethbtc on bitfinex
    std::__1::string url;
    if (params.tradedPair().compare("BTC/USD") == 0) {
        url = "/v1/ticker/btcusd";
    } else if (params.tradedPair().compare("ETH/BTC") == 0) {
        url = "/v1/ticker/ethbtc";
    }
    unique_json root{exchange.getRequest(url)};

    //  following line was old code (no ethbtc); replaced by 7 lines above.
    //  unique_json root { exchange.getRequest("/v1/ticker/btcusd") };

    const char  *quote         = json_string_value(json_object_get(root.get(), "bid"));
    double      bidValue       = quote ? std::__1::stod(quote) : 0.0;

    quote = json_string_value(json_object_get(root.get(), "ask"));
    double askValue = quote ? std::__1::stod(quote) : 0.0;

    return std::__1::make_pair(bidValue, askValue);
}

double AbstractExchange::getAvail(Parameters &params, std::__1::string currency) {
    unique_json root{authRequest(params, "/v1/balances", "")};
    double      availability = 0.0;
    for (size_t i            = json_array_size(root.get()); i--;) {
        const char   *each_type, *each_currency, *each_amount;
        json_error_t err;
        int          unpack_fail = json_unpack_ex(json_array_get(root.get(), i),
                                                  &err, 0,
                                                  "{s:s, s:s, s:s}",
                                                  "type", &each_type,
                                                  "currency", &each_currency,
                                                  "amount", &each_amount);
        if (unpack_fail) {
            *params.logFile << "<Bitfinex> Error with JSON: "
                            << err.text << std::__1::endl;
        } else if (each_type == std::__1::string("trading") && each_currency == currency) {
            availability = std::__1::stod(each_amount);
            break;
        }
    }
    return availability;
}

std::__1::string
AbstractExchange::sendLongOrder(Parameters &params, std::__1::string direction, double quantity, double price) {
    return sendOrder(params, direction, quantity, price);
}

std::__1::string
AbstractExchange::sendShortOrder(Parameters &params, std::__1::string direction, double quantity, double price) {
    return sendOrder(params, direction, quantity, price);
}

std::__1::string
AbstractExchange::sendOrder(Parameters &params, std::__1::string direction, double quantity, double price) {
    *params.logFile << "<Bitfinex> Trying to send a \"" << direction << "\" limit order: "
                    << std::__1::setprecision(6) << quantity << "@$"
                    << std::__1::setprecision(2) << price << "...\n";
    std::__1::ostringstream oss;
    oss << "\"symbol\":\"btcusd\", \"amount\":\"" << quantity << "\", \"price\":\"" << price
        << "\", \"exchange\":\"bitfinex\", \"side\":\"" << direction << "\", \"type\":\"limit\"";
    std::__1::string options = oss.str();
    unique_json      root{authRequest(params, "/v1/order/new", options)};
    auto             orderId = std::__1::to_string(json_integer_value(json_object_get(root.get(), "order_id")));
    *params.logFile << "<Bitfinex> Done (order ID: " << orderId << ")\n" << std::__1::endl;
    return orderId;
}

bool AbstractExchange::isOrderComplete(Parameters &params, std::__1::string orderId) {
    if (orderId == "0")
        return true;

    auto        options = "\"order_id\":" + orderId;
    unique_json root{authRequest(params, "/v1/order/status", options)};
    return json_is_false(json_object_get(root.get(), "is_live"));
}

double AbstractExchange::getActivePos(Parameters &params) {
    unique_json root{authRequest(params, "/v1/positions", "")};
    double      position;
    if (json_array_size(root.get()) == 0) {
        *params.logFile << "<Bitfinex> WARNING: BTC position not available, return 0.0" << std::__1::endl;
        position = 0.0;
    } else {
        position = atof(json_string_value(json_object_get(json_array_get(root.get(), 0), "amount")));
    }
    return position;
}

double AbstractExchange::getLimitPrice(Parameters &params, double volume, bool isBid) {
    auto        &exchange = queryHandle(params);
    unique_json root{exchange.getRequest("/v1/book/btcusd")};
    json_t      *bidask   = json_object_get(root.get(), isBid ? "bids" : "asks");

    *params.logFile << "<Bitfinex> Looking for a limit price to fill "
                    << std::__1::setprecision(6) << fabs(volume) << " BTC...\n";
    double   tmpVol = 0.0;
    double   p      = 0.0;
    double   v;

    // loop on volume
    for (int i      = 0, n = json_array_size(bidask); i < n; ++i) {
        p = atof(json_string_value(json_object_get(json_array_get(bidask, i), "price")));
        v = atof(json_string_value(json_object_get(json_array_get(bidask, i), "amount")));
        *params.logFile << "<Bitfinex> order book: "
                        << std::__1::setprecision(6) << v << "@$"
                        << std::__1::setprecision(2) << p << std::__1::endl;
        tmpVol += v;
        if (tmpVol >= fabs(volume) * params.orderBookFactor)
            break;
    }

    return p;
}

json_t *AbstractExchange::authRequest(Parameters &params, std::__1::string request, std::__1::string options) {
    using namespace std;

    static uint64_t nonce   = time(nullptr) * 4;
    string          payload = "{\"request\":\"" + request +
                              "\",\"nonce\":\"" + to_string(++nonce);
    if (options.empty()) {
        payload += "\"}";
    } else {
        payload += "\", " + options + "}";
    }

    payload                 = base64_encode(reinterpret_cast<const uint8_t *>(payload.c_str()), payload.length());

    // signature
    uint8_t          *digest   = HMAC(EVP_sha384(),
                                      params.bitfinexSecret.c_str(), params.bitfinexSecret.length(),
                                      reinterpret_cast<const uint8_t *> (payload.data()), payload.size(),
                                      NULL, NULL);
    array<string, 3> headers
                             {
                                     "X-BFX-APIKEY:" + params.bitfinexApi,
                                     "X-BFX-SIGNATURE:" + hex_str(digest, digest + SHA384_DIGEST_LENGTH),
                                     "X-BFX-PAYLOAD:" + payload,
                             };
    auto             &exchange = queryHandle(params);
    auto             root      = exchange.postRequest(request,
                                                      make_slist(begin(headers), end(headers)));
    return checkResponse(*params.logFile, root);
}

RestApi &AbstractExchange::queryHandle(Parameters &params) {
    std::cout << "Calling queryHandle in " << exchange_name << std::endl;
    static RestApi query(api_url,
                         params.cacert.c_str(),
                         *params.logFile
    );
    return query;
}

json_t *AbstractExchange::checkResponse(std::ostream &logFile, json_t *root) {
    auto msg = json_object_get(root, "message");
    if (msg) {
        logFile << "<AbstractExchange> Error with response: "
                << json_string_value(msg) << '\n';
    }
    return root;
}
