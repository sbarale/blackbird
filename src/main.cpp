#include <iostream>       // std::cout, std::endl
#include <iomanip>
#include <thread>         // std::this_thread::sleep_for
#include "components/symbol.h"
#include "components/result.h"
#include "utils/time_fun.h"
#include "utils/curl_fun.h"
#include "utils/db_fun.h"
#include "components/parameters.h"
#include "components/check_entry_exit.h"
#include "exchanges/bitfinex.h"
#include "utils/send_email.h"
#include "utils/utils.h"
#include <unistd.h>
#include <cmath>
#include "exchanges/btce.h"
#include "utils/ExchangeFactory.h"
#include <signal.h>

using std::this_thread::sleep_for;
using millisecs = std::chrono::milliseconds;
using secs      = std::chrono::seconds;

static bool interrupted = false;

void int_handler(int s) {
    std::cout << "Interrupted, cleaning up..." << std::endl;
    interrupted = true;
}

void loadExchanges(const Parameters &params, int numExch, vector<Symbol, allocator<Symbol>> &exchanges);
void getBidAndAskPrices(const string *dbTableName, const vector<AbstractExchange *, allocator<AbstractExchange *>> &pool, int numExch, vector<Symbol, allocator<Symbol>> &symbol, time_t currTime, Parameters &params, ofstream &logFile);
void writeBalances(const Parameters &params, ofstream &logFile, int numExch, const vector<Balance, allocator<Balance>> &balance, bool inMarket);
void verifyParameters(Parameters &params);
void checkTimeslot(const Parameters &params, const Result &res, bool inMarket, time_t &rawtime, time_t diffTime, ofstream &logFile, tm &timeinfo, time_t &currTime);
void loadExchangesConfiguration(Parameters &params, string *dbTableName, vector<AbstractExchange *, allocator<AbstractExchange *>> &pool);
void logCashExposure(Parameters &params, ofstream &logFile);
void waitToStart(const Parameters &params, ofstream &logFile, time_t &rawtime, tm &timeinfo);
void updateBalances(const vector<AbstractExchange *, allocator<AbstractExchange *>> &pool, Parameters &params, vector<Balance, allocator<Balance>> &balance);
void computeVolatility(Parameters &params, int numExch, vector<Symbol, allocator<Symbol>> &symbol, Result &res);
void sendOrdersAndWaitForCompletion(const vector<AbstractExchange *, allocator<AbstractExchange *>> &pool, const Result &res, double volumeLong, double volumeShort, double limPriceLong, double limPriceShort, Parameters &params, ofstream &logFile, string &longOrderId, string &shortOrderId);
void calculatePositions(const vector<AbstractExchange *, allocator<AbstractExchange *>> &pool, int numExch, time_t currTime, const vector<double, allocator<double>> &btcUsed, double volumeLong, double volumeShort, double limPriceLong, double limPriceShort, Parameters &params, ofstream &csvFile, ofstream &logFile, vector<Balance, allocator<Balance>> &balance, Result &res, bool &inMarket);
bool analyzeOpportinity(const Parameters &params, ofstream &logFile, Result &res);
bool computeLimitPricesBasedOnVolume(const vector<AbstractExchange *, allocator<AbstractExchange *>> &pool, const vector<Symbol, allocator<Symbol>> &symbol, int resultId, time_t currTime, Parameters &params, ofstream &logFile, Result &res, bool &inMarket, double &volumeLong, double &volumeShort, double &limPriceLong, double &limPriceShort, string &longOrderId, string &shortOrderId);

