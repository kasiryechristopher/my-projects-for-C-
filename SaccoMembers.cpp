/*
 * =============================================================================
 *  File    : SaccoMembers.cpp
 *  Purpose : SaccoSystem functions for MEMBERS and their MONEY:
 *              - register, view, update and delete members (CRUD)
 *              - savings deposits and withdrawals
 *              - share capital purchases
 *              - member ledger (transaction history)
 * =============================================================================
 */
#include "SaccoSystem.h"
#include "Config.h"
#include "Utils.h"
#include "InputHelper.h"

#include <iostream>
#include <iomanip>

// =============================================================================
//  CREATE
// =============================================================================
void SaccoSystem::registerMember()
{
    Utils::printTitle("Register new member");
    Utils::printInfo("Opening balances may be entered for members moving from the paper registers.");

    std::string name = InputHelper::readValidated("  Full name (e.g. Nakato Sarah)       : ", InputHelper::validateName);
    std::string nin  = InputHelper::readValidated("  National ID Number (NIN)            : ", InputHelper::validateNIN);
    int existing = findMemberIndexByNIN(nin);
    if (existing >= 0)
    {
        Utils::printError("This NIN is already registered to " + members[existing].getMemberId() + " (" +
                          members[existing].getFullName() + "). Registration cancelled.");
        return;
    }
    std::string phone   = InputHelper::readValidated("  Phone number (e.g. 0772123456)      : ", InputHelper::validatePhone);
    std::string address = InputHelper::readValidated("  Address / village                   : ", InputHelper::validateAddress);
    std::string today   = Utils::today();
    std::string joined  = InputHelper::readDate("  Date joined (YYYY-MM-DD)", today, Config::EARLIEST_DATE, today);

    std::cout << "  Each share costs " << Utils::formatUGX(Config::SHARE_PRICE) << ". At least 1 share must be bought.\n";
    int shares = InputHelper::readInt("  Number of shares bought             : ", 1, 2500);
    double openingSavings = InputHelper::readAmount("  Opening savings deposit (UGX, 0 = none): ", 0, Config::MAX_TRANSACTION);
    double shareAmount = shares * Config::SHARE_PRICE;

    // Show a summary and ask for confirmation before anything is saved.
    Utils::printTitle("Please confirm");
    std::cout << "  Name          : " << name << "\n"
              << "  NIN           : " << nin << "\n"
              << "  Phone         : " << phone << "\n"
              << "  Address       : " << address << "\n"
              << "  Date joined   : " << joined << "\n"
              << "  Share capital : " << Utils::formatUGX(shareAmount) << " (" << shares << " shares)\n"
              << "  Savings       : " << Utils::formatUGX(openingSavings) << "\n";
    if (!InputHelper::readYesNo("  Save this member?"))
    {
        Utils::printInfo("Registration cancelled. Nothing was saved.");
        return;
    }

    std::string id = settings.generateMemberId();
    Member member(id, name, nin, phone, address, joined, 0, 0);
    member.addShareCapital(shareAmount);
    recordTransaction(id, TXN_SHARE_PURCHASE, shareAmount, member.getShareCapital(),
                      "Shares at registration (" + Utils::toString(shares) + ")", "");
    if (openingSavings > 0)
    {
        member.depositSavings(openingSavings);
        recordTransaction(id, TXN_SAVINGS_DEPOSIT, openingSavings, member.getSavingsBalance(),
                          "Opening savings deposit", "");
    }
    members.push_back(member);
    saveAll();
    logAudit("Registered member " + id + " (" + name + ")");
    Utils::printSuccess("Member registered successfully. Member ID: " + id);
}

// =============================================================================
//  READ
// =============================================================================
void SaccoSystem::viewAllMembers() const
{
    Utils::printTitle("All registered members");
    if (members.empty())
    {
        Utils::printInfo("No members registered yet.");
        return;
    }
    double totalShares = 0.0, totalSavings = 0.0;
    Member::printTableHeader(std::cout);
    for (std::size_t i = 0; i < members.size(); ++i)
    {
        members[i].printTableRow(std::cout);
        totalShares  += members[i].getShareCapital();
        totalSavings += members[i].getSavingsBalance();
    }
    std::cout << "  " << std::string(90, '-') << "\n"
              << "  " << std::left << std::setw(61) << ("TOTAL (" + Utils::toString(static_cast<int>(members.size())) + " members)")
              << std::right << std::setw(14) << Utils::formatMoney(totalShares)
              << std::setw(15) << Utils::formatMoney(totalSavings) << "\n";
}

void SaccoSystem::viewMemberDetails()
{
    Utils::printTitle("View member details");
    int index = promptMemberIndex();
    if (index >= 0)
    {
        showMemberOverview(members[index]);
    }
}

