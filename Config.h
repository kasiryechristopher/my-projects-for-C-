/*
 * =============================================================================
 *  NDEJJE FARMERS SACCO - LOAN MANAGEMENT INFORMATION SYSTEM
 *  File    : Config.h
 *  Purpose : One central place for fixed values used across the system
 *            (SACCO details, data file names and fixed business values).
 *            Keeping them here means a value is changed in ONE place only.
 * =============================================================================
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <string>

namespace Config
{
    // ---------------- Organisation details ----------------
    const std::string SACCO_NAME     = "NDEJJE FARMERS SACCO";
    const std::string SACCO_ADDRESS  = "Ndejje Town Council, Luweero District, Uganda";
    const std::string SYSTEM_NAME    = "Loan Management Information System";
    const std::string SYSTEM_VERSION = "1.0";

    // ---------------- Data files (plain text, one record per line) ----------------
    const std::string MEMBERS_FILE      = "ndejje_members.txt";
    const std::string LOANS_FILE        = "ndejje_loans.txt";
    const std::string TRANSACTIONS_FILE = "ndejje_transactions.txt";
    const std::string USERS_FILE        = "ndejje_users.txt";
    const std::string SETTINGS_FILE     = "ndejje_settings.txt";
    const std::string AUDIT_LOG_FILE    = "ndejje_audit_log.txt";

    const char FIELD_DELIMITER = '|';   // separates the fields inside one record line

    // ---------------- Fixed business values ----------------
    const double SHARE_PRICE           = 20000.0;      // UGX per share
    const double MIN_TRANSACTION       = 1000.0;       // smallest deposit / withdrawal (UGX)
    const double MAX_TRANSACTION       = 100000000.0;  // safety ceiling of UGX 100 million
    const double MIN_OPERATING_BALANCE = 10000.0;      // savings that must stay on every account
    const std::string EARLIEST_DATE    = "2000-01-01"; // oldest date the system accepts

    // ---------------- User interface ----------------
    const int MAX_LOGIN_ATTEMPTS = 3;
    const int SCREEN_WIDTH       = 96;
}

#endif // CONFIG_H