// 'main' function.
int main(int argc, char **argv) {
    struct sigaction sigIntHandler;

    sigIntHandler.sa_handler = int_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    sigaction(SIGINT, &sigIntHandler, NULL);

    std::cout << "Blackbird Bitcoin Arbitrage" << std::endl;
    std::cout << "DISCLAIMER: USE THE SOFTWARE AT YOUR OWN RISK\n" << std::endl;
    // Replaces the C++ global locale with the user-preferred locale
    std::locale                     mylocale("");
    // Loads all the parameters
    Parameters                      params("blackbird.conf");

    // Function arrays containing all the exchanges functions
    // using the 'typedef' declarations from above.
    std::string                     dbTableName[10];
    std::vector<AbstractExchange *> pool;

    verifyParameters(params);
    loadExchangesConfiguration(params, dbTableName, pool);

    // Creates the CSV file that will collect the trade results
    std::string   currDateTime = printDateTimeFileName();
    std::string   csvFileName  = "output/blackbird_result_" + currDateTime + ".csv";
    std::ofstream csvFile(csvFileName, std::ofstream::trunc);
    csvFile << "TRADE_ID,EXCHANGE_LONG,EXHANGE_SHORT,ENTRY_TIME,EXIT_TIME,DURATION,"
            << "TOTAL_EXPOSURE,BALANCE_BEFORE,BALANCE_AFTER,RETURN"
            << std::endl;
    // Creates the log file where all events will be saved
    //std::string   logFileName = "output/blackbird_log_" + currDateTime + ".log";
    std::string   logFileName = "output/blackbird.log";
    std::ofstream logFile(logFileName, std::ofstream::trunc);
    logFile.imbue(mylocale);
    logFile.precision(2);
    logFile << std::fixed;

    params.logFile = &logFile;
    // Log file header
    logFile << "--------------------------------------------" << std::endl;
    logFile << "|   Blackbird Bitcoin Arbitrage Log File   |" << std::endl;
    logFile << "--------------------------------------------\n" << std::endl;
    logFile << "Blackbird started on " << printDateTime() << "\n" << std::endl;

    logFile << "Connected to database \'" << params.dbFile << "\'\n" << std::endl;

    if (params.demoMode) {
        logFile << "Demo mode: trades won't be generated\n" << std::endl;
    }

    // Shows which pair we are trading (e.g. BTC/USD)
    logFile << "Pair traded: " << params.tradedPair() << "\n" << std::endl;
    std::cout << "Log file generated: " << logFileName << "\nBlackbird is running... (pid " << getpid() << ")\n"
              << std::endl;

    int                 numExch = params.nbExch();

    // The exchanges vector contains details about every exchange,
    // like fees, as specified in exchanges.h
    std::vector<Symbol> symbol;
    loadExchanges(params, numExch, symbol);

    // Inits cURL connections
    params.curl            = curl_easy_init();
    // Shows the spreads
    logFile << "[ Targets ]" << std::endl
            << std::setprecision(2)
            << "   Spread Entry:  " << params.spreadEntry * 100.0 << "%" << std::endl
            << "   Spread Target: " << params.spreadTarget * 100.0 << "%" << std::endl;

    // SpreadEntry and SpreadTarget have to be positive,
    // Otherwise we will loose money on every trade.
    if (params.spreadEntry <= 0.0) {
        logFile << "   WARNING: Spread Entry should be positive" << std::endl;
    }
    if (params.spreadTarget <= 0.0) {
        logFile << "   WARNING: Spread Target should be positive" << std::endl;
    }
    logFile << std::endl;

    logFile << "[ Current balances ]" << std::endl;
    // Gets the the balances from every exchange
    // This is only done when not in Demo mode.
    std::vector<Balance> balance(numExch);
    Result               res;
    res.reset();
    time_t   rawtime;
    tm       timeinfo;
    int      resultId      = 0;
    unsigned currIteration = 0;
    bool     stillRunning  = true;
    time_t   currTime;
    time_t   diffTime;

    // Checks for a restore.txt file, to see if
    // the program exited with an open position.
    bool     inMarket      = res.loadPartialResult("restore.txt");

    updateBalances(pool, params, balance);
    writeBalances(params, logFile, numExch, balance, inMarket);
    logCashExposure(params, logFile);
    waitToStart(params, logFile, rawtime, timeinfo);

    // Main analysis loop
    while (stillRunning && !interrupted) {
        checkTimeslot(params, res, inMarket, rawtime, diffTime, logFile, timeinfo, currTime);
        getBidAndAskPrices(dbTableName, pool, numExch, symbol, currTime, params, logFile);
        computeVolatility(params, numExch, symbol, res);

        // Looks for arbitrage opportunities on all the exchange combinations
        if (!inMarket) {
            for (int i = 0; i < numExch; ++i) {
                for (int j = 0; j < numExch; ++j) {
                    if (i != j) {
                        if (checkEntry(&symbol[i], &symbol[j], res, params)) {

                            double volumeLong;
                            double volumeShort;
                            double limPriceLong;
                            double limPriceShort;
                            string longOrderId;
                            string shortOrderId;
                            bool   shouldContinue = true;

                            // An entry opportunity has been found!
                            res.exposure = std::min(balance[res.idExchLong].leg2, balance[res.idExchShort].leg2);

                            shouldContinue = analyzeOpportinity(params, logFile, res);
                            if (!shouldContinue)
                                break;

                            shouldContinue = computeLimitPricesBasedOnVolume(pool, symbol, resultId, currTime, params, logFile, res, inMarket, volumeLong, volumeShort, limPriceLong, limPriceShort, longOrderId, shortOrderId);
                            if (!shouldContinue)
                                break;

                            sendOrdersAndWaitForCompletion(pool, res, volumeLong, volumeShort, limPriceLong, limPriceShort, params, logFile, longOrderId, shortOrderId);

                            // Stores the partial result to file in case
                            // the program exits before closing the position.
                            res.savePartialResult("restore.txt");

                            // Resets the order ids
                            longOrderId  = "0";
                            shortOrderId = "0";
                            break;
                        }
                    }
                }
                if (inMarket) {
                    break;
                }
            }
            if (params.verbose) {
                logFile << std::endl;
            }
        } else if (inMarket) {
            // We are in market and looking for an exit opportunity
            if (checkExit(&symbol[res.idExchLong], &symbol[res.idExchShort], res, params, currTime)) {
                // An exit opportunity has been found!
                // We check the current leg1 exposure
                std::vector<double> btcUsed(numExch);
                for (int            i             = 0; i < numExch; ++i) {
                    btcUsed[i] = pool[i]->getActivePos(params);
                }
                // Checks the volumes and computes the limit prices that will be sent to the exchanges
                double              volumeLong    = btcUsed[res.idExchLong];
                double              volumeShort   = btcUsed[res.idExchShort];
                double              limPriceLong  = pool[res.idExchLong]->getLimitPrice(params, volumeLong, true);
                double              limPriceShort = pool[res.idExchShort]->getLimitPrice(params, volumeShort, false);

                calculatePositions(pool, numExch, currTime, btcUsed, volumeLong, volumeShort, limPriceLong, limPriceShort, params, csvFile, logFile, balance, res, inMarket);
            }
            if (params.verbose)
                logFile << std::endl;
        }
        // Moves to the next iteration, unless
        // the maxmum is reached.
        timeinfo.tm_sec += params.interval;
        currIteration++;
        if (currIteration >= params.debugMaxIteration) {
            logFile << "Max iteration reached (" << params.debugMaxIteration << ")" << std::endl;
            stillRunning = false;
        }
        // Exits if a 'stop_after_notrade' file is found
        // Warning: by default on GitHub the file has a underscore
        // at the end, so Blackbird is not stopped by default.
        std::ifstream infile("stop_after_notrade");
        if (infile && !inMarket) {
            logFile << "Exit after last trade (file stop_after_notrade found)\n";
            stillRunning = false;
        }
    }
    // Analysis loop exited, does some cleanup

    for (int i             = 0; i < pool.size(); i++) {
        delete (pool[i]);
    }

    curl_easy_cleanup(params.curl);
    csvFile.close();
    logFile.close();

    return 0;
}

