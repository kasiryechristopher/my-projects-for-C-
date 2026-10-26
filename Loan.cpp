/*
 * =============================================================================
 *  File    : Loan.cpp
 *  Purpose : Implementation of the Loan class: workflow, interest,
 *            amortisation schedule, repayments and arrears.
 * =============================================================================
 */
#include "Loan.h"
#include "Utils.h"
#include "Config.h"

#include <cmath>
#include <iomanip>
#include <stdexcept>

// Small tolerance so rounding to whole shillings never leaves "1 shilling" owed.
static const double MONEY_TOLERANCE = 0.5;

// ---------------------------------------------------------------------------
//  Constructors
// ---------------------------------------------------------------------------
Loan::Loan()
    : loanId(""), memberId(""), principal(0.0), annualInterestRate(0.0), durationMonths(0),
      purpose(""), status(LOAN_PENDING), applicationDate(""), decisionDate(""),
      disbursementDate(""), amountPaid(0.0), appliedBy(""), decidedBy(""), remarks("")
{
}

Loan::Loan(const std::string& id, const std::string& member, double amount, double ratePercent,
           int months, const std::string& loanPurpose, const std::string& appliedOn,
           const std::string& officer)
    : loanId(id), memberId(member), principal(amount), annualInterestRate(ratePercent),
      durationMonths(months), purpose(loanPurpose), status(LOAN_PENDING), applicationDate(appliedOn),
      decisionDate(""), disbursementDate(""), amountPaid(0.0), appliedBy(officer), decidedBy(""),
      remarks("")
{
}

// ---------------------------------------------------------------------------
//  Getters
// ---------------------------------------------------------------------------
std::string Loan::getLoanId() const           { return loanId; }
std::string Loan::getMemberId() const         { return memberId; }
double Loan::getPrincipal() const             { return principal; }
double Loan::getInterestRate() const          { return annualInterestRate; }
int Loan::getDurationMonths() const           { return durationMonths; }
std::string Loan::getPurpose() const          { return purpose; }
LoanStatus Loan::getStatus() const            { return status; }
std::string Loan::getApplicationDate() const  { return applicationDate; }
std::string Loan::getDecisionDate() const     { return decisionDate; }
std::string Loan::getDisbursementDate() const { return disbursementDate; }
double Loan::getAmountPaid() const            { return amountPaid; }
std::string Loan::getRemarks() const          { return remarks; }

bool Loan::isOpen() const
{
    return status == LOAN_PENDING || status == LOAN_APPROVED || status == LOAN_ACTIVE;
}

std::string Loan::getStatusName() const
{
    return statusToCode(status);
}

// ---------------------------------------------------------------------------
//  Workflow
// ---------------------------------------------------------------------------

// Only a PENDING application may still be changed.
bool Loan::updateApplication(double amount, int months, const std::string& loanPurpose)
{
    if (status != LOAN_PENDING || amount <= 0 || months <= 0 || loanPurpose.empty())
    {
        return false;
    }
    principal = amount;
    durationMonths = months;
    purpose = loanPurpose;
    return true;
}

bool Loan::approve(const std::string& date, const std::string& administrator, const std::string& note)
{
    if (status != LOAN_PENDING)
    {
        return false;
    }
    status = LOAN_APPROVED;
    decisionDate = date;
    decidedBy = administrator;
    remarks = note;
    return true;
}

bool Loan::reject(const std::string& date, const std::string& administrator, const std::string& reason)
{
    if (status != LOAN_PENDING)
    {
        return false;
    }
    status = LOAN_REJECTED;
    decisionDate = date;
    decidedBy = administrator;
    remarks = reason;
    return true;
}

bool Loan::disburse(const std::string& date)
{
    if (status != LOAN_APPROVED)
    {
        return false;
    }
    status = LOAN_ACTIVE;
    disbursementDate = date;
    return true;
}

// Adds a repayment. A loan becomes COMPLETED automatically when nothing is owed.
bool Loan::recordRepayment(double amount)
{
    if (status != LOAN_ACTIVE || amount <= 0 || amount > getOutstandingBalance() + MONEY_TOLERANCE)
    {
        return false;
    }
    amountPaid += amount;
    if (getOutstandingBalance() <= MONEY_TOLERANCE)
    {
        status = LOAN_COMPLETED;
    }
    return true;
}

// ---------------------------------------------------------------------------
//  Calculations
// ---------------------------------------------------------------------------

