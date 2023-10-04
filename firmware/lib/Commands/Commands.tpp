#ifndef __COMMANDS_T__
#define __COMMANDS_T__
#include "Commands.h"

template <class C> typename std::enable_if_t<std::is_base_of_v<CommonCommands, C>, void> CommandEntry::prompt(C&& commands)
{
    if (commandPhaseFlag)
        CommandParser().start<C>(std::forward<C>(commands));
    else
        Serial.println("Command phase flag not set. Command phase will not start.");
    commandPhaseFlag = false;
}

template <class C> typename std::enable_if_t<std::is_base_of_v<CommonCommands, C>, void> CommandParser::start(C&& commands)
{
    Serial.println("COMMAND PHASE");
    while (true)
    {
        auto buffer{CommandParser::readLine()};
        if (!buffer)
        {
            Serial.println("Command phase timeout. Exiting...");
            return;
        }
        switch (parseLine(buffer->data(), std::forward<C>(commands)))
        {
        case COMMAND_EXIT:
            Serial.println("Exiting command phase...");
            return;
        default:
            break;
        }
    }
}

template <class C, size_t I = 0> CommandCode CommandParser::parseLine(char* line, C&& commands)
{
    char* command{line};
    if constexpr (I == 0)
    {
        command = strtok(line, " \r\n");
        if (command == nullptr) // when no command entered, simply loop again
            return COMMAND_NOT_FOUND;
    }
    constexpr auto commandsTuple{C::getCommands()};
    if constexpr (I >= std::tuple_size_v<decltype(commandsTuple)>)
    {
        Serial.printf("Command '%s' not found.", command);
        return COMMAND_NOT_FOUND;
    }
    else
    {
        constexpr auto pair{std::get<I>(commandsTuple)};
        constexpr auto aliases{pair.getAliases()};
        for (const char* alias : aliases)
            if (strcmp(command, alias) == 0)
            {
                constexpr auto function{pair.getCommand()};
                std::array<char*, pair.getCommandArgCount()> commandArgs;
                for (char*& commandArg : commandArgs)
                {
                    commandArg = strtok(nullptr, " \r\n");
                    if (commandArg == nullptr)
                    {
                        Serial.printf("Command '%s' expects %u arguments but received fewer.", alias, pair.getCommandArgCount());
                        return COMMAND_NOT_FOUND;
                    }
                }
                return std::apply(function, std::tuple_cat(std::forward_as_tuple(std::forward<C>(commands)), commandArgs));
            }

        return parseLine<C, I + 1>(command, std::forward<C>(commands));
    }
}
#endif