bool computeLimitPricesBasedOnVolume(const vector<AbstractExchange *, allocator<AbstractExchange *>> &pool, const vector<Symbol, allocator<Symbol>> &symbol, int resultId, time_t currTime, Parameters &params, ofstream &logFile, Result &res, bool &inMarket, double &volumeLong, double &volumeShort, double &limPriceLong, double &limPriceShort, string &longOrderId, string &shortOrderId) {
    volumeLong    = res.exposure / symbol[res.idExchLong].getAsk();
    volumeShort   = res.exposure / symbol[res.idExchShort].getBid();
    limPriceLong  = pool[res.idExchLong]->getLimitPrice(params, volumeLong, false);
    limPriceShort = pool[res.idExchShort]->getLimitPrice(params, volumeShort, true);// Checks the volumes and, based on that, computes the limit prices
    // that will be sent to the exchanges
    if (limPriceLong == 0.0 || limPriceShort == 0.0) {
        logFile
                << "WARNING: Opportunity found but error with the order books (limit price is null). Trade canceled\n";
        logFile.precision(2);
        logFile << "         Long limit price:  " << limPriceLong << endl;
        logFile << "         Short limit price: " << limPriceShort << endl;
        res.trailing[res.idExchLong][res.idExchShort] = -1.0;
        return false;
    }

    if (limPriceLong - res.priceLongIn > params.priceDeltaLim ||
        res.priceShortIn - limPriceShort > params.priceDeltaLim) {
        logFile << "WARNING: Opportunity found but not enough liquidity. Trade canceled\n";
        logFile.precision(2);
        logFile << "         Target long price:  " << res.priceLongIn << ", Real long price:  "
                << limPriceLong << endl;
        logFile << "         Target short price: " << res.priceShortIn << ", Real short price: "
                << limPriceShort << endl;
        res.trailing[res.idExchLong][res.idExchShort] = -1.0;
        return false;
    }
    // We are in market now, meaning we have positions on leg1 (the hedged on)
    // We store the details of that first trade into the Result structure.
    inMarket = true;
    resultId++;

    res.id           = resultId;
    res.entryTime    = currTime;
    res.priceLongIn  = limPriceLong;
    res.priceShortIn = limPriceShort;
    res.printEntryInfo(*params.logFile);
    res.maxSpread[res.idExchLong][res.idExchShort] = -1.0;
    res.minSpread[res.idExchLong][res.idExchShort] = 1.0;
    res.trailing[res.idExchLong][res.idExchShort]  = 1.0;
    return true;
}

