/*
 * =============================================================================
 *  File    : SaccoReports.cpp
 *  Purpose : SaccoSystem functions for SEARCHING and REPORTING:
 *              - search members by Member ID, NIN, name or phone; loans by ID
 *              - SACCO financial summary
 *              - member register, active loans, arrears (defaulters)
 *              - individual member statement (screen or text file)
 *              - daily transactions report (end-of-day cash balancing)
 *              - member self-service (read-only access for members)
 * =============================================================================
 */
#include "SaccoSystem.h"
#include "Config.h"
#include "Utils.h"
#include "InputHelper.h"
#include "FileManager.h"

#include <iostream>
#include <iomanip>
#include <fstream>

namespace
{
    // Prints "  Label ........... value" lines used in the summary report.
    void summaryLine(const std::string& label, const std::string& value)
    {
        std::string dots(label.size() < 44 ? 44 - label.size() : 1, '.');
        std::cout << "    " << label << " " << dots << " " << value << "\n";
    }
}

// =============================================================================
//  SEARCH FACILITY
// =============================================================================
void SaccoSystem::searchByMemberId()
{
    Utils::printTitle("Search by Member ID");
    int index = promptMemberIndex();
    if (index >= 0)
    {
        Utils::printSuccess("Record found.");
        showMemberOverview(members[index]);
    }
}

void SaccoSystem::searchByNIN()
{
    Utils::printTitle("Search by National ID (NIN)");
    std::string nin = Utils::toUpper(Utils::removeChar(Utils::trim(InputHelper::readLine("  Enter NIN: ")), ' '));
    if (nin.empty())
    {
        Utils::printError("NIN cannot be empty.");
        return;
    }
    int index = findMemberIndexByNIN(nin);
    if (index < 0)
    {
        Utils::printError("No member found with NIN '" + nin + "'.");
        return;
    }
    Utils::printSuccess("Record found.");
    showMemberOverview(members[index]);
}

// Partial, case-insensitive name search: "nak" finds "Nakato Florence".
void SaccoSystem::searchByName()
{
    Utils::printTitle("Search by name");
    std::string text = InputHelper::readText("  Enter full or part of the name: ", 2, 50);
    std::vector<int> matches;
    for (std::size_t i = 0; i < members.size(); ++i)
    {
        if (Utils::containsIgnoreCase(members[i].getFullName(), text))
        {
            matches.push_back(static_cast<int>(i));
        }
    }
    if (matches.empty())
    {
        Utils::printError("No member found whose name contains '" + text + "'.");
        return;
    }
    if (matches.size() == 1)
    {
        Utils::printSuccess("1 record found.");
        showMemberOverview(members[matches[0]]);
        return;
    }
    Utils::printSuccess(Utils::toString(static_cast<int>(matches.size())) + " records found.");
    Member::printTableHeader(std::cout);
    for (std::size_t i = 0; i < matches.size(); ++i)
    {
        members[matches[i]].printTableRow(std::cout);
    }
    std::string pick = Utils::toUpper(Utils::trim(
        InputHelper::readLine("\n  Type a Member ID from the list for details (or press Enter to go back): ")));
    if (!pick.empty())
    {
        int index = findMemberIndex(pick);
        if (index >= 0)
        {
            showMemberOverview(members[index]);
        }
        else
        {
            Utils::printError("No member found with ID '" + pick + "'.");
        }
    }
}

void SaccoSystem::searchByPhone()
{
    Utils::printTitle("Search by phone number");
    std::string phone = InputHelper::readLine("  Enter phone number: ");
    std::string error;
    if (!InputHelper::validatePhone(phone, error))
    {
        Utils::printError(error);
        return;
    }
    int found = 0;
    for (std::size_t i = 0; i < members.size(); ++i)
    {
        if (members[i].getPhoneNumber() == phone)
        {
            if (found == 0)
            {
                Member::printTableHeader(std::cout);
            }
            members[i].printTableRow(std::cout);
            ++found;
        }
    }
    if (found == 0)
    {
        Utils::printError("No member found with phone number " + phone + ".");
    }
    else
    {
        Utils::printSuccess(Utils::toString(found) + " record(s) found.");
    }
}

