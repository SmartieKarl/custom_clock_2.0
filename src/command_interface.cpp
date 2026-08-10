#include "command_interface.h"

#include "config.h"
#include "time_sync.h"
#include "utils.h"
#include "weather_sync.h"

#include <Arduino.h>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <freertos/task.h>

CommandInterface commandInterface; // Global shared instance

#define CMD_APPEND(fmt, ...) \
    std::snprintf(cmdOut_ + std::strlen(cmdOut_), CMD_OUT_SIZE - std::strlen(cmdOut_), fmt, ##__VA_ARGS__)

// Constructor
CommandInterface::CommandInterface()
    : logCount_(0)
{
    cmdOut_[0] = '\0';
    logBuffer_[0] = '\0';
}

// ========== CommandInterface member definitions ==========

// Takes tokenized command line and dispatches command data to respective command function.
void CommandInterface::dispatchCommand(int argc, char *argv[])
{
    if (argc == 0)
        return;

    cmdOut_[0] = '\0';
    CMD_APPEND("[CLK]: ");

    for (const Command &cmd : commands_)
        if (std::strcmp(argv[0], cmd.name) == 0)
            return (this->*cmd.handler)(argc, argv);

    CMD_APPEND("command <%s> not recognized.", argv[0]);
}

// Processes input line and tokenizes it for the dispatcher.
void CommandInterface::processCommandLine(char *line)
{
    for (char *p = line; *p; ++p)
        *p = std::tolower(static_cast<unsigned char>(*p));

    char *argv[MAX_ARGS];
    int argc = 0;

    char *token = std::strtok(line, " \t\r\n");
    while (token && argc < MAX_ARGS)
    {
        argv[argc++] = token;
        token = std::strtok(nullptr, " \t\r\n");
    }

    if (argc == 0)
        return;

    dispatchCommand(argc, argv);
}

// Called when network detects an incoming message from AdafruitIO.
const char *CommandInterface::handleBlynkIn(const char *line)
{
    if (!line)
        return "";

    char buf[CMD_IN_SIZE];
    std::strncpy(buf, line, sizeof(buf));
    buf[sizeof(buf) - 1] = '\0';

    if (std::strncmp(buf, "[CLK]:", 6) == 0)
        return "";

    processCommandLine(buf);
    return cmdOut_;
}

// Called every loop. Reads serial command line, converts into char* and passes to processor.
void CommandInterface::handleSerialIn()
{
    static char inputBuffer[CMD_IN_SIZE];
    static uint8_t inputPos = 0;

    while (Serial.available())
    {
        char c = Serial.read();

        if (c == '\n' || c == '\r')
        {
            if (inputPos > 0)
            {
                inputBuffer[inputPos] = '\0';
                processCommandLine(inputBuffer);
                Serial.println(cmdOut_);
                inputPos = 0;
            }
        }
        else if (inputPos < sizeof(inputBuffer) - 1)
        {
            inputBuffer[inputPos++] = c;
        }
        else
        {
            inputPos = 0;
            Serial.println("ERR: input too long");
        }
    }
}

// Prints all available commands and usage.
void CommandInterface::cmdHelp(int argc, char *argv[])
{
    CMD_APPEND("--------------------\nAVAILABLE COMMANDS:\n");
    for (const Command &cmd : commands_)
    {
        CMD_APPEND("%s: %s\n", cmd.name, cmd.description);
    }
    CMD_APPEND("--------------------\n");
}

// Prints current time and date.
void CommandInterface::cmdStatus(int argc, char *argv[])
{
    DateTime now = timekeeper.time();
    CMD_APPEND("time: %02d:%02d:%02d\n", now.hour(), now.minute(), now.second());
    CMD_APPEND("date: %02d/%02d/%04d\n", now.month(), now.day(), now.year());
}

void CommandInterface::cmdTime(int argc, char *argv[])
{
    DateTime time = timekeeper.time();

    if (argc == 1)
    {
        CMD_APPEND("It is currently %02d/%02d/%04d, %02d:%02d:%02d.",
                   time.month(),
                   time.day(),
                   time.year(),
                   time.hour(),
                   time.minute(),
                   time.second());
        return;
    }

    if (std::strcmp(argv[1], "set") == 0)
    {
        if (argc != 8)
        {
            CMD_APPEND("Usage: time set <year> <month> <day> <hour> <minute> <second>");
            return;
        }

        long year, month, day, hr, min, sec;

        if (!parseLong(argv[2], year, "year") ||
            !parseLong(argv[3], month, "month") ||
            !parseLong(argv[4], day, "day") ||
            !parseLong(argv[5], hr, "hour") ||
            !parseLong(argv[6], min, "minute") ||
            !parseLong(argv[7], sec, "second"))
        {
            CMD_APPEND("Usage: time set <year> <month> <day> <hour> <minute> <second>");
            return;
        }

        if (year < 2000 || year > 2099)
        {
            CMD_APPEND("Err: year must be 2000-2099");
            return;
        }

        if (month < 1 || month > 12)
        {
            CMD_APPEND("Err: month must be 1-12");
            return;
        }

        if (day < 1 || day > 31)
        {
            CMD_APPEND("Err: day must be 1-31");
            return;
        }

        if (hr < 0 || hr > 23)
        {
            CMD_APPEND("Err: hour must be 0-23");
            return;
        }

        if (min < 0 || min > 59)
        {
            CMD_APPEND("Err: minute must be 0-59");
            return;
        }

        if (sec < 0 || sec > 59)
        {
            CMD_APPEND("Err: second must be 0-59");
            return;
        }

        timekeeper.setTime(DateTime(
            static_cast<int>(year),
            static_cast<int>(month),
            static_cast<int>(day),
            static_cast<int>(hr),
            static_cast<int>(min),
            static_cast<int>(sec)));

        CMD_APPEND("Time set to %02ld/%02ld/%04ld %02ld:%02ld:%02ld",
                   month, day, year,
                   hr, min, sec);

        return;
    }

    CMD_APPEND("Usage: time set <year> <month> <day> <hour> <minute> <second>");
}

// Executes log system functions.
void CommandInterface::cmdLog(int argc, char *argv[])
{
    if (argc < 2)
    {
        CMD_APPEND("Usage: log <log <message>> || <pop> || <size> || <printall> || <dumpbuffer> || <save> || <load> || <clear>");
        return;
    }

    if (std::strcmp(argv[1], "log") == 0)
    {
        if (argc < 3)
        {
            CMD_APPEND("Usage: log log <message>");
            return;
        }

        char msg[CMD_IN_SIZE] = {0};

        for (int i = 2; i < argc; ++i)
        {
            std::strncat(msg, argv[i], sizeof(msg) - std::strlen(msg) - 1);

            if (i < argc - 1)
                std::strncat(msg, " ", sizeof(msg) - std::strlen(msg) - 1);
        }

        if (std::strlen(logBuffer_) > 0)
            std::strncat(logBuffer_, "\n", sizeof(logBuffer_) - std::strlen(logBuffer_) - 1);
        std::strncat(logBuffer_, msg, sizeof(logBuffer_) - std::strlen(logBuffer_) - 1);
        ++logCount_;

        CMD_APPEND("Logged: %s", msg);
    }
    else if (std::strcmp(argv[1], "pop") == 0)
    {
        char msg[CMD_IN_SIZE];
        if (logCount_ == 0)
        {
            CMD_APPEND("Err: log is empty.");
            return;
        }

        char *newline = std::strchr(logBuffer_, '\n');
        if (newline)
        {
            const size_t len = static_cast<size_t>(newline - logBuffer_);
            std::strncpy(msg, logBuffer_, len);
            msg[len] = '\0';
            std::memmove(logBuffer_, newline + 1, std::strlen(newline + 1) + 1);
        }
        else
        {
            std::strncpy(msg, logBuffer_, sizeof(msg) - 1);
            msg[sizeof(msg) - 1] = '\0';
            logBuffer_[0] = '\0';
        }

        --logCount_;
        CMD_APPEND("%s", msg);
    }
    else if (std::strcmp(argv[1], "size") == 0)
    {
        CMD_APPEND("%u", static_cast<unsigned>(logCount_));
    }
    else if (std::strcmp(argv[1], "printall") == 0)
    {
        if (logCount_ == 0)
            CMD_APPEND("Err: log is empty.");
        else
        {
            Serial.println("[CMD LOG]");
            Serial.println(logBuffer_);
            CMD_APPEND("Log has been printed to Serial output.");
        }
    }
    else if (std::strcmp(argv[1], "dumpbuffer") == 0)
    {
        Serial.println(logBuffer_);
        CMD_APPEND("Raw log buffer has been dumped to Serial output.");
    }
    else if (std::strcmp(argv[1], "save") == 0)
    {
        // The current firmware does not expose a flash-backed logger.
        CMD_APPEND("Log save is not implemented in the current build.");
    }
    else if (std::strcmp(argv[1], "load") == 0)
    {
        // The current firmware does not expose a flash-backed logger.
        CMD_APPEND("Log restore is not implemented in the current build.");
    }
    else if (std::strcmp(argv[1], "clear") == 0)
    {
        logBuffer_[0] = '\0';
        logCount_ = 0;
        CMD_APPEND("Log data cleared.");
    }
    else
    {
        CMD_APPEND("arg '%s' not recognized.", argv[1]);
    }
}

// Either sets alarm at given time, or disables alarm.
void CommandInterface::cmdAlarm(int argc, char *argv[])
{
    if (argc < 2)
    {
        CMD_APPEND("Usage: alarm <set hr min> || <disable>");
        return;
    }

    if (std::strcmp(argv[1], "set") == 0)
    {
        if (argc != 4)
        {
            CMD_APPEND("Usage: alarm set <hr> <min>   (ex: alarm set 21 30)");
            return;
        }

        char *endptr_hr;
        long hr = std::strtol(argv[2], &endptr_hr, 10);

        char *endptr_min;
        long min = std::strtol(argv[3], &endptr_min, 10);

        if (endptr_hr == argv[2] || *endptr_hr != '\0' ||
            endptr_min == argv[3] || *endptr_min != '\0')
        {
            CMD_APPEND("Err: hour and minute must be valid numbers");
            return;
        }

        if (hr < 0 || hr > 23)
        {
            CMD_APPEND("Err: hour must be between 0 and 23");
            return;
        }

        if (min < 0 || min > 59)
        {
            CMD_APPEND("Err: minute must be between 0 and 59");
            return;
        }

        alarmSystem.setAlarm(static_cast<uint8_t>(hr), static_cast<uint8_t>(min), true);
        CMD_APPEND("Alarm set to %02d:%02d", static_cast<int>(hr), static_cast<int>(min));
    }
    else if (std::strcmp(argv[1], "disable") == 0)
    {
        alarmSystem.setAlarm(0, 0, false);
        CMD_APPEND("Alarm disabled");
    }
    else
    {
        CMD_APPEND("Usage: alarm <set hr min> || <disable>");
    }
}

// Configures the sound the clock plays when the alarm goes off.
void CommandInterface::cmdAlarmVolume(int argc, char *argv[])
{
    if (argc != 2)
    {
        CMD_APPEND("Usage: alarmvolume <vol>");
        return;
    }

    int volume = alarmSystem.getAlarmVolume();
    if (argc == 2)
    {
        char *endptr;
        long v = std::strtol(argv[2], &endptr, 10);
        if (endptr == argv[2] || *endptr != '\0' || v < 0 || v > 21)
        {
            CMD_APPEND("Err: volume must be between 0 and 21");
            return;
        }
        volume = static_cast<int>(v);
    }

    // The current alarm module does not expose track-range selection.
    alarmSystem.setAlarmVolume(static_cast<uint8_t>(volume));
    CMD_APPEND("Alarm volume set to %d", volume);
}

// Sets the file location for the alarm song to play.
void CommandInterface::cmdAlarmTrack(int argc, char *argv[])
{
    if (argc != 2)
    {
        CMD_APPEND("Usage: alarmtrack <filePath>");
        return;
    }

    if (alarmSystem.setAlarmTrackFilePath(argv[1]))
    {
        CMD_APPEND("Alarm track set to %s", alarmSystem.getAlarmTrackFilePath());
    }
    else
    {
        CMD_APPEND("Err: file '%s' not found.", argv[1]);
    }
}

// Sets audio volume to given int.
void CommandInterface::cmdVol(int argc, char *argv[])
{
    if (argc < 2)
    {
        CMD_APPEND("Usage: vol <0-21>");
        return;
    }

    char *endptr_vol;
    long v = std::strtol(argv[1], &endptr_vol, 10);

    if (endptr_vol == argv[1] || *endptr_vol != '\0' || v < 0 || v > 21)
    {
        CMD_APPEND("Err: volume must be between 0 and 21");
        return;
    }

    audioControl.setVolume(static_cast<uint8_t>(v));
    CMD_APPEND("Volume set to %d", static_cast<int>(v));
}

// Plays track; this firmware uses a different audio layer.
void CommandInterface::cmdPlay(int argc, char *argv[])
{
    if (argc < 2 || argc > 3)
    {
        CMD_APPEND("Usage: play <filePath> <vol = DEFAULT>");
        return;
    }

    if (!isValidSdPath(argv[1]))
    {
        CMD_APPEND("Err: file '%s' not found.", argv[1]);
        return;
    }

    uint8_t volume = AUDIO_DEFAULT_VOLUME;
    if (argc == 3)
    {
        char *endptr;
        long v = std::strtol(argv[2], &endptr, 10);
        if (endptr == argv[2] || *endptr != '\0' || v < 0 || v > 21)
        {
            CMD_APPEND("Err: volume must be between 0 and 21");
            return;
        }
        volume = static_cast<uint8_t>(v);
    }

    audioControl.setVolume(volume);
    audioControl.connectToSD(argv[1]);
    CMD_APPEND("Now playing %s at volume %d", argv[1], volume);
}

// Stops audio playback.
void CommandInterface::cmdStop(int argc, char *argv[])
{
    audioControl.stop();
    CMD_APPEND("Ok: Audio playback stopped.");
}

// Starts/stops persistent wifi session.
void CommandInterface::cmdWiFiSession(int argc, char *argv[])
{
    if (argc != 2)
    {
        CMD_APPEND("Usage: wifisession <on> || <off>");
        return;
    }

    if (std::strcmp(argv[1], "on") == 0)
    {
        if (!networkManager.isWiFiPersistent())
        {
            networkManager.setWiFiPersistent(true);
            CMD_APPEND("Wifi session started.");
        }
        else
            CMD_APPEND("Wifi session is already active.");
    }
    else if (std::strcmp(argv[1], "off") == 0)
    {
        if (networkManager.isWiFiPersistent())
        {
            networkManager.setWiFiPersistent(false);
            CMD_APPEND("Wifi session stopped. Goodbye!");
        }
        else
            CMD_APPEND("Wifi session is already disabled.");
    }
    else
        CMD_APPEND("Err: arg was invalid (on || off)");
}

// Syncs time or weather data.
void CommandInterface::cmdSync(int argc, char *argv[])
{
    if (argc < 2)
    {
        CMD_APPEND("Usage: sync <time || weather>");
        return;
    }

    if (std::strcmp(argv[1], "time") == 0)
    {
        if (timeSync.sync())
            CMD_APPEND("Time sync successful.");
        else
            CMD_APPEND("Err: unable to sync time to RTC.");
    }
    else if (std::strcmp(argv[1], "weather") == 0)
    {
        if (weatherSync.sync())
        {
            CMD_APPEND("Weather fetch successful.");
        }
        else
            CMD_APPEND("Err: unable to fetch weather data.");
    }
    else
        CMD_APPEND("Err: arg was invalid (time || weather)");
}

bool CommandInterface::parseLong(char *arg, long &out, const char *name)
{
    char *endptr;
    out = std::strtol(arg, &endptr, 10);

    if (endptr == arg || *endptr != '\0')
    {
        CMD_APPEND("Err: %s must be a valid number", name);
        return false;
    }

    return true;
}