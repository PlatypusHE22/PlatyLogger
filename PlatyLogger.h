#pragma once

#include <cstdio>
#include <mutex>
#include <ctime>
#include <fstream>
#include <filesystem>


#ifdef PLATY_WINDOWS
    #include <Windows.h>
    #define SET_COLOR(console, color) {SetConsoleTextAttribute(console, color);}
#else
    #define SET_COLOR(console, color)
#endif

/* Todo:
    - Comments and documentation (Maybe separate declaration and implementation for easier readability)
    - DONE - Selecting log levels to display and write to a file (Binary flags or extra fields)
    - (maybe) Create a logger message for errors and warnings
 */

class Logger {
public:
    enum {
        LOGLEVEL_NONE = 0,
        LOGLEVEL_TRACE = 1,
        LOGLEVEL_INFO = 1 << 1,
        LOGLEVEL_DEBUG = 1 << 2,
        LOGLEVEL_WARNING = 1 << 3,
        LOGLEVEL_ERROR = 1 << 4,
        LOGLEVEL_FATAL = 1 << 5,
        LOGLEVEL_ALL = 63
    };

    // Levels to display int he console
    [[maybe_unused]]static void SetLevelsToDisplay(unsigned int logLevels)
    {
        logLevelsToDisplay = logLevels;
    }

    // Levels to save into log files
    [[maybe_unused]]static void SetLevelsToSave(unsigned int logLevels)
    {
        logLevelsToSave = logLevels;
    }

    [[maybe_unused]]static void SetNumberOfFilesToSave(unsigned int numberToSave)
    {
        pastLogsToKeep = numberToSave;
    }

private:
    static void* console;
    static const unsigned int traceColor;
    static const unsigned int infoColor;
    static const unsigned int debugColor;
    static const unsigned int warnColor;
    static const unsigned int errorColor;
    static const unsigned int fatalColor;

    static const std::string latestLogFilepath;
    static const std::string pastLogsFilepath;
    static const std::string logsFilepath;
    static bool shouldCreateNewFile;
    static unsigned int pastLogsToKeep;

    static std::mutex mutex;
    static std::fstream fs;

    static unsigned int logLevelsToDisplay;
    static unsigned int logLevelsToSave;

public:

    /// Logging methods
    template<typename... Args>
    [[maybe_unused]] static void Trace(const char* message, Args... format)
    {
        Log(LOGLEVEL_TRACE, "Trace", traceColor, message, format...);
    }

    template<typename... Args>
    [[maybe_unused]] static void Info(const char* message, Args... format)
    {
        Log(LOGLEVEL_INFO, "Info", infoColor, message, format...);
    }

    template<typename... Args>
    [[maybe_unused]] static void Debug(const char* message, Args... format)
    {
        Log(LOGLEVEL_DEBUG, "Debug", debugColor, message, format...);
    }

    template<typename... Args>
    [[maybe_unused]] static void Warning(const char* message, Args... format)
    {
        Log(LOGLEVEL_WARNING, "Warning", warnColor, message, format...);
    }

    template<typename... Args>
    [[maybe_unused]] static void Error(const char* message, Args... format)
    {
        Log(LOGLEVEL_ERROR, "Error", errorColor, message, format...);
    }

    template<typename... Args>
    [[maybe_unused]] static void Fatal(const char* message, Args... format)
    {
        Log(LOGLEVEL_FATAL, "Fatal", fatalColor, message, format...);

    }

private:
    /// Helper functions
    template<typename... Args>
    static void Log(const int logLevel, const char* logLevelStr, unsigned int color, const char* message, Args... format)
    {
        mutex.lock();

        SET_COLOR(console, color);
        char header[32], messageBuffer[1000];
        sprintf(header, "[%i:%i:%i] <%s>", GetTime()->tm_hour, GetTime()->tm_min, GetTime()->tm_sec, logLevelStr);
        sprintf(messageBuffer, message, format...);

        if((logLevelsToDisplay & logLevel) != 0)
            printf("%s - %s\n", header, messageBuffer);

        if((logLevelsToSave & logLevel) != 0)
            LogToFile(header, messageBuffer);

        mutex.unlock();
    }