bool analyzeOpportinity(const Parameters &params, ofstream &logFile, Result &res) {
    if (params.demoMode) {
        logFile << "INFO: Opportunity found but no trade will be generated (Demo mode)"
                << endl;
        return false;
    }
    if (res.exposure == 0.0) {
        logFile << "WARNING: Opportunity found but no cash available. Trade canceled"
                << endl;
        return false;
    }
    if (params.useFullExposure == false && res.exposure <= params.testedExposure) {
        logFile
                << "WARNING: Opportunity found but no enough cash. Need more than TEST cash (min. $"
                << setprecision(2) << params.testedExposure << "). Trade canceled"
                << endl;
        return false;
    }
    if (params.useFullExposure) {
        // Removes 1% of the exposure to have
        // a little bit of margin.
        res.exposure -= 0.01 * res.exposure;
        if (res.exposure > params.maxExposure) {
            logFile << "WARNING: Opportunity found but exposure ("
                    << setprecision(2)
                    << res.exposure << ") above the limit\n"
                    << "         Max exposure will be used instead (" << params.maxExposure
                    << ")" << endl;
            res.exposure = params.maxExposure;
        }
    } else {
        res.exposure = params.testedExposure;
    }
    return true;
}

void calculatePositions(const vector<AbstractExchange *, allocator<AbstractExchange *>> &pool, int numExch, time_t currTime, const vector<double, allocator<double>> &btcUsed, double volumeLong, double volumeShort, double limPriceLong, double limPriceShort, Parameters &params, ofstream &csvFile, ofstream &logFile, vector<Balance, allocator<Balance>> &balance, Result &res, bool &inMarket) {
    if (limPriceLong == 0.0 || limPriceShort == 0.0) {
        logFile
                << "WARNING: Opportunity found but error with the order books (limit price is null). Trade canceled\n";
        logFile.precision(2);
        logFile << "         Long limit price:  " << limPriceLong << endl;
        logFile << "         Short limit price: " << limPriceShort << endl;
        res.trailing[res.idExchLong][res.idExchShort] = 1.0;
    } else if (res.priceLongOut - limPriceLong > params.priceDeltaLim ||
               limPriceShort - res.priceShortOut > params.priceDeltaLim) {
        logFile << "WARNING: Opportunity found but not enough liquidity. Trade canceled\n";
        logFile.precision(2);
        logFile << "         Target long price:  " << res.priceLongOut << ", Real long price:  "
                << limPriceLong << endl;
        logFile << "         Target short price: " << res.priceShortOut << ", Real short price: "
                << limPriceShort << endl;
        res.trailing[res.idExchLong][res.idExchShort] = 1.0;
    } else {
        res.exitTime      = currTime;
        res.priceLongOut  = limPriceLong;
        res.priceShortOut = limPriceShort;
        res.printExitInfo(*params.logFile);

        logFile.precision(6);
        logFile << params.leg1 << " exposure on " << params.exchName[res.idExchLong] << ": " << volumeLong
                << '\n'
                << params.leg1 << " exposure on " << params.exchName[res.idExchShort] << ": " << volumeShort
                << '\n'
                << endl;
        auto longOrderId  = pool[res.idExchLong]->sendLongOrder(params, "sell", fabs(btcUsed[res.idExchLong]),
                                                                limPriceLong);
        auto shortOrderId = pool[res.idExchShort]->sendShortOrder(params, "buy", fabs(btcUsed[res.idExchShort]),
                                                                  limPriceShort);
        logFile << "Waiting for the two orders to be filled..." << endl;
        sleep_for(millisecs(5000));
        bool isLongOrderComplete  = pool[res.idExchLong]->isOrderComplete(params, longOrderId);
        bool isShortOrderComplete = pool[res.idExchShort]->isOrderComplete(params, shortOrderId);
        // Loops until both orders are completed
        while (!isLongOrderComplete || !isShortOrderComplete) {
            sleep_for(millisecs(3000));
            if (!isLongOrderComplete) {
                logFile << "Long order on " << params.exchName[res.idExchLong] << " still open..."
                        << endl;
                isLongOrderComplete = pool[res.idExchLong]->isOrderComplete(params, longOrderId);
            }
            if (!isShortOrderComplete) {
                logFile << "Short order on " << params.exchName[res.idExchShort] << " still open..."
                        << endl;
                isShortOrderComplete = pool[res.idExchShort]->isOrderComplete(params, shortOrderId);
            }
        }
        logFile << "Done\n" << endl;
        longOrderId  = "0";
        shortOrderId = "0";
        inMarket     = false;
        for (int i = 0; i < numExch; ++i) {
            balance[i].leg2After = pool[i]->getAvail(params, "usd"); // FIXME: currency hard-coded
            balance[i].leg1After = pool[i]->getAvail(params, "btc"); // FIXME: currency hard-coded
        }
        for (int i = 0; i < numExch; ++i) {
            logFile << "New balance on " << params.exchName[i] << ":  \t";
            logFile.precision(2);
            logFile << balance[i].leg2After << " " << params.leg2 << " (perf "
                    << balance[i].leg2After - balance[i].leg2 << "), ";
            logFile << setprecision(6) << balance[i].leg1After << " " << params.leg1 << "\n";
        }
        logFile << endl;
        // Update total leg2 balance
        for (int i = 0; i < numExch; ++i) {
            res.leg2TotBalanceBefore += balance[i].leg2;
            res.leg2TotBalanceAfter += balance[i].leg2After;
        }
        // Update current balances
        for (int i = 0; i < numExch; ++i) {
            balance[i].leg2 = balance[i].leg2After;
            balance[i].leg1 = balance[i].leg1After;
        }
        // Prints the result in the result CSV file
        logFile.precision(2);
        logFile << "ACTUAL PERFORMANCE: " << "$" << res.leg2TotBalanceAfter - res.leg2TotBalanceBefore
                << " (" << res.actualPerf() * 100.0 << "%)\n" << endl;
        csvFile << res.id << ","
                << res.exchNameLong << ","
                << res.exchNameShort << ","
                << printDateTimeCsv(res.entryTime) << ","
                << printDateTimeCsv(res.exitTime) << ","
                << res.getTradeLengthInMinute() << ","
                << res.exposure * 2.0 << ","
                << res.leg2TotBalanceBefore << ","
                << res.leg2TotBalanceAfter << ","
                << res.actualPerf() << endl;
        // Sends an email with the result of the trade
        if (params.sendEmail) {
            sendEmail(res, params);
            logFile << "Email sent" << endl;
        }
        res.reset();
        // Removes restore.txt since this trade is done.
        ofstream resFile("restore.txt", ios_base::trunc);
        resFile.close();
    }
}