// Profile + loan position + how much the member could borrow.
void SaccoSystem::showMemberOverview(const Member& member) const
{
    std::cout << "\n";
    member.displayProfile(std::cout);

    int loanCount = 0;
    for (std::size_t i = 0; i < loans.size(); ++i)
    {
        if (loans[i].getMemberId() == member.getMemberId()) ++loanCount;
    }
    std::cout << "  Loans on record  : " << loanCount << "\n";

    int open = findOpenLoanIndex(member.getMemberId(), "");
    if (open >= 0)
    {
        const Loan& loan = loans[open];
        std::cout << "  Current loan     : " << loan.getLoanId() << " - " << loan.getStatusName()
                  << ", principal " << Utils::formatUGX(loan.getPrincipal());
        if (loan.getStatus() == LOAN_ACTIVE)
        {
            std::cout << ", balance " << Utils::formatUGX(loan.getOutstandingBalance());
        }
        std::cout << "\n";
    }
    else
    {
        std::cout << "  Current loan     : None\n"
                  << "  Borrowing limit  : " << Utils::formatUGX(maximumLoanFor(member))
                  << " (subject to eligibility rules)\n";
    }
}

// =============================================================================
//  UPDATE
// =============================================================================
void SaccoSystem::updateMember()
{
    Utils::printTitle("Update member details");
    int index = promptMemberIndex();
    if (index < 0)
    {
        return;
    }
    Member& member = members[index];
    std::cout << "\n";
    member.displayProfile(std::cout);
    std::cout << "\n";
    Utils::printInfo("Press Enter to keep the value shown in [brackets].");
    Utils::printInfo("Balances change only through deposits, withdrawals and share purchases.");

    std::string name    = InputHelper::readValidatedOrKeep("  Full name", member.getFullName(), InputHelper::validateName);
    std::string nin     = InputHelper::readValidatedOrKeep("  NIN", member.getNationalId(), InputHelper::validateNIN);
    std::string phone   = InputHelper::readValidatedOrKeep("  Phone", member.getPhoneNumber(), InputHelper::validatePhone);
    std::string address = InputHelper::readValidatedOrKeep("  Address", member.getAddress(), InputHelper::validateAddress);

    if (nin != member.getNationalId())
    {
        int other = findMemberIndexByNIN(nin);
        if (other >= 0 && other != index)
        {
            Utils::printError("That NIN already belongs to " + members[other].getMemberId() + ". No changes were saved.");
            return;
        }
    }
    if (name == member.getFullName() && nin == member.getNationalId() &&
        phone == member.getPhoneNumber() && address == member.getAddress())
    {
        Utils::printInfo("Nothing was changed.");
        return;
    }
    if (!InputHelper::readYesNo("  Save these changes?"))
    {
        Utils::printInfo("Update cancelled. Nothing was saved.");
        return;
    }
    member.setFullName(name);
    member.setNationalId(nin);
    member.setPhoneNumber(phone);
    member.setAddress(address);
    saveAll();
    logAudit("Updated details of member " + member.getMemberId());
    Utils::printSuccess("Member " + member.getMemberId() + " updated successfully.");
}

// =============================================================================
//  DELETE (Administrator only)
// =============================================================================
void SaccoSystem::deleteMember()
{
    Utils::printTitle("Delete member");
    int index = promptMemberIndex();
    if (index < 0)
    {
        return;
    }
    std::string id = members[index].getMemberId();
    std::string name = members[index].getFullName();
    std::cout << "\n";
    members[index].displayProfile(std::cout);

    int open = findOpenLoanIndex(id, "");
    if (open >= 0)
    {
        Utils::printError("Cannot delete: the member has an unfinished loan " + loans[open].getLoanId() + " (" +
                          loans[open].getStatusName() + "). Complete or cancel it first.");
        return;
    }
    double refund = members[index].getTotalFunds();
    if (refund > 0)
    {
        Utils::printWarning("Savings and share capital of " + Utils::formatUGX(refund) +
                            " will be refunded to the member (account closure).");
    }
    Utils::printInfo("Past transactions and finished loans are kept for the auditors.");
    if (!InputHelper::readYesNo("  Permanently delete member " + id + "?"))
    {
        Utils::printInfo("Delete cancelled. Nothing was changed.");
        return;
    }
    if (refund > 0)
    {
        members[index].closeAccount();
        recordTransaction(id, TXN_ACCOUNT_CLOSURE, refund, 0, "Refund on account closure", "");
    }
    members.erase(members.begin() + index);
    saveAll();
    logAudit("Deleted member " + id + " (" + name + "), refunded " + Utils::formatUGX(refund));
    Utils::printSuccess("Member " + id + " (" + name + ") has been deleted.");
}

// =============================================================================
//  SAVINGS AND SHARES
// =============================================================================
void SaccoSystem::depositSavings()
{
    Utils::printTitle("Deposit savings");
    int index = promptMemberIndex();
    if (index < 0)
    {
        return;
    }
    Member& member = members[index];
    std::cout << "  Member: " << member.getFullName() << "   |   Current savings: "
              << Utils::formatUGX(member.getSavingsBalance()) << "\n";

    double amount = InputHelper::readAmount("  Deposit amount (UGX): ", Config::MIN_TRANSACTION, Config::MAX_TRANSACTION);
    if (!InputHelper::readYesNo("  Deposit " + Utils::formatUGX(amount) + " for " + member.getMemberId() + "?"))
    {
        Utils::printInfo("Deposit cancelled.");
        return;
    }
    member.depositSavings(amount);
    recordTransaction(member.getMemberId(), TXN_SAVINGS_DEPOSIT, amount, member.getSavingsBalance(), "Cash deposit", "");
    saveAll();
    logAudit("Savings deposit " + Utils::formatUGX(amount) + " for " + member.getMemberId());
    Utils::printSuccess("Deposit recorded. New savings balance: " + Utils::formatUGX(member.getSavingsBalance()));
}

