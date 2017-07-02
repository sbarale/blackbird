#include <array>
#include <utils/base64.h>
#include <utils/hex_str.hpp>
#include "utils/unique_json.hpp"
#include "utils/restapi.h"
#include "components/parameters.h"
#include "AbstractExchange.h"

/*
 * TODO: This is NOT PROPER CODE STRUCTURE. This is temporary to go through the
 *       refactoring process w/o that much pain
 */


AbstractExchange::AbstractExchange() {
    exchange_name = "Abstract";
    config.api.url = "NONE";
}

AbstractExchange::~AbstractExchange() {

}

quote_t AbstractExchange::getQuote(Parameters &params) {
    auto        exchange = queryHandle(params);
    unique_json root{exchange.getRequest(config.api.endpoint.quote)};
    const char  *quote   = json_string_value(json_object_get(root.get(), "bid"));
    auto        bidValue = quote ? atof(quote) : 0.0;

    quote = json_string_value(json_object_get(root.get(), "ask"));
    auto askValue = quote ? atof(quote) : 0.0;

    return std::make_pair(bidValue, askValue);
}

double AbstractExchange::getAvail(Parameters &params, std::string currency) {
    unique_json root{authRequest(params, config.api.endpoint.balance, "")};
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
                            << err.text << std::endl;
        } else if (each_type == std::string("trading") && each_currency == currency) {
            availability = std::stod(each_amount);
            break;
        }
    }
    return availability;
}

std::string
AbstractExchange::sendLongOrder(Parameters &params, std::string direction, double quantity, double price) {
    return sendOrder(params, direction, quantity, price);
}

std::string
AbstractExchange::sendShortOrder(Parameters &params, std::string direction, double quantity, double price) {
    return sendOrder(params, direction, quantity, price);
}

std::string
AbstractExchange::sendOrder(Parameters &params, std::string direction, double quantity, double price) {
    *params.logFile << "<Bitfinex> Trying to send a \"" << direction << "\" limit order: "
                    << std::setprecision(6) << quantity << "@$"
                    << std::setprecision(2) << price << "...\n";
    std::ostringstream oss;
    oss << "\"symbol\":\"btcusd\", \"amount\":\"" << quantity << "\", \"price\":\"" << price
        << "\", \"exchange\":\"bitfinex\", \"side\":\"" << direction << "\", \"type\":\"limit\"";
    std::string options = oss.str();
    unique_json root{authRequest(params, config.api.endpoint.order.new_one, options)};
    auto        orderId = std::to_string(json_integer_value(json_object_get(root.get(), "order_id")));
    *params.logFile << "<Bitfinex> Done (order ID: " << orderId << ")\n" << std::endl;
    return orderId;
}

bool AbstractExchange::isOrderComplete(Parameters &params, std::string orderId) {
    if (orderId == "0")
        return true;

    auto        options = "\"order_id\":" + orderId;
    unique_json root{authRequest(params, config.api.endpoint.order.status, options)};
    return json_is_false(json_object_get(root.get(), "is_live"));
}

double AbstractExchange::getActivePos(Parameters &params) {
    unique_json root{authRequest(params, config.api.endpoint.positions, "")};
    double      position;
    if (json_array_size(root.get()) == 0) {
        *params.logFile << "<Bitfinex> WARNING: BTC position not available, return 0.0" << std::endl;
        position = 0.0;
    } else {
        position = atof(json_string_value(json_object_get(json_array_get(root.get(), 0), "amount")));
    }
    return position;
}

double AbstractExchange::getLimitPrice(Parameters &params, double volume, bool isBid) {
    auto        exchange = queryHandle(params);
    unique_json root{exchange.getRequest(config.api.endpoint.order.book + "btcusd")};
    json_t      *bidask  = json_object_get(root.get(), isBid ? "bids" : "asks");

    *params.logFile << "<Bitfinex> Looking for a limit price to fill "
                    << std::setprecision(6) << fabs(volume) << " BTC...\n";
    double   tmpVol = 0.0;
    double   p      = 0.0;
    double   v;

    // loop on volume
    for (int i      = 0, n = json_array_size(bidask); i < n; ++i) {
        p = atof(json_string_value(json_object_get(json_array_get(bidask, i), "price")));
        v = atof(json_string_value(json_object_get(json_array_get(bidask, i), "amount")));
        *params.logFile << "<Bitfinex> order book: "
                        << std::setprecision(6) << v << "@$"
                        << std::setprecision(2) << p << std::endl;
        tmpVol += v;
        if (tmpVol >= fabs(volume) * params.orderBookFactor)
            break;
    }

    return p;
}

json_t *AbstractExchange::authRequest(Parameters &params, std::string request, std::string options) {
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
    uint8_t          *digest  = HMAC(EVP_sha384(),
                                     params.bitfinexSecret.c_str(), params.bitfinexSecret.length(),
                                     reinterpret_cast<const uint8_t *> (payload.data()), payload.size(),
                                     NULL, NULL);
    array<string, 3> headers
                             {
                                     "X-BFX-APIKEY:" + params.bitfinexApi,
                                     "X-BFX-SIGNATURE:" + hex_str(digest, digest + SHA384_DIGEST_LENGTH),
                                     "X-BFX-PAYLOAD:" + payload,
                             };
    auto             exchange = queryHandle(params);
    auto             root     = exchange.postRequest(request,
                                                     make_slist(begin(headers), end(headers)));
    return checkResponse(*params.logFile, root);
}

RestApi AbstractExchange::queryHandle(Parameters &params) {
    //std::cout << "Calling queryHandle in AbstractExchange for " << exchange_name << std::endl;
    RestApi query(config.api.url,
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
