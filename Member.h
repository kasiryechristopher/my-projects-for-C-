/*
 * =============================================================================
 *  File    : Member.h
 *  Purpose : A farmer who belongs to Ndejje Farmers SACCO.
 *            Holds personal details plus two money balances:
 *              - share capital (ownership shares, cannot be withdrawn)
 *              - savings balance (can be deposited and withdrawn)
 *            Balances are PRIVATE and can only change through member
 *            functions that check the rules (encapsulation).
 * =============================================================================
 */
#ifndef MEMBER_H
#define MEMBER_H

#include <string>
#include <iostream>
#include "Person.h"

class Member : public Person
{
private:
    std::string memberId;        // e.g. NFS0001
    std::string nationalId;      // NIN, e.g. CM85012345ABCD
    std::string address;         // village / ward
    std::string dateJoined;      // YYYY-MM-DD
    double      shareCapital;    // UGX
    double      savingsBalance;  // UGX

public:
    // ---------- Constructors ----------
    Member();
    Member(const std::string& id, const std::string& name, const std::string& nin,
           const std::string& phone, const std::string& homeAddress, const std::string& joinDate,
           double shares, double savings);

    // ---------- Getters ----------
    std::string getMemberId() const;
    std::string getNationalId() const;
    std::string getAddress() const;
    std::string getDateJoined() const;
    double      getShareCapital() const;
    double      getSavingsBalance() const;
    double      getTotalFunds() const;        // savings + share capital
    int         getNumberOfShares() const;

    // ---------- Setters for editable personal details ----------
    void setNationalId(const std::string& nin);
    void setAddress(const std::string& homeAddress);

    // ---------- Money operations (return false when not allowed) ----------
    bool   depositSavings(double amount);
    bool   withdrawSavings(double amount);
    bool   addShareCapital(double amount);
    double closeAccount();                    // refunds everything, returns the total

    // ---------- Person interface (polymorphism) ----------
    std::string getRoleTitle() const;
    void displayProfile(std::ostream& out) const;

    // ---------- Table output ----------
    static void printTableHeader(std::ostream& out);
    void printTableRow(std::ostream& out) const;

    // ---------- File storage ----------
    std::string toRecord() const;
    static bool fromRecord(const std::string& line, Member& member);
};

#endif // MEMBER_H
