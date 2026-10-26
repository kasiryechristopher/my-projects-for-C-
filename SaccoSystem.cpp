/*
 * =============================================================================
 *  File    : SaccoSystem.cpp
 *  Purpose : Core of the SaccoSystem class:
 *              - loading and saving all data files
 *              - staff login, main menu and sub-menus
 *              - administration (policy, staff users, backup, audit log,
 *                demonstration data)
 *              - shared look-up helpers used by the other modules
 * =============================================================================
 */
#include "SaccoSystem.h"
#include "Config.h"
#include "Utils.h"
#include "InputHelper.h"
#include "FileManager.h"

#include <iostream>
#include <algorithm>

// ---------------------------------------------------------------------------
//  File-local helpers
// ---------------------------------------------------------------------------
namespace
{
    // Column headings written at the top of each data file (for human readers).
    const std::string MEMBERS_HEADER =
        "# MemberID|FullName|NIN|Phone|Address|DateJoined|ShareCapital|SavingsBalance";
    const std::string LOANS_HEADER =
        "# LoanID|MemberID|Principal|InterestRate|Months|Purpose|Status|AppliedOn|DecisionDate|"
        "DisbursedOn|AmountPaid|AppliedBy|DecidedBy|Remarks";
    const std::string TRANSACTIONS_HEADER =
        "# TxnID|MemberID|Type|Amount|BalanceAfter|Date|Time|Description|LoanID|RecordedBy";
    const std::string USERS_HEADER = "# Username|PasswordHash|Role|FullName|Phone|Active";

    // "NFS0012" with prefix length 3 -> 12. Returns 0 if there is no number.
    int idNumber(const std::string& id, std::size_t prefixLength)
    {
        int number = 0;
        if (id.size() > prefixLength && Utils::parseInt(id.substr(prefixLength), number))
        {
            return number;
        }
        return 0;
    }

    // Sort rule for the ledger: earlier date/time first.
    bool happensEarlier(const Transaction& first, const Transaction& second)
    {
        return first.getDate() + first.getTime() < second.getDate() + second.getTime();
    }

    // Keeps a copy of a data file that contained damaged lines, before it is rewritten.
    void keepDamagedCopy(const std::string& fileName, int badLines)
    {
        if (badLines > 0)
        {
            std::string copyName = fileName + ".damaged_" + Utils::removeChar(Utils::today(), '-');
            FileManager::copyFile(fileName, copyName);
            Utils::printWarning(Utils::toString(badLines) + " damaged line(s) skipped in " + fileName +
                                ". Original kept as " + copyName);
        }
    }
}

// =============================================================================
//  START-UP, LOADING AND SAVING
// =============================================================================
SaccoSystem::SaccoSystem() : currentUserIndex(-1), firstRun(false)
{
    loadAllData();
}

void SaccoSystem::loadAllData()
{
    int badMembers = 0, badLoans = 0, badTransactions = 0, badUsers = 0;

    FileManager::loadSettings(Config::SETTINGS_FILE, settings);   // missing file = default policy
    FileManager::loadRecords(Config::MEMBERS_FILE, members, badMembers);
    FileManager::loadRecords(Config::LOANS_FILE, loans, badLoans);
    FileManager::loadRecords(Config::TRANSACTIONS_FILE, transactions, badTransactions);
    FileManager::loadRecords(Config::USERS_FILE, users, badUsers);

    keepDamagedCopy(Config::MEMBERS_FILE, badMembers);
    keepDamagedCopy(Config::LOANS_FILE, badLoans);
    keepDamagedCopy(Config::TRANSACTIONS_FILE, badTransactions);
    keepDamagedCopy(Config::USERS_FILE, badUsers);

    // Make sure newly generated IDs can never repeat an ID already in the files.
    int highestMember = 0, highestLoan = 0, highestTransaction = 0;
    for (std::size_t i = 0; i < members.size(); ++i)
        highestMember = std::max(highestMember, idNumber(members[i].getMemberId(), 3));
    for (std::size_t i = 0; i < loans.size(); ++i)
        highestLoan = std::max(highestLoan, idNumber(loans[i].getLoanId(), 2));
    for (std::size_t i = 0; i < transactions.size(); ++i)
        highestTransaction = std::max(highestTransaction, idNumber(transactions[i].getTransactionId(), 2));
    settings.ensureCountersAbove(highestMember, highestLoan, highestTransaction);

    if (users.empty())
    {
        createDefaultUsers();
    }
    FileManager::saveSettings(Config::SETTINGS_FILE, settings);
}

