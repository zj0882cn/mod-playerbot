#ifndef _BOT_GROUP_SCRIPT_H_
#define _BOT_GROUP_SCRIPT_H_

#include "GroupScript.h"
#include "ObjectGuid.h"

class Group;
class Player;

class BotGroupScript : public GroupScript
{
public:
    BotGroupScript();

    void OnInviteMember(Group* group, ObjectGuid guid) override;
    void OnAddMember(Group* group, ObjectGuid guid) override;
    void OnRemoveMember(Group* group, ObjectGuid guid, RemoveMethod method, 
                        ObjectGuid kicker, const char* reason) override;
    void OnDisband(Group* group) override;
};

#endif
