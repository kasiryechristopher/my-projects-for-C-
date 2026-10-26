/*
 * =============================================================================
 *  File    : Settings.cpp
 *  Purpose : Implementation of the loan policy and ID counters.
 * =============================================================================
 */
#include "Settings.h"
#include "Utils.h"

#include <iomanip>

// Default policy used the first time the system runs.
Settings::Settings()
    : annualInterestRate(18.0),
      loanMultiplier(3.0),
      minimumShareCapital(100000.0),
      minimumSavings(50000.0),
      minimumLoanAmount(100000.0),
      maximumLoanAmount(20000000.0),
      maximumDurationMonths(24),
      minimumMembershipMonths(3),
      nextMemberNumber(1),
      nextLoanNumber(1),
      nextTransactionNumber(1)
{
}

// ---------------------------------------------------------------------------
//  Getters
// ---------------------------------------------------------------------------
double Settings::getAnnualInterestRate() const     { return annualInterestRate; }
double Settings::getLoanMultiplier() const         { return loanMultiplier; }
double Settings::getMinimumShareCapital() const    { return minimumShareCapital; }
double Settings::getMinimumSavings() const         { return minimumSavings; }
double Settings::getMinimumLoanAmount() const      { return minimumLoanAmount; }
double Settings::getMaximumLoanAmount() const      { return maximumLoanAmount; }
int Settings::getMaximumDurationMonths() const     { return maximumDurationMonths; }
int Settings::getMinimumMembershipMonths() const   { return minimumMembershipMonths; }

// ---------------------------------------------------------------------------
//  Validated setters
// ---------------------------------------------------------------------------
bool Settings::setAnnualInterestRate(double rate)
{
    if (rate < 0 || rate > 60) return false;
    annualInterestRate = rate;
    return true;
}

bool Settings::setLoanMultiplier(double multiplier)
{
    if (multiplier < 1 || multiplier > 10) return false;
    loanMultiplier = multiplier;
    return true;
}

bool Settings::setMinimumShareCapital(double amount)
{
    if (amount < 0 || amount > 10000000) return false;
    minimumShareCapital = amount;
    return true;
}

bool Settings::setMinimumSavings(double amount)
{
    if (amount < 0 || amount > 10000000) return false;
    minimumSavings = amount;
    return true;
}

bool Settings::setLoanLimits(double minimum, double maximum)
{
    if (minimum < 10000 || maximum > 500000000 || minimum >= maximum) return false;
    minimumLoanAmount = minimum;
    maximumLoanAmount = maximum;
    return true;
}

bool Settings::setMaximumDurationMonths(int months)
{
    if (months < 1 || months > 60) return false;
    maximumDurationMonths = months;
    return true;
}

bool Settings::setMinimumMembershipMonths(int months)
{
    if (months < 0 || months > 24) return false;
    minimumMembershipMonths = months;
    return true;
}

// ---------------------------------------------------------------------------
//  ID generation
// ---------------------------------------------------------------------------
std::string Settings::generateMemberId()
{
    return "NFS" + Utils::zeroPad(nextMemberNumber++, 4);
}

std::string Settings::generateLoanId()
{
    return "LN" + Utils::zeroPad(nextLoanNumber++, 4);
}

std::string Settings::generateTransactionId()
{
    return "TX" + Utils::zeroPad(nextTransactionNumber++, 6);
}

// Protects against duplicate IDs if the settings file was lost or edited.
void Settings::ensureCountersAbove(int highestMember, int highestLoan, int highestTransaction)
{
    if (nextMemberNumber <= highestMember)           nextMemberNumber = highestMember + 1;
    if (nextLoanNumber <= highestLoan)               nextLoanNumber = highestLoan + 1;
    if (nextTransactionNumber <= highestTransaction) nextTransactionNumber = highestTransaction + 1;
}

void Settings::resetTransactionCounter(int nextNumber)
{
    if (nextNumber >= 1)
    {
        nextTransactionNumber = nextNumber;
    }
}

// ---------------------------------------------------------------------------
//  Display and storage
// ---------------------------------------------------------------------------
void Settings::display(std::ostream& out) const
{
    out << "  Interest rate (reducing balance) : " << Utils::toString(annualInterestRate, 2) << "% per year\n"
        << "  Maximum loan multiplier          : " << Utils::toString(loanMultiplier, 1)
        << " x (savings + share capital)\n"
        << "  Minimum share capital to borrow  : " << Utils::formatUGX(minimumShareCapital) << "\n"
        << "  Minimum savings to borrow        : " << Utils::formatUGX(minimumSavings) << "\n"
        << "  Smallest loan allowed            : " << Utils::formatUGX(minimumLoanAmount) << "\n"
        << "  Largest loan allowed             : " << Utils::formatUGX(maximumLoanAmount) << "\n"
        << "  Longest repayment period         : " << maximumDurationMonths << " months\n"
        << "  Minimum membership period        : " << minimumMembershipMonths << " months\n";
}

std::vector<std::string> Settings::toLines() const
{
    std::vector<std::string> lines;
    lines.push_back("# NDEJJE FARMERS SACCO - system settings (key=value)");
    lines.push_back("annual_interest_rate=" + Utils::toString(annualInterestRate, 2));
    lines.push_back("loan_multiplier=" + Utils::toString(loanMultiplier, 2));
    lines.push_back("minimum_share_capital=" + Utils::toString(minimumShareCapital, 0));
    lines.push_back("minimum_savings=" + Utils::toString(minimumSavings, 0));
    lines.push_back("minimum_loan_amount=" + Utils::toString(minimumLoanAmount, 0));
    lines.push_back("maximum_loan_amount=" + Utils::toString(maximumLoanAmount, 0));
    lines.push_back("maximum_duration_months=" + Utils::toString(maximumDurationMonths));
    lines.push_back("minimum_membership_months=" + Utils::toString(minimumMembershipMonths));
    lines.push_back("next_member_number=" + Utils::toString(nextMemberNumber));
    lines.push_back("next_loan_number=" + Utils::toString(nextLoanNumber));
    lines.push_back("next_transaction_number=" + Utils::toString(nextTransactionNumber));
    return lines;
}

// Applies one "key=value" line from the settings file. Bad values are ignored
// so a damaged file can never break the policy.
bool Settings::applyLine(const std::string& key, const std::string& value)
{
    double number = 0.0;
    if (!Utils::parseDouble(value, number))
    {
        return false;
    }
    int whole = static_cast<int>(number);

    if (key == "annual_interest_rate")      return setAnnualInterestRate(number);
    if (key == "loan_multiplier")           return setLoanMultiplier(number);
    if (key == "minimum_share_capital")     return setMinimumShareCapital(number);
    if (key == "minimum_savings")           return setMinimumSavings(number);
    if (key == "minimum_loan_amount")       return setLoanLimits(number, maximumLoanAmount > number ? maximumLoanAmount : number + 1);
    if (key == "maximum_loan_amount")       return setLoanLimits(minimumLoanAmount, number);
    if (key == "maximum_duration_months")   return setMaximumDurationMonths(whole);
    if (key == "minimum_membership_months") return setMinimumMembershipMonths(whole);
    if (key == "next_member_number" && whole >= 1)      { nextMemberNumber = whole; return true; }
    if (key == "next_loan_number" && whole >= 1)        { nextLoanNumber = whole; return true; }
    if (key == "next_transaction_number" && whole >= 1) { nextTransactionNumber = whole; return true; }
    return false;
}
