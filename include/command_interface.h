#pragma once

#include "alarm_system.h"
#include "audio_control.h"
#include "network_manager.h"
#include "timekeeper.h"

#include <Arduino.h>
#include <stddef.h>

class UI;

// command_interface.h
// Abstraction class between received system commands and logic

class CommandInterface
{
  public:
    CommandInterface();

    // Source control
    const char *handleBlynkIn(const char *line);
    void handleSerialIn();

    // For manual command input
    void processCommandLine(char *line);

  private:
    void dispatchCommand(int argc, char *argv[]);

    // ========== COMMAND HANDLERS ==========
    // General
    void cmdHelp(int argc, char *argv[]);
    void cmdStatus(int argc, char *argv[]);
    void cmdTime(int argc, char *argv[]);

    // Debug
    void cmdLog(int argc, char *argv[]);
    void cmdStackUsage(int argc, char *argv[]);

    // Alarm
    void cmdAlarm(int argc, char *argv[]);
    void cmdAlarmVolume(int argc, char *argv[]);
    void cmdAlarmTrack(int argc, char *argv[]);

    // Player
    void cmdVol(int argc, char *argv[]);
    void cmdPlay(int argc, char *argv[]);
    void cmdStop(int argc, char *argv[]);

    // Network
    void cmdWiFiSession(int argc, char *argv[]);
    void cmdSync(int argc, char *argv[]);

  private:
    static constexpr size_t MAX_ARGS = 8;
    static constexpr size_t CMD_IN_SIZE = 128; // max size of input buffer
    static constexpr size_t CMD_OUT_SIZE = 1024; // max size of output buffer

    static constexpr size_t NUM_COMMANDS = 12; // Update when new command is added!

    using CommandHandler = void (CommandInterface::*)(int argc, char *argv[]);
    struct Command
    {
        const char *name;
        CommandHandler handler;
        const char *description;
    };

    // Command table
    const Command commands_[NUM_COMMANDS] = {
        {"help", &CommandInterface::cmdHelp, "prints this index of commands and usage"},
        {"status", &CommandInterface::cmdStatus, "status"},
        {"time", &CommandInterface::cmdTime, "time <set> <hour> <minute> <month> <day> <year>"},
        {"log", &CommandInterface::cmdLog, "log <log <message>> || <pop> || <size> || <printall> || <dumpbuffer> || <save> || <load>"},
        {"alarm", &CommandInterface::cmdAlarm, "alarm <set> <hour> <minute> || <disable>"},
        {"alarmvolume", &CommandInterface::cmdAlarmVolume, "alarmvolume <vol>"},
        {"alarmtrack", &CommandInterface::cmdAlarmTrack, "alarmtrack <fileLocation>"},
        {"vol", &CommandInterface::cmdVol, "vol <0-30>"},
        {"play", &CommandInterface::cmdPlay, "play <folder> <track> [vol = DEFAULT]"},
        {"stop", &CommandInterface::cmdStop, "stop"},
        {"sync", &CommandInterface::cmdSync, "sync <time> || <weather>"},
        {"wifisession", &CommandInterface::cmdWiFiSession, "wifisession <on> || <off>"}};

    // Command output buffer
    char cmdOut_[CMD_OUT_SIZE];

    // Small in-memory log buffer for the current build
    char logBuffer_[CMD_OUT_SIZE];
    size_t logCount_;

    // Helpers
    bool parseLong(char *arg, long &out, const char *name);
};

extern CommandInterface commandInterface; // Universal CommandInterface