void SaccoSystem::searchLoanById()
{
    viewLoanDetails();
}

// =============================================================================
//  REPORTS
// =============================================================================
void SaccoSystem::financialSummaryReport() const
{
    Utils::printTitle("SACCO financial summary as at " + Utils::today());
    std::string today = Utils::today();

    double totalSavings = 0.0, totalShares = 0.0;
    for (std::size_t i = 0; i < members.size(); ++i)
    {
        totalSavings += members[i].getSavingsBalance();
        totalShares  += members[i].getShareCapital();
    }

    int statusCount[5] = { 0, 0, 0, 0, 0 };       // indexed by LoanStatus
    double disbursed = 0.0, interestExpected = 0.0, repaid = 0.0;
    double outstanding = 0.0, arrears = 0.0, atRisk = 0.0, pendingValue = 0.0;
    for (std::size_t i = 0; i < loans.size(); ++i)
    {
        const Loan& loan = loans[i];
        ++statusCount[loan.getStatus()];
        if (loan.getStatus() == LOAN_PENDING || loan.getStatus() == LOAN_APPROVED)
        {
            pendingValue += loan.getPrincipal();
        }
        if (loan.getStatus() == LOAN_ACTIVE || loan.getStatus() == LOAN_COMPLETED)
        {
            disbursed        += loan.getPrincipal();
            interestExpected += loan.getTotalInterest();
            repaid           += loan.getAmountPaid();
        }
        if (loan.getStatus() == LOAN_ACTIVE)
        {
            outstanding += loan.getOutstandingBalance();
            double overdue = loan.getArrears(today);
            arrears += overdue;
            if (overdue > 0)
            {
                atRisk += loan.getOutstandingBalance();   // whole balance of a late loan is "at risk"
            }
        }
    }

    double moneyIn = 0.0, moneyOut = 0.0;
    for (std::size_t i = 0; i < transactions.size(); ++i)
    {
        if (transactions[i].isMoneyIn()) moneyIn  += transactions[i].getAmount();
        else                             moneyOut += transactions[i].getAmount();
    }
    double portfolioAtRisk = outstanding > 0 ? atRisk / outstanding * 100.0 : 0.0;

    std::cout << "\n  MEMBERSHIP\n";
    summaryLine("Registered members", Utils::toString(static_cast<int>(members.size())));
    std::cout << "\n  MEMBER FUNDS\n";
    summaryLine("Total savings", Utils::formatUGX(totalSavings));
    summaryLine("Total share capital", Utils::formatUGX(totalShares));
    summaryLine("Total member funds", Utils::formatUGX(totalSavings + totalShares));
    std::cout << "\n  LOAN PORTFOLIO\n";
    summaryLine("Pending applications", Utils::toString(statusCount[LOAN_PENDING]));
    summaryLine("Approved, waiting for payout", Utils::toString(statusCount[LOAN_APPROVED]));
    summaryLine("Value of pending + approved loans", Utils::formatUGX(pendingValue));
    summaryLine("Active loans", Utils::toString(statusCount[LOAN_ACTIVE]));
    summaryLine("Completed loans", Utils::toString(statusCount[LOAN_COMPLETED]));
    summaryLine("Rejected applications", Utils::toString(statusCount[LOAN_REJECTED]));
    summaryLine("Total principal paid out", Utils::formatUGX(disbursed));
    summaryLine("Total interest on loans paid out", Utils::formatUGX(interestExpected));
    summaryLine("Total repayments received", Utils::formatUGX(repaid));
    summaryLine("Outstanding on active loans", Utils::formatUGX(outstanding));
    summaryLine("Amount overdue (arrears)", Utils::formatUGX(arrears));
    summaryLine("Portfolio at risk (PAR)", Utils::toString(portfolioAtRisk, 1) + "%");
    std::cout << "\n  CASH POSITION (from the transaction ledger)\n";
    summaryLine("Money received", Utils::formatUGX(moneyIn));
    summaryLine("Money paid out", Utils::formatUGX(moneyOut));
    summaryLine("Cash that should be at hand / in bank", Utils::formatUGX(moneyIn - moneyOut));
    std::cout << "\n  Money received = deposits + shares + repayments. "
                 "Money paid out = withdrawals + loans + refunds.\n";
}

