/*
 * =============================================================================
 *  File    : SaccoLoans.cpp
 *  Purpose : SaccoSystem functions for LOANS:
 *              - eligibility rules and checklist
 *              - application, update and cancellation (CRUD for loans)
 *              - approval / rejection (Administrator)
 *              - disbursement (payout) and repayments
 *              - loan calculator
 *
 *  ELIGIBILITY RULES (values come from the loan policy in Settings):
 *    1. Member for at least the minimum membership period (default 3 months)
 *    2. Share capital of at least the minimum (default UGX 100,000)
 *    3. Savings of at least the minimum (default UGX 50,000)
 *    4. No other unfinished loan (one loan at a time)
 *    5. Amount between the smallest and largest loan allowed
 *    6. Amount not more than multiplier x (savings + share capital) (default 3x)
 *    7. Repayment period within the longest period allowed (default 24 months)
 * =============================================================================
 */
#include "SaccoSystem.h"
#include "Config.h"
#include "Utils.h"
#include "InputHelper.h"

#include <iostream>
#include <iomanip>
#include <cmath>

namespace
{
    // Adds one rule to the checklist; any failed rule makes the member ineligible.
    void addRule(EligibilityResult& result, const std::string& description, bool passed, const std::string& detail)
    {
        EligibilityRule rule;
        rule.description = description;
        rule.passed = passed;
        rule.detail = detail;
        result.rules.push_back(rule);
        if (!passed)
        {
            result.eligible = false;
        }
    }
}

// =============================================================================
//  ELIGIBILITY
// =============================================================================

// Largest loan the member could get: multiplier x (savings + shares), capped by
// the policy maximum and rounded down to whole thousands.
double SaccoSystem::maximumLoanFor(const Member& member) const
{
    double limit = settings.getLoanMultiplier() * member.getTotalFunds();
    if (limit > settings.getMaximumLoanAmount())
    {
        limit = settings.getMaximumLoanAmount();
    }
    return std::floor(limit / 1000.0) * 1000.0;
}

EligibilityResult SaccoSystem::checkEligibility(const Member& member, double amount, int months,
                                                const std::string& excludeLoanId) const
{
    EligibilityResult result;
    result.eligible = true;
    result.maximumLoan = maximumLoanFor(member);

    int monthsAsMember = Utils::fullMonthsBetween(member.getDateJoined(), Utils::today());
    addRule(result, "Member for at least " + Utils::toString(settings.getMinimumMembershipMonths()) + " month(s)",
            monthsAsMember >= settings.getMinimumMembershipMonths(),
            "Member for " + Utils::toString(monthsAsMember) + " month(s)");

    addRule(result, "Share capital at least " + Utils::formatUGX(settings.getMinimumShareCapital()),
            member.getShareCapital() >= settings.getMinimumShareCapital(),
            "Has " + Utils::formatUGX(member.getShareCapital()));

    addRule(result, "Savings at least " + Utils::formatUGX(settings.getMinimumSavings()),
            member.getSavingsBalance() >= settings.getMinimumSavings(),
            "Has " + Utils::formatUGX(member.getSavingsBalance()));

    int open = findOpenLoanIndex(member.getMemberId(), excludeLoanId);
    addRule(result, "No other unfinished loan", open < 0,
            open < 0 ? "None" : loans[open].getLoanId() + " is " + loans[open].getStatusName());

    addRule(result, "Amount between " + Utils::formatMoney(settings.getMinimumLoanAmount()) + " and " +
                    Utils::formatMoney(settings.getMaximumLoanAmount()),
            amount >= settings.getMinimumLoanAmount() && amount <= settings.getMaximumLoanAmount(),
            "Asked for " + Utils::formatUGX(amount));

    double fundsLimit = settings.getLoanMultiplier() * member.getTotalFunds();
    addRule(result, "Amount within " + Utils::toString(settings.getLoanMultiplier(), 1) + " x (savings + shares)",
            amount <= fundsLimit, "Limit " + Utils::formatUGX(fundsLimit));

    addRule(result, "Period of 1 to " + Utils::toString(settings.getMaximumDurationMonths()) + " months",
            months >= 1 && months <= settings.getMaximumDurationMonths(),
            "Asked for " + Utils::toString(months) + " month(s)");

    return result;
}

