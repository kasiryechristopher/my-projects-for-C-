/*
 * =============================================================================
 *  File    : Transaction.cpp
 *  Purpose : Implementation of the Transaction (ledger entry) class.
 * =============================================================================
 */
#include "Transaction.h"
#include "Utils.h"
#include "Config.h"

#include <iomanip>
#include <vector>
#include <stdexcept>

Transaction::Transaction()
    : transactionId(""), memberId(""), type(TXN_SAVINGS_DEPOSIT), amount(0.0), balanceAfter(0.0),
      date(""), time(""), description(""), loanId(""), recordedBy("")
{
}

Transaction::Transaction(const std::string& id, const std::string& member, TransactionType txnType,
                         double txnAmount, double balance, const std::string& txnDate,
                         const std::string& txnTime, const std::string& details,
                         const std::string& loan, const std::string& user)
    : transactionId(id), memberId(member), type(txnType), amount(txnAmount), balanceAfter(balance),
      date(txnDate), time(txnTime), description(details), loanId(loan), recordedBy(user)
{
}

std::string Transaction::getTransactionId() const { return transactionId; }
std::string Transaction::getMemberId() const      { return memberId; }
TransactionType Transaction::getType() const      { return type; }
double Transaction::getAmount() const             { return amount; }
double Transaction::getBalanceAfter() const       { return balanceAfter; }
std::string Transaction::getDate() const          { return date; }
std::string Transaction::getTime() const          { return time; }
std::string Transaction::getDescription() const   { return description; }
std::string Transaction::getLoanId() const        { return loanId; }
std::string Transaction::getRecordedBy() const    { return recordedBy; }

void Transaction::setTransactionId(const std::string& id)
{
    transactionId = id;
}

std::string Transaction::getTypeName() const
{
    switch (type)
    {
        case TXN_SAVINGS_DEPOSIT:    return "Savings Deposit";
        case TXN_SAVINGS_WITHDRAWAL: return "Savings Withdrawal";
        case TXN_SHARE_PURCHASE:     return "Share Purchase";
        case TXN_LOAN_DISBURSEMENT:  return "Loan Disbursement";
        case TXN_LOAN_REPAYMENT:     return "Loan Repayment";
        case TXN_ACCOUNT_CLOSURE:    return "Account Closure";
    }
    return "Unknown";
}

// Deposits, share purchases and repayments bring cash INTO the SACCO.
bool Transaction::isMoneyIn() const
{
    return type == TXN_SAVINGS_DEPOSIT || type == TXN_SHARE_PURCHASE || type == TXN_LOAN_REPAYMENT;
}

std::string Transaction::typeToCode(TransactionType txnType)
{
    switch (txnType)
    {
        case TXN_SAVINGS_DEPOSIT:    return "DEPOSIT";
        case TXN_SAVINGS_WITHDRAWAL: return "WITHDRAWAL";
        case TXN_SHARE_PURCHASE:     return "SHARES";
        case TXN_LOAN_DISBURSEMENT:  return "LOAN_OUT";
        case TXN_LOAN_REPAYMENT:     return "REPAYMENT";
        case TXN_ACCOUNT_CLOSURE:    return "CLOSURE";
    }
    return "DEPOSIT";
}

bool Transaction::codeToType(const std::string& code, TransactionType& txnType)
{
    if (code == "DEPOSIT")         txnType = TXN_SAVINGS_DEPOSIT;
    else if (code == "WITHDRAWAL") txnType = TXN_SAVINGS_WITHDRAWAL;
    else if (code == "SHARES")     txnType = TXN_SHARE_PURCHASE;
    else if (code == "LOAN_OUT")   txnType = TXN_LOAN_DISBURSEMENT;
    else if (code == "REPAYMENT")  txnType = TXN_LOAN_REPAYMENT;
    else if (code == "CLOSURE")    txnType = TXN_ACCOUNT_CLOSURE;
    else return false;
    return true;
}

// ---------------------------------------------------------------------------
//  Table layouts
// ---------------------------------------------------------------------------
void Transaction::printLedgerHeader(std::ostream& out)
{
    out << "  " << std::left << std::setw(12) << "Date" << std::setw(10) << "Txn ID"
        << std::setw(20) << "Type" << std::right << std::setw(14) << "Amount (UGX)"
        << std::setw(15) << "Balance After" << "  " << std::left << "Details\n";
    out << "  " << std::string(92, '-') << "\n";
}

void Transaction::printLedgerRow(std::ostream& out) const
{
    out << "  " << std::left << std::setw(12) << date << std::setw(10) << transactionId
        << std::setw(20) << getTypeName() << std::right << std::setw(14) << Utils::formatMoney(amount)
        << std::setw(15) << Utils::formatMoney(balanceAfter) << "  " << std::left
        << Utils::fitWidth(description, 19) << "\n";
}

void Transaction::printDailyHeader(std::ostream& out)
{
    out << "  " << std::left << std::setw(10) << "Time" << std::setw(10) << "Txn ID"
        << std::setw(10) << "Member" << std::setw(20) << "Type" << std::right
        << std::setw(14) << "Money In" << std::setw(14) << "Money Out" << "  " << std::left
        << "By\n";
    out << "  " << std::string(90, '-') << "\n";
}

void Transaction::printDailyRow(std::ostream& out) const
{
    out << "  " << std::left << std::setw(10) << time << std::setw(10) << transactionId
        << std::setw(10) << memberId << std::setw(20) << getTypeName() << std::right
        << std::setw(14) << (isMoneyIn() ? Utils::formatMoney(amount) : "-")
        << std::setw(14) << (isMoneyIn() ? "-" : Utils::formatMoney(amount)) << "  " << std::left
        << recordedBy << "\n";
}

// ---------------------------------------------------------------------------
//  File storage
// ---------------------------------------------------------------------------
std::string Transaction::toRecord() const
{
    const char d = Config::FIELD_DELIMITER;
    return transactionId + d + memberId + d + typeToCode(type) + d + Utils::toString(amount, 2) + d +
           Utils::toString(balanceAfter, 2) + d + date + d + time + d + description + d + loanId + d +
           recordedBy;
}

bool Transaction::fromRecord(const std::string& line, Transaction& txn)
{
    std::vector<std::string> field = Utils::split(line, Config::FIELD_DELIMITER);
    TransactionType txnType;
    if (field.size() != 10 || field[0].empty() || !codeToType(field[2], txnType) ||
        !Utils::isValidDate(field[5]))
    {
        return false;
    }
    try
    {
        double txnAmount = Utils::numberOrThrow(field[3]);
        double balance   = Utils::numberOrThrow(field[4]);
        if (txnAmount < 0)
        {
            return false;
        }
        txn = Transaction(field[0], field[1], txnType, txnAmount, balance, field[5], field[6],
                          field[7], field[8], field[9]);
        return true;
    }
    catch (const std::invalid_argument&)   // damaged number in the file
    {
        return false;
    }
}