void SaccoSystem::memberRegisterReport() const
{
    Utils::printTitle("Member register as at " + Utils::today());
    if (members.empty())
    {
        Utils::printInfo("No members registered yet.");
        return;
    }
    std::cout << "  " << std::left << std::setw(9) << "ID" << std::setw(22) << "Full Name" << std::setw(12) << "Phone"
              << std::setw(12) << "Joined" << std::right << std::setw(12) << "Shares" << std::setw(13) << "Savings"
              << "  " << std::left << "Current Loan\n";
    std::cout << "  " << std::string(92, '-') << "\n";
    double totalShares = 0.0, totalSavings = 0.0;
    for (std::size_t i = 0; i < members.size(); ++i)
    {
        const Member& m = members[i];
        int open = findOpenLoanIndex(m.getMemberId(), "");
        std::cout << "  " << std::left << std::setw(9) << m.getMemberId() << std::setw(22) << Utils::fitWidth(m.getFullName(), 21)
                  << std::setw(12) << m.getPhoneNumber() << std::setw(12) << m.getDateJoined() << std::right
                  << std::setw(12) << Utils::formatMoney(m.getShareCapital())
                  << std::setw(13) << Utils::formatMoney(m.getSavingsBalance()) << "  " << std::left
                  << (open >= 0 ? loans[open].getLoanId() + " " + loans[open].getStatusName() : "-") << "\n";
        totalShares += m.getShareCapital();
        totalSavings += m.getSavingsBalance();
    }
    std::cout << "  " << std::string(92, '-') << "\n"
              << "  " << std::left << std::setw(55) << ("TOTAL (" + Utils::toString(static_cast<int>(members.size())) + " members)")
              << std::right << std::setw(12) << Utils::formatMoney(totalShares)
              << std::setw(13) << Utils::formatMoney(totalSavings) << "\n";
}

void SaccoSystem::activeLoansReport() const
{
    Utils::printTitle("Active loans report as at " + Utils::today());
    std::string today = Utils::today();
    int count = 0;
    double totalPrincipal = 0.0, totalRepaid = 0.0, totalOutstanding = 0.0, totalArrears = 0.0;

    for (std::size_t i = 0; i < loans.size(); ++i)
    {
        const Loan& loan = loans[i];
        if (loan.getStatus() != LOAN_ACTIVE)
        {
            continue;
        }
        if (count == 0)
        {
            std::cout << "  " << std::left << std::setw(8) << "Loan ID" << std::setw(19) << "Member" << std::right
                      << std::setw(12) << "Principal" << std::setw(12) << "Repaid" << std::setw(13) << "Outstanding"
                      << std::setw(13) << "Oldest Due" << std::setw(12) << "Arrears" << "\n";
            std::cout << "  " << std::string(89, '-') << "\n";
        }
        double overdue = loan.getArrears(today);
        std::cout << "  " << std::left << std::setw(8) << loan.getLoanId()
                  << std::setw(19) << Utils::fitWidth(memberNameOf(loan.getMemberId()), 18) << std::right
                  << std::setw(12) << Utils::formatMoney(loan.getPrincipal())
                  << std::setw(12) << Utils::formatMoney(loan.getAmountPaid())
                  << std::setw(13) << Utils::formatMoney(loan.getOutstandingBalance())
                  << std::setw(13) << loan.getNextDueDate()
                  << std::setw(12) << Utils::formatMoney(overdue) << "\n";
        totalPrincipal += loan.getPrincipal();
        totalRepaid += loan.getAmountPaid();
        totalOutstanding += loan.getOutstandingBalance();
        totalArrears += overdue;
        ++count;
    }
    if (count == 0)
    {
        Utils::printInfo("There are no active loans.");
        return;
    }
    std::cout << "  " << std::string(89, '-') << "\n"
              << "  " << std::left << std::setw(27) << ("TOTAL (" + Utils::toString(count) + " loans)") << std::right
              << std::setw(12) << Utils::formatMoney(totalPrincipal) << std::setw(12) << Utils::formatMoney(totalRepaid)
              << std::setw(13) << Utils::formatMoney(totalOutstanding) << std::setw(13) << ""
              << std::setw(12) << Utils::formatMoney(totalArrears) << "\n";
}

