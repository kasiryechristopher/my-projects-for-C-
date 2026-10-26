/*
 * =============================================================================
 *  File    : InputHelper.h
 *  Purpose : Safe keyboard input. Every value typed by a user passes through
 *            these functions, which keep asking until the value is valid.
 *            This stops crashes caused by letters typed where numbers are
 *            expected, negative money, empty names, wrong NIN formats, etc.
 * =============================================================================
 */
#ifndef INPUT_HELPER_H
#define INPUT_HELPER_H

#include <string>
#include <stdexcept>

// Thrown when the input stream closes (Ctrl+Z / Ctrl+D, or the end of a
// redirected test file) so the program can save and exit cleanly instead of
// looping forever.
class InputClosedException : public std::runtime_error
{
public:
    InputClosedException() : std::runtime_error("The input stream was closed.") {}
};

namespace InputHelper
{
    // A validator checks a value (and may tidy it, e.g. upper-case a NIN).
    // It returns false and explains the problem in 'error' if the value is bad.
    typedef bool (*Validator)(std::string& value, std::string& error);

    // ---------------- Basic reading ----------------
    bool        inputIsKeyboard();
    std::string readLine(const std::string& prompt);
    std::string readText(const std::string& prompt, std::size_t minLength, std::size_t maxLength);
    std::string readValidated(const std::string& prompt, Validator validator);
    std::string readValidatedOrKeep(const std::string& prompt, const std::string& currentValue,
                                    Validator validator);

    // ---------------- Numbers ----------------
    int    readMenuChoice(int minChoice, int maxChoice);
    int    readInt(const std::string& prompt, int minValue, int maxValue);
    int    readIntOrKeep(const std::string& prompt, int currentValue, int minValue, int maxValue);
    double readDecimal(const std::string& prompt, double minValue, double maxValue);
    double readDecimalOrKeep(const std::string& prompt, double currentValue, double minValue, double maxValue);
    double readAmount(const std::string& prompt, double minValue, double maxValue);
    double readAmountOrKeep(const std::string& prompt, double currentValue, double minValue, double maxValue);

    // ---------------- Other ----------------
    bool        readYesNo(const std::string& prompt);
    std::string readDate(const std::string& prompt, const std::string& defaultDate,
                         const std::string& earliestDate, const std::string& latestDate);
    std::string readPassword(const std::string& prompt);
    void        pause();

    // ---------------- Validators shared by the whole system ----------------
    bool isSafeText(const std::string& value);
    bool validateName(std::string& value, std::string& error);
    bool validateNIN(std::string& value, std::string& error);
    bool validatePhone(std::string& value, std::string& error);
    bool validateAddress(std::string& value, std::string& error);
    bool validateUsername(std::string& value, std::string& error);
}

#endif // INPUT_HELPER_H
