/*
 * =============================================================================
 *  File    : Loan.h
 *  Purpose : A loan given to a SACCO member, with its full life cycle:
 *
 *      PENDING --approve--> APPROVED --disburse--> ACTIVE --fully repaid--> COMPLETED
 *         |
 *         +----reject----> REJECTED
 *
 *            Interest uses the REDUCING BALANCE method: each month interest is
 *            charged only on the money still owed, and the member pays an equal
 *            monthly installment (amortisation), worked out with the formula
 *
 *                 installment = P x r x (1 + r)^n / ((1 + r)^n - 1)
 *
 *            P = principal, r = monthly rate (annual rate / 12 / 100), n = months
 * =============================================================================
 */
#ifndef LOAN_H
#define LOAN_H

#include <string>
#include <vector>
#include <iostream>

enum LoanStatus
{
    LOAN_PENDING,
    LOAN_APPROVED,
    LOAN_ACTIVE,
    LOAN_COMPLETED,
    LOAN_REJECTED
};

// One row of a repayment schedule (one month).
struct Installment
{
    int         number;
    std::string dueDate;          // empty until the loan is disbursed
    double      openingBalance;
    double      payment;          // installment = interest + principal
    double      interest;
    double      principal;
    double      closingBalance;
};

class Loan
{
private:
    std::string loanId;            // e.g. LN0001
    std::string memberId;
    double      principal;         // amount borrowed (UGX)
    double      annualInterestRate;// percent per year, kept with the loan
    int         durationMonths;
    std::string purpose;
    LoanStatus  status;
    std::string applicationDate;
    std::string decisionDate;      // approval or rejection date
    std::string disbursementDate;  // the date money was given out
    double      amountPaid;        // total repaid so far (UGX)
    std::string appliedBy;         // staff who captured the application
    std::string decidedBy;         // administrator who approved / rejected
    std::string remarks;

public:
    // ---------- Constructors ----------
    Loan();
    Loan(const std::string& id, const std::string& member, double amount, double ratePercent,
         int months, const std::string& loanPurpose, const std::string& appliedOn,
         const std::string& officer);

    // ---------- Getters ----------
    std::string getLoanId() const;
    std::string getMemberId() const;
    double      getPrincipal() const;
    double      getInterestRate() const;
    int         getDurationMonths() const;
    std::string getPurpose() const;
    LoanStatus  getStatus() const;
    std::string getStatusName() const;
    std::string getApplicationDate() const;
    std::string getDecisionDate() const;
    std::string getDisbursementDate() const;
    double      getAmountPaid() const;
    std::string getRemarks() const;
    bool        isOpen() const;              // pending, approved or active

    // ---------- Workflow (each returns false if the step is not allowed now) ----------
    bool updateApplication(double amount, int months, const std::string& loanPurpose);
    bool approve(const std::string& date, const std::string& administrator, const std::string& note);
    bool reject(const std::string& date, const std::string& administrator, const std::string& reason);
    bool disburse(const std::string& date);
    bool recordRepayment(double amount);

    // ---------- Calculations (static versions also serve the loan calculator) ----------
    static double calculateInstallment(double amount, double ratePercent, int months);
    static std::vector<Installment> buildSchedule(double amount, double ratePercent, int months,
                                                  const std::string& startDate);
    static double totalPayableFor(double amount, double ratePercent, int months);

    double getInstallment() const;
    std::vector<Installment> getSchedule() const;
    double getTotalPayable() const;
    double getTotalInterest() const;
    double getOutstandingBalance() const;
    double getAmountDueBy(const std::string& date) const;
    double getArrears(const std::string& date) const;
    int    getDaysOverdue(const std::string& date) const;
    std::string getNextDueDate() const;
    static std::string arrearsCategory(int daysOverdue);

    // ---------- Display ----------
    void displayDetails(std::ostream& out) const;
    void displaySchedule(std::ostream& out) const;
    static void printSchedule(std::ostream& out, const std::vector<Installment>& schedule,
                              double paidSoFar, bool showPaymentStatus);
    static void printTableHeader(std::ostream& out);
    void printTableRow(std::ostream& out, const std::string& memberName) const;

    // ---------- File storage ----------
    static std::string statusToCode(LoanStatus loanStatus);
    static bool        codeToStatus(const std::string& code, LoanStatus& loanStatus);
    std::string toRecord() const;
    static bool fromRecord(const std::string& line, Loan& loan);
};

#endif // LOAN_H
