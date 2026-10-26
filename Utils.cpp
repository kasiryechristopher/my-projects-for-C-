/*
 * =============================================================================
 *  File    : Utils.cpp
 *  Purpose : Implementation of the general helper functions declared in Utils.h
 * =============================================================================
 */
#include "Utils.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <cstdio>
#include <stdexcept>

// Detect whether output goes to a real screen (terminal) or to a file/pipe.
#ifdef _WIN32
    #include <io.h>
    #define OUTPUT_IS_TERMINAL() (_isatty(_fileno(stdout)) != 0)
#else
    #include <unistd.h>
    #define OUTPUT_IS_TERMINAL() (isatty(STDOUT_FILENO) != 0)
#endif

namespace Utils
{
// =============================================================================
//  TEXT HELPERS
// =============================================================================

// Removes spaces, tabs and line breaks from both ends of the text.
std::string trim(const std::string& text)
{
    const std::string spaces = " \t\r\n";
    std::size_t start = text.find_first_not_of(spaces);
    if (start == std::string::npos)
    {
        return "";
    }
    std::size_t end = text.find_last_not_of(spaces);
    return text.substr(start, end - start + 1);
}

std::string toUpper(std::string text)
{
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        text[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(text[i])));
    }
    return text;
}

std::string toLower(std::string text)
{
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        text[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
    }
    return text;
}

// True when 'part' appears anywhere inside 'text', ignoring upper/lower case.
bool containsIgnoreCase(const std::string& text, const std::string& part)
{
    return toLower(text).find(toLower(part)) != std::string::npos;
}

bool equalsIgnoreCase(const std::string& first, const std::string& second)
{
    return toLower(first) == toLower(second);
}

// Breaks one record line into its fields, e.g. "a|b||c" -> {"a","b","","c"}.
std::vector<std::string> split(const std::string& line, char delimiter)
{
    std::vector<std::string> fields;
    std::string field;
    std::istringstream stream(line);

    while (std::getline(stream, field, delimiter))
    {
        fields.push_back(field);
    }
    // getline drops a final empty field (line ending with the delimiter), so add it back
    if (!line.empty() && line[line.size() - 1] == delimiter)
    {
        fields.push_back("");
    }
    return fields;
}

// Shortens long text so that it fits in a table column, e.g. "Ssemakula John..."
std::string fitWidth(const std::string& text, std::size_t width)
{
    if (text.size() <= width)
    {
        return text;
    }
    if (width <= 3)
    {
        return text.substr(0, width);
    }
    return text.substr(0, width - 3) + "...";
}

// Adds spaces on the right so text fills a fixed width (for neat log columns).
std::string padRight(const std::string& text, std::size_t width)
{
    std::string fitted = fitWidth(text, width);
    return fitted + std::string(width - fitted.size(), ' ');
}

// zeroPad(7, 4) -> "0007"
std::string zeroPad(int number, int width)
{
    std::ostringstream out;
    out << std::setw(width) << std::setfill('0') << number;
    return out.str();
}

std::string removeChar(const std::string& text, char unwanted)
{
    std::string result;
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] != unwanted)
        {
            result += text[i];
        }
    }
    return result;
}

bool isAllDigits(const std::string& text)
{
    if (text.empty())
    {
        return false;
    }
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        if (!std::isdigit(static_cast<unsigned char>(text[i])))
        {
            return false;
        }
    }
    return true;
}

// =============================================================================
//  NUMBER AND MONEY HELPERS
// =============================================================================

// Converts text to a whole number. Rejects text such as "12abc" or "4.5".
bool parseInt(const std::string& text, int& result)
{
    std::istringstream stream(trim(text));
    int value = 0;
    char leftover = '\0';

    if (!(stream >> value))
    {
        return false;              // not a number at all, or too large
    }
    if (stream >> leftover)
    {
        return false;              // extra characters after the number
    }
    result = value;
    return true;
}