// Saves every data file. Called after EVERY change so nothing is lost if the
// computer switches off. Each file is saved even if an earlier one fails.
bool SaccoSystem::saveAll()
{
    bool membersSaved      = FileManager::saveRecords(Config::MEMBERS_FILE, MEMBERS_HEADER, members);
    bool loansSaved        = FileManager::saveRecords(Config::LOANS_FILE, LOANS_HEADER, loans);
    bool transactionsSaved = FileManager::saveRecords(Config::TRANSACTIONS_FILE, TRANSACTIONS_HEADER, transactions);
    bool usersSaved        = FileManager::saveRecords(Config::USERS_FILE, USERS_HEADER, users);
    bool settingsSaved     = FileManager::saveSettings(Config::SETTINGS_FILE, settings);

    bool allSaved = membersSaved && loansSaved && transactionsSaved && usersSaved && settingsSaved;
    if (!allSaved)
    {
        Utils::printError("Some data could not be saved. Check that the program folder is not "
                          "read-only and the disk is not full.");
    }
    return allSaved;
}

void SaccoSystem::createDefaultUsers()
{
    users.push_back(StaffUser("admin", "admin123", ROLE_ADMINISTRATOR, "System Administrator", "0700000001"));
    users.push_back(StaffUser("officer", "officer123", ROLE_LOAN_OFFICER, "Default Loan Officer", "0700000002"));
    FileManager::saveRecords(Config::USERS_FILE, USERS_HEADER, users);
    firstRun = true;
}

// Every important action is written to the audit log with date, time and user.
void SaccoSystem::logAudit(const std::string& action)
{
    std::string line = Utils::today() + " " + Utils::currentTime() + " | " +
                       Utils::padRight(currentUsername(), 12) + " | " + action;
    if (!FileManager::appendLine(Config::AUDIT_LOG_FILE, line))
    {
        Utils::printWarning("Could not write to the audit log file.");
    }
}

// =============================================================================
//  SESSION HELPERS
// =============================================================================
bool SaccoSystem::isAdministrator() const
{
    return currentUserIndex >= 0 && users[currentUserIndex].isAdministrator();
}

std::string SaccoSystem::currentUsername() const
{
    return currentUserIndex >= 0 ? users[currentUserIndex].getUsername() : "self-service";
}

bool SaccoSystem::requireAdministrator() const
{
    if (!isAdministrator())
    {
        Utils::printError("Access denied. Only an Administrator can do this.");
        return false;
    }
    return true;
}

void SaccoSystem::showBanner(const std::string& screenTitle) const
{
    Utils::clearScreen();
    Utils::printLine('=');
    Utils::printCentered(Config::SACCO_NAME);
    Utils::printCentered(Config::SYSTEM_NAME + "  (Version " + Config::SYSTEM_VERSION + ")");
    Utils::printCentered(Config::SACCO_ADDRESS);
    Utils::printLine('=');
    std::cout << "  Date: " << Utils::today();
    if (currentUserIndex >= 0)
    {
        std::cout << "   |   Logged in: " << users[currentUserIndex].getFullName() << " ("
                  << users[currentUserIndex].getRoleTitle() << ")";
    }
    std::cout << "\n";
    Utils::printTitle(screenTitle);
}

// =============================================================================
//  PROGRAM ENTRY AND MENUS
// =============================================================================
void SaccoSystem::run()
{
    bool finished = false;
    while (!finished)
    {
        try
        {
            welcomeMenu();
            finished = true;                       // user chose Exit
        }
        catch (const InputClosedException&)
        {
            std::cout << "\n";
            Utils::printInfo("Input closed. All data has been saved. Goodbye.");
            finished = true;
        }
        catch (const std::exception& error)
        {
            // Last safety net: report the problem and return to the welcome screen.
            Utils::printError(std::string("Unexpected problem: ") + error.what() + ". Returning to the welcome screen.");
            currentUserIndex = -1;
        }
    }
    saveAll();
}

void SaccoSystem::welcomeMenu()
{
    int choice = 0;
    do
    {
        currentUserIndex = -1;
        showBanner("Welcome");
        if (firstRun)
        {
            Utils::printInfo("First run: default logins were created.");
            std::cout << "         Administrator -> username: admin    password: admin123\n"
                      << "         Loan Officer  -> username: officer  password: officer123\n"
                      << "         Please change these passwords after logging in.\n\n";
        }
        std::cout << "  1. Staff Login (Administrator / Loan Officer)\n"
                  << "  2. Member Self-Service (check my account)\n"
                  << "  0. Exit\n";
        choice = InputHelper::readMenuChoice(0, 2);

        switch (choice)
        {
            case 1:
                if (staffLogin())
                {
                    staffMainMenu();
                }
                break;
            case 2:
                memberSelfService();
                break;
            case 0:
                Utils::printInfo("Thank you for using the system. All data has been saved. Goodbye!");
                break;
        }
    } while (choice != 0);
}

