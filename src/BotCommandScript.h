#ifndef _BOT_COMMAND_SCRIPT_H_
#define _BOT_COMMAND_SCRIPT_H_

#include "CommandScript.h"
#include "Chat.h"

class BotCommandScript : public CommandScript
{
public:
    BotCommandScript();

    std::vector<Acore::ChatCommands::ChatCommandBuilder> GetCommands() const override;

    static bool HandleBotCommand(ChatHandler* handler, char const* args);
};

void AddBotCommandScripts();

#endif
