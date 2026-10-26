/*
 * =============================================================================
 *  File    : InputHelper.cpp
 *  Purpose : Implementation of safe, validated keyboard input.
 *            Rule followed everywhere: read the WHOLE line as text first,
 *            then check it. This avoids the classic "cin >> number" crash
 *            loop when a user types letters.
 * =============================================================================
 */
#include "InputHelper.h"
#include "Utils.h"
#include "Config.h"

#include <iostream>
#include <sstream>
#include <cctype>
#include <cmath>
#include <cstdio>

#ifdef _WIN32
    #include <conio.h>     // _getch() for hidden password typing
    #include <io.h>
    #define INPUT_IS_TERMINAL() (_isatty(_fileno(stdin)) != 0)
#else
    #include <termios.h>   // to switch off screen echo for passwords
    #include <unistd.h>
    #define INPUT_IS_TERMINAL() (isatty(STDIN_FILENO) != 0)
#endif

namespace InputHelper
{
// =============================================================================
//  BASIC READING
// =============================================================================

bool inputIsKeyboard()
{
    return INPUT_IS_TERMINAL();
}

// Reads one full line. When input is redirected from a file (automated
// testing) the typed value is echoed so the saved transcript shows it.
std::string readLine(const std::string& prompt)
{
    std::cout << prompt << std::flush;
    std::string line;
    if (!std::getline(std::cin, line))
    {
        throw InputClosedException();
    }
    if (!line.empty() && line[line.size() - 1] == '\r')
    {
        line.erase(line.size() - 1);      // remove Windows line ending
    }
    if (!inputIsKeyboard())
    {
        std::cout << line << "\n";
    }
    return line;
}

// Text must be printable and must not contain the '|' symbol, because '|'
// separates fields in the data files.
bool isSafeText(const std::string& value)
{
    for (std::size_t i = 0; i < value.size(); ++i)
    {
        unsigned char ch = static_cast<unsigned char>(value[i]);
        if (ch == static_cast<unsigned char>(Config::FIELD_DELIMITER) || ch < 32 || ch == 127)
        {
            return false;
        }
    }
    return true;
}

std::string readText(const std::string& prompt, std::size_t minLength, std::size_t maxLength)
{
    while (true)
    {
        std::string text = Utils::trim(readLine(prompt));
        if (text.size() < minLength)
        {
            Utils::printError(minLength <= 1 ? "This field cannot be empty."
                              : "Please type at least " + Utils::toString(static_cast<int>(minLength)) + " characters.");
        }
        else if (text.size() > maxLength)
        {
            Utils::printError("Please type at most " + Utils::toString(static_cast<int>(maxLength)) + " characters.");
        }
        else if (!isSafeText(text))
        {
            Utils::printError("The '|' symbol and control characters are not allowed.");
        }
        else
        {
            return text;
        }
    }
}

std::string readValidated(const std::string& prompt, Validator validator)
{
    while (true)
    {
        std::string value = readLine(prompt);
        std::string error;
        if (validator(value, error))
        {
            return value;
        }
        Utils::printError(error);
    }
}

// Used when updating records: pressing Enter keeps the current value.
std::string readValidatedOrKeep(const std::string& prompt, const std::string& currentValue,
                                Validator validator)
{
    while (true)
    {
        std::string value = readLine(prompt + " [" + currentValue + "]: ");
        if (Utils::trim(value).empty())
        {
            return currentValue;
        }
        std::string error;
        if (validator(value, error))
        {
            return value;
        }
        Utils::printError(error);
    }
}

// =============================================================================
//  NUMBERS
// =============================================================================

int readMenuChoice(int minChoice, int maxChoice)
{
    while (true)
    {
        std::string text = readLine("\n  Enter your choice: ");
        int choice = 0;
        if (Utils::parseInt(text, choice) && choice >= minChoice && choice <= maxChoice)
        {
            return choice;
        }
        Utils::printError("Invalid choice. Please enter a number from " + Utils::toString(minChoice) +
                          " to " + Utils::toString(maxChoice) + ".");
    }
}

// Shows a limit in a friendly way for the error messages below.
static std::string describeLimit(double value, bool isMoney, bool wholeOnly)
{
    if (isMoney)
    {
        return Utils::formatUGX(value);
    }
    if (wholeOnly)
    {
        return Utils::toString(static_cast<int>(value));
    }
    return Utils::toString(value, 2);
}

// One core routine for every kind of number, so the checking rules are
// written once only (no repetition).
static double readNumberCore(const std::string& prompt, double minValue, double maxValue,
                             bool wholeOnly, bool isMoney, bool allowKeep, double currentValue)
{
    while (true)
    {
        std::string text = Utils::trim(readLine(prompt));
        if (isMoney)
        {
            text = Utils::removeChar(text, ',');   // accept "1,500,000"
        }
        if (text.empty())
        {
            if (allowKeep)
            {
                return currentValue;
            }
            Utils::printError(isMoney ? "Amount cannot be empty." : "A value is required.");
            continue;
        }

        double value = 0.0;
        if (!Utils::parseDouble(text, value))
        {
            Utils::printError(isMoney ? "Invalid amount. Type digits only, e.g. 50000."
                                      : "Invalid number. Type digits only.");
            continue;
        }
        if (value < 0)
        {
            Utils::printError(isMoney ? "Amount cannot be negative." : "Value cannot be negative.");
            continue;
        }
        if (wholeOnly && value != std::floor(value))
        {
            Utils::printError(isMoney ? "Enter whole shillings only (no decimals)." : "Please enter a whole number.");
            continue;
        }
        if (value < minValue)
        {
            Utils::printError("Value must be at least " + describeLimit(minValue, isMoney, wholeOnly) + ".");
            continue;
        }
        if (value > maxValue)
        {
            Utils::printError("Value must not be more than " + describeLimit(maxValue, isMoney, wholeOnly) + ".");
            continue;
        }
        return value;
    }
}

int readInt(const std::string& prompt, int minValue, int maxValue)
{
    return static_cast<int>(readNumberCore(prompt, minValue, maxValue, true, false, false, 0));
}

int readIntOrKeep(const std::string& prompt, int currentValue, int minValue, int maxValue)
{
    std::string fullPrompt = prompt + " [" + Utils::toString(currentValue) + "]: ";
    return static_cast<int>(readNumberCore(fullPrompt, minValue, maxValue, true, false, true, currentValue));
}

double readDecimal(const std::string& prompt, double minValue, double maxValue)
{
    return readNumberCore(prompt, minValue, maxValue, false, false, false, 0);
}

double readDecimalOrKeep(const std::string& prompt, double currentValue, double minValue, double maxValue)
{
    std::string fullPrompt = prompt + " [" + Utils::toString(currentValue, 2) + "]: ";
    return readNumberCore(fullPrompt, minValue, maxValue, false, false, true, currentValue);
}

double readAmount(const std::string& prompt, double minValue, double maxValue)
{
    return readNumberCore(prompt, minValue, maxValue, true, true, false, 0);
}

double readAmountOrKeep(const std::string& prompt, double currentValue, double minValue, double maxValue)
{
    std::string fullPrompt = prompt + " [" + Utils::formatMoney(currentValue) + "]: ";
    return readNumberCore(fullPrompt, minValue, maxValue, true, true, true, currentValue);
}

// =============================================================================
//  OTHER INPUT
// =============================================================================

bool readYesNo(const std::string& prompt)
{
    while (true)
    {
        std::string answer = Utils::toUpper(Utils::trim(readLine(prompt + " (Y/N): ")));
        if (answer == "Y" || answer == "YES")
        {
            return true;
        }
        if (answer == "N" || answer == "NO")
        {
            return false;
        }
        Utils::printError("Please type Y for Yes or N for No.");
    }
}

std::string readDate(const std::string& prompt, const std::string& defaultDate,
                     const std::string& earliestDate, const std::string& latestDate)
{
    while (true)
    {
        std::string text = Utils::trim(readLine(prompt + " [Enter = " + defaultDate + "]: "));
        if (text.empty())
        {
            return defaultDate;
        }
        if (!Utils::isValidDate(text))
        {
            Utils::printError("Invalid date. Use the format YYYY-MM-DD, e.g. 2026-03-15.");
        }
        else if (text < earliestDate || text > latestDate)   // ISO dates compare correctly as text
        {
            Utils::printError("Date must be between " + earliestDate + " and " + latestDate + ".");
        }
        else
        {
            return text;
        }
    }
}

// Reads a password while showing '*' instead of the real characters.
std::string readPassword(const std::string& prompt)
{
    std::cout << prompt << std::flush;
    std::string password;

    if (!inputIsKeyboard())
    {
        // Input redirected from a file (automated tests): read normally.
        if (!std::getline(std::cin, password))
        {
            throw InputClosedException();
        }
        if (!password.empty() && password[password.size() - 1] == '\r')
        {
            password.erase(password.size() - 1);
        }
        std::cout << std::string(password.size(), '*') << "\n";
        return password;
    }

#ifdef _WIN32
    while (true)
    {
        int ch = _getch();
        if (ch == '\r' || ch == '\n')
        {
            break;
        }
        if (ch == 0 || ch == 224)
        {
            _getch();                  // ignore arrow and function keys
        }
        else if (ch == '\b')
        {
            if (!password.empty())
            {
                password.erase(password.size() - 1);
                std::cout << "\b \b" << std::flush;
            }
        }
        else if (ch >= 32 && ch < 127)
        {
            password += static_cast<char>(ch);
            std::cout << '*' << std::flush;
        }
    }
#else
    termios oldSettings;
    tcgetattr(STDIN_FILENO, &oldSettings);
    termios hiddenSettings = oldSettings;
    hiddenSettings.c_lflag &= ~static_cast<tcflag_t>(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSANOW, &hiddenSettings);

    while (true)
    {
        int ch = std::getchar();
        if (ch == EOF)
        {
            tcsetattr(STDIN_FILENO, TCSANOW, &oldSettings);
            throw InputClosedException();
        }
        if (ch == '\n' || ch == '\r')
        {
            break;
        }
        if (ch == 127 || ch == 8)      // backspace
        {
            if (!password.empty())
            {
                password.erase(password.size() - 1);
                std::cout << "\b \b" << std::flush;
            }
        }
        else if (ch >= 32 && ch < 127)
        {
            password += static_cast<char>(ch);
            std::cout << '*' << std::flush;
        }
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &oldSettings);
#endif

    std::cout << "\n";
    return password;
}

void pause()
{
    readLine("\n  Press Enter to continue...");
}

// =============================================================================
//  VALIDATORS
// =============================================================================

// Full name: letters only (plus space ' - .), at least two names, 3-50 characters.
// Also tidies the name: extra spaces removed and each name starts with a capital.
bool validateName(std::string& value, std::string& error)
{
    std::string trimmed = Utils::trim(value);
    if (trimmed.empty())
    {
        error = "Name cannot be empty.";
        return false;
    }
    for (std::size_t i = 0; i < trimmed.size(); ++i)
    {
        char ch = trimmed[i];
        if (!std::isalpha(static_cast<unsigned char>(ch)) && ch != ' ' && ch != '\'' && ch != '-' && ch != '.')
        {
            error = "Name may contain letters, spaces, apostrophes (') and hyphens (-) only.";
            return false;
        }
    }

    std::istringstream words(trimmed);
    std::string word, cleaned;
    int wordCount = 0;
    while (words >> word)
    {
        word[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(word[0])));
        cleaned += (wordCount > 0 ? " " : "") + word;
        ++wordCount;
    }
    if (cleaned.size() < 3 || cleaned.size() > 50)
    {
        error = "Name must be 3 to 50 characters long.";
        return false;
    }
    if (wordCount < 2)
    {
        error = "Please enter at least two names, e.g. Nakato Sarah.";
        return false;
    }
    value = cleaned;
    return true;
}