void sendOrdersAndWaitForCompletion(const vector<AbstractExchange *, allocator<AbstractExchange *>> &pool, const Result &res, double volumeLong, double volumeShort, double limPriceLong, double limPriceShort, Parameters &params, ofstream &logFile, string &longOrderId, string &shortOrderId) {
    longOrderId               = pool[res.idExchLong]->sendLongOrder(params, "buy", volumeLong, limPriceLong);
    shortOrderId              = pool[res.idExchShort]->sendShortOrder(params, "sell", volumeShort,
                                                                      limPriceShort);// Send the orders to the two exchangeslogFile << "Waiting for the two orders to be filled..." << endl;
    sleep_for(millisecs(5000));
    bool isLongOrderComplete  = pool[res.idExchLong]->isOrderComplete(params, longOrderId);
    bool isShortOrderComplete = pool[res.idExchShort]->isOrderComplete(params, shortOrderId);
    // Loops until both orders are completed
    while (!isLongOrderComplete || !isShortOrderComplete) {
        sleep_for(millisecs(3000));
        if (!isLongOrderComplete) {
            logFile << "Long order on " << params.exchName[res.idExchLong] << " still open..."
                    << endl;
            isLongOrderComplete = pool[res.idExchLong]->isOrderComplete(params, longOrderId);
        }
        if (!isShortOrderComplete) {
            logFile << "Short order on " << params.exchName[res.idExchShort] << " still open..."
                    << endl;
            isShortOrderComplete = pool[res.idExchShort]->isOrderComplete(params, shortOrderId);
        }
    }
    // Both orders are now fully executed
    logFile << "Done" << endl;
}