void SaccoSystem::displayEligibility(const EligibilityResult& result) const
{
    Utils::printTitle("Eligibility checklist");
    std::cout << "  " << std::left << std::setw(4) << "No" << std::setw(44) << "Rule" << std::setw(8) << "Result"
              << "Details\n";
    std::cout << "  " << std::string(90, '-') << "\n";
    int failed = 0;
    for (std::size_t i = 0; i < result.rules.size(); ++i)
    {
        const EligibilityRule& rule = result.rules[i];
        std::cout << "  " << std::left << std::setw(4) << (i + 1) << std::setw(44) << Utils::fitWidth(rule.description, 43)
                  << std::setw(8) << (rule.passed ? "PASS" : "FAIL") << rule.detail << "\n";
        if (!rule.passed)
        {
            ++failed;
        }
    }
    std::cout << "  " << std::string(90, '-') << "\n";
    if (result.eligible)
    {
        std::cout << "  RESULT: ELIGIBLE - all " << result.rules.size() << " rules passed.\n";
    }
    else
    {
        std::cout << "  RESULT: NOT ELIGIBLE - " << failed << " rule(s) failed (marked FAIL).\n";
    }
    std::cout << "  Most this member can borrow now: " << Utils::formatUGX(result.maximumLoan) << "\n";
}

void SaccoSystem::displayLoanPreview(double amount, double ratePercent, int months) const
{
    double total = Loan::totalPayableFor(amount, ratePercent, months);
    Utils::printTitle("Loan cost summary");
    std::cout << "  Loan amount          : " << Utils::formatUGX(amount) << "\n"
              << "  Interest rate        : " << Utils::toString(ratePercent, 2) << "% per year (reducing balance)\n"
              << "  Repayment period     : " << months << " month(s)\n"
              << "  Monthly installment  : " << Utils::formatUGX(Loan::calculateInstallment(amount, ratePercent, months)) << "\n"
              << "  Total interest       : " << Utils::formatUGX(total - amount) << "\n"
              << "  Total to repay       : " << Utils::formatUGX(total) << "\n";
}

std::string SaccoSystem::readLoanPurpose()
{
    std::cout << "  Loan purpose:\n"
              << "    1. Crop farming inputs (seeds, fertiliser, chemicals)\n"
              << "    2. Livestock or poultry\n"
              << "    3. Farm tools or equipment\n"
              << "    4. Produce trade or storage\n"
              << "    5. School fees\n"
              << "    6. Other business or need\n";
    int choice = InputHelper::readInt("  Choose purpose (1-6): ", 1, 6);
    std::string category;
    switch (choice)
    {
        case 1:  category = "Crop farming inputs";      break;
        case 2:  category = "Livestock or poultry";     break;
        case 3:  category = "Farm tools or equipment";  break;
        case 4:  category = "Produce trade or storage"; break;
        case 5:  category = "School fees";              break;
        default: category = "Other business or need";  break;
    }
    std::string details = InputHelper::readText("  Short description (e.g. buy 2 dairy heifers): ", 3, 50);
    return category + " - " + details;
}

void SaccoSystem::checkEligibilityScreen()
{
    Utils::printTitle("Check loan eligibility");
    int index = promptMemberIndex();
    if (index < 0)
    {
        return;
    }
    const Member& member = members[index];
    std::cout << "  Member: " << member.getFullName() << "   |   Savings: " << Utils::formatUGX(member.getSavingsBalance())
              << "   |   Shares: " << Utils::formatUGX(member.getShareCapital()) << "\n";

    double amount = InputHelper::readAmount("  Loan amount wanted (UGX): ", 1, Config::MAX_TRANSACTION);
    int months = InputHelper::readInt("  Repayment period (months): ", 1, 120);
    EligibilityResult result = checkEligibility(member, amount, months, "");
    displayEligibility(result);
    if (result.eligible)
    {
        displayLoanPreview(amount, settings.getAnnualInterestRate(), months);
    }
}