// Converts text to a decimal number. Rejects text such as "50k" or "abc".
bool parseDouble(const std::string& text, double& result)
{
    std::istringstream stream(trim(text));
    double value = 0.0;
    char leftover = '\0';

    if (!(stream >> value))
    {
        return false;
    }
    if (stream >> leftover)
    {
        return false;
    }
    if (value != value || value > 1e15 || value < -1e15)
    {
        return false;              // reject NaN and absurdly large values
    }
    result = value;
    return true;
}

// Strict conversions used when reading data files. They THROW an exception
// (std::invalid_argument) for damaged text, so the caller can catch it.
// (Written by hand instead of std::stod/std::stoi, which some older MinGW
// compilers on Windows do not provide.)
double numberOrThrow(const std::string& text)
{
    double value = 0.0;
    if (!parseDouble(text, value))
    {
        throw std::invalid_argument("Not a valid number: '" + text + "'");
    }
    return value;
}

int wholeNumberOrThrow(const std::string& text)
{
    int value = 0;
    if (!parseInt(text, value))
    {
        throw std::invalid_argument("Not a valid whole number: '" + text + "'");
    }
    return value;
}

std::string toString(int value)
{
    std::ostringstream out;
    out << value;
    return out.str();
}

std::string toString(double value, int decimals)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(decimals) << value;
    return out.str();
}

// Uganda Shillings have no cents in daily use, so money is rounded to whole shillings.
double roundMoney(double amount)
{
    if (amount >= 0)
    {
        return std::floor(amount + 0.5);
    }
    return -std::floor(-amount + 0.5);
}

// Adds thousands separators: 1250000 -> "1,250,000"
std::string formatMoney(double amount)
{
    double rounded = roundMoney(amount);
    bool negative = rounded < 0;
    if (negative)
    {
        rounded = -rounded;
    }

    std::ostringstream raw;
    raw << std::fixed << std::setprecision(0) << rounded;
    std::string digits = raw.str();

    std::string result;
    int counter = 0;
    for (int i = static_cast<int>(digits.size()) - 1; i >= 0; --i)
    {
        result.insert(result.begin(), digits[i]);
        ++counter;
        if (counter % 3 == 0 && i > 0)
        {
            result.insert(result.begin(), ',');
        }
    }
    return negative ? "-" + result : result;
}

std::string formatUGX(double amount)
{
    return "UGX " + formatMoney(amount);
}

// =============================================================================
//  DATE HELPERS
// =============================================================================

// Reads the computer clock safely on every compiler.
static std::tm localNow()
{
    std::time_t now = std::time(0);
    std::tm local;
#if defined(_MSC_VER)
    localtime_s(&local, &now);       // Visual Studio's safe version
#else
    local = *std::localtime(&now);
#endif
    return local;
}