void computeVolatility(Parameters &params, int numExch, vector<Symbol, allocator<Symbol>> &symbol, Result &res) {// Stores all the spreads in arrays to
    // compute the volatility. The volatility
    // is not used for the moment.
    if (params.useVolatility) {
        for (int i = 0; i < numExch; ++i) {
            for (int j = 0; j < numExch; ++j) {
                if (i != j) {
                    if (symbol[j].getHasShort()) {
                        double longMidPrice  = symbol[i].getMidPrice();
                        double shortMidPrice = symbol[j].getMidPrice();
                        if (longMidPrice > 0.0 && shortMidPrice > 0.0) {
                            if (res.volatility[i][j].size() >= params.volatilityPeriod) {
                                res.volatility[i][j].pop_back();
                            }
                            res.volatility[i][j].push_front((longMidPrice - shortMidPrice) / longMidPrice);
                        }
                    }
                }
            }
        }
    }
}

void updateBalances(const vector<AbstractExchange *, allocator<AbstractExchange *>> &pool, Parameters &params, vector<Balance, allocator<Balance>> &balance) {
    if (!params.demoMode) {
        // TODO: Iterate through the pool vector and pull the balances.

        //
        //    std::transform(getAvail, getAvail + numExch,
        //                   begin(balance),
        //                   [&params](decltype(*getAvail) apply) {
        //                       Balance tmp{};
        //                       tmp.leg1 = apply(params, "btc");
        //                       tmp.leg2 = apply(params, "usd");
        //                       return tmp;
        //                   });

        /*
         * TODO: Verify that it replaces the above functionality
         */
        for (int i = 0; i < pool.size(); i++) {
            balance[i].leg1 = pool[i]->getAvail(params, "btc");
            balance[i].leg2 = pool[i]->getAvail(params, "usd");
        }

    }
}

