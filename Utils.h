/*
 * =============================================================================
 *  File    : Utils.h
 *  Purpose : General helper functions used by every module:
 *            - text helpers (trim, upper/lower case, split)
 *            - number and money formatting (e.g. 1250000 -> "1,250,000")
 *            - date arithmetic (today, add months, days between dates)
 *            - screen output helpers (titles, lines, coloured-style messages)
 * =============================================================================
 */
#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include "Config.h"

namespace Utils
{
    // ---------------- Text helpers ----------------
    std::string trim(const std::string& text);
    std::string toUpper(std::string text);
    std::string toLower(std::string text);
    bool containsIgnoreCase(const std::string& text, const std::string& part);
    bool equalsIgnoreCase(const std::string& first, const std::string& second);
    std::vector<std::string> split(const std::string& line, char delimiter);
    std::string fitWidth(const std::string& text, std::size_t width);
    std::string padRight(const std::string& text, std::size_t width);
    std::string zeroPad(int number, int width);
    std::string removeChar(const std::string& text, char unwanted);
    bool isAllDigits(const std::string& text);

    // ---------------- Number and money helpers ----------------
    bool parseInt(const std::string& text, int& result);
    bool parseDouble(const std::string& text, double& result);
    double numberOrThrow(const std::string& text);   // throws std::invalid_argument
    int    wholeNumberOrThrow(const std::string& text);
    std::string toString(int value);
    std::string toString(double value, int decimals);
    double roundMoney(double amount);            // round to the nearest shilling
    std::string formatMoney(double amount);      // 1250000 -> "1,250,000"
    std::string formatUGX(double amount);        // 1250000 -> "UGX 1,250,000"

    // ---------------- Date helpers (all dates use the format YYYY-MM-DD) ----------------
    std::string today();
    std::string currentTime();                   // HH:MM:SS
    bool isLeapYear(int year);
    int  daysInMonth(int year, int month);
    bool splitDate(const std::string& date, int& year, int& month, int& day);
    bool isValidDate(const std::string& date);
    std::string makeDate(int year, int month, int day);
    std::string addMonths(const std::string& date, int months);
    long daysBetween(const std::string& fromDate, const std::string& toDate);
    int  fullMonthsBetween(const std::string& fromDate, const std::string& toDate);

    // ---------------- Screen helpers ----------------
    void clearScreen();
    void printLine(char symbol = '-', int width = Config::SCREEN_WIDTH);
    void printCentered(const std::string& text, int width = Config::SCREEN_WIDTH);
    void printTitle(const std::string& title);
    void printSuccess(const std::string& message);
    void printError(const std::string& message);
    void printWarning(const std::string& message);
    void printInfo(const std::string& message);
}

#endif // UTILS_H
