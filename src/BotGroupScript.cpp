#include "BotGroupScript.h"
#include "BotCommon.h"
#include "PlayerBotMgr.h"
#include "Group.h"
#include "GroupMgr.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Chat.h"
#include "Log.h"
#include "MotionMaster.h"

#define PLAYERBOT_VERSION "v2.2.0"

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

    PB_LOG(1, "Hook OnInviteMember: bot '{}' invited to group (leader '{}')",
        sPlayerBotMgr->GetCharacterName(guid), sPlayerBotMgr->GetCharacterName(group->GetLeaderGUID()));

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
        BotNotify(bot, "|cffff0000[Bot]|r Already has a Master.");
        return;
    }

    sPlayerBotMgr->SetMaster(guid, inviterGuid);

    // Simulate the normal "accept invite" flow (HandleGroupAcceptOpcode) so
    // the client actually builds its group state. A bare AddMember() here
    // leaves the client stuck in "invited" state (no group UI on either side).
    group->RemoveInvite(bot);
    if (!group->IsCreated())
    {
        // New group: the leader must be formally created/joined first.
        Player* leader = ObjectAccessor::FindConnectedPlayer(group->GetLeaderGUID());
        if (!leader)
        {
            group->RemoveAllInvites();
            return;
        }
        group->RemoveInvite(leader);
        group->Create(leader);
        sGroupMgr->AddGroup(group);
    }

    if (group->AddMember(bot))
    {
        group->BroadcastGroupUpdate();
        BotNotify(bot, "|cff00ff00[Bot]|r |cffff00ff{}|r is now your Master!", 
            inviter->GetName());
        PB_LOG(1, "Bot '{}' auto-accepted group invite, master '{}'",
            bot->GetName(), inviter->GetName());
        LOG_INFO("playerbots", "[{}] Bot '{}' assigned Master '{}'", 
            PLAYERBOT_VERSION, bot->GetName(), inviter->GetName());
    }
}

void BotGroupScript::OnAddMember(Group* group, ObjectGuid guid)
{
    if (!sPlayerBotMgr->IsBot(guid))
        return;

    PB_LOG(1, "Hook OnAddMember: bot '{}' added to group (leader '{}')",
        sPlayerBotMgr->GetCharacterName(guid), sPlayerBotMgr->GetCharacterName(group->GetLeaderGUID()));

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
        BotNotify(bot, "|cff00ff00[Bot]|r Joined group. Master: |cffff00ff{}|r", 
            sPlayerBotMgr->GetMasterName(guid).c_str());
    }
}

void BotGroupScript::OnRemoveMember(Group* group, ObjectGuid guid, RemoveMethod method, 
                                    ObjectGuid kicker, const char* reason)
{
    if (!sPlayerBotMgr->IsBot(guid))
        return;

    PB_LOG(1, "Hook OnRemoveMember: bot '{}' removed from group (method {}, kicker '{}', reason '{}')",
        sPlayerBotMgr->GetCharacterName(guid), int(method),
        sPlayerBotMgr->GetCharacterName(kicker), reason ? reason : "");

    ObjectGuid masterGuid = sPlayerBotMgr->GetMaster(guid);
    if (!masterGuid.IsEmpty() && group && !group->IsMember(masterGuid))
    {
        sPlayerBotMgr->ClearMaster(guid);
    }

    Player* bot = ObjectAccessor::FindConnectedPlayer(guid);
    if (bot)
    {
        // Notify master BEFORE clearing, otherwise master info is lost.
        BotNotify(bot, "|cff00ff00[Bot]|r Removed from group. Master cleared.");
        sPlayerBotMgr->ClearMaster(guid);
        // 踢出后停止跟随旧 master 的移动生成器（修复：master 已清但 bot 仍物理跟随）
        if (bot->GetMotionMaster()->GetCurrentMovementGeneratorType() == FOLLOW_MOTION_TYPE)
            bot->GetMotionMaster()->MoveIdle();
    }
}

void BotGroupScript::OnDisband(Group* group)
{
    PB_LOG(1, "Hook OnDisband: group disbanded ({} members)", group->GetMembersCount());
    for (auto const& slot : group->GetMemberSlots())
    {
        if (sPlayerBotMgr->IsBot(slot.guid))
        {
            sPlayerBotMgr->ClearMaster(slot.guid);
        }
    }
}