// Equal monthly installment (EMI) using the reducing-balance amortisation formula.
double Loan::calculateInstallment(double amount, double ratePercent, int months)
{
    if (amount <= 0 || months <= 0)
    {
        return 0.0;
    }
    double monthlyRate = ratePercent / 100.0 / 12.0;
    if (monthlyRate <= 0)
    {
        return Utils::roundMoney(amount / months);          // interest-free loan
    }
    double factor = std::pow(1.0 + monthlyRate, months);
    double installment = amount * monthlyRate * factor / (factor - 1.0);
    return Utils::roundMoney(installment);
}

// Builds the month-by-month repayment schedule. The last installment is
// adjusted so the balance ends at exactly zero after rounding.
std::vector<Installment> Loan::buildSchedule(double amount, double ratePercent, int months,
                                             const std::string& startDate)
{
    std::vector<Installment> schedule;
    if (amount <= 0 || months <= 0)
    {
        return schedule;
    }

    double monthlyRate = ratePercent / 100.0 / 12.0;
    double installment = calculateInstallment(amount, ratePercent, months);
    double balance = amount;

    for (int month = 1; month <= months; ++month)
    {
        Installment row;
        row.number = month;
        row.dueDate = startDate.empty() ? "" : Utils::addMonths(startDate, month);
        row.openingBalance = balance;
        row.interest = Utils::roundMoney(balance * monthlyRate);

        if (month == months)
        {
            row.principal = balance;                 // clear whatever is left
        }
        else
        {
            row.principal = installment - row.interest;
            if (row.principal > balance) row.principal = balance;
            if (row.principal < 0)       row.principal = 0;
        }
        row.payment = row.principal + row.interest;
        row.closingBalance = balance - row.principal;
        balance = row.closingBalance;
        schedule.push_back(row);
    }
    return schedule;
}

double Loan::totalPayableFor(double amount, double ratePercent, int months)
{
    std::vector<Installment> schedule = buildSchedule(amount, ratePercent, months, "");
    double total = 0.0;
    for (std::size_t i = 0; i < schedule.size(); ++i)
    {
        total += schedule[i].payment;
    }
    return total;
}

double Loan::getInstallment() const
{
    return calculateInstallment(principal, annualInterestRate, durationMonths);
}

std::vector<Installment> Loan::getSchedule() const
{
    return buildSchedule(principal, annualInterestRate, durationMonths, disbursementDate);
}

double Loan::getTotalPayable() const
{
    return totalPayableFor(principal, annualInterestRate, durationMonths);
}

double Loan::getTotalInterest() const
{
    return getTotalPayable() - principal;
}

double Loan::getOutstandingBalance() const
{
    if (status == LOAN_PENDING || status == LOAN_APPROVED || status == LOAN_REJECTED)
    {
        return 0.0;                         // nothing is owed until money is given out
    }
    double balance = getTotalPayable() - amountPaid;
    return balance > MONEY_TOLERANCE ? balance : 0.0;
}

// Total of all installments whose due date has arrived by the given date.
double Loan::getAmountDueBy(const std::string& date) const
{
    if (status != LOAN_ACTIVE && status != LOAN_COMPLETED)
    {
        return 0.0;
    }
    std::vector<Installment> schedule = getSchedule();
    double due = 0.0;
    for (std::size_t i = 0; i < schedule.size(); ++i)
    {
        if (!schedule[i].dueDate.empty() && schedule[i].dueDate <= date)
        {
            due += schedule[i].payment;
        }
    }
    return due;
}

// Money that should have been paid by 'date' but has not been paid.
double Loan::getArrears(const std::string& date) const
{
    if (status != LOAN_ACTIVE)
    {
        return 0.0;
    }
    double arrears = getAmountDueBy(date) - amountPaid;
    return arrears > MONEY_TOLERANCE ? Utils::roundMoney(arrears) : 0.0;
}

// Days since the oldest unpaid installment fell due.
int Loan::getDaysOverdue(const std::string& date) const
{
    if (getArrears(date) <= 0)
    {
        return 0;
    }
    std::vector<Installment> schedule = getSchedule();
    double cumulative = 0.0;
    for (std::size_t i = 0; i < schedule.size(); ++i)
    {
        cumulative += schedule[i].payment;
        if (cumulative > amountPaid + MONEY_TOLERANCE)
        {
            long days = Utils::daysBetween(schedule[i].dueDate, date);
            return days > 0 ? static_cast<int>(days) : 0;
        }
    }
    return 0;
}

// Due date of the oldest installment that is not yet fully paid.
std::string Loan::getNextDueDate() const
{
    if (status != LOAN_ACTIVE)
    {
        return "-";
    }
    std::vector<Installment> schedule = getSchedule();
    double cumulative = 0.0;
    for (std::size_t i = 0; i < schedule.size(); ++i)
    {
        cumulative += schedule[i].payment;
        if (cumulative > amountPaid + MONEY_TOLERANCE)
        {
            return schedule[i].dueDate;
        }
    }
    return "-";
}

