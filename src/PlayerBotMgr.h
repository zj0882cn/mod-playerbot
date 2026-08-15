#ifndef _PLAYERBOT_MGR_H_
#define _PLAYERBOT_MGR_H_

#include "Common.h"
#include "ObjectGuid.h"
#include <map>
#include <set>
#include <string>

class Player;

class PlayerBotMgr
{
public:
    enum BotStance
    {
        STANCE_DEFENSIVE = 0,
        STANCE_PASSIVE = 1,
        STANCE_AGGRESSIVE = 2
    };

    static PlayerBotMgr* instance();

    bool IsBot(ObjectGuid guid) const;
    void AddBot(ObjectGuid guid);
    void RemoveBot(ObjectGuid guid);
    size_t GetCount() const;
    std::set<ObjectGuid> GetAllBots() const;

    void SetMaster(ObjectGuid botGuid, ObjectGuid masterGuid);
    ObjectGuid GetMaster(ObjectGuid botGuid) const;
    void ClearMaster(ObjectGuid botGuid);
    std::string GetMasterName(ObjectGuid botGuid) const;
    bool IsMasterOf(ObjectGuid masterGuid, ObjectGuid botGuid) const;

    void SetBotStance(ObjectGuid botGuid, BotStance stance);
    BotStance GetBotStance(ObjectGuid botGuid) const;

    ObjectGuid FindPlayerByName(std::string const& name);

private:
    PlayerBotMgr() = default;
    ~PlayerBotMgr() = default;

    std::set<ObjectGuid> _botGuids;
    std::map<ObjectGuid, ObjectGuid> _botMasters;
    std::map<ObjectGuid, BotStance> _botStances;
};

#define sPlayerBotMgr PlayerBotMgr::instance()

#endif
