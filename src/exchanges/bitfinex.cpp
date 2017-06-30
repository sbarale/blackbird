#include "bitfinex.h"

Bitfinex::Bitfinex() {
    std::cout << "Init Bitfinex" << std::endl;
    exchange_name = "Bitfinex";
    api_url = "https://api.bitfinex.com";
}