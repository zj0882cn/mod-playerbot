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

void BotGroupScript::OnInviteMember(Group* group, ObjectGuid guid)
{
    if (!sPlayerBotMgr->IsBot(guid))
        return;

    Player* bot = ObjectAccessor::FindConnectedPlayer(guid);
    if (!bot)
        return;

    // NOTE: GetGroupInvite() is ALWAYS non-empty here because Group::AddInvite()
    // calls SetGroupInvite() BEFORE firing the OnInviteMember hook. Checking it
    // would make the bot never auto-accept group invites, so it is intentionally
    // omitted. AddInvite() already guards against bots that are in a group or
    // already have a pending invite.
    Group* playerGroup = bot->GetGroup();
    if (playerGroup && (playerGroup->isBGGroup() || playerGroup->isBFGroup()))
        playerGroup = bot->GetOriginalGroup();
    if (playerGroup)
        return;

    if (group->IsFull())
        return;

    ObjectGuid inviterGuid = group->GetLeaderGUID();
    Player* inviter = ObjectAccessor::FindConnectedPlayer(inviterGuid);
    if (!inviter)
        return;

    ObjectGuid currentMaster = sPlayerBotMgr->GetMaster(guid);

    if (!currentMaster.IsEmpty())
    {
        ChatHandler handler(bot->GetSession());
        handler.PSendSysMessage("|cffff0000[Bot]|r Already has a Master.");
        return;
    }

    sPlayerBotMgr->SetMaster(guid, inviterGuid);

    if (group->AddMember(bot))
    {
        group->BroadcastGroupUpdate();
        ChatHandler handler(bot->GetSession());
        handler.PSendSysMessage("|cff00ff00[Bot]|r |cffff00ff{}|r is now your Master!", 
            inviter->GetName());
        LOG_INFO("playerbots", "[{}] Bot '{}' assigned Master '{}'", 
            PLAYERBOT_VERSION, bot->GetName(), inviter->GetName());
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