// =============================================================================
//  CREATE / READ / UPDATE / DELETE LOAN APPLICATIONS
// =============================================================================
void SaccoSystem::applyForLoan()
{
    Utils::printTitle("Apply for a loan");
    int index = promptMemberIndex();
    if (index < 0)
    {
        return;
    }
    const Member& member = members[index];

    // Quick check first so the officer does not type a whole application for nothing.
    int open = findOpenLoanIndex(member.getMemberId(), "");
    if (open >= 0)
    {
        Utils::printError(member.getFullName() + " already has an unfinished loan " + loans[open].getLoanId() + " (" +
                          loans[open].getStatusName() + "). Only one loan at a time is allowed.");
        return;
    }
    std::cout << "  Member: " << member.getFullName() << "   |   Borrowing limit: "
              << Utils::formatUGX(maximumLoanFor(member)) << "\n";

    double amount = InputHelper::readAmount("  Loan amount (UGX): ", 1, Config::MAX_TRANSACTION);
    int months = InputHelper::readInt("  Repayment period (months): ", 1, 120);
    std::string purpose = readLoanPurpose();

    EligibilityResult result = checkEligibility(member, amount, months, "");
    displayEligibility(result);
    if (!result.eligible)
    {
        Utils::printError("Application NOT registered: the member does not meet the rules marked FAIL.");
        logAudit("Loan application refused for " + member.getMemberId() + " (" + Utils::formatUGX(amount) + ")");
        return;
    }
    displayLoanPreview(amount, settings.getAnnualInterestRate(), months);
    if (!InputHelper::readYesNo("  Submit this loan application?"))
    {
        Utils::printInfo("Application cancelled. Nothing was saved.");
        return;
    }
    Loan loan(settings.generateLoanId(), member.getMemberId(), amount, settings.getAnnualInterestRate(), months,
              purpose, Utils::today(), currentUsername());
    loans.push_back(loan);
    saveAll();
    logAudit("Loan application " + loan.getLoanId() + " for " + member.getMemberId() + " (" + Utils::formatUGX(amount) + ")");
    Utils::printSuccess("Loan application " + loan.getLoanId() + " saved with status PENDING. An Administrator must approve it.");
}

// Prints a table of loans (all of them, or one status only). Returns how many were shown.
int SaccoSystem::listLoans(bool allStatuses, LoanStatus status) const
{
    int count = 0;
    double totalPrincipal = 0.0, totalOutstanding = 0.0;
    for (std::size_t i = 0; i < loans.size(); ++i)
    {
        if (!allStatuses && loans[i].getStatus() != status)
        {
            continue;
        }
        if (count == 0)
        {
            Loan::printTableHeader(std::cout);
        }
        loans[i].printTableRow(std::cout, memberNameOf(loans[i].getMemberId()));
        totalPrincipal += loans[i].getPrincipal();
        totalOutstanding += loans[i].getOutstandingBalance();
        ++count;
    }
    if (count == 0)
    {
        Utils::printInfo("No loans found in this category.");
        return 0;
    }
    std::cout << "  " << std::string(93, '-') << "\n"
              << "  " << std::left << std::setw(36) << ("TOTAL (" + Utils::toString(count) + " loans)") << std::right
              << std::setw(12) << Utils::formatMoney(totalPrincipal) << std::setw(20) << ""
              << std::setw(13) << Utils::formatMoney(totalOutstanding) << "\n";
    return count;
}