bool SaccoSystem::staffLogin()
{
    showBanner("Staff Login");
    for (int attempt = 1; attempt <= Config::MAX_LOGIN_ATTEMPTS; ++attempt)
    {
        std::string username = Utils::toLower(Utils::trim(InputHelper::readLine("  Username: ")));
        std::string password = InputHelper::readPassword("  Password: ");
        int index = findUserIndex(username);

        if (index >= 0 && users[index].checkPassword(password))
        {
            if (!users[index].isActive())
            {
                Utils::printError("This account has been deactivated. Please contact the Administrator.");
                InputHelper::pause();
                return false;
            }
            currentUserIndex = index;
            firstRun = false;
            logAudit("Logged in");
            Utils::printSuccess("Welcome, " + users[index].getFullName() + "!");
            if (users[index].checkPassword(users[index].getUsername() + "123"))
            {
                Utils::printWarning("You are using the default password. Please change it (Main Menu > 7).");
            }
            InputHelper::pause();
            return true;
        }

        int attemptsLeft = Config::MAX_LOGIN_ATTEMPTS - attempt;
        Utils::printError("Wrong username or password." +
                          (attemptsLeft > 0 ? " Attempts left: " + Utils::toString(attemptsLeft) : std::string("")));
    }
    Utils::printError("Too many failed attempts. Returning to the welcome screen.");
    FileManager::appendLine(Config::AUDIT_LOG_FILE, Utils::today() + " " + Utils::currentTime() +
                            " | " + Utils::padRight("unknown", 12) + " | Login blocked after 3 failed attempts");
    InputHelper::pause();
    return false;
}

void SaccoSystem::staffMainMenu()
{
    int choice = 0;
    do
    {
        showBanner("Main Menu");
        std::cout << "  1. Member Management\n"
                  << "  2. Savings and Share Capital\n"
                  << "  3. Loan Management\n"
                  << "  4. Search Records\n"
                  << "  5. Reports\n"
                  << "  6. Administration" << (isAdministrator() ? "" : "   (Administrator only)") << "\n"
                  << "  7. Change My Password\n"
                  << "  0. Logout\n";
        choice = InputHelper::readMenuChoice(0, 7);

        // Any unexpected error inside a module is caught here, reported, and the
        // user is returned to the main menu - the program never crashes.
        try
        {
            switch (choice)
            {
                case 1: memberMenu();  break;
                case 2: savingsMenu(); break;
                case 3: loanMenu();    break;
                case 4: searchMenu();  break;
                case 5: reportsMenu(); break;
                case 6:
                    if (requireAdministrator())
                    {
                        administrationMenu();
                    }
                    else
                    {
                        InputHelper::pause();
                    }
                    break;
                case 7:
                    changeMyPassword();
                    InputHelper::pause();
                    break;
                case 0:
                    logAudit("Logged out");
                    Utils::printInfo("You have been logged out.");
                    break;
            }
        }
        catch (const InputClosedException&)
        {
            throw;                                   // handled in run()
        }
        catch (const std::exception& error)
        {
            Utils::printError(std::string("Unexpected problem: ") + error.what() +
                              ". The operation was cancelled and your data is safe.");
            InputHelper::pause();
        }
    } while (choice != 0);
    currentUserIndex = -1;
}

void SaccoSystem::memberMenu()
{
    int choice = 0;
    do
    {
        showBanner("Member Management");
        std::cout << "  1. Register New Member\n"
                  << "  2. View All Members\n"
                  << "  3. View Member Details\n"
                  << "  4. Update Member Details\n"
                  << "  5. Delete Member" << (isAdministrator() ? "" : "   (Administrator only)") << "\n"
                  << "  0. Back to Main Menu\n";
        choice = InputHelper::readMenuChoice(0, 5);
        switch (choice)
        {
            case 1: registerMember();    break;
            case 2: viewAllMembers();    break;
            case 3: viewMemberDetails(); break;
            case 4: updateMember();      break;
            case 5:
                if (requireAdministrator())
                {
                    deleteMember();
                }
                break;
        }
        if (choice != 0)
        {
            InputHelper::pause();
        }
    } while (choice != 0);
}

void SaccoSystem::savingsMenu()
{
    int choice = 0;
    do
    {
        showBanner("Savings and Share Capital");
        std::cout << "  1. Deposit Savings\n"
                  << "  2. Withdraw Savings\n"
                  << "  3. Buy Shares (share capital contribution)\n"
                  << "  4. View Member Ledger (transaction history)\n"
                  << "  0. Back to Main Menu\n";
        choice = InputHelper::readMenuChoice(0, 4);
        switch (choice)
        {
            case 1: depositSavings();   break;
            case 2: withdrawSavings();  break;
            case 3: buyShares();        break;
            case 4: viewMemberLedger(); break;
        }
        if (choice != 0)
        {
            InputHelper::pause();
        }
    } while (choice != 0);
}

