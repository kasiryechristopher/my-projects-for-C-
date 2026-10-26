/*
 * =============================================================================
 *  File    : FileManager.h
 *  Purpose : All reading and writing of data files happens here, so the rest
 *            of the program never deals with fstream details.
 *
 *  Safety features:
 *    - "Safe save": data is first written to a temporary file (*.tmp) and only
 *      then swapped in, so a power cut while saving cannot destroy the
 *      previous good copy.
 *    - Damaged lines are skipped and counted instead of crashing the program.
 *    - Lines starting with '#' are comments (column headings for humans).
 * =============================================================================
 */
#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <string>
#include <vector>
#include "Settings.h"

class FileManager
{
public:
    static bool fileExists(const std::string& fileName);
    static bool readDataLines(const std::string& fileName, std::vector<std::string>& lines);
    static bool writeLinesSafely(const std::string& fileName, const std::vector<std::string>& lines);
    static bool appendLine(const std::string& fileName, const std::string& line);
    static std::vector<std::string> readLastLines(const std::string& fileName, std::size_t count);
    static bool copyFile(const std::string& source, const std::string& destination);

    static bool saveSettings(const std::string& fileName, const Settings& settings);
    static bool loadSettings(const std::string& fileName, Settings& settings);

    // Saves any list of records whose class provides toRecord().
    // (A template lets ONE function save members, loans, transactions and users.)
    template <typename T>
    static bool saveRecords(const std::string& fileName, const std::string& headerComment,
                            const std::vector<T>& records)
    {
        std::vector<std::string> lines;
        lines.push_back(headerComment);
        for (std::size_t i = 0; i < records.size(); ++i)
        {
            lines.push_back(records[i].toRecord());
        }
        return writeLinesSafely(fileName, lines);
    }

    // Loads any list of records whose class provides static fromRecord().
    // Returns the number loaded; damaged lines are counted in badLines.
    template <typename T>
    static int loadRecords(const std::string& fileName, std::vector<T>& records, int& badLines)
    {
        records.clear();
        badLines = 0;
        std::vector<std::string> lines;
        if (!readDataLines(fileName, lines))
        {
            return 0;                         // no file yet = empty list (first run)
        }
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            T item;
            if (T::fromRecord(lines[i], item))
            {
                records.push_back(item);
            }
            else
            {
                ++badLines;
            }
        }
        return static_cast<int>(records.size());
    }
};

#endif // FILE_MANAGER_H