// Standard microfinance classification of a loan by how late it is.
std::string Loan::arrearsCategory(int daysOverdue)
{
    if (daysOverdue <= 0)   return "Performing";
    if (daysOverdue <= 30)  return "Watch";
    if (daysOverdue <= 90)  return "Substandard";
    if (daysOverdue <= 180) return "Doubtful";
    return "Loss";
}

// ---------------------------------------------------------------------------
//  Display
// ---------------------------------------------------------------------------
void Loan::displayDetails(std::ostream& out) const
{
    out << "  Loan ID              : " << loanId << "\n"
        << "  Member ID            : " << memberId << "\n"
        << "  Purpose              : " << purpose << "\n"
        << "  Principal (borrowed) : " << Utils::formatUGX(principal) << "\n"
        << "  Interest Rate        : " << Utils::toString(annualInterestRate, 2)
        << "% per year (reducing balance)\n"
        << "  Duration             : " << durationMonths << " month(s)\n"
        << "  Monthly Installment  : " << Utils::formatUGX(getInstallment()) << "\n"
        << "  Total Interest       : " << Utils::formatUGX(getTotalInterest()) << "\n"
        << "  Total Payable        : " << Utils::formatUGX(getTotalPayable()) << "\n"
        << "  Status               : " << getStatusName() << "\n"
        << "  Applied On           : " << applicationDate << " (captured by " << appliedBy << ")\n";

    if (!decisionDate.empty())
    {
        out << "  Decision Date        : " << decisionDate << " (by " << decidedBy << ")\n";
    }
    if (!remarks.empty())
    {
        out << "  Remarks              : " << remarks << "\n";
    }
    if (!disbursementDate.empty())
    {
        std::string today = Utils::today();
        out << "  Disbursed On         : " << disbursementDate << "\n"
            << "  Amount Repaid        : " << Utils::formatUGX(amountPaid) << "\n"
            << "  Outstanding Balance  : " << Utils::formatUGX(getOutstandingBalance()) << "\n";
        if (status == LOAN_ACTIVE)
        {
            int days = getDaysOverdue(today);
            out << "  Next Due Date        : " << getNextDueDate() << "\n"
                << "  Arrears (overdue)    : " << Utils::formatUGX(getArrears(today));
            if (days > 0)
            {
                out << " - " << days << " day(s) late [" << arrearsCategory(days) << "]";
            }
            out << "\n";
        }
    }
}

void Loan::displaySchedule(std::ostream& out) const
{
    bool showStatus = (status == LOAN_ACTIVE || status == LOAN_COMPLETED);
    printSchedule(out, getSchedule(), amountPaid, showStatus);
}

void Loan::printSchedule(std::ostream& out, const std::vector<Installment>& schedule,
                         double paidSoFar, bool showPaymentStatus)
{
    std::string today = Utils::today();
    out << "  " << std::left << std::setw(5) << "No." << std::setw(12) << "Due Date" << std::right
        << std::setw(13) << "Opening Bal" << std::setw(13) << "Installment" << std::setw(11) << "Interest"
        << std::setw(12) << "Principal" << std::setw(13) << "Closing Bal" << "  " << std::left
        << "Status\n";
    out << "  " << std::string(92, '-') << "\n";

    double cumulative = 0.0, totalPayment = 0.0, totalInterest = 0.0, totalPrincipal = 0.0;
    for (std::size_t i = 0; i < schedule.size(); ++i)
    {
        const Installment& row = schedule[i];
        double before = cumulative;
        cumulative += row.payment;

        std::string rowStatus = "-";
        if (showPaymentStatus)
        {
            if (paidSoFar + MONEY_TOLERANCE >= cumulative)        rowStatus = "PAID";
            else if (paidSoFar > before + MONEY_TOLERANCE)         rowStatus = "PART PAID";
            else if (!row.dueDate.empty() && row.dueDate <= today) rowStatus = "OVERDUE";
            else                                                   rowStatus = "UPCOMING";
        }

        out << "  " << std::left << std::setw(5) << row.number
            << std::setw(12) << (row.dueDate.empty() ? "Month " + Utils::toString(row.number) : row.dueDate)
            << std::right << std::setw(13) << Utils::formatMoney(row.openingBalance)
            << std::setw(13) << Utils::formatMoney(row.payment) << std::setw(11) << Utils::formatMoney(row.interest)
            << std::setw(12) << Utils::formatMoney(row.principal)
            << std::setw(13) << Utils::formatMoney(row.closingBalance) << "  " << std::left << rowStatus << "\n";

        totalPayment += row.payment;
        totalInterest += row.interest;
        totalPrincipal += row.principal;
    }
    out << "  " << std::string(92, '-') << "\n";
    out << "  " << std::left << std::setw(17) << "TOTALS" << std::right << std::setw(13) << ""
        << std::setw(13) << Utils::formatMoney(totalPayment) << std::setw(11) << Utils::formatMoney(totalInterest)
        << std::setw(12) << Utils::formatMoney(totalPrincipal) << "\n";
}

