/*
 * =============================================================================
 *  File    : StaffUser.h
 *  Purpose : A SACCO staff member who logs in to the system.
 *            Two roles exist:
 *              - Administrator : full access (approves loans, deletes
 *                                members, changes loan policy, manages users)
 *              - Loan Officer  : day-to-day work (members, savings, loan
 *                                applications, disbursement, repayments)
 *            Passwords are never stored as plain text - only a hash.
 * =============================================================================
 */
#ifndef STAFF_USER_H
#define STAFF_USER_H

#include <string>
#include <iostream>
#include "Person.h"

enum UserRole
{
    ROLE_ADMINISTRATOR,
    ROLE_LOAN_OFFICER
};

class StaffUser : public Person
{
private:
    std::string username;
    std::string passwordHash;
    UserRole    role;
    bool        active;

    static std::string hashPassword(const std::string& user, const std::string& password);

public:
    StaffUser();
    StaffUser(const std::string& user, const std::string& password, UserRole userRole,
              const std::string& name, const std::string& phone);

    std::string getUsername() const;
    UserRole    getRole() const;
    bool        isActive() const;
    bool        isAdministrator() const;

    bool checkPassword(const std::string& password) const;
    void changePassword(const std::string& newPassword);
    void setActive(bool status);

    // Person interface
    std::string getRoleTitle() const;
    void displayProfile(std::ostream& out) const;

    static void printTableHeader(std::ostream& out);
    void printTableRow(std::ostream& out) const;

    // File storage: Username|PasswordHash|Role|FullName|Phone|Active
    std::string toRecord() const;
    static bool fromRecord(const std::string& line, StaffUser& user);
};

#endif // STAFF_USER_H
