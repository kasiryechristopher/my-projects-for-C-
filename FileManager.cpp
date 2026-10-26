/*
 * =============================================================================
 *  File    : FileManager.cpp
 *  Purpose : Implementation of file reading and writing.
 * =============================================================================
 */
#include "FileManager.h"
#include "Utils.h"

#include <fstream>
#include <cstdio>
#include <deque>

bool FileManager::fileExists(const std::string& fileName)
{
    std::ifstream file(fileName.c_str());
    return file.good();
}

// Reads all useful lines (skips blank lines and '#' comments).
// If the main file is missing but a .tmp copy survived an interrupted save,
// the .tmp copy is used so no data is lost.
bool FileManager::readDataLines(const std::string& fileName, std::vector<std::string>& lines)
{
    lines.clear();
    std::string source = fileName;
    if (!fileExists(source) && fileExists(fileName + ".tmp"))
    {
        source = fileName + ".tmp";
    }

    std::ifstream file(source.c_str());
    if (!file)
    {
        return false;
    }
    std::string line;
    while (std::getline(file, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
        {
            line.erase(line.size() - 1);        // file edited on Windows
        }
        if (Utils::trim(line).empty() || line[0] == '#')
        {
            continue;
        }
        lines.push_back(line);
    }
    return true;
}

// Writes to "<name>.tmp" first, then replaces the real file.
bool FileManager::writeLinesSafely(const std::string& fileName, const std::vector<std::string>& lines)
{
    const std::string tempName = fileName + ".tmp";
    {
        std::ofstream out(tempName.c_str(), std::ios::out | std::ios::trunc);
        if (!out)
        {
            return false;
        }
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            out << lines[i] << "\n";
        }
        out.flush();
        if (!out)
        {
            return false;                       // e.g. disk full
        }
    }   // the file is closed here

    std::remove(fileName.c_str());              // Windows cannot rename over an existing file
    return std::rename(tempName.c_str(), fileName.c_str()) == 0;
}

bool FileManager::appendLine(const std::string& fileName, const std::string& line)
{
    std::ofstream out(fileName.c_str(), std::ios::out | std::ios::app);
    if (!out)
    {
        return false;
    }
    out << line << "\n";
    return out.good();
}

// Returns only the last 'count' lines (used to show the latest audit entries).
std::vector<std::string> FileManager::readLastLines(const std::string& fileName, std::size_t count)
{
    std::deque<std::string> window;
    std::ifstream file(fileName.c_str());
    std::string line;
    while (std::getline(file, line))
    {
        if (Utils::trim(line).empty())
        {
            continue;
        }
        window.push_back(line);
        if (window.size() > count)
        {
            window.pop_front();
        }
    }
    return std::vector<std::string>(window.begin(), window.end());
}

bool FileManager::copyFile(const std::string& source, const std::string& destination)
{
    std::ifstream in(source.c_str(), std::ios::binary);
    if (!in)
    {
        return false;
    }
    std::ofstream out(destination.c_str(), std::ios::binary | std::ios::trunc);
    if (!out)
    {
        return false;
    }
    out << in.rdbuf();
    return out.good();
}

bool FileManager::saveSettings(const std::string& fileName, const Settings& settings)
{
    return writeLinesSafely(fileName, settings.toLines());
}

bool FileManager::loadSettings(const std::string& fileName, Settings& settings)
{
    std::vector<std::string> lines;
    if (!readDataLines(fileName, lines))
    {
        return false;                           // first run: defaults stay in place
    }
    for (std::size_t i = 0; i < lines.size(); ++i)
    {
        std::size_t equals = lines[i].find('=');
        if (equals != std::string::npos)
        {
            settings.applyLine(Utils::trim(lines[i].substr(0, equals)), Utils::trim(lines[i].substr(equals + 1)));
        }
    }
    return true;
}