void SaccoSystem::loanMenu()
{
    int choice = 0;
    do
    {
        showBanner("Loan Management");
        std::cout << "   1. Check Loan Eligibility\n"
                  << "   2. Apply for a Loan\n"
                  << "   3. View All Loans\n"
                  << "   4. View Loan Details and Repayment Schedule\n"
                  << "   5. Update a Pending Loan Application\n"
                  << "   6. Cancel a Pending Loan Application\n"
                  << "   7. Approve / Reject a Loan" << (isAdministrator() ? "" : "   (Administrator only)") << "\n"
                  << "   8. Disburse an Approved Loan\n"
                  << "   9. Record a Loan Repayment\n"
                  << "  10. Loan Calculator (interest and installments)\n"
                  << "   0. Back to Main Menu\n";
        choice = InputHelper::readMenuChoice(0, 10);
        switch (choice)
        {
            case 1:  checkEligibilityScreen(); break;
            case 2:  applyForLoan();           break;
            case 3:  viewAllLoans();           break;
            case 4:  viewLoanDetails();        break;
            case 5:  updateLoanApplication();  break;
            case 6:  cancelLoanApplication();  break;
            case 7:
                if (requireAdministrator())
                {
                    approveOrRejectLoan();
                }
                break;
            case 8:  disburseLoan();           break;
            case 9:  recordRepayment();        break;
            case 10: loanCalculator();         break;
        }
        if (choice != 0)
        {
            InputHelper::pause();
        }
    } while (choice != 0);
}

void SaccoSystem::searchMenu()
{
    int choice = 0;
    do
    {
        showBanner("Search Records");
        std::cout << "  1. Search Member by Member ID\n"
                  << "  2. Search Member by National ID (NIN)\n"
                  << "  3. Search Member by Name (full or part of name)\n"
                  << "  4. Search Member by Phone Number\n"
                  << "  5. Search Loan by Loan ID\n"
                  << "  0. Back to Main Menu\n";
        choice = InputHelper::readMenuChoice(0, 5);
        switch (choice)
        {
            case 1: searchByMemberId(); break;
            case 2: searchByNIN();      break;
            case 3: searchByName();     break;
            case 4: searchByPhone();    break;
            case 5: searchLoanById();   break;
        }
        if (choice != 0)
        {
            InputHelper::pause();
        }
    } while (choice != 0);
}

void SaccoSystem::reportsMenu()
{
    int choice = 0;
    do
    {
        showBanner("Reports");
        std::cout << "  1. SACCO Financial Summary\n"
                  << "  2. Member Register (all members and balances)\n"
                  << "  3. Active Loans Report\n"
                  << "  4. Loans in Arrears (defaulters) Report\n"
                  << "  5. Individual Member Statement\n"
                  << "  6. Daily Transactions Report\n"
                  << "  7. Export Member Statement to a Text File\n"
                  << "  0. Back to Main Menu\n";
        choice = InputHelper::readMenuChoice(0, 7);
        switch (choice)
        {
            case 1: financialSummaryReport();  break;
            case 2: memberRegisterReport();    break;
            case 3: activeLoansReport();       break;
            case 4: arrearsReport();           break;
            case 5: memberStatementScreen();   break;
            case 6: dailyTransactionsReport(); break;
            case 7: exportMemberStatement();   break;
        }
        if (choice != 0)
        {
            InputHelper::pause();
        }
    } while (choice != 0);
}

void SaccoSystem::administrationMenu()
{
    int choice = 0;
    do
    {
        showBanner("Administration");
        std::cout << "  1. View Loan Policy Settings\n"
                  << "  2. Update Loan Policy Settings\n"
                  << "  3. Add Staff User\n"
                  << "  4. View Staff Users\n"
                  << "  5. Reset a Staff Password\n"
                  << "  6. Activate / Deactivate a Staff User\n"
                  << "  7. Back Up All Data Files\n"
                  << "  8. View Audit Log (latest 30 entries)\n"
                  << "  9. Load Demonstration Data (empty system only)\n"
                  << "  0. Back to Main Menu\n";
        choice = InputHelper::readMenuChoice(0, 9);
        switch (choice)
        {
            case 1:
                Utils::printTitle("Current loan policy");
                settings.display(std::cout);
                break;
            case 2: updateLoanPolicy();       break;
            case 3: addStaffUser();           break;
            case 4: listStaffUsers();         break;
            case 5: resetStaffPassword();     break;
            case 6: toggleStaffStatus();      break;
            case 7: backupData();             break;
            case 8: viewAuditLog();           break;
            case 9: loadDemonstrationData();  break;
        }
        if (choice != 0)
        {
            InputHelper::pause();
        }
    } while (choice != 0);
}

// =============================================================================
//  ADMINISTRATION
// =============================================================================

