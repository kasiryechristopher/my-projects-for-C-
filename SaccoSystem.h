/*
 * =============================================================================
 *  File    : SaccoSystem.h
 *  Purpose : The controller class that runs the whole SACCO system.
 *            It OWNS all records (in vectors), applies the business rules,
 *            shows the menus and asks FileManager to save after every change.
 *
 *  Because this class is large, its member functions are split across
 *  four .cpp files by topic (all belong to the same class):
 *      SaccoSystem.cpp   - start-up, data loading/saving, login, main menus,
 *                          administration
 *      SaccoMembers.cpp  - member registration (CRUD), savings and shares
 *      SaccoLoans.cpp    - eligibility, applications, approval, disbursement,
 *                          repayments, loan calculator
 *      SaccoReports.cpp  - search facility, reports, statements, self-service
 * =============================================================================
 */
#ifndef SACCO_SYSTEM_H
#define SACCO_SYSTEM_H

#include <string>
#include <vector>
#include <iostream>

#include "Member.h"
#include "Loan.h"
#include "Transaction.h"
#include "StaffUser.h"
#include "Settings.h"

// One line of the loan eligibility checklist.
struct EligibilityRule
{
    std::string description;
    bool        passed;
    std::string detail;
};

// The full result of an eligibility check.
struct EligibilityResult
{
    bool                         eligible;
    double                       maximumLoan;
    std::vector<EligibilityRule> rules;
};

class SaccoSystem
{
private:
    // ---------- All records held in memory while the program runs ----------
    std::vector<Member>      members;
    std::vector<Loan>        loans;
    std::vector<Transaction> transactions;
    std::vector<StaffUser>   users;
    Settings                 settings;
    int                      currentUserIndex;     // -1 = nobody logged in
    bool                     firstRun;             // true when default logins were just created

    // ======================= SaccoSystem.cpp =======================
    // Data loading and saving
    void loadAllData();
    bool saveAll();
    void createDefaultUsers();
    void logAudit(const std::string& action);

    // Session helpers
    bool        isAdministrator() const;
    std::string currentUsername() const;
    void        showBanner(const std::string& screenTitle) const;
    bool        requireAdministrator() const;

    // Login and menus
    void welcomeMenu();
    bool staffLogin();
    void staffMainMenu();
    void memberMenu();
    void savingsMenu();
    void loanMenu();
    void searchMenu();
    void reportsMenu();
    void administrationMenu();

    // Administration
    void changeMyPassword();
    void updateLoanPolicy();
    void addStaffUser();
    void listStaffUsers() const;
    void resetStaffPassword();
    void toggleStaffStatus();
    void backupData();
    void viewAuditLog() const;
    void loadDemonstrationData();
    std::string readNewPassword(const std::string& username);

    // Shared look-ups
    int  findMemberIndex(const std::string& memberId) const;
    int  findMemberIndexByNIN(const std::string& nin) const;
    int  findLoanIndex(const std::string& loanId) const;
    int  findOpenLoanIndex(const std::string& memberId, const std::string& excludeLoanId) const;
    int  findUserIndex(const std::string& username) const;
    std::string memberNameOf(const std::string& memberId) const;
    int  promptMemberIndex();
    int  promptLoanIndex();
    void recordTransaction(const std::string& memberId, TransactionType type, double amount,
                           double balanceAfter, const std::string& description,
                           const std::string& loanId);
    void addTransactionOn(const std::string& date, const std::string& time, const std::string& memberId,
                          TransactionType type, double amount, double balanceAfter,
                          const std::string& description, const std::string& loanId);

    // ======================= SaccoMembers.cpp =======================
    void registerMember();
    void viewAllMembers() const;
    void viewMemberDetails();
    void showMemberOverview(const Member& member) const;
    void updateMember();
    void deleteMember();
    void depositSavings();
    void withdrawSavings();
    void buyShares();
    void viewMemberLedger();

    // ======================= SaccoLoans.cpp =======================
    double            maximumLoanFor(const Member& member) const;
    EligibilityResult checkEligibility(const Member& member, double amount, int months,
                                       const std::string& excludeLoanId) const;
    void              displayEligibility(const EligibilityResult& result) const;
    void              displayLoanPreview(double amount, double ratePercent, int months) const;
    std::string       readLoanPurpose();
    void checkEligibilityScreen();
    void applyForLoan();
    void viewAllLoans() const;
    void viewLoanDetails();
    void updateLoanApplication();
    void cancelLoanApplication();
    void approveOrRejectLoan();
    void disburseLoan();
    void recordRepayment();
    void loanCalculator() const;
    int  listLoans(bool allStatuses, LoanStatus status) const;

    // ======================= SaccoReports.cpp =======================
    void searchByMemberId();
    void searchByNIN();
    void searchByName();
    void searchByPhone();
    void searchLoanById();
    void financialSummaryReport() const;
    void memberRegisterReport() const;
    void activeLoansReport() const;
    void arrearsReport() const;
    void memberStatementScreen();
    void printMemberStatement(const Member& member, std::ostream& out) const;
    void dailyTransactionsReport();
    void exportMemberStatement();
    void memberSelfService();

public:
    SaccoSystem();
    void run();          // starts the program (called from main)
};

#endif // SACCO_SYSTEM_H
