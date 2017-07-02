//
// Created by Macbook on 6/30/17.
//

#ifndef BLACKBIRD_APIPARAMETERS_H
#define BLACKBIRD_APIPARAMETERS_H

#include <iostream>

using namespace std;
struct Order {
    string new_one;
    string book;
    string status;
};
struct Endpoint {
    string quote;
    string auth;
    Order  order;
    string balance;
    string positions;
};
struct ApiParameters {
    string   url;
    Endpoint endpoint;
};
#endif //BLACKBIRD_APIPARAMETERS_H