void SaccoSystem::arrearsReport() const
{
    Utils::printTitle("Loans in arrears (defaulters) as at " + Utils::today());
    std::string today = Utils::today();
    int count = 0;
    double totalArrears = 0.0, totalAtRisk = 0.0, totalOutstanding = 0.0;

    for (std::size_t i = 0; i < loans.size(); ++i)
    {
        const Loan& loan = loans[i];
        if (loan.getStatus() != LOAN_ACTIVE)
        {
            continue;
        }
        totalOutstanding += loan.getOutstandingBalance();
        double overdue = loan.getArrears(today);
        if (overdue <= 0)
        {
            continue;
        }
        if (count == 0)
        {
            std::cout << "  " << std::left << std::setw(8) << "Loan ID" << std::setw(19) << "Member" << std::setw(12)
                      << "Phone" << std::right << std::setw(12) << "Overdue" << std::setw(7) << "Days" << "  "
                      << std::left << std::setw(13) << "Category" << std::right << std::setw(13) << "Outstanding" << "\n";
            std::cout << "  " << std::string(88, '-') << "\n";
        }
        int memberIndex = findMemberIndex(loan.getMemberId());
        int days = loan.getDaysOverdue(today);
        std::cout << "  " << std::left << std::setw(8) << loan.getLoanId()
                  << std::setw(19) << Utils::fitWidth(memberNameOf(loan.getMemberId()), 18)
                  << std::setw(12) << (memberIndex >= 0 ? members[memberIndex].getPhoneNumber() : "-") << std::right
                  << std::setw(12) << Utils::formatMoney(overdue) << std::setw(7) << days << "  " << std::left
                  << std::setw(13) << Loan::arrearsCategory(days) << std::right
                  << std::setw(13) << Utils::formatMoney(loan.getOutstandingBalance()) << "\n";
        totalArrears += overdue;
        totalAtRisk += loan.getOutstandingBalance();
        ++count;
    }
    if (count == 0)
    {
        Utils::printSuccess("No loans are in arrears. All active loans are up to date.");
        return;
    }
    double par = totalOutstanding > 0 ? totalAtRisk / totalOutstanding * 100.0 : 0.0;
    std::cout << "  " << std::string(88, '-') << "\n"
              << "  Loans in arrears          : " << count << "\n"
              << "  Total amount overdue      : " << Utils::formatUGX(totalArrears) << "\n"
              << "  Balance of late loans     : " << Utils::formatUGX(totalAtRisk) << "\n"
              << "  Portfolio at risk (PAR)   : " << Utils::toString(par, 1) << "% of all active loan balances\n"
              << "  Categories: Watch 1-30 days, Substandard 31-90, Doubtful 91-180, Loss over 180.\n"
              << "  Action: call these members and remind them to pay.\n";
}

void SaccoSystem::memberStatementScreen()
{
    Utils::printTitle("Member statement");
    int index = promptMemberIndex();
    if (index >= 0)
    {
        printMemberStatement(members[index], std::cout);
    }
}