void SaccoSystem::viewAllLoans() const
{
    Utils::printTitle("View loans");
    if (loans.empty())
    {
        Utils::printInfo("No loans have been recorded yet.");
        return;
    }
    std::cout << "  Show: 1. All  2. Pending  3. Approved  4. Active  5. Completed  6. Rejected\n";
    int filter = InputHelper::readInt("  Choose (1-6): ", 1, 6);
    LoanStatus wanted = LOAN_PENDING;
    switch (filter)
    {
        case 2: wanted = LOAN_PENDING;   break;
        case 3: wanted = LOAN_APPROVED;  break;
        case 4: wanted = LOAN_ACTIVE;    break;
        case 5: wanted = LOAN_COMPLETED; break;
        case 6: wanted = LOAN_REJECTED;  break;
        default: break;
    }
    std::cout << "\n";
    listLoans(filter == 1, wanted);
}

void SaccoSystem::viewLoanDetails()
{
    Utils::printTitle("Loan details and repayment schedule");
    int index = promptLoanIndex();
    if (index < 0)
    {
        return;
    }
    const Loan& loan = loans[index];
    std::cout << "\n  Member Name          : " << memberNameOf(loan.getMemberId()) << "\n";
    loan.displayDetails(std::cout);
    Utils::printTitle("Repayment schedule");
    loan.displaySchedule(std::cout);
    if (loan.getDisbursementDate().empty())
    {
        Utils::printInfo("Due dates are set when the loan is paid out (Month 1 = one month after payout).");
    }
}

void SaccoSystem::updateLoanApplication()
{
    Utils::printTitle("Update a pending loan application");
    int index = promptLoanIndex();
    if (index < 0)
    {
        return;
    }
    Loan& loan = loans[index];
    if (loan.getStatus() != LOAN_PENDING)
    {
        Utils::printError("Only PENDING applications can be changed. Loan " + loan.getLoanId() + " is " +
                          loan.getStatusName() + ".");
        return;
    }
    int memberIndex = findMemberIndex(loan.getMemberId());
    if (memberIndex < 0)
    {
        Utils::printError("The member for this loan no longer exists.");
        return;
    }
    std::cout << "\n";
    loan.displayDetails(std::cout);
    std::cout << "\n";
    Utils::printInfo("Press Enter to keep the value shown in [brackets].");

    double amount = InputHelper::readAmountOrKeep("  Loan amount (UGX)", loan.getPrincipal(), 1, Config::MAX_TRANSACTION);
    int months = InputHelper::readIntOrKeep("  Repayment period (months)", loan.getDurationMonths(), 1, 120);
    std::string purpose = loan.getPurpose();
    if (InputHelper::readYesNo("  Change the loan purpose?"))
    {
        purpose = readLoanPurpose();
    }

    EligibilityResult result = checkEligibility(members[memberIndex], amount, months, loan.getLoanId());
    displayEligibility(result);
    if (!result.eligible)
    {
        Utils::printError("Changes NOT saved: the updated application breaks the rules marked FAIL.");
        return;
    }
    displayLoanPreview(amount, loan.getInterestRate(), months);
    if (!InputHelper::readYesNo("  Save these changes?"))
    {
        Utils::printInfo("Update cancelled. Nothing was saved.");
        return;
    }
    loan.updateApplication(amount, months, purpose);
    saveAll();
    logAudit("Updated loan application " + loan.getLoanId());
    Utils::printSuccess("Loan application " + loan.getLoanId() + " updated.");
}

void SaccoSystem::cancelLoanApplication()
{
    Utils::printTitle("Cancel a loan application");
    int index = promptLoanIndex();
    if (index < 0)
    {
        return;
    }
    const Loan& loan = loans[index];
    if (loan.getStatus() != LOAN_PENDING && loan.getStatus() != LOAN_APPROVED)
    {
        Utils::printError("Only loans not yet paid out (PENDING or APPROVED) can be cancelled. Loan " +
                          loan.getLoanId() + " is " + loan.getStatusName() + ".");
        return;
    }
    std::cout << "\n";
    loan.displayDetails(std::cout);
    std::string loanId = loan.getLoanId();
    if (!InputHelper::readYesNo("  Cancel and delete application " + loanId + "?"))
    {
        Utils::printInfo("Nothing was changed.");
        return;
    }
    loans.erase(loans.begin() + index);
    saveAll();
    logAudit("Cancelled loan application " + loanId);
    Utils::printSuccess("Loan application " + loanId + " has been cancelled and deleted.");
}

