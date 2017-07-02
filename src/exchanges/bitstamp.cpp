#include "bitstamp.h"
#include "curl_fun.h"
#include "unique_json.hpp"
#include <openssl/sha.h>
#include "bitstamp.h"
#include "curl_fun.h"
#include "unique_json.hpp"
#include "openssl/sha.h"
#include "openssl/hmac.h"
#include <thread>
#include <cmath>
#include <sstream>
#include <iomanip>

Bitstamp::Bitstamp() {
    //std::cout << "Init BitStamp" << std::endl;
    exchange_name = "Bitstamp";
    api.url                    = "https://www.bitstamp.net";
    api.endpoint.auth          = "";
    api.endpoint.balance       = "/api/balance/";
    api.endpoint.order.book    = "/api/order_book/";
    api.endpoint.order.new_one = "/api/";
    api.endpoint.order.status  = "/api/order_status/";
    api.endpoint.quote         = "/api/ticker/";
}

double Bitstamp::getAvail(Parameters &params, std::string currency) {
    unique_json root{authRequest(params, api.url + api.endpoint.balance, "")};
    while (json_object_get(root.get(), "message") != NULL) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        auto dump = json_dumps(root.get(), 0);
        *params.logFile << exchange_name << " Error with JSON: " << dump << ". Retrying..." << std::endl;
        free(dump);
        root.reset(authRequest(params, api.url + api.endpoint.balance, ""));
    }
    double      availability  = 0.0;
    const char  *returnedText = NULL;
    currency     = currency + "_balance";
    returnedText = json_string_value(json_object_get(root.get(), currency.c_str()));

    if (returnedText != NULL) {
        availability = atof(returnedText);
    } else {
        *params.logFile << exchange_name << " Error with the credentials." << std::endl;
        availability = 0.0;
    }

    return availability;
}

std::string Bitstamp::sendLongOrder(Parameters &params, std::string direction, double quantity, double price) {
    *params.logFile << "<Bitstamp> Trying to send a \"" << direction << "\" limit order: "
                    << std::setprecision(6) << quantity << "@$"
                    << std::setprecision(2) << price << "...\n";
    std::ostringstream oss;
    oss << api.url + api.endpoint.order.new_one << direction << "/";
    std::string url = oss.str();
    oss.clear();
    oss.str("");
    oss << "amount=" << quantity << "&price=" << std::fixed << std::setprecision(2) << price;
    std::string options = oss.str();
    unique_json root{authRequest(params, url, options)};
    auto        orderId = std::to_string(json_integer_value(json_object_get(root.get(), "id")));
    if (orderId == "0") {
        auto dump = json_dumps(root.get(), 0);
        *params.logFile << "<Bitstamp> Order ID = 0. Message: " << dump << '\n';
        free(dump);
    }
    *params.logFile << "<Bitstamp> Done (order ID: " << orderId << ")\n" << std::endl;

    return orderId;
}

bool Bitstamp::isOrderComplete(Parameters &params, std::string orderId) {
    if (orderId == "0")
        return true;

    auto        options = "id=" + orderId;
    unique_json root{authRequest(params, api.url + api.endpoint.order.status, options)};
    std::string status  = json_string_value(json_object_get(root.get(), "status"));
    return status == "Finished";
}

double Bitstamp::getActivePos(Parameters &params) {
    return getAvail(params, "btc");
}

double Bitstamp::getLimitPrice(Parameters &params, double volume, bool isBid) {
    auto        exchange  = queryHandle(params);
    unique_json root{exchange.getRequest(api.endpoint.order.book)};
    auto        orderbook = json_object_get(root.get(), isBid ? "bids" : "asks");

    // loop on volume
    *params.logFile << "<Bitstamp> Looking for a limit price to fill "
                    << std::setprecision(6) << fabs(volume) << " BTC...\n";
    double tmpVol = 0.0;
    double p      = 0.0;
    double v;
    int    i      = 0;
    while (tmpVol < fabs(volume) * params.orderBookFactor) {
        p = atof(json_string_value(json_array_get(json_array_get(orderbook, i), 0)));
        v = atof(json_string_value(json_array_get(json_array_get(orderbook, i), 1)));
        *params.logFile << "<Bitstamp> order book: "
                        << std::setprecision(6) << v << "@$"
                        << std::setprecision(2) << p << std::endl;
        tmpVol += v;
        i++;
    }
    return p;
}

json_t *Bitstamp::authRequest(Parameters &params, std::string url, std::string options) {
    static uint64_t    nonce = time(nullptr) * 4;
    std::ostringstream oss;
    oss << ++nonce << params.bitstampClientId << params.bitstampApi;
    unsigned char *digest;
    digest                   = HMAC(EVP_sha256(), params.bitstampSecret.c_str(), params.bitstampSecret.size(), (unsigned char *) oss.str().data(), oss.str().size(), NULL, NULL);
    char     mdString[SHA256_DIGEST_LENGTH + 100];  // FIXME +100
    for (int i               = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        sprintf(&mdString[i * 2], "%02X", (unsigned int) digest[i]);
    }
    oss.clear();
    oss.str("");
    oss << "key=" << params.bitstampApi << "&signature=" << mdString << "&nonce=" << nonce << "&" << options;
    std::string postParams = oss.str().c_str();

    if (params.curl) {
        std::string readBuffer;
        curl_easy_setopt(params.curl, CURLOPT_POST, 1L);
        curl_easy_setopt(params.curl, CURLOPT_POSTFIELDS, postParams.c_str());
        curl_easy_setopt(params.curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(params.curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(params.curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(params.curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(params.curl, CURLOPT_CONNECTTIMEOUT, 10L);
        CURLcode resCurl   = curl_easy_perform(params.curl);
        using std::this_thread::sleep_for;
        using secs = std::chrono::seconds;

        while (resCurl != CURLE_OK) {
            *params.logFile << "<Bitstamp> Error with cURL. Retry in 2 sec..." << std::endl;
            sleep_for(secs(2));
            readBuffer = "";
            resCurl    = curl_easy_perform(params.curl);
        }
        json_error_t error;
        json_t       *root = json_loads(readBuffer.c_str(), 0, &error);
        while (!root) {
            *params.logFile << "<Bitstamp> Error with JSON:\n" << error.text << '\n'
                            << "<Bitstamp> Buffer:\n" << readBuffer << '\n'
                            << "<Bitstamp> Retrying..." << std::endl;
            sleep_for(secs(2));
            readBuffer = "";
            resCurl    = curl_easy_perform(params.curl);
            while (resCurl != CURLE_OK) {
                *params.logFile << "<Bitstamp> Error with cURL. Retry in 2 sec..." << std::endl;
                sleep_for(secs(2));
                readBuffer = "";
                resCurl    = curl_easy_perform(params.curl);
            }
            root       = json_loads(readBuffer.c_str(), 0, &error);
        }
        curl_easy_reset(params.curl);
        return root;
    } else {
        *params.logFile << "<Bitstamp> Error with cURL init." << std::endl;
        return NULL;
    }
}