// Writes a full statement to ANY output stream: the screen (std::cout) or a
// text file (std::ofstream). One function, two uses - no repeated code.
void SaccoSystem::printMemberStatement(const Member& member, std::ostream& out) const
{
    std::string line(92, '=');
    std::string today = Utils::today();
    out << "\n  " << line << "\n"
        << "  " << Config::SACCO_NAME << " - MEMBER STATEMENT\n"
        << "  " << Config::SACCO_ADDRESS << "\n"
        << "  Statement date: " << today << "     Prepared by: " << currentUsername() << "\n"
        << "  " << line << "\n";
    member.displayProfile(out);

    out << "\n  TRANSACTIONS\n";
    Transaction::printLedgerHeader(out);
    int count = 0;
    for (std::size_t i = 0; i < transactions.size(); ++i)
    {
        if (transactions[i].getMemberId() == member.getMemberId())
        {
            transactions[i].printLedgerRow(out);
            ++count;
        }
    }
    if (count == 0)
    {
        out << "  (no transactions)\n";
    }

    out << "\n  LOANS\n";
    double owed = 0.0;
    int loanCount = 0;
    for (std::size_t i = 0; i < loans.size(); ++i)
    {
        const Loan& loan = loans[i];
        if (loan.getMemberId() != member.getMemberId())
        {
            continue;
        }
        ++loanCount;
        owed += loan.getOutstandingBalance();
        out << "  " << loan.getLoanId() << "  " << std::left << std::setw(10) << loan.getStatusName()
            << " Principal " << Utils::formatUGX(loan.getPrincipal())
            << " | Total payable " << Utils::formatUGX(loan.getTotalPayable())
            << " | Repaid " << Utils::formatUGX(loan.getAmountPaid())
            << " | Balance " << Utils::formatUGX(loan.getOutstandingBalance()) << "\n";
        if (loan.getStatus() == LOAN_ACTIVE)
        {
            out << "          Installment " << Utils::formatUGX(loan.getInstallment())
                << " | Oldest unpaid due " << loan.getNextDueDate()
                << " | Overdue " << Utils::formatUGX(loan.getArrears(today)) << "\n";
        }
    }
    if (loanCount == 0)
    {
        out << "  (no loans)\n";
    }

    out << "\n  SUMMARY\n"
        << "  Savings balance         : " << Utils::formatUGX(member.getSavingsBalance()) << "\n"
        << "  Share capital           : " << Utils::formatUGX(member.getShareCapital()) << "\n"
        << "  Loan balance owed       : " << Utils::formatUGX(owed) << "\n"
        << "  Net position            : " << Utils::formatUGX(member.getTotalFunds() - owed)
        << "  (savings + shares - loan balance)\n"
        << "  " << line << "\n";
}

void SaccoSystem::dailyTransactionsReport()
{
    Utils::printTitle("Daily transactions report");
    std::string today = Utils::today();
    std::string date = InputHelper::readDate("  Report date (YYYY-MM-DD)", today, Config::EARLIEST_DATE, today);

    double typeTotals[6] = { 0, 0, 0, 0, 0, 0 };       // indexed by TransactionType
    double moneyIn = 0.0, moneyOut = 0.0;
    int count = 0;
    std::cout << "\n  Transactions on " << date << "\n";
    for (std::size_t i = 0; i < transactions.size(); ++i)
    {
        const Transaction& txn = transactions[i];
        if (txn.getDate() != date)
        {
            continue;
        }
        if (count == 0)
        {
            Transaction::printDailyHeader(std::cout);
        }
        txn.printDailyRow(std::cout);
        typeTotals[txn.getType()] += txn.getAmount();
        if (txn.isMoneyIn()) moneyIn += txn.getAmount();
        else                 moneyOut += txn.getAmount();
        ++count;
    }
    if (count == 0)
    {
        Utils::printInfo("No transactions were recorded on " + date + ".");
        return;
    }
    std::cout << "  " << std::string(90, '-') << "\n"
              << "  Savings deposits     : " << Utils::formatUGX(typeTotals[TXN_SAVINGS_DEPOSIT]) << "\n"
              << "  Share purchases      : " << Utils::formatUGX(typeTotals[TXN_SHARE_PURCHASE]) << "\n"
              << "  Loan repayments      : " << Utils::formatUGX(typeTotals[TXN_LOAN_REPAYMENT]) << "\n"
              << "  Savings withdrawals  : " << Utils::formatUGX(typeTotals[TXN_SAVINGS_WITHDRAWAL]) << "\n"
              << "  Loans paid out       : " << Utils::formatUGX(typeTotals[TXN_LOAN_DISBURSEMENT]) << "\n"
              << "  Account refunds      : " << Utils::formatUGX(typeTotals[TXN_ACCOUNT_CLOSURE]) << "\n"
              << "  ------------------------------------------\n"
              << "  TOTAL MONEY IN       : " << Utils::formatUGX(moneyIn) << "\n"
              << "  TOTAL MONEY OUT      : " << Utils::formatUGX(moneyOut) << "\n"
              << "  NET CASH FOR THE DAY : " << Utils::formatUGX(moneyIn - moneyOut) << "\n"
              << "  Number of entries    : " << count << "\n";
}

