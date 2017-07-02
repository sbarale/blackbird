#include "bitfinex.h"
#include "unique_json.hpp"

Bitfinex::Bitfinex() {
    //std::cout << "Init Bitfinex" << std::endl;
    exchange_name = "Bitfinex";
    api.url                    = "https://api.bitfinex.com";
    api.endpoint.auth          = "";
    api.endpoint.balance       = "";
    api.endpoint.order.book    = "/v1/book/";
    api.endpoint.order.new_one = "/v1/order/new/";
    api.endpoint.order.status  = "/v1/order/status/";
    api.endpoint.quote         = "/v1/ticker/";
    api.endpoint.positions     = "/v1/positions";

}

quote_t Bitfinex::getQuote(Parameters &params) {
    auto        exchange = queryHandle(params);

    // Added to handle ethbtc on bitfinex
    std::string url;
    if (params.tradedPair().compare("BTC/USD") == 0) {
        url = api.endpoint.quote + "btcusd";
    } else if (params.tradedPair().compare("ETH/BTC") == 0) {
        url = api.endpoint.quote + "ethbtc";
    }
    unique_json root{exchange.getRequest(url)};
    const char  *quote   = json_string_value(json_object_get(root.get(), "bid"));
    double      bidValue = quote ? std::stod(quote) : 0.0;

    quote = json_string_value(json_object_get(root.get(), "ask"));
    double askValue = quote ? std::stod(quote) : 0.0;

    return std::make_pair(bidValue, askValue);
}