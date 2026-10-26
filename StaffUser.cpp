/*
 * =============================================================================
 *  File    : StaffUser.cpp
 *  Purpose : Implementation of the StaffUser class (logins and roles).
 * =============================================================================
 */
#include "StaffUser.h"
#include "Utils.h"
#include "Config.h"

#include <iomanip>
#include <sstream>
#include <vector>

StaffUser::StaffUser()
    : Person(), username(""), passwordHash(""), role(ROLE_LOAN_OFFICER), active(true)
{
}

StaffUser::StaffUser(const std::string& user, const std::string& password, UserRole userRole,
                     const std::string& name, const std::string& phone)
    : Person(name, phone), username(Utils::toLower(user)), passwordHash(""), role(userRole), active(true)
{
    passwordHash = hashPassword(username, password);
}

// Turns a password into a fixed 16-character code (FNV-1a hash, 1000 rounds,
// salted with the username). The real password is never saved to disk.
// NOTE: good enough for a classroom project; a bank-grade system would use
// a dedicated algorithm such as bcrypt or Argon2.
std::string StaffUser::hashPassword(const std::string& user, const std::string& password)
{
    const unsigned long long FNV_OFFSET = 1469598103934665603ULL;
    const unsigned long long FNV_PRIME  = 1099511628211ULL;
    const std::string data = user + ":" + password + ":NDEJJE-FARMERS-SACCO";

    unsigned long long hash = FNV_OFFSET;
    for (int round = 0; round < 1000; ++round)
    {
        for (std::size_t i = 0; i < data.size(); ++i)
        {
            hash ^= static_cast<unsigned char>(data[i]);
            hash *= FNV_PRIME;
        }
    }
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
}

std::string StaffUser::getUsername() const { return username; }
UserRole StaffUser::getRole() const         { return role; }
bool StaffUser::isActive() const            { return active; }
bool StaffUser::isAdministrator() const     { return role == ROLE_ADMINISTRATOR; }

bool StaffUser::checkPassword(const std::string& password) const
{
    return hashPassword(username, password) == passwordHash;
}

void StaffUser::changePassword(const std::string& newPassword)
{
    passwordHash = hashPassword(username, newPassword);
}

void StaffUser::setActive(bool status)
{
    active = status;
}

std::string StaffUser::getRoleTitle() const
{
    return (role == ROLE_ADMINISTRATOR) ? "Administrator" : "Loan Officer";
}

void StaffUser::displayProfile(std::ostream& out) const
{
    out << "  Username : " << username << "\n"
        << "  Name     : " << fullName << "\n"
        << "  Phone    : " << phoneNumber << "\n"
        << "  Role     : " << getRoleTitle() << "\n"
        << "  Status   : " << (active ? "Active" : "Deactivated") << "\n";
}

void StaffUser::printTableHeader(std::ostream& out)
{
    out << "  " << std::left << std::setw(16) << "Username" << std::setw(28) << "Full Name"
        << std::setw(13) << "Phone" << std::setw(16) << "Role" << "Status\n";
    out << "  " << std::string(84, '-') << "\n";
}

void StaffUser::printTableRow(std::ostream& out) const
{
    out << "  " << std::left << std::setw(16) << username << std::setw(28) << Utils::fitWidth(fullName, 27)
        << std::setw(13) << phoneNumber << std::setw(16) << getRoleTitle()
        << (active ? "Active" : "Deactivated") << "\n";
}

std::string StaffUser::toRecord() const
{
    const char d = Config::FIELD_DELIMITER;
    return username + d + passwordHash + d + (role == ROLE_ADMINISTRATOR ? "ADMIN" : "OFFICER") + d +
           fullName + d + phoneNumber + d + (active ? "1" : "0");
}

bool StaffUser::fromRecord(const std::string& line, StaffUser& user)
{
    std::vector<std::string> field = Utils::split(line, Config::FIELD_DELIMITER);
    if (field.size() != 6 || field[0].empty() || field[1].size() != 16)
    {
        return false;
    }
    if (field[2] != "ADMIN" && field[2] != "OFFICER")
    {
        return false;
    }
    user = StaffUser();
    user.username     = field[0];
    user.passwordHash = field[1];
    user.role         = (field[2] == "ADMIN") ? ROLE_ADMINISTRATOR : ROLE_LOAN_OFFICER;
    user.fullName     = field[3];
    user.phoneNumber  = field[4];
    user.active       = (field[5] == "1");
    return true;
}