void SaccoSystem::exportMemberStatement()
{
    Utils::printTitle("Export member statement to a text file");
    int index = promptMemberIndex();
    if (index < 0)
    {
        return;
    }
    const Member& member = members[index];
    std::string fileName = "statement_" + member.getMemberId() + "_" + Utils::removeChar(Utils::today(), '-') + ".txt";
    std::ofstream file(fileName.c_str());
    if (!file)
    {
        Utils::printError("Could not create the file " + fileName + ".");
        return;
    }
    printMemberStatement(member, file);
    file.close();
    if (!file)
    {
        Utils::printError("Writing the statement file failed.");
        return;
    }
    logAudit("Exported statement for " + member.getMemberId() + " to " + fileName);
    Utils::printSuccess("Statement saved as '" + fileName + "' in the program folder. It can be printed.");
}

// =============================================================================
//  MEMBER SELF-SERVICE (read-only; identity checked with Member ID + NIN)
// =============================================================================
void SaccoSystem::memberSelfService()
{
    showBanner("Member Self-Service");
    Utils::printInfo("Enter your Member ID and your National ID Number (NIN) to view your account.");
    int index = promptMemberIndex();
    if (index < 0)
    {
        InputHelper::pause();
        return;
    }
    std::string nin = Utils::toUpper(Utils::removeChar(Utils::trim(InputHelper::readLine("  Your NIN: ")), ' '));
    if (!Utils::equalsIgnoreCase(members[index].getNationalId(), nin))
    {
        Utils::printError("The NIN does not match our records for this Member ID. Access denied.");
        logAudit("Failed self-service check for " + members[index].getMemberId());
        InputHelper::pause();
        return;
    }
    const std::string memberId = members[index].getMemberId();
    logAudit("Member " + memberId + " opened self-service");

    int choice = 0;
    do
    {
        int current = findMemberIndex(memberId);
        if (current < 0)
        {
            return;
        }
        const Member& member = members[current];
        showBanner("Member Self-Service: " + member.getFullName());
        std::cout << "  1. My Account Summary\n"
                  << "  2. My Statement\n"
                  << "  3. My Loans and Repayment Schedule\n"
                  << "  4. How Much Can I Borrow?\n"
                  << "  0. Exit Self-Service\n";
        choice = InputHelper::readMenuChoice(0, 4);
        switch (choice)
        {
            case 1:
                showMemberOverview(member);
                break;
            case 2:
                printMemberStatement(member, std::cout);
                break;
            case 3:
            {
                int shown = 0;
                for (std::size_t i = 0; i < loans.size(); ++i)
                {
                    if (loans[i].getMemberId() == memberId)
                    {
                        Utils::printTitle("Loan " + loans[i].getLoanId());
                        loans[i].displayDetails(std::cout);
                        std::cout << "\n";
                        loans[i].displaySchedule(std::cout);
                        ++shown;
                    }
                }
                if (shown == 0)
                {
                    Utils::printInfo("You have no loans on record.");
                }
                break;
            }
            case 4:
            {
                double limit = maximumLoanFor(member);
                std::cout << "\n  You may borrow up to " << Utils::formatUGX(limit) << " over up to "
                          << settings.getMaximumDurationMonths() << " months, if every rule below passes:\n";
                displayEligibility(checkEligibility(member, limit, settings.getMaximumDurationMonths(), ""));
                Utils::printInfo("Visit the SACCO office to apply. A loan officer will guide you.");
                break;
            }
            default:
                break;
        }
        if (choice != 0)
        {
            InputHelper::pause();
        }
    } while (choice != 0);
}
