#ifndef CHECK_ENTRY_EXIT_H
#define CHECK_ENTRY_EXIT_H

#include <string>
#include <ctime>


class  Symbol;
struct Result;
struct Parameters;

std::string percToStr(double perc);

// Checks for entry opportunity between two exchanges
// and returns True if an opporunity is found.
bool checkEntry(Symbol* btcLong, Symbol* btcShort, Result& res, Parameters& params);

// Checks for exit opportunity between two exchanges
// and returns True if an opporunity is found.
bool checkExit(Symbol* btcLong, Symbol* btcShort, Result& res, Parameters& params, std::time_t period);

#endif