void waitToStart(const Parameters &params, ofstream &logFile, time_t &rawtime, tm &timeinfo) {
    rawtime  = time(nullptr);
    timeinfo = *localtime(&rawtime);// Code implementing the loop function, that runs
    // every 'Interval' seconds.// Waits for the next 'interval' seconds before starting the loop
    while ((int) timeinfo.tm_sec % params.interval != 0) {
        this_thread::sleep_for(millisecs(100));
        time(&rawtime);
        timeinfo = *localtime(&rawtime);
    }

    if (!params.verbose) {
        logFile << "Running..." << endl;
    }
}

void logCashExposure(Parameters &params, ofstream &logFile) {
    logFile << "[ Cash exposure ]\n";
    if (params.demoMode) {
        logFile << "   No cash - Demo mode\n";
    } else {
        if (params.useFullExposure) {
            logFile << "   FULL exposure used!\n";
        } else {
            logFile << "   TEST exposure used\n   Value: "
                    << setprecision(2) << params.testedExposure << '\n';
        }
    }
    logFile << endl;
}

void loadExchangesConfiguration(Parameters &params, string *dbTableName, vector<AbstractExchange *, allocator<AbstractExchange *>> &pool) {// Adds the exchange functions to the arrays for all the defined exchanges
    int      index = 0;
    for (int i     = 0; i < params.exchanges.size(); ++i) {
        AbstractExchange *e = ExchangeFactory::make(params.exchanges[i]);
        if (e->config.enabled && e->canTrade(params.tradedPair())) {
            pool.push_back(e);
            params.addExchange(params.exchanges[i], e->config.fees.transaction, true, true);
            dbTableName[index] = e->exchange_name;
            createTable(dbTableName[index], params);
            index++;
        } else {
            std::cout << "Exchange " << e->exchange_name << " disabled or cannot trade current pair ("
                      << params.tradedPair() << ")" << std::endl;
        }
    }

    // We need at least two exchanges to run Blackbird
    if (index < 2) {
        cout
                << "ERROR: Blackbird needs at least two Bitcoin exchanges. Please edit the config.json file to add new exchanges\n"
                << endl;
        exit(EXIT_FAILURE);
    }
}

void checkTimeslot(const Parameters &params, const Result &res, bool inMarket, time_t &rawtime, time_t diffTime, ofstream &logFile, tm &timeinfo, time_t &currTime) {
    currTime = mktime(&timeinfo);
    time(&rawtime);
    diffTime = difftime(rawtime, currTime);
    // Checks if we are already too late in the current iteration
    // If that's the case we wait until the next iteration
    // and we show a warning in the log file.
    if (diffTime > 0) {
        logFile << "WARNING: " << diffTime << " second(s) too late at " << printDateTime(currTime) << endl;
        timeinfo.tm_sec += (ceil(diffTime / params.interval) + 1) * params.interval;
        currTime = mktime(&timeinfo);
        this_thread::sleep_for(std::chrono::seconds(params.interval - (diffTime % params.interval)));
        logFile << endl;
    } else if (diffTime < 0) {
        this_thread::sleep_for(std::chrono::seconds(-diffTime));
    }
    // Header for every iteration of the loop
    if (params.verbose) {
        if (!inMarket) {
            logFile << "[ " << printDateTime(currTime) << " ]" << endl;
        } else {
            logFile << "[ " << printDateTime(currTime) << " IN MARKET: Long " << res.exchNameLong << " / Short "
                    << res.exchNameShort << " ]" << endl;
        }
    }
    if (params.verbose) {
        logFile << "   ----------------------------" << std::endl;
    }
}

