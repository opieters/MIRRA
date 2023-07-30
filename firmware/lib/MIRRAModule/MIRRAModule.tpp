#ifndef __MIRRAMODULE_T__
#define __MIRRAMODULE_T__
#include <MIRRAModule.h>

template <class T> void MIRRAModule::Commands<T>::prompt()
{
    Serial.println("Press the BOOT pin to enter command phase ...");
    for (size_t i = 0; i < UART_PHASE_ENTRY_PERIOD * 10; i++)
    {
        if (commandPhaseFlag)
        {
            Log::info("Entering command phase...");
            start();
            commandPhaseFlag = false;
            return;
        }
        delay(100);
    }
}

template <class T> std::optional<std::array<char, MIRRAModule::Commands<T>::lineMaxLength>> MIRRAModule::Commands<T>::readLine()
{
    uint64_t timeout{static_cast<uint64_t>(esp_timer_get_time()) + (UART_PHASE_TIMEOUT * 1000 * 1000)};
    uint8_t length{0};
    std::array<char, lineMaxLength> buffer{};
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
    return buffer;
}

template <class T> void MIRRAModule::Commands<T>::start()
{
    Serial.println("COMMAND PHASE");
    while (true)
    {
        auto buffer{readLine()};
        if (!buffer)
            return;
        switch (static_cast<typename T::Commands*>(this)->processCommands(buffer->data()))
        {
        case COMMAND_FOUND:
            break;
        case COMMAND_NOT_FOUND:
            Serial.printf("Command '%s' not found or invalid argument(s) given.\n", buffer->data());
            break;
        case COMMAND_EXIT:
            return;
        }
    }
}

template <class T> typename MIRRAModule::Commands<T>::CommandCode MIRRAModule::Commands<T>::processCommands(char* command)
{
    if (strcmp(command, "exit") == 0 || strcmp(command, "close") == 0)
    {
        Serial.println("Exiting command phase...");
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
template <class T> void MIRRAModule::Commands<T>::listFiles()
{
    File root{LittleFS.open("/")};
    File file{root.openNextFile()};
    while (file)
    {
        Serial.printf("%s\t%uB\n", file.path(), file.size());
        file.close();
        file = root.openNextFile();
    }
    file.close();
    root.close();
}

template <class T> void MIRRAModule::Commands<T>::printFile(const char* filename, bool hex)
{
    if (!LittleFS.exists(filename))
    {
        Serial.printf("File '%s' does not exist.\n", filename);
        return;
    }
    File file{LittleFS.open(filename)};
    if (!file)
    {
        Serial.printf("Error while opening file '%s'\n", filename);
        return;
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
}

template <class T> void MIRRAModule::Commands<T>::removeFile(const char* filename)
{
    if (!LittleFS.remove(filename))
        Serial.printf("Could not delete the file '%s' because it does not exist.\n", filename);
}

template <class T> void MIRRAModule::Commands<T>::touchFile(const char* filename)
{
    File touch{LittleFS.open(filename, "w", true)};
    if (!touch)
        Serial.printf("Could not create file '%s'.\n", filename);
    touch.close();
}
#endif