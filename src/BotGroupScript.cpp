#include "BotGroupScript.h"
#include "PlayerBotMgr.h"
#include "Group.h"
#include "GroupMgr.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Chat.h"
#include "Log.h"

#define PLAYERBOT_VERSION "v2.1.0.2"

BotGroupScript::BotGroupScript() : GroupScript("BotGroupScript", {
    GROUPHOOK_ON_INVITE_MEMBER,
    GROUPHOOK_ON_ADD_MEMBER,
    GROUPHOOK_ON_REMOVE_MEMBER,
    GROUPHOOK_ON_DISBAND,
}) { }

void BotGroupScript::OnInviteMember(Group* /*group*/, ObjectGuid guid)
{
    // Bots do not auto-accept group invites anymore.
    // The bot's player must accept the invite manually via the client UI.
    if (!sPlayerBotMgr->IsBot(guid))
        return;

    Player* bot = ObjectAccessor::FindConnectedPlayer(guid);
    if (bot)
    {
        ChatHandler handler(bot->GetSession());
        handler.PSendSysMessage("|cff00ff00[Bot]|r You received a group invite. Accept it manually to join.");
    }
}

void BotGroupScript::OnAddMember(Group* group, ObjectGuid guid)
{
    if (!sPlayerBotMgr->IsBot(guid))
        return;

    ObjectGuid currentMaster = sPlayerBotMgr->GetMaster(guid);
    if (currentMaster.IsEmpty())
    {
        ObjectGuid leaderGuid = group->GetLeaderGUID();
        if (!leaderGuid.IsEmpty())
        {
            sPlayerBotMgr->SetMaster(guid, leaderGuid);
        }
    }

    Player* bot = ObjectAccessor::FindConnectedPlayer(guid);
    if (bot)
    {
        ChatHandler handler(bot->GetSession());
        handler.PSendSysMessage("|cff00ff00[Bot]|r Joined group. Master: |cffff00ff{}|r", 
            sPlayerBotMgr->GetMasterName(guid).c_str());
    }
}

void BotGroupScript::OnRemoveMember(Group* group, ObjectGuid guid, RemoveMethod /*method*/, 
                                    ObjectGuid /*kicker*/, const char* /*reason*/)
{
    if (!sPlayerBotMgr->IsBot(guid))
        return;

    ObjectGuid masterGuid = sPlayerBotMgr->GetMaster(guid);
    if (!masterGuid.IsEmpty() && group && !group->IsMember(masterGuid))
    {
        sPlayerBotMgr->ClearMaster(guid);
    }

    Player* bot = ObjectAccessor::FindConnectedPlayer(guid);
    if (bot)
    {
        sPlayerBotMgr->ClearMaster(guid);
        ChatHandler handler(bot->GetSession());
        handler.PSendSysMessage("|cff00ff00[Bot]|r Removed from group. Master cleared.");
    }
}

void BotGroupScript::OnDisband(Group* group)
{
    for (auto const& slot : group->GetMemberSlots())
    {
        if (sPlayerBotMgr->IsBot(slot.guid))
        {
            sPlayerBotMgr->ClearMaster(slot.guid);
        }
    }
}