void verifyParameters(Parameters &params) {// Does some verifications about the parameters
    if (!params.demoMode) {
        if (!params.useFullExposure) {
            if (params.testedExposure < 10.0 && params.leg2.compare("USD") == 0) {
                // TODO do the same check for other currencies. Is there a limit ?
                cout << "ERROR: Minimum USD needed: $10.00" << endl;
                cout << "       Otherwise some exchanges will reject the orders\n" << endl;
                exit(EXIT_FAILURE);
            }
            if (params.testedExposure > params.maxExposure) {
                cout << "ERROR: Test exposure (" << params.testedExposure << ") is above max exposure ("
                     << params.maxExposure << ")\n" << endl;
                exit(EXIT_FAILURE);
            }
        }
    }

    // Connects to the SQLite3 database.
    // This database is used to collect bid and ask information
    // from the exchanges. Not really used for the moment, but
    // would be useful to collect historical bid/ask data.
    if (createDbConnection(params) != 0) {
        std::cerr << "ERROR: cannot connect to the database \'" << params.dbFile << "\'\n" << std::endl;
        exit(EXIT_FAILURE);
    }

    // We only trade BTC/USD and ETH/BTC for the moment
    if (params.tradedPair().compare("BTC/USD") != 0 && params.tradedPair().compare("ETH/BTC") != 0) {
        std::cout << "ERROR: Pair '" << params.tradedPair()
                  << "' is unknown. Valid pairs for now are BTC/USD and ETH/BTC\n" << std::endl;
        exit(EXIT_FAILURE);
    }

    // ETH/BTC is not fully implemented yet, but the spreads are shown in Demo mode
    if (params.tradedPair().compare("ETH/BTC") == 0 && !params.demoMode == false) {
        std::cout << "ERROR: ETH/BTC is only available in Demo mode\n" << std::endl;
        exit(EXIT_FAILURE);
    }

}

void writeBalances(const Parameters &params, ofstream &logFile, int numExch, const vector<Balance, allocator<Balance>> &balance, bool inMarket) {// Writes the current balances into the log file
    for (int i = 0; i < numExch; ++i) {
        logFile << "   " << params.exchName[i] << ":\t";
        if (params.demoMode) {
            logFile << "n/a (demo mode)" << endl;
        } else if (!params.isImplemented[i]) {
            logFile << "n/a (API not implemented)" << endl;
        } else {
            logFile << setprecision(2) << balance[i].leg2 << " " << params.leg2 << "\t"
                    << setprecision(6) << balance[i].leg1 << " " << params.leg1 << endl;
        }
        if (balance[i].leg1 > 0.0050 && !inMarket) { // FIXME: hard-coded number
            logFile << "ERROR: All " << params.leg1 << " accounts must be empty before starting Blackbird" << endl;
            exit(EXIT_FAILURE);
        }
    }
    logFile << endl;
}

void getBidAndAskPrices(const string *dbTableName, const vector<AbstractExchange *, allocator<AbstractExchange *>> &pool, int numExch, vector<Symbol, allocator<Symbol>> &symbol, time_t currTime, Parameters &params, ofstream &logFile) {
    // Gets the bid and ask of all the exchanges
    for (int i = 0; i < numExch; ++i) {
        AbstractExchange *e    = pool[i];
        quote_t          quote = e->getQuote(params);
        double           bid   = quote.bid();
        double           ask   = quote.ask();

        // Saves the bid/ask into the SQLite database
        addBidAskToDb(dbTableName[i], printDateTimeDb(currTime), bid, ask, params);

        // If there is an error with the bid or ask (i.e. value is null),
        // we show a warning but we don't stop the loop.
        if (bid == 0.0) {
            logFile << "   WARNING: " << params.exchName[i] << " bid is null" << endl;
        }
        if (ask == 0.0) {
            logFile << "   WARNING: " << params.exchName[i] << " ask is null" << endl;
        }
        // Shows the bid/ask information in the log file
        if (params.verbose) {
            logFile << "   " << params.exchName[i] << ": \t"
                    << setprecision(4)
                    << bid << " / " << ask << endl;
            // TODO: precision should be:
            // 2 for USD
            // 4 for cryptocurrencies
        }
        // Updates the Bitcoin vector with the latest bid/ask data
        symbol[i].updateData(quote);
        curl_easy_reset(params.curl);
    }
}

void loadExchanges(const Parameters &params, int numExch, vector<Symbol, allocator<Symbol>> &exchanges) {
    exchanges.reserve(numExch);
    // Creates a new Symbol structure within exchanges for every exchange we want to trade on
    for (int i = 0; i < numExch; ++i) {
        exchanges.push_back(Symbol(i, params.exchName[i], params.fees[i], params.canShort[i], params.isImplemented[i]));
    }
}
