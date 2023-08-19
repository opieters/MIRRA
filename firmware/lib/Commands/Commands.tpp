#ifndef __COMMANDS_T__
#define __COMMANDS_T__
#include "Commands.h"

template <class C> typename std::enable_if_t<std::is_base_of_v<BaseCommands, C>, void> CommandEntry::prompt()
{
    if (commandPhaseFlag)
        CommandParser().start<C>();
    else
        Serial.println("Command phase flag not set. Command phase will not start.");
    commandPhaseFlag = false;
}

template <class C> typename std::enable_if_t<std::is_base_of_v<BaseCommands, C>, void> CommandParser::start()
{
    Serial.println("COMMAND PHASE");
    while (true)
    {
        auto buffer{readLine()};
        if (!buffer)
        {
            Serial.println("Command phase timeout. Exiting...");
            return;
        }
        switch (processCommands<C::commands>(buffer->data()))
        {
        case COMMAND_NOT_FOUND:
            Serial.printf("Command '%s' not found or invalid argument(s) given.\n", buffer->data());
            break;
        case COMMAND_EXIT:
            Serial.println("Exiting command phase...");
            return;
        default:
            break;
        }
    }
}

template <size_t nAliases, class... CommandArgs>
struct AliasesCommandPair : std::pair<std::array<const std::string_view, nAliases>, CommandCode (*)(CommandArgs...)>
{
    constexpr AliasesCommandPair(const std::array<const std::string_view, nAliases>&& aliases, CommandCode (*&&f)(CommandArgs...))
        : std::pair<std::array<const std::string_view, nAliases>, CommandCode (*)(CommandArgs...)>(
              std::forward<std::array<const std::string_view, nAliases>>(aliases), std::forward<CommandCode (*)(CommandArgs...)>(f))
    {
    }

    constexpr AliasesCommandPair(const std::string_view (&&aliases)[nAliases], CommandCode (*&&f)(CommandArgs...))
        : std::pair<std::array<const std::string_view, nAliases>, CommandCode (*)(CommandArgs...)>(
              std::array<const std::string_view, nAliases>(std::forward<const std::string_view[nAliases]>(aliases)),
              std::forward<CommandCode (*)(CommandArgs...)>(f))
    {
    }
};

CommandCode testCommand(const char* arg) { return COMMAND_SUCCESS; }

constexpr AliasesCommandPair lol{{std::string_view("test")}, &testCommand};

template <class T, class... Ts> struct parseCommands
{
    template <size_t nAliases, const char*... CommandArgs, AliasesCommandPair<nAliases, CommandArgs...>& pair, Ts... pairs>
    CommandCode operator()(char* command)
    {
        constexpr auto& aliases{std::get<0>(pair)};
        constexpr auto& function{std::get<1>(pair)};
        for (const char*& alias : aliases)
            if (strcmp(command, alias) == 0)
            {
                constexpr std::array<char*, sizeof...(CommandArgs)> commandArgs;
                for (char*& commandArg : commandArgs)
                    commandArg = strtok(nullptr, " \r\n");
                return std::apply(function, commandArgs);
            }
        return parseCommands().operator()<pairs...>(command);
    }

    CommandCode operator()(char* command) { return COMMAND_NOT_FOUND; }
};

template <class C, C CommandsTuple> CommandCode CommandParser::processCommands(char* line)
{
    char* command = strtok(command, " \r\n");
    if (command == nullptr) // when no command entered, simply loop again
        return COMMAND_NOT_FOUND;
    executeCommand<CommandsTuple>(command);

    if (strcmp(command, "exit") == 0 || strcmp(command, "close") == 0)
    {
        return COMMAND_EXIT;
    }
    else if (strcmp(command, "ls") == 0 || strcmp(command, "list") == 0)
    {
        listFiles();
    }
    else if (strncmp(command, "print ", 6) == 0)
    {
        printFile(&command[6]);
    }
    else if (strncmp(command, "printhex ", 9) == 0)
    {
        printFile(&command[9], true);
    }
    else if (strncmp(command, "rm ", 3) == 0)
    {
        removeFile(&command[3]);
    }
    else if (strncmp(command, "touch ", 6) == 0)
    {
        touchFile(&command[6]);
    }
    else if (strcmp(command, "format") == 0)
    {
        Serial.println("Formatting flash memory (this can take some time)...");
        LittleFS.format();
        Serial.println("Restarting ...");
        ESP.restart();
    }
    else if (strncmp(command, "echo ", 5) == 0)
    {
        Serial.println(&command[5]);
    }
    else
    {
        return COMMAND_NOT_FOUND;
    }
    return COMMAND_FOUND;
}
#endif