void Loan::printTableHeader(std::ostream& out)
{
    out << "  " << std::left << std::setw(8) << "Loan ID" << std::setw(9) << "Member" << std::setw(19)
        << "Member Name" << std::right << std::setw(12) << "Principal" << std::setw(7) << "Months"
        << "  " << std::left << std::setw(11) << "Status" << std::right << std::setw(13) << "Outstanding"
        << std::setw(12) << "Applied" << "\n";
    out << "  " << std::string(93, '-') << "\n";
}

void Loan::printTableRow(std::ostream& out, const std::string& memberName) const
{
    out << "  " << std::left << std::setw(8) << loanId << std::setw(9) << memberId << std::setw(19)
        << Utils::fitWidth(memberName, 18) << std::right << std::setw(12) << Utils::formatMoney(principal)
        << std::setw(7) << durationMonths << "  " << std::left << std::setw(11) << getStatusName()
        << std::right << std::setw(13) << Utils::formatMoney(getOutstandingBalance())
        << std::setw(12) << applicationDate << "\n";
}

// ---------------------------------------------------------------------------
//  File storage:
//  LoanID|MemberID|Principal|Rate|Months|Purpose|Status|AppliedOn|DecisionDate|
//  DisbursedOn|AmountPaid|AppliedBy|DecidedBy|Remarks
// ---------------------------------------------------------------------------
std::string Loan::statusToCode(LoanStatus loanStatus)
{
    switch (loanStatus)
    {
        case LOAN_PENDING:   return "PENDING";
        case LOAN_APPROVED:  return "APPROVED";
        case LOAN_ACTIVE:    return "ACTIVE";
        case LOAN_COMPLETED: return "COMPLETED";
        case LOAN_REJECTED:  return "REJECTED";
    }
    return "PENDING";
}

bool Loan::codeToStatus(const std::string& code, LoanStatus& loanStatus)
{
    if (code == "PENDING")        loanStatus = LOAN_PENDING;
    else if (code == "APPROVED")  loanStatus = LOAN_APPROVED;
    else if (code == "ACTIVE")    loanStatus = LOAN_ACTIVE;
    else if (code == "COMPLETED") loanStatus = LOAN_COMPLETED;
    else if (code == "REJECTED")  loanStatus = LOAN_REJECTED;
    else return false;
    return true;
}

std::string Loan::toRecord() const
{
    const char d = Config::FIELD_DELIMITER;
    return loanId + d + memberId + d + Utils::toString(principal, 2) + d +
           Utils::toString(annualInterestRate, 2) + d + Utils::toString(durationMonths) + d + purpose + d +
           statusToCode(status) + d + applicationDate + d + decisionDate + d + disbursementDate + d +
           Utils::toString(amountPaid, 2) + d + appliedBy + d + decidedBy + d + remarks;
}

bool Loan::fromRecord(const std::string& line, Loan& loan)
{
    std::vector<std::string> field = Utils::split(line, Config::FIELD_DELIMITER);
    LoanStatus loanStatus;
    if (field.size() != 14 || field[0].empty() || !codeToStatus(field[6], loanStatus) ||
        !Utils::isValidDate(field[7]))
    {
        return false;
    }
    // An active or completed loan must have a valid disbursement date.
    if ((loanStatus == LOAN_ACTIVE || loanStatus == LOAN_COMPLETED) && !Utils::isValidDate(field[9]))
    {
        return false;
    }
    try
    {
        loan = Loan(field[0], field[1], Utils::numberOrThrow(field[2]), Utils::numberOrThrow(field[3]),
                    Utils::wholeNumberOrThrow(field[4]), field[5], field[7], field[11]);
        loan.status           = loanStatus;
        loan.decisionDate     = field[8];
        loan.disbursementDate = field[9];
        loan.amountPaid       = Utils::numberOrThrow(field[10]);
        loan.decidedBy        = field[12];
        loan.remarks          = field[13];
        return loan.principal > 0 && loan.durationMonths > 0 && loan.amountPaid >= 0;
    }
    catch (const std::invalid_argument&)   // damaged number in the file
    {
        return false;
    }
}