// Uganda National Identification Number: 14 letters/digits starting with
// CM (male) or CF (female), e.g. CM85012345ABCD. Stored in capital letters.
bool validateNIN(std::string& value, std::string& error)
{
    std::string nin = Utils::toUpper(Utils::removeChar(Utils::trim(value), ' '));
    bool ok = nin.size() == 14 && (nin.substr(0, 2) == "CM" || nin.substr(0, 2) == "CF");
    for (std::size_t i = 0; ok && i < nin.size(); ++i)
    {
        if (!std::isalnum(static_cast<unsigned char>(nin[i])))
        {
            ok = false;
        }
    }
    if (!ok)
    {
        error = "NIN must be 14 letters/digits starting with CM or CF, e.g. CM85012345ABCD.";
        return false;
    }
    value = nin;
    return true;
}

// Ugandan mobile number. Accepts 0772123456, +256772123456 or 256772123456
// and always stores the short form 0772123456.
bool validatePhone(std::string& value, std::string& error)
{
    std::string phone = Utils::removeChar(Utils::removeChar(Utils::trim(value), ' '), '-');
    if (phone.size() == 13 && phone.substr(0, 4) == "+256")
    {
        phone = "0" + phone.substr(4);
    }
    else if (phone.size() == 12 && phone.substr(0, 3) == "256")
    {
        phone = "0" + phone.substr(3);
    }
    if (phone.size() != 10 || !Utils::isAllDigits(phone) || phone.substr(0, 2) != "07")
    {
        error = "Phone must be a Ugandan mobile number, e.g. 0772123456 or +256772123456.";
        return false;
    }
    value = phone;
    return true;
}

bool validateAddress(std::string& value, std::string& error)
{
    std::string address = Utils::trim(value);
    if (address.size() < 3 || address.size() > 60)
    {
        error = "Address must be 3 to 60 characters long (e.g. Ndejje Central Ward).";
        return false;
    }
    if (!isSafeText(address))
    {
        error = "Address must not contain the '|' symbol.";
        return false;
    }
    value = address;
    return true;
}

// Staff usernames: 3-20 characters, letters/digits/underscore, start with a letter.
bool validateUsername(std::string& value, std::string& error)
{
    std::string name = Utils::toLower(Utils::trim(value));
    bool ok = name.size() >= 3 && name.size() <= 20 && std::isalpha(static_cast<unsigned char>(name[0]));
    for (std::size_t i = 0; ok && i < name.size(); ++i)
    {
        if (!std::isalnum(static_cast<unsigned char>(name[i])) && name[i] != '_')
        {
            ok = false;
        }
    }
    if (!ok)
    {
        error = "Username must be 3-20 letters, digits or _ and start with a letter.";
        return false;
    }
    value = name;
    return true;
}

} // namespace InputHelper
