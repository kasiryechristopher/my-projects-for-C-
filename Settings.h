/*
 * =============================================================================
 *  File    : Settings.h
 *  Purpose : The SACCO's loan policy (interest rate, eligibility limits) and
 *            the counters used to create unique IDs. The Administrator can
 *            change the policy without changing the program code. Each
 *            setter checks that the new value is sensible.
 * =============================================================================
 */
#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>
#include <vector>
#include <iostream>

class Settings
{
private:
    // ---------- Loan policy ----------
    double annualInterestRate;        // % per year, reducing balance
    double loanMultiplier;            // max loan = multiplier x (savings + shares)
    double minimumShareCapital;       // UGX a member must own before borrowing
    double minimumSavings;            // UGX a member must have saved before borrowing
    double minimumLoanAmount;         // UGX
    double maximumLoanAmount;         // UGX
    int    maximumDurationMonths;
    int    minimumMembershipMonths;   // how long a member must belong before borrowing

    // ---------- ID counters (never reused, even after a delete) ----------
    int nextMemberNumber;
    int nextLoanNumber;
    int nextTransactionNumber;

public:
    Settings();   // loads the default policy

    // ---------- Getters ----------
    double getAnnualInterestRate() const;
    double getLoanMultiplier() const;
    double getMinimumShareCapital() const;
    double getMinimumSavings() const;
    double getMinimumLoanAmount() const;
    double getMaximumLoanAmount() const;
    int    getMaximumDurationMonths() const;
    int    getMinimumMembershipMonths() const;

    // ---------- Setters (return false and keep the old value if invalid) ----------
    bool setAnnualInterestRate(double rate);
    bool setLoanMultiplier(double multiplier);
    bool setMinimumShareCapital(double amount);
    bool setMinimumSavings(double amount);
    bool setLoanLimits(double minimum, double maximum);
    bool setMaximumDurationMonths(int months);
    bool setMinimumMembershipMonths(int months);

    // ---------- Unique ID generation ----------
    std::string generateMemberId();        // NFS0001, NFS0002, ...
    std::string generateLoanId();          // LN0001, ...
    std::string generateTransactionId();   // TX000001, ...
    void ensureCountersAbove(int highestMember, int highestLoan, int highestTransaction);
    void resetTransactionCounter(int nextNumber);

    // ---------- Display and storage ----------
    void display(std::ostream& out) const;
    std::vector<std::string> toLines() const;                       // key=value lines
    bool applyLine(const std::string& key, const std::string& value);
};

#endif // SETTINGS_H