// =============================================================================
//  APPROVAL WORKFLOW
// =============================================================================
void SaccoSystem::approveOrRejectLoan()
{
    Utils::printTitle("Approve or reject a loan");
    std::cout << "  Loans waiting for a decision:\n";
    if (listLoans(false, LOAN_PENDING) == 0)
    {
        return;
    }
    std::cout << "\n";
    int index = promptLoanIndex();
    if (index < 0)
    {
        return;
    }
    Loan& loan = loans[index];
    if (loan.getStatus() != LOAN_PENDING)
    {
        Utils::printError("Loan " + loan.getLoanId() + " is " + loan.getStatusName() +
                          ". Only PENDING loans can be approved or rejected.");
        return;
    }
    int memberIndex = findMemberIndex(loan.getMemberId());
    std::cout << "\n  Member Name          : " << memberNameOf(loan.getMemberId()) << "\n";
    loan.displayDetails(std::cout);

    bool eligible = false;
    if (memberIndex >= 0)
    {
        EligibilityResult result = checkEligibility(members[memberIndex], loan.getPrincipal(),
                                                    loan.getDurationMonths(), loan.getLoanId());
        displayEligibility(result);
        eligible = result.eligible;
    }
    if (!eligible)
    {
        Utils::printWarning("The member no longer meets the rules, so this loan can only be rejected.");
    }

    std::cout << "\n  1. Approve\n  2. Reject\n  0. Back (decide later)\n";
    int decision = InputHelper::readMenuChoice(0, 2);
    std::string today = Utils::today();
    switch (decision)
    {
        case 1:
            if (!eligible)
            {
                Utils::printError("Approval blocked: the eligibility rules are not met.");
                return;
            }
            {
                std::string note = InputHelper::readText("  Approval remarks (optional, Enter to skip): ", 0, 60);
                loan.approve(today, currentUsername(), note.empty() ? "Approved" : note);
            }
            saveAll();
            logAudit("Approved loan " + loan.getLoanId());
            Utils::printSuccess("Loan " + loan.getLoanId() + " APPROVED. It can now be paid out (Loan Menu > 8).");
            break;
        case 2:
            {
                std::string reason = InputHelper::readText("  Reason for rejection: ", 3, 60);
                loan.reject(today, currentUsername(), reason);
            }
            saveAll();
            logAudit("Rejected loan " + loan.getLoanId());
            Utils::printSuccess("Loan " + loan.getLoanId() + " REJECTED. The member can apply again later.");
            break;
        default:
            Utils::printInfo("No decision was made.");
            break;
    }
}

void SaccoSystem::disburseLoan()
{
    Utils::printTitle("Disburse (pay out) an approved loan");
    std::cout << "  Approved loans waiting for payout:\n";
    if (listLoans(false, LOAN_APPROVED) == 0)
    {
        return;
    }
    std::cout << "\n";
    int index = promptLoanIndex();
    if (index < 0)
    {
        return;
    }
    Loan& loan = loans[index];
    if (loan.getStatus() != LOAN_APPROVED)
    {
        Utils::printError("Only APPROVED loans can be paid out. Loan " + loan.getLoanId() + " is " +
                          loan.getStatusName() + ".");
        return;
    }
    std::cout << "  Member: " << loan.getMemberId() << " - " << memberNameOf(loan.getMemberId()) << "\n";
    displayLoanPreview(loan.getPrincipal(), loan.getInterestRate(), loan.getDurationMonths());
    if (!InputHelper::readYesNo("  Pay out " + Utils::formatUGX(loan.getPrincipal()) + " to " + loan.getMemberId() + " now?"))
    {
        Utils::printInfo("Payout cancelled.");
        return;
    }
    loan.disburse(Utils::today());
    recordTransaction(loan.getMemberId(), TXN_LOAN_DISBURSEMENT, loan.getPrincipal(), loan.getTotalPayable(),
                      "Loan paid out", loan.getLoanId());
    saveAll();
    logAudit("Disbursed loan " + loan.getLoanId() + " (" + Utils::formatUGX(loan.getPrincipal()) + ")");
    Utils::printSuccess("Loan " + loan.getLoanId() + " is now ACTIVE. First installment due on " +
                        loan.getSchedule().front().dueDate + ".");
    Utils::printTitle("Repayment schedule");
    loan.displaySchedule(std::cout);
}

