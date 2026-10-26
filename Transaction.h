/*
 * =============================================================================
 *  File    : Transaction.h
 *  Purpose : One line in the SACCO ledger. EVERY movement of money creates a
 *            Transaction (deposit, withdrawal, share purchase, loan payout,
 *            loan repayment, account closure). Transactions are never edited
 *            or deleted, which gives auditors a complete history.
 * =============================================================================
 */
#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <string>
#include <iostream>

enum TransactionType
{
    TXN_SAVINGS_DEPOSIT,
    TXN_SAVINGS_WITHDRAWAL,
    TXN_SHARE_PURCHASE,
    TXN_LOAN_DISBURSEMENT,
    TXN_LOAN_REPAYMENT,
    TXN_ACCOUNT_CLOSURE
};

class Transaction
{
private:
    std::string     transactionId;   // e.g. TX000015
    std::string     memberId;
    TransactionType type;
    double          amount;          // UGX, always positive
    double          balanceAfter;    // savings, share capital or loan balance after this entry
    std::string     date;            // YYYY-MM-DD
    std::string     time;            // HH:MM:SS
    std::string     description;
    std::string     loanId;          // filled only for loan transactions
    std::string     recordedBy;      // username of the staff member (audit trail)

public:
    Transaction();
    Transaction(const std::string& id, const std::string& member, TransactionType txnType,
                double txnAmount, double balance, const std::string& txnDate, const std::string& txnTime,
                const std::string& details, const std::string& loan, const std::string& user);

    std::string     getTransactionId() const;
    std::string     getMemberId() const;
    TransactionType getType() const;
    double          getAmount() const;
    double          getBalanceAfter() const;
    std::string     getDate() const;
    std::string     getTime() const;
    std::string     getDescription() const;
    std::string     getLoanId() const;
    std::string     getRecordedBy() const;
    std::string     getTypeName() const;
    bool            isMoneyIn() const;   // true when cash comes INTO the SACCO

    void setTransactionId(const std::string& id);   // used only when renumbering demo data

    static std::string typeToCode(TransactionType txnType);
    static bool        codeToType(const std::string& code, TransactionType& txnType);

    // Member ledger layout (statement) and daily report layout
    static void printLedgerHeader(std::ostream& out);
    void        printLedgerRow(std::ostream& out) const;
    static void printDailyHeader(std::ostream& out);
    void        printDailyRow(std::ostream& out) const;

    // File storage: TxnID|MemberID|Type|Amount|BalanceAfter|Date|Time|Description|LoanID|RecordedBy
    std::string toRecord() const;
    static bool fromRecord(const std::string& line, Transaction& txn);
};

#endif // TRANSACTION_H
