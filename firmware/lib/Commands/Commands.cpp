#include "Commands.h"

std::optional<std::array<char, CommandParser::lineMaxLength>> CommandParser::readLine()
{
    uint64_t timeout{static_cast<uint64_t>(esp_timer_get_time()) + (UART_PHASE_TIMEOUT * 1000 * 1000)};
    uint8_t length{0};
    std::array<char, lineMaxLength> buffer{0};
    while (length < lineMaxLength - 1)
    {
        while (!Serial.available())
        {
            if (esp_timer_get_time() >= timeout)
                return std::nullopt;
        }
        char c = Serial.read();
        Serial.print(c);
        if (c == '\r' || c == '\n')
        {
            if (Serial.available()) // on Windows: both CR and LF
                Serial.print(static_cast<char>(Serial.read()));
            break;
        }
        else if (c == '\b')
        {
            if (length == 0)
                continue;
            length--;
            buffer[length] = '\0';
        }
        else
        {
            buffer[length] = c;
            length++;
            if (length == lineMaxLength - 1)
                break;
        }
    };
    return std::make_optional(buffer);
}

CommandCode CommonCommands::listFiles()
{
    File root{LittleFS.open("/")};
    if (!root)
    {
        Serial.println("Could not open filesystem root.");
        return COMMAND_ERROR;
    }
    File file{root.openNextFile()};
    while (file)
    {
        Serial.printf("%s\t%uB\n", file.path(), file.size());
        file.close();
        file = root.openNextFile();
    }
    file.close();
    root.close();
    return COMMAND_SUCCESS;
}

CommandCode printFileImpl(const char* filename, bool hex)
{
    if (!LittleFS.exists(filename))
    {
        Serial.printf("File '%s' does not exist.\n", filename);
        return COMMAND_ERROR;
    }
    File file{LittleFS.open(filename)};
    if (!file)
    {
        Serial.printf("Error while opening file '%s'\n", filename);
        return COMMAND_ERROR;
    }
    Serial.printf("%s with size %u bytes\n", filename, file.size());
    if (hex)
    {
        while (file.available())
        {
            Serial.printf("%X", file.read());
        }
    }
    else
    {
        while (file.available())
        {
            Serial.write(file.read());
        }
    }
    Serial.print('\n');
    file.close();
    Serial.flush();
    return COMMAND_SUCCESS;
}

CommandCode CommonCommands::printFile(const char* filename) { return printFileImpl(filename, false); }

CommandCode CommonCommands::printFileHex(const char* filename) { return printFileImpl(filename, true); }

CommandCode CommonCommands::removeFile(const char* filename)
{
    if (!LittleFS.remove(filename))
    {
        Serial.printf("Could not delete the file '%s' because it does not exist.\n", filename);
        return COMMAND_ERROR;
    }
    return COMMAND_SUCCESS;
}

CommandCode CommonCommands::touchFile(const char* filename)
{
    File touch{LittleFS.open(filename, "w", true)};
    if (!touch)
    {
        Serial.printf("Could not create file '%s'.\n", filename);
        return COMMAND_ERROR;
    }
    touch.close();
    return COMMAND_SUCCESS;
}

CommandCode CommonCommands::format()
{
    Serial.println("Formatting flash memory (this can take some time)...");
    LittleFS.format();
    Serial.println("Restarting ...");
    ESP.restart();
    return COMMAND_SUCCESS;
}

CommandCode CommonCommands::echo(const char* arg)
{
    Serial.print(arg);
    return COMMAND_SUCCESS;
}

CommandCode CommonCommands::exit() { return COMMAND_EXIT; }