// Asks for a new password twice. Returns "" if the user fails 3 times.
std::string SaccoSystem::readNewPassword(const std::string& username)
{
    for (int attempt = 1; attempt <= 3; ++attempt)
    {
        std::string password = InputHelper::readPassword("  New password (6-30 characters): ");
        if (password.size() < 6 || password.size() > 30)
        {
            Utils::printError("Password must be 6 to 30 characters long.");
            continue;
        }
        if (Utils::equalsIgnoreCase(password, username))
        {
            Utils::printError("Password must not be the same as the username.");
            continue;
        }
        std::string confirm = InputHelper::readPassword("  Confirm new password: ");
        if (confirm != password)
        {
            Utils::printError("The two passwords do not match.");
            continue;
        }
        return password;
    }
    Utils::printError("Password not set after 3 attempts.");
    return "";
}

void SaccoSystem::changeMyPassword()
{
    Utils::printTitle("Change my password");
    StaffUser& me = users[currentUserIndex];
    std::string current = InputHelper::readPassword("  Current password: ");
    if (!me.checkPassword(current))
    {
        Utils::printError("Current password is wrong. Password not changed.");
        return;
    }
    std::string newPassword = readNewPassword(me.getUsername());
    if (newPassword.empty())
    {
        return;
    }
    me.changePassword(newPassword);
    saveAll();
    logAudit("Changed own password");
    Utils::printSuccess("Your password has been changed.");
}

void SaccoSystem::updateLoanPolicy()
{
    Utils::printTitle("Update loan policy");
    settings.display(std::cout);
    std::cout << "\n";
    Utils::printInfo("Press Enter to keep the current value shown in [brackets].");

    double rate     = InputHelper::readDecimalOrKeep("  Interest rate (% per year)", settings.getAnnualInterestRate(), 0, 60);
    double multiple = InputHelper::readDecimalOrKeep("  Loan multiplier (x savings + shares)", settings.getLoanMultiplier(), 1, 10);
    double minShare = InputHelper::readAmountOrKeep("  Minimum share capital to borrow (UGX)", settings.getMinimumShareCapital(), 0, 10000000);
    double minSave  = InputHelper::readAmountOrKeep("  Minimum savings to borrow (UGX)", settings.getMinimumSavings(), 0, 10000000);
    double minLoan  = InputHelper::readAmountOrKeep("  Smallest loan allowed (UGX)", settings.getMinimumLoanAmount(), 10000, 500000000);
    double maxLoan  = InputHelper::readAmountOrKeep("  Largest loan allowed (UGX)", settings.getMaximumLoanAmount(), 10000, 500000000);
    int maxMonths   = InputHelper::readIntOrKeep("  Longest repayment period (months)", settings.getMaximumDurationMonths(), 1, 60);
    int minMember   = InputHelper::readIntOrKeep("  Minimum membership period (months)", settings.getMinimumMembershipMonths(), 0, 24);

    if (minLoan >= maxLoan)
    {
        Utils::printError("The smallest loan must be less than the largest loan. No changes were saved.");
        return;
    }
    if (!InputHelper::readYesNo("  Save these policy changes?"))
    {
        Utils::printInfo("No changes were made.");
        return;
    }
    double oldRate = settings.getAnnualInterestRate();
    settings.setAnnualInterestRate(rate);
    settings.setLoanMultiplier(multiple);
    settings.setMinimumShareCapital(minShare);
    settings.setMinimumSavings(minSave);
    settings.setLoanLimits(minLoan, maxLoan);
    settings.setMaximumDurationMonths(maxMonths);
    settings.setMinimumMembershipMonths(minMember);
    saveAll();
    logAudit("Updated loan policy (interest " + Utils::toString(oldRate, 2) + "% -> " + Utils::toString(rate, 2) + "%)");
    Utils::printSuccess("Loan policy updated. Existing loans keep the interest rate they were given.");
}

void SaccoSystem::addStaffUser()
{
    Utils::printTitle("Add staff user");
    std::string username = InputHelper::readValidated("  New username: ", InputHelper::validateUsername);
    if (findUserIndex(username) >= 0)
    {
        Utils::printError("The username '" + username + "' is already taken.");
        return;
    }
    std::string name  = InputHelper::readValidated("  Full name: ", InputHelper::validateName);
    std::string phone = InputHelper::readValidated("  Phone number: ", InputHelper::validatePhone);
    std::cout << "  Roles: 1. Administrator   2. Loan Officer\n";
    int roleChoice = InputHelper::readInt("  Choose role (1-2): ", 1, 2);
    std::string password = readNewPassword(username);
    if (password.empty())
    {
        return;
    }
    UserRole role = (roleChoice == 1) ? ROLE_ADMINISTRATOR : ROLE_LOAN_OFFICER;
    users.push_back(StaffUser(username, password, role, name, phone));
    saveAll();
    logAudit("Added staff user " + username + " (" + users.back().getRoleTitle() + ")");
    Utils::printSuccess("Staff user '" + username + "' created as " + users.back().getRoleTitle() + ".");
}

