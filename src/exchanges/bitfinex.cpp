#include "bitfinex.h"
#include "utils/unique_json.hpp"

Bitfinex::Bitfinex() {
    //std::cout << "Init Bitfinex" << std::endl;
    exchange_name = "Bitfinex";
    config.api.url                    = "https://api.bitfinex.com";
    config.api.endpoint.auth          = "";
    config.api.endpoint.balance       = "";
    config.api.endpoint.order.book    = "/v1/book/";
    config.api.endpoint.order.new_one = "/v1/order/new/";
    config.api.endpoint.order.status  = "/v1/order/status/";
    config.api.endpoint.quote         = "/v1/ticker/";
    config.api.endpoint.positions     = "/v1/positions";

}

quote_t Bitfinex::getQuote(Parameters &params) {
    auto        exchange = queryHandle(params);

    // Added to handle ethbtc on bitfinex
    std::string url;
    if (params.tradedPair().compare("BTC/USD") == 0) {
        url = config.api.endpoint.quote + "btcusd";
    } else if (params.tradedPair().compare("ETH/BTC") == 0) {
        url = config.api.endpoint.quote + "ethbtc";
    }
    unique_json root{exchange.getRequest(url)};
    const char  *quote   = json_string_value(json_object_get(root.get(), "bid"));
    double      bidValue = quote ? std::stod(quote) : 0.0;

    quote = json_string_value(json_object_get(root.get(), "ask"));
    double askValue = quote ? std::stod(quote) : 0.0;

    return std::make_pair(bidValue, askValue);
}