void SaccoSystem::withdrawSavings()
{
    Utils::printTitle("Withdraw savings");
    int index = promptMemberIndex();
    if (index < 0)
    {
        return;
    }
    Member& member = members[index];
    std::cout << "  Member: " << member.getFullName() << "   |   Current savings: "
              << Utils::formatUGX(member.getSavingsBalance()) << "\n";

    // Savings are security for loans, so they are locked while a loan is unfinished.
    int open = findOpenLoanIndex(member.getMemberId(), "");
    if (open >= 0)
    {
        Utils::printError("Savings are held as security for loan " + loans[open].getLoanId() + " (" +
                          loans[open].getStatusName() + "). Withdrawal is not allowed until it is completed.");
        return;
    }
    double available = member.getSavingsBalance() - Config::MIN_OPERATING_BALANCE;
    if (available < Config::MIN_TRANSACTION)
    {
        Utils::printError("Not enough savings. A minimum balance of " + Utils::formatUGX(Config::MIN_OPERATING_BALANCE) +
                          " must stay on the account.");
        return;
    }
    std::cout << "  Available to withdraw: " << Utils::formatUGX(available)
              << " (" << Utils::formatUGX(Config::MIN_OPERATING_BALANCE) << " must remain)\n";

    double amount = InputHelper::readAmount("  Withdrawal amount (UGX): ", Config::MIN_TRANSACTION, available);
    if (!InputHelper::readYesNo("  Withdraw " + Utils::formatUGX(amount) + " for " + member.getMemberId() + "?"))
    {
        Utils::printInfo("Withdrawal cancelled.");
        return;
    }
    member.withdrawSavings(amount);
    recordTransaction(member.getMemberId(), TXN_SAVINGS_WITHDRAWAL, amount, member.getSavingsBalance(), "Cash withdrawal", "");
    saveAll();
    logAudit("Savings withdrawal " + Utils::formatUGX(amount) + " for " + member.getMemberId());
    Utils::printSuccess("Withdrawal recorded. New savings balance: " + Utils::formatUGX(member.getSavingsBalance()));
}

void SaccoSystem::buyShares()
{
    Utils::printTitle("Buy shares");
    int index = promptMemberIndex();
    if (index < 0)
    {
        return;
    }
    Member& member = members[index];
    std::cout << "  Member: " << member.getFullName() << "   |   Share capital: "
              << Utils::formatUGX(member.getShareCapital()) << " (" << member.getNumberOfShares() << " shares)\n"
              << "  Price per share: " << Utils::formatUGX(Config::SHARE_PRICE) << "\n";

    int shares = InputHelper::readInt("  Number of shares to buy (1-500): ", 1, 500);
    double amount = shares * Config::SHARE_PRICE;
    if (!InputHelper::readYesNo("  Buy " + Utils::toString(shares) + " share(s) for " + Utils::formatUGX(amount) + "?"))
    {
        Utils::printInfo("Share purchase cancelled.");
        return;
    }
    member.addShareCapital(amount);
    recordTransaction(member.getMemberId(), TXN_SHARE_PURCHASE, amount, member.getShareCapital(),
                      "Bought " + Utils::toString(shares) + " share(s)", "");
    saveAll();
    logAudit("Share purchase " + Utils::formatUGX(amount) + " for " + member.getMemberId());
    Utils::printSuccess("Shares recorded. New share capital: " + Utils::formatUGX(member.getShareCapital()) +
                        " (" + Utils::toString(member.getNumberOfShares()) + " shares)");
}

void SaccoSystem::viewMemberLedger()
{
    Utils::printTitle("Member ledger");
    int index = promptMemberIndex();
    if (index < 0)
    {
        return;
    }
    const Member& member = members[index];
    std::cout << "\n  " << member.getMemberId() << " - " << member.getFullName() << "\n\n";

    int count = 0;
    Transaction::printLedgerHeader(std::cout);
    for (std::size_t i = 0; i < transactions.size(); ++i)
    {
        if (transactions[i].getMemberId() == member.getMemberId())
        {
            transactions[i].printLedgerRow(std::cout);
            ++count;
        }
    }
    if (count == 0)
    {
        Utils::printInfo("No transactions recorded for this member yet.");
    }
    std::cout << "\n  Transactions: " << count
              << "   |   Savings: " << Utils::formatUGX(member.getSavingsBalance())
              << "   |   Share capital: " << Utils::formatUGX(member.getShareCapital()) << "\n"
              << "  (Balance After = savings balance for deposits/withdrawals, share capital for shares,\n"
              << "   and loan balance still owed for loan entries.)\n";
}