void SaccoSystem::listStaffUsers() const
{
    Utils::printTitle("Staff users");
    StaffUser::printTableHeader(std::cout);
    for (std::size_t i = 0; i < users.size(); ++i)
    {
        users[i].printTableRow(std::cout);
    }
    std::cout << "\n  Total staff users: " << users.size() << "\n";
}

void SaccoSystem::resetStaffPassword()
{
    Utils::printTitle("Reset a staff password");
    std::string username = Utils::toLower(Utils::trim(InputHelper::readLine("  Username: ")));
    int index = findUserIndex(username);
    if (index < 0)
    {
        Utils::printError("No staff user called '" + username + "'.");
        return;
    }
    std::string password = readNewPassword(username);
    if (password.empty())
    {
        return;
    }
    users[index].changePassword(password);
    saveAll();
    logAudit("Reset password for " + username);
    Utils::printSuccess("Password for '" + username + "' has been reset.");
}

void SaccoSystem::toggleStaffStatus()
{
    Utils::printTitle("Activate / deactivate a staff user");
    std::string username = Utils::toLower(Utils::trim(InputHelper::readLine("  Username: ")));
    int index = findUserIndex(username);
    if (index < 0)
    {
        Utils::printError("No staff user called '" + username + "'.");
        return;
    }
    if (index == currentUserIndex)
    {
        Utils::printError("You cannot deactivate your own account.");
        return;
    }
    StaffUser& user = users[index];
    if (user.isActive() && user.isAdministrator())
    {
        int activeAdmins = 0;
        for (std::size_t i = 0; i < users.size(); ++i)
        {
            if (users[i].isActive() && users[i].isAdministrator()) ++activeAdmins;
        }
        if (activeAdmins <= 1)
        {
            Utils::printError("At least one active Administrator must remain.");
            return;
        }
    }
    std::string action = user.isActive() ? "Deactivate" : "Activate";
    if (!InputHelper::readYesNo("  " + action + " '" + username + "'?"))
    {
        Utils::printInfo("No changes were made.");
        return;
    }
    user.setActive(!user.isActive());
    saveAll();
    logAudit(action + "d staff user " + username);
    Utils::printSuccess("User '" + username + "' is now " + (user.isActive() ? "active." : "deactivated."));
}

// Copies every data file to a time-stamped backup copy.
void SaccoSystem::backupData()
{
    Utils::printTitle("Back up data files");
    saveAll();
    std::string stamp = Utils::removeChar(Utils::today(), '-') + "_" + Utils::removeChar(Utils::currentTime(), ':');
    const std::string files[] = { Config::MEMBERS_FILE, Config::LOANS_FILE, Config::TRANSACTIONS_FILE,
                                  Config::USERS_FILE, Config::SETTINGS_FILE, Config::AUDIT_LOG_FILE };
    const int fileCount = sizeof(files) / sizeof(files[0]);
    int copied = 0;
    for (int i = 0; i < fileCount; ++i)
    {
        if (!FileManager::fileExists(files[i]))
        {
            continue;
        }
        std::string backupName = "backup_" + stamp + "_" + files[i];
        if (FileManager::copyFile(files[i], backupName))
        {
            std::cout << "  Copied " << files[i] << "  ->  " << backupName << "\n";
            ++copied;
        }
        else
        {
            Utils::printError("Could not copy " + files[i]);
        }
    }
    logAudit("Backed up " + Utils::toString(copied) + " data file(s)");
    Utils::printSuccess(Utils::toString(copied) + " file(s) backed up. Store copies on a flash disk as well.");
}

void SaccoSystem::viewAuditLog() const
{
    Utils::printTitle("Audit log - latest 30 entries");
    std::vector<std::string> lines = FileManager::readLastLines(Config::AUDIT_LOG_FILE, 30);
    if (lines.empty())
    {
        Utils::printInfo("The audit log is empty.");
        return;
    }
    std::cout << "  Date       Time     | User         | Action\n";
    Utils::printLine('-');
    for (std::size_t i = 0; i < lines.size(); ++i)
    {
        std::cout << "  " << lines[i] << "\n";
    }
}

