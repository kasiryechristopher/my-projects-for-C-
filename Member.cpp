/*
 * =============================================================================
 *  File    : Member.cpp
 *  Purpose : Implementation of the Member class.
 * =============================================================================
 */
#include "Member.h"
#include "Utils.h"
#include "Config.h"

#include <iomanip>
#include <vector>
#include <stdexcept>

// ---------------------------------------------------------------------------
//  Constructors
// ---------------------------------------------------------------------------
Member::Member()
    : Person(), memberId(""), nationalId(""), address(""), dateJoined(""),
      shareCapital(0.0), savingsBalance(0.0)
{
}

Member::Member(const std::string& id, const std::string& name, const std::string& nin,
               const std::string& phone, const std::string& homeAddress, const std::string& joinDate,
               double shares, double savings)
    : Person(name, phone), memberId(id), nationalId(nin), address(homeAddress), dateJoined(joinDate),
      shareCapital(shares < 0 ? 0 : shares), savingsBalance(savings < 0 ? 0 : savings)
{
}

// ---------------------------------------------------------------------------
//  Getters
// ---------------------------------------------------------------------------
std::string Member::getMemberId() const     { return memberId; }
std::string Member::getNationalId() const   { return nationalId; }
std::string Member::getAddress() const      { return address; }
std::string Member::getDateJoined() const   { return dateJoined; }
double Member::getShareCapital() const      { return shareCapital; }
double Member::getSavingsBalance() const    { return savingsBalance; }
double Member::getTotalFunds() const        { return shareCapital + savingsBalance; }

int Member::getNumberOfShares() const
{
    return static_cast<int>(shareCapital / Config::SHARE_PRICE);
}

// ---------------------------------------------------------------------------
//  Setters
// ---------------------------------------------------------------------------
void Member::setNationalId(const std::string& nin)
{
    if (!nin.empty())
    {
        nationalId = nin;
    }
}

void Member::setAddress(const std::string& homeAddress)
{
    if (!homeAddress.empty())
    {
        address = homeAddress;
    }
}

// ---------------------------------------------------------------------------
//  Money operations - the ONLY way balances can change
// ---------------------------------------------------------------------------
bool Member::depositSavings(double amount)
{
    if (amount <= 0)
    {
        return false;
    }
    savingsBalance += amount;
    return true;
}

bool Member::withdrawSavings(double amount)
{
    if (amount <= 0 || amount > savingsBalance)
    {
        return false;                 // cannot withdraw more than is saved
    }
    savingsBalance -= amount;
    return true;
}

bool Member::addShareCapital(double amount)
{
    if (amount <= 0)
    {
        return false;
    }
    shareCapital += amount;
    return true;
}

double Member::closeAccount()
{
    double refund = shareCapital + savingsBalance;
    shareCapital = 0.0;
    savingsBalance = 0.0;
    return refund;
}

// ---------------------------------------------------------------------------
//  Display
// ---------------------------------------------------------------------------
std::string Member::getRoleTitle() const
{
    return "SACCO Member";
}

void Member::displayProfile(std::ostream& out) const
{
    out << "  Member ID        : " << memberId << "\n"
        << "  Full Name        : " << fullName << "\n"
        << "  National ID (NIN): " << nationalId << "\n"
        << "  Phone Number     : " << phoneNumber << "\n"
        << "  Address          : " << address << "\n"
        << "  Date Joined      : " << dateJoined << "\n"
        << "  Share Capital    : " << Utils::formatUGX(shareCapital)
        << " (" << getNumberOfShares() << " shares)\n"
        << "  Savings Balance  : " << Utils::formatUGX(savingsBalance) << "\n";
}

void Member::printTableHeader(std::ostream& out)
{
    out << "  " << std::left
        << std::setw(9)  << "ID"
        << std::setw(24) << "Full Name"
        << std::setw(16) << "NIN"
        << std::setw(12) << "Phone"
        << std::right
        << std::setw(14) << "Shares (UGX)"
        << std::setw(15) << "Savings (UGX)" << "\n";
    out << "  " << std::string(90, '-') << "\n";
}

void Member::printTableRow(std::ostream& out) const
{
    out << "  " << std::left
        << std::setw(9)  << memberId
        << std::setw(24) << Utils::fitWidth(fullName, 23)
        << std::setw(16) << nationalId
        << std::setw(12) << phoneNumber
        << std::right
        << std::setw(14) << Utils::formatMoney(shareCapital)
        << std::setw(15) << Utils::formatMoney(savingsBalance) << "\n";
}

// ---------------------------------------------------------------------------
//  File storage: MemberID|FullName|NIN|Phone|Address|DateJoined|ShareCapital|Savings
// ---------------------------------------------------------------------------
std::string Member::toRecord() const
{
    const char d = Config::FIELD_DELIMITER;
    return memberId + d + fullName + d + nationalId + d + phoneNumber + d + address + d +
           dateJoined + d + Utils::toString(shareCapital, 2) + d + Utils::toString(savingsBalance, 2);
}

bool Member::fromRecord(const std::string& line, Member& member)
{
    std::vector<std::string> field = Utils::split(line, Config::FIELD_DELIMITER);
    if (field.size() != 8 || field[0].empty() || !Utils::isValidDate(field[5]))
    {
        return false;
    }
    try
    {
        double shares  = Utils::numberOrThrow(field[6]);   // throws if the text is not a number
        double savings = Utils::numberOrThrow(field[7]);
        if (shares < 0 || savings < 0)
        {
            return false;
        }
        member = Member(field[0], field[1], field[2], field[3], field[4], field[5], shares, savings);
        return true;
    }
    catch (const std::invalid_argument&)
    {
        return false;               // a damaged number in the file: skip this line
    }
}