std::string today()
{
    std::tm local = localNow();
    return makeDate(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
}

std::string currentTime()
{
    std::tm local = localNow();
    std::ostringstream out;
    out << zeroPad(local.tm_hour, 2) << ":" << zeroPad(local.tm_min, 2) << ":" << zeroPad(local.tm_sec, 2);
    return out.str();
}

bool isLeapYear(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int daysInMonth(int year, int month)
{
    switch (month)
    {
        case 2:
            return isLeapYear(year) ? 29 : 28;
        case 4: case 6: case 9: case 11:
            return 30;
        default:
            return 31;
    }
}

// Splits "2026-10-13" into 2026, 10 and 13. Returns false for a badly formed date.
bool splitDate(const std::string& date, int& year, int& month, int& day)
{
    if (date.size() != 10 || date[4] != '-' || date[7] != '-')
    {
        return false;
    }
    std::string y = date.substr(0, 4), m = date.substr(5, 2), d = date.substr(8, 2);
    if (!isAllDigits(y) || !isAllDigits(m) || !isAllDigits(d))
    {
        return false;
    }
    return parseInt(y, year) && parseInt(m, month) && parseInt(d, day);
}

bool isValidDate(const std::string& date)
{
    int year = 0, month = 0, day = 0;
    if (!splitDate(date, year, month, day))
    {
        return false;
    }
    if (year < 1900 || year > 2100 || month < 1 || month > 12)
    {
        return false;
    }
    return day >= 1 && day <= daysInMonth(year, month);
}

std::string makeDate(int year, int month, int day)
{
    return zeroPad(year, 4) + "-" + zeroPad(month, 2) + "-" + zeroPad(day, 2);
}

// Adds (or subtracts) whole months. 2026-01-31 + 1 month -> 2026-02-28.
std::string addMonths(const std::string& date, int months)
{
    int year = 0, month = 0, day = 0;
    if (!splitDate(date, year, month, day))
    {
        return date;
    }
    int totalMonths = year * 12 + (month - 1) + months;
    int newYear = totalMonths / 12;
    int newMonth = totalMonths % 12 + 1;
    int lastDay = daysInMonth(newYear, newMonth);
    int newDay = (day > lastDay) ? lastDay : day;
    return makeDate(newYear, newMonth, newDay);
}

// Converts a calendar date into a running day number so two dates can be subtracted.
static long dayNumber(int year, int month, int day)
{
    // Standard "days from civil date" algorithm (works for any Gregorian date).
    year -= (month <= 2) ? 1 : 0;
    long era = (year >= 0 ? year : year - 399) / 400;
    long yearOfEra = year - era * 400;
    long dayOfYear = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    long dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    return era * 146097 + dayOfEra - 719468;
}

long daysBetween(const std::string& fromDate, const std::string& toDate)
{
    int y1 = 0, m1 = 0, d1 = 0, y2 = 0, m2 = 0, d2 = 0;
    if (!splitDate(fromDate, y1, m1, d1) || !splitDate(toDate, y2, m2, d2))
    {
        return 0;
    }
    return dayNumber(y2, m2, d2) - dayNumber(y1, m1, d1);
}

// Number of complete months from one date to another (never negative).
int fullMonthsBetween(const std::string& fromDate, const std::string& toDate)
{
    int y1 = 0, m1 = 0, d1 = 0, y2 = 0, m2 = 0, d2 = 0;
    if (!splitDate(fromDate, y1, m1, d1) || !splitDate(toDate, y2, m2, d2))
    {
        return 0;
    }
    int months = (y2 - y1) * 12 + (m2 - m1);
    if (d2 < d1)
    {
        --months;                  // the last month is not yet complete
    }
    return months < 0 ? 0 : months;
}

// =============================================================================
//  SCREEN HELPERS
// =============================================================================

// Clears the console only when output goes to a real screen
// (so saved test transcripts are not filled with control characters).
void clearScreen()
{
    if (!OUTPUT_IS_TERMINAL())
    {
        std::cout << "\n";
        return;
    }
#ifdef _WIN32
    std::system("cls");
#else
    std::cout << "\033[2J\033[H" << std::flush;
#endif
}

void printLine(char symbol, int width)
{
    std::cout << std::string(static_cast<std::size_t>(width), symbol) << "\n";
}

void printCentered(const std::string& text, int width)
{
    int padding = (width - static_cast<int>(text.size())) / 2;
    if (padding < 0)
    {
        padding = 0;
    }
    std::cout << std::string(static_cast<std::size_t>(padding), ' ') << text << "\n";
}

void printTitle(const std::string& title)
{
    std::cout << "\n  >> " << toUpper(title) << "\n";
    printLine('-');
}

void printSuccess(const std::string& message)
{
    std::cout << "  [SUCCESS] " << message << "\n";
}

void printError(const std::string& message)
{
    std::cout << "  [ERROR] " << message << "\n";
}

void printWarning(const std::string& message)
{
    std::cout << "  [WARNING] " << message << "\n";
}

void printInfo(const std::string& message)
{
    std::cout << "  [INFO] " << message << "\n";
}

} // namespace Utils