// Fills an EMPTY system with realistic sample records so every feature
// (including arrears and completed loans) can be demonstrated at once.
void SaccoSystem::loadDemonstrationData()
{
    Utils::printTitle("Load demonstration data");
    if (!members.empty() || !loans.empty() || !transactions.empty())
    {
        Utils::printError("Demonstration data can only be loaded into an empty system.");
        return;
    }
    Utils::printInfo("This adds 6 sample members, their savings and shares, and 5 loans at different stages.");
    if (!InputHelper::readYesNo("  Load demonstration data now?"))
    {
        Utils::printInfo("Cancelled.");
        return;
    }

    const std::string today = Utils::today();

    struct DemoMember
    {
        const char* name;
        const char* nin;
        const char* phone;
        const char* address;
        int monthsAgo;
        double shares;
        double savings;
    };
    const DemoMember demo[] =
    {
        { "Ssemakula John Bosco", "CM78001234ABCD", "0772123456", "Ndejje Central Ward",   24, 200000, 600000 },
        { "Nakato Florence",      "CF85023456BCDE", "0701234567", "Ndejje North Ward",     18, 150000, 400000 },
        { "Kizza Richard",        "CM90034567CDEF", "0756345678", "Ndejje South Ward",     30, 100000, 250000 },
        { "Namubiru Agnes",       "CF88045678DEFG", "0782456789", "Ndejje East Ward",      12, 120000, 300000 },
        { "Mukasa Peter",         "CM92056789EFGH", "0703567890", "Ndejje West Ward",       8, 100000, 180000 },
        { "Nansubuga Ruth",       "CF95067890FGHI", "0774678901", "Ndejje Trading Centre",  1,  40000,  30000 }
    };
    const int demoCount = sizeof(demo) / sizeof(demo[0]);

    for (int i = 0; i < demoCount; ++i)
    {
        std::string id = settings.generateMemberId();
        std::string joined = Utils::addMonths(today, -demo[i].monthsAgo);
        Member member(id, demo[i].name, demo[i].nin, demo[i].phone, demo[i].address, joined, 0, 0);

        member.addShareCapital(demo[i].shares);
        addTransactionOn(joined, "09:00:00", id, TXN_SHARE_PURCHASE, demo[i].shares, member.getShareCapital(),
                         "Shares at registration", "");
        double firstDeposit = demo[i].savings / 2;
        member.depositSavings(firstDeposit);
        addTransactionOn(joined, "09:15:00", id, TXN_SAVINGS_DEPOSIT, firstDeposit, member.getSavingsBalance(),
                         "Opening savings deposit", "");
        member.depositSavings(demo[i].savings - firstDeposit);
        addTransactionOn(Utils::addMonths(joined, 1), "10:00:00", id, TXN_SAVINGS_DEPOSIT,
                         demo[i].savings - firstDeposit, member.getSavingsBalance(), "Cash deposit", "");
        members.push_back(member);
    }

    // Loans: {member index, amount, months, purpose, months since payout (-1 = not paid out),
    //         installments repaid, final stage}
    struct DemoLoan
    {
        int memberIndex;
        double amount;
        int months;
        const char* purpose;
        int disbursedMonthsAgo;
        int installmentsPaid;
        bool approveOnly;
    };
    const DemoLoan demoLoans[] =
    {
        { 0, 2000000, 12, "Livestock or poultry - two dairy heifers",        5, 5,  false },  // active, up to date
        { 1, 1500000,  6, "Crop farming inputs - maize seed and fertiliser", 4, 1,  false },  // active, in arrears
        { 2,  800000,  6, "Farm tools or equipment - irrigation pump",       9, 6,  false },  // completed
        { 3, 1000000, 10, "Produce trade or storage - coffee drying shed",  -1, 0,  false },  // pending
        { 4,  600000,  6, "School fees - secondary school fees",            -1, 0,  true  }   // approved
    };
    const int loanCount = sizeof(demoLoans) / sizeof(demoLoans[0]);

    for (int i = 0; i < loanCount; ++i)
    {
        const DemoLoan& d = demoLoans[i];
        const Member& member = members[d.memberIndex];
        std::string payoutDate = d.disbursedMonthsAgo >= 0 ? Utils::addMonths(today, -d.disbursedMonthsAgo) : today;
        Loan loan(settings.generateLoanId(), member.getMemberId(), d.amount, settings.getAnnualInterestRate(),
                  d.months, d.purpose, payoutDate, "officer");

        if (d.approveOnly || d.disbursedMonthsAgo >= 0)
        {
            loan.approve(payoutDate, "admin", "Meets all eligibility rules");
        }
        if (d.disbursedMonthsAgo >= 0)
        {
            loan.disburse(payoutDate);
            addTransactionOn(payoutDate, "11:00:00", member.getMemberId(), TXN_LOAN_DISBURSEMENT, d.amount,
                             loan.getTotalPayable(), "Loan paid out", loan.getLoanId());
            std::vector<Installment> schedule = loan.getSchedule();
            for (int k = 0; k < d.installmentsPaid && k < static_cast<int>(schedule.size()); ++k)
            {
                loan.recordRepayment(schedule[k].payment);
                addTransactionOn(schedule[k].dueDate, "14:00:00", member.getMemberId(), TXN_LOAN_REPAYMENT,
                                 schedule[k].payment, loan.getOutstandingBalance(),
                                 "Installment " + Utils::toString(k + 1), loan.getLoanId());
            }
        }
        loans.push_back(loan);
    }

    // Put the ledger in date order and number the transactions in that order.
    std::stable_sort(transactions.begin(), transactions.end(), happensEarlier);
    settings.resetTransactionCounter(1);
    for (std::size_t i = 0; i < transactions.size(); ++i)
    {
        transactions[i].setTransactionId(settings.generateTransactionId());
    }

    saveAll();
    logAudit("Loaded demonstration data");
    Utils::printSuccess("Loaded " + Utils::toString(demoCount) + " members, " + Utils::toString(loanCount) +
                        " loans and " + Utils::toString(static_cast<int>(transactions.size())) + " transactions.");
}

