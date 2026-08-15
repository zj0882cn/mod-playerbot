#include "PlayerBotMgr.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Log.h"

#define PLAYERBOT_VERSION "v2.1.0.2"

PlayerBotMgr* PlayerBotMgr::instance()
{
    static PlayerBotMgr inst;
    return &inst;
}

bool PlayerBotMgr::IsBot(ObjectGuid guid) const
{
    return _botGuids.find(guid) != _botGuids.end();
}

void PlayerBotMgr::AddBot(ObjectGuid guid)
{
    _botGuids.insert(guid);
    LOG_INFO("playerbots", "[{}] Bot added: {}", PLAYERBOT_VERSION, guid.ToString());
}

void PlayerBotMgr::RemoveBot(ObjectGuid guid)
{
    _botGuids.erase(guid);
    _botMasters.erase(guid);
    _botStances.erase(guid);
    LOG_INFO("playerbots", "[{}] Bot removed: {}", PLAYERBOT_VERSION, guid.ToString());
}

size_t PlayerBotMgr::GetCount() const
{
    return _botGuids.size();
}

std::set<ObjectGuid> PlayerBotMgr::GetAllBots() const
{
    return _botGuids;
}

void PlayerBotMgr::SetMaster(ObjectGuid botGuid, ObjectGuid masterGuid)
{
    _botMasters[botGuid] = masterGuid;
    LOG_DEBUG("playerbots", "[{}] Bot {} master set to {}", 
        PLAYERBOT_VERSION, botGuid.ToString(), masterGuid.ToString());
}

ObjectGuid PlayerBotMgr::GetMaster(ObjectGuid botGuid) const
{
    auto itr = _botMasters.find(botGuid);
    if (itr != _botMasters.end())
        return itr->second;
    return ObjectGuid::Empty;
}

void PlayerBotMgr::ClearMaster(ObjectGuid botGuid)
{
    _botMasters.erase(botGuid);
    LOG_DEBUG("playerbots", "[{}] Bot {} master cleared", PLAYERBOT_VERSION, botGuid.ToString());
}

std::string PlayerBotMgr::GetMasterName(ObjectGuid botGuid) const
{
    auto itr = _botMasters.find(botGuid);
    if (itr != _botMasters.end())
    {
        Player* master = ObjectAccessor::FindConnectedPlayer(itr->second);
        if (master)
            return master->GetName();
        return "Offline Master";
    }
    return "None";
}

bool PlayerBotMgr::IsMasterOf(ObjectGuid masterGuid, ObjectGuid botGuid) const
{
    auto itr = _botMasters.find(botGuid);
    return itr != _botMasters.end() && itr->second == masterGuid;
}

void PlayerBotMgr::SetBotStance(ObjectGuid botGuid, BotStance stance)
{
    _botStances[botGuid] = stance;
    LOG_DEBUG("playerbots", "[{}] Bot {} stance set to {}", 
        PLAYERBOT_VERSION, botGuid.ToString(), stance);
}

PlayerBotMgr::BotStance PlayerBotMgr::GetBotStance(ObjectGuid botGuid) const
{
    auto itr = _botStances.find(botGuid);
    if (itr != _botStances.end())
        return itr->second;
    return STANCE_DEFENSIVE;
}

ObjectGuid PlayerBotMgr::FindPlayerByName(std::string const& name)
{
    auto const& players = ObjectAccessor::GetPlayers();
    std::string searchLower = name;
    std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

    for (auto const& [guid, player] : players)
    {
        if (!player)
            continue;
        std::string pName = player->GetName();
        std::transform(pName.begin(), pName.end(), pName.begin(), ::tolower);
        if (pName == searchLower)
            return guid;
    }

    LOG_DEBUG("playerbots", "[{}] Player '{}' not found online", PLAYERBOT_VERSION, name);
    return ObjectGuid::Empty;
}