    static void LogToFile(const char* header, const char* message)
    {
        //Creating directories for the logs
        if(!std::filesystem::exists(logsFilepath) || !std::filesystem::exists(pastLogsFilepath))
        {
            CreateLoggingDirectories();
        }

        // For the first log it saves the latest_log file and creates a new one
        if(shouldCreateNewFile)
        {
            if(std::filesystem::exists(latestLogFilepath))
                SaveLatestLog();

            fs.open(latestLogFilepath, std::ios::out);
            shouldCreateNewFile = false;

            tm* t = GetTime();
            char creationDate[64];
            sprintf(creationDate, "Created - %i. %i. %i. %i:%i:%i\n\n", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
            fs << creationDate;
         }
        else
        {
            fs.open(latestLogFilepath, std::ios::app);
        }

        // Writes the log
        if(fs.is_open())
        {
            fs << header << " - " << message << "\n";
        }
        else
        {
            SetLevelsToSave(LOGLEVEL_NONE);
            fs.close();
            return;
        }

        fs.close();
    }

    static void SaveLatestLog()
    {
        // Creates the logging directories if they are missing
        if(!std::filesystem::exists(latestLogFilepath))
        {
            CreateLoggingDirectories();
        }

        // If the past_logs folder is full it deletes the oldest log
        if(CountFiles(pastLogsFilepath.c_str()) >= pastLogsToKeep)
        {
            std::filesystem::path fileToRemove = GetOldestLog();
            SET_COLOR(console, errorColor);
            printf("Maximum number of past logs reached, removing: %s\n", fileToRemove.string().c_str());
            std::filesystem::remove(fileToRemove);
        }

        // Saves the first line of the log to format its new name
        std::string newFilename;
        fs.open(latestLogFilepath);
        if(fs.is_open())
        {
            std::getline(fs, newFilename);
        }
        else
        {
            // Throw an error
        }

        fs.close();

        // Formats the new name of the file
        newFilename.erase(0, 10);
        newFilename.erase(std::remove_if(newFilename.begin(), newFilename.end(), isspace), newFilename.end());
        std::replace(newFilename.begin(), newFilename.end(), ':', '-');
        newFilename = "log_" + newFilename + ".txt";

        // Renames and copies the file into the past_logs directory
        std::string newFileLocation = pastLogsFilepath + newFilename;
        std::filesystem::copy(latestLogFilepath, newFileLocation, std::filesystem::copy_options::update_existing);
    }

    static void CreateLoggingDirectories()
    {
        std::filesystem::create_directory(logsFilepath);
        std::filesystem::create_directory(pastLogsFilepath);
    }

    static tm* GetTime()
    {
        time_t now = std::time(nullptr);
        return std::localtime(&now);
    }

    static unsigned long long GetFileCreationTime(const char* filePath)
    {
        if(!std::filesystem::exists(filePath))
        {
            return 0;
        }
#ifdef PLATY_WINDOWS
        WIN32_FILE_ATTRIBUTE_DATA fileData;
        FILETIME fileTime;
        ULARGE_INTEGER lInt;

        GetFileAttributesExA(filePath, GetFileExInfoStandard, &fileData);
        fileTime = fileData.ftCreationTime;
        lInt.LowPart = fileTime.dwLowDateTime;
        lInt.HighPart = fileTime.dwHighDateTime;

        return lInt.QuadPart;
#else
        printf("fuck you no unix support");
         return 0;
#endif
    }

    static unsigned long long GetFileCreationTime(const std::filesystem::path& filePath)
    {
        return GetFileCreationTime(filePath.string().c_str());
    }

    static std::filesystem::path GetOldestLog()
    {
        std::filesystem::path oldestFile;
        for(const std::filesystem::path file : std::filesystem::directory_iterator(pastLogsFilepath))
        {
            if(GetFileCreationTime(oldestFile) > GetFileCreationTime(file) || GetFileCreationTime(oldestFile) == 0)
            {
                oldestFile = file;
            }
        }
        return oldestFile;
    }

    static unsigned int CountFiles(const char* directory)
    {
        unsigned int fileCount = 0;
        std::filesystem::directory_iterator dir(directory);
        for(const auto& e : dir)
        {
            if(e.is_regular_file())
                fileCount++;
        }

        return fileCount;
    }
};


#ifdef PLATY_WINDOWS
    HANDLE Logger::console = GetStdHandle(STD_OUTPUT_HANDLE);
    unsigned const int Logger::traceColor = FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_BLUE;
    unsigned const int Logger::infoColor = FOREGROUND_BLUE;
    unsigned const int Logger::debugColor = FOREGROUND_GREEN;
    unsigned const int Logger::warnColor = FOREGROUND_RED | FOREGROUND_GREEN;
    unsigned const int Logger::errorColor = FOREGROUND_RED;
    unsigned const int Logger::fatalColor = FOREGROUND_RED;
#else
    void* Logger::console = nullptr;
    unsigned int Logger::traceColor = 0;
    unsigned int Logger::infoColor = 0;
    unsigned int Logger::debugColor = 0;
    unsigned int Logger::warnColor = 0;
    unsigned int Logger::errorColor = 0;
    unsigned int Logger::fatalColor = 0;
#endif


const std::string Logger::latestLogFilepath = "./logs/latest_log.txt";
const std::string Logger::logsFilepath = "./logs";
const std::string Logger::pastLogsFilepath = "./logs/past_logs/";
bool Logger::shouldCreateNewFile = true;
unsigned int Logger::pastLogsToKeep = 5;

std::mutex Logger::mutex = std::mutex();
std::fstream Logger::fs = std::fstream();

unsigned int Logger::logLevelsToDisplay = LOGLEVEL_ALL;
unsigned int Logger::logLevelsToSave = LOGLEVEL_ALL;