// =============================================================================
//  SHARED LOOK-UP HELPERS
// =============================================================================
int SaccoSystem::findMemberIndex(const std::string& memberId) const
{
    for (std::size_t i = 0; i < members.size(); ++i)
    {
        if (Utils::equalsIgnoreCase(members[i].getMemberId(), memberId))
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int SaccoSystem::findMemberIndexByNIN(const std::string& nin) const
{
    for (std::size_t i = 0; i < members.size(); ++i)
    {
        if (Utils::equalsIgnoreCase(members[i].getNationalId(), nin))
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int SaccoSystem::findLoanIndex(const std::string& loanId) const
{
    for (std::size_t i = 0; i < loans.size(); ++i)
    {
        if (Utils::equalsIgnoreCase(loans[i].getLoanId(), loanId))
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// A member may only have ONE unfinished (pending, approved or active) loan.
int SaccoSystem::findOpenLoanIndex(const std::string& memberId, const std::string& excludeLoanId) const
{
    for (std::size_t i = 0; i < loans.size(); ++i)
    {
        if (loans[i].getMemberId() == memberId && loans[i].isOpen() && loans[i].getLoanId() != excludeLoanId)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int SaccoSystem::findUserIndex(const std::string& username) const
{
    for (std::size_t i = 0; i < users.size(); ++i)
    {
        if (users[i].getUsername() == username)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::string SaccoSystem::memberNameOf(const std::string& memberId) const
{
    int index = findMemberIndex(memberId);
    return index >= 0 ? members[index].getFullName() : "(former member)";
}

// Asks for a Member ID. Typing only the number (e.g. 1) is accepted as NFS0001.
int SaccoSystem::promptMemberIndex()
{
    std::string input = Utils::toUpper(Utils::trim(InputHelper::readLine("  Enter Member ID (e.g. NFS0001): ")));
    if (input.empty())
    {
        Utils::printError("Member ID cannot be empty.");
        return -1;
    }
    int number = 0;
    if (Utils::isAllDigits(input) && Utils::parseInt(input, number))
    {
        input = "NFS" + Utils::zeroPad(number, 4);
    }
    int index = findMemberIndex(input);
    if (index < 0)
    {
        Utils::printError("No member found with ID '" + input + "'. Please check the ID and try again.");
    }
    return index;
}

// Asks for a Loan ID, or a Member ID (then that member's unfinished loan,
// or their most recent loan, is used).
int SaccoSystem::promptLoanIndex()
{
    std::string input = Utils::toUpper(Utils::trim(
        InputHelper::readLine("  Enter Loan ID (e.g. LN0001) or Member ID (e.g. NFS0001): ")));
    if (input.empty())
    {
        Utils::printError("Loan ID cannot be empty.");
        return -1;
    }
    int number = 0;
    if (Utils::isAllDigits(input) && Utils::parseInt(input, number))
    {
        input = "LN" + Utils::zeroPad(number, 4);
    }
    if (input.compare(0, 3, "NFS") == 0)
    {
        if (findMemberIndex(input) < 0)
        {
            Utils::printError("No member found with ID '" + input + "'.");
            return -1;
        }
        int open = findOpenLoanIndex(input, "");
        if (open >= 0)
        {
            return open;
        }
        for (int i = static_cast<int>(loans.size()) - 1; i >= 0; --i)
        {
            if (loans[i].getMemberId() == input)
            {
                return i;
            }
        }
        Utils::printError("Member " + input + " has no loans.");
        return -1;
    }
    int index = findLoanIndex(input);
    if (index < 0)
    {
        Utils::printError("No loan found with ID '" + input + "'. Please check the ID and try again.");
    }
    return index;
}

void SaccoSystem::recordTransaction(const std::string& memberId, TransactionType type, double amount,
                                    double balanceAfter, const std::string& description,
                                    const std::string& loanId)
{
    addTransactionOn(Utils::today(), Utils::currentTime(), memberId, type, amount, balanceAfter, description, loanId);
}

void SaccoSystem::addTransactionOn(const std::string& date, const std::string& time, const std::string& memberId,
                                   TransactionType type, double amount, double balanceAfter,
                                   const std::string& description, const std::string& loanId)
{
    transactions.push_back(Transaction(settings.generateTransactionId(), memberId, type, amount, balanceAfter,
                                       date, time, description, loanId, currentUsername()));
}