// =============================================================================
//  REPAYMENTS
// =============================================================================
void SaccoSystem::recordRepayment()
{
    Utils::printTitle("Record a loan repayment");
    int index = promptLoanIndex();
    if (index < 0)
    {
        return;
    }
    Loan& loan = loans[index];
    if (loan.getStatus() != LOAN_ACTIVE)
    {
        Utils::printError("Repayments can only be made on ACTIVE loans. Loan " + loan.getLoanId() + " is " +
                          loan.getStatusName() + ".");
        return;
    }
    std::string today = Utils::today();
    double outstanding = loan.getOutstandingBalance();
    std::cout << "  Loan ID              : " << loan.getLoanId() << "\n"
              << "  Member               : " << loan.getMemberId() << " - " << memberNameOf(loan.getMemberId()) << "\n"
              << "  Monthly installment  : " << Utils::formatUGX(loan.getInstallment()) << "\n"
              << "  Repaid so far        : " << Utils::formatUGX(loan.getAmountPaid()) << "\n"
              << "  Outstanding balance  : " << Utils::formatUGX(outstanding) << "\n"
              << "  Overdue (arrears)    : " << Utils::formatUGX(loan.getArrears(today)) << "\n"
              << "  Oldest unpaid due on : " << loan.getNextDueDate() << "\n";

    double minimum = (outstanding < Config::MIN_TRANSACTION) ? 1 : Config::MIN_TRANSACTION;
    double amount = InputHelper::readAmount("  Repayment amount (UGX): ", minimum, outstanding);
    if (!InputHelper::readYesNo("  Record repayment of " + Utils::formatUGX(amount) + " on " + loan.getLoanId() + "?"))
    {
        Utils::printInfo("Repayment cancelled.");
        return;
    }
    if (!loan.recordRepayment(amount))
    {
        Utils::printError("The repayment could not be recorded.");
        return;
    }
    recordTransaction(loan.getMemberId(), TXN_LOAN_REPAYMENT, amount, loan.getOutstandingBalance(),
                      "Loan repayment", loan.getLoanId());
    saveAll();
    logAudit("Repayment " + Utils::formatUGX(amount) + " on loan " + loan.getLoanId());

    if (loan.getStatus() == LOAN_COMPLETED)
    {
        Utils::printSuccess("Repayment recorded. Loan " + loan.getLoanId() + " is now FULLY PAID (COMPLETED).");
    }
    else
    {
        Utils::printSuccess("Repayment recorded. Outstanding balance: " + Utils::formatUGX(loan.getOutstandingBalance()));
        double arrears = loan.getArrears(today);
        if (arrears > 0)
        {
            Utils::printWarning("Still overdue: " + Utils::formatUGX(arrears));
        }
    }
}

// =============================================================================
//  LOAN CALCULATOR (nothing is saved)
// =============================================================================
void SaccoSystem::loanCalculator() const
{
    Utils::printTitle("Loan calculator");
    Utils::printInfo("Works out the installments for any amount. Nothing is saved.");
    double amount = InputHelper::readAmount("  Loan amount (UGX): ", 1000, 500000000);
    int months = InputHelper::readInt("  Repayment period (months, 1-60): ", 1, 60);
    double rate = InputHelper::readDecimalOrKeep("  Interest rate (% per year)", settings.getAnnualInterestRate(), 0, 60);
    displayLoanPreview(amount, rate, months);
    Utils::printTitle("Repayment schedule (reducing balance)");
    Loan::printSchedule(std::cout, Loan::buildSchedule(amount, rate, months, ""), 0, false);
}
