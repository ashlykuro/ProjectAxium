#include "Common.h"
#include "ObjectMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "DatabaseEnv.h"

#include "AccountMgr.h"
#include "CellImpl.h"
#include "Chat.h"
#include "GridNotifiersImpl.h"
#include "Language.h"
#include "Log.h"
#include "Opcodes.h"
#include "Player.h"
#include "UpdateMask.h"
#include "SpellMgr.h"
#include "ScriptMgr.h"
#include "ChatLink.h"

bool ChatHandler::load_command_table = true;

// wrapper for old-style handlers
template<bool (ChatHandler::*F)(const char*)>
bool OldHandler(ChatHandler* chatHandler, const char* args)
{
    return (chatHandler->*F)(args);
}

// get number of commands in table
static size_t getCommandTableSize(const ChatCommand* commands)
{
    if (!commands)
        return 0;
    size_t count = 0;
    while (commands[count].Name != NULL)
        count++;
    return count;
}

// append source command table to target, return number of appended commands
static size_t appendCommandTable(ChatCommand* target, const ChatCommand* source)
{
    const size_t count = getCommandTableSize(source);
    if (count)
        memcpy(target, source, count * sizeof(ChatCommand));
    return count;
}

ChatCommand* ChatHandler::getCommandTable()
{
    static ChatCommand banCommandTable[] =
    {
        { "account",        SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleBanAccountCommand>,          "", NULL },
        { "character",      SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleBanCharacterCommand>,        "", NULL },
        { "playeraccount",  SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleBanAccountByCharCommand>,    "", NULL },
        { "ip",             SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleBanIPCommand>,               "", NULL },
        { NULL,             0,               false, NULL,                                                       "", NULL }
    };

    static ChatCommand baninfoCommandTable[] =
    {
        { "account",        SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleBanInfoAccountCommand>,      "", NULL },
        { "character",      SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleBanInfoCharacterCommand>,    "", NULL },
        { "ip",             SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleBanInfoIPCommand>,           "", NULL },
        { NULL,             0,               false, NULL,                                                       "", NULL }
    };

    static ChatCommand banlistCommandTable[] =
    {
        { "account",        SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleBanListAccountCommand>,      "", NULL },
        { "character",      SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleBanListCharacterCommand>,    "", NULL },
        { "ip",             SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleBanListIPCommand>,           "", NULL },
        { NULL,             0,               false, NULL,                                                       "", NULL }
    };

    static ChatCommand castCommandTable[] =
    {
        { "back",           SEC_GAMEMASTER,  false, OldHandler<&ChatHandler::HandleCastBackCommand>,            "", NULL },
        { "dist",           SEC_GAMEMASTER,  false, OldHandler<&ChatHandler::HandleCastDistCommand>,            "", NULL },
        { "self",           SEC_GAMEMASTER,  false, OldHandler<&ChatHandler::HandleCastSelfCommand>,            "", NULL },
        { "target",         SEC_GAMEMASTER,  false, OldHandler<&ChatHandler::HandleCastTargetCommand>,          "", NULL },
        { "dest",           SEC_GAMEMASTER,  false, OldHandler<&ChatHandler::HandleCastDestCommand>,            "", NULL },
        { "",               SEC_GAMEMASTER,  false, OldHandler<&ChatHandler::HandleCastCommand>,                "", NULL },
        { NULL,             0,               false, NULL,                                                       "", NULL }
    };

    static ChatCommand characterDeletedCommandTable[] =
    {
        { "delete",         SEC_CONSOLE,     true,  OldHandler<&ChatHandler::HandleCharacterDeletedDeleteCommand>,   "", NULL },
        { "list",           SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleCharacterDeletedListCommand>,     "", NULL },
        { "restore",        SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleCharacterDeletedRestoreCommand>,  "", NULL },
        { "old",            SEC_CONSOLE,     true,  OldHandler<&ChatHandler::HandleCharacterDeletedOldCommand>,      "", NULL },
        { NULL,             0,               false, NULL,                                                            "", NULL }
    };

    static ChatCommand characterCommandTable[] =
    {
        { "customize",      SEC_GAMEMASTER,       true,  OldHandler<&ChatHandler::HandleCharacterCustomizeCommand>,     "", NULL },
        { "changefaction",  SEC_GAMEMASTER,       true,  OldHandler<&ChatHandler::HandleCharacterChangeFactionCommand>, "", NULL },
        { "changerace",     SEC_GAMEMASTER,       true,  OldHandler<&ChatHandler::HandleCharacterChangeRaceCommand>,    "", NULL },
        { "deleted",        SEC_GAMEMASTER,       true,  NULL,                                  "", characterDeletedCommandTable },
        { "erase",          SEC_CONSOLE,          true,  OldHandler<&ChatHandler::HandleCharacterEraseCommand>,         "", NULL },
        { "level",          SEC_GAMEMASTER,       true,  OldHandler<&ChatHandler::HandleCharacterLevelCommand>,         "", NULL },
        { "rename",         SEC_GAMEMASTER,       true,  OldHandler<&ChatHandler::HandleCharacterRenameCommand>,        "", NULL },
        { "reputation",     SEC_GAMEMASTER,       true,  OldHandler<&ChatHandler::HandleCharacterReputationCommand>,    "", NULL },
        { "titles",         SEC_GAMEMASTER,       true,  OldHandler<&ChatHandler::HandleCharacterTitlesCommand>,        "", NULL },
        { NULL,             0,                    false, NULL,                                                          "", NULL }
    };

    static ChatCommand channelSetCommandTable[] =
    {
        { "ownership",      SEC_GAMEMASTER,  false,  OldHandler<&ChatHandler::HandleChannelSetOwnership>, "", NULL },
        { NULL,             0,               false, NULL,                                                 "", NULL }
    };

    static ChatCommand channelCommandTable[] =
    {
        { "set",      SEC_GAMEMASTER,     true,  NULL,  "", channelSetCommandTable },
        { NULL,       0,                  false, NULL,  "", NULL                   }
    };

    static ChatCommand guildCommandTable[] =
    {
        { "create",         SEC_GAMEMASTER,     true,  OldHandler<&ChatHandler::HandleGuildCreateCommand>,         "", NULL },
        { "delete",         SEC_GAMEMASTER,     true,  OldHandler<&ChatHandler::HandleGuildDeleteCommand>,         "", NULL },
        { "invite",         SEC_GAMEMASTER,     true,  OldHandler<&ChatHandler::HandleGuildInviteCommand>,         "", NULL },
        { "uninvite",       SEC_GAMEMASTER,     true,  OldHandler<&ChatHandler::HandleGuildUninviteCommand>,       "", NULL },
        { "rank",           SEC_GAMEMASTER,     true,  OldHandler<&ChatHandler::HandleGuildRankCommand>,           "", NULL },
        { NULL,             0,                  false, NULL,                                                       "", NULL }
    };

    static ChatCommand instanceCommandTable[] =
    {
        { "listbinds",      SEC_GAMEMASTER,  false, OldHandler<&ChatHandler::HandleInstanceListBindsCommand>,   "", NULL },
        { "unbind",         SEC_GAMEMASTER,  false, OldHandler<&ChatHandler::HandleInstanceUnbindCommand>,      "", NULL },
        { "stats",          SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleInstanceStatsCommand>,       "", NULL },
        { "savedata",       SEC_GAMEMASTER,  false, OldHandler<&ChatHandler::HandleInstanceSaveDataCommand>,    "", NULL },
        { NULL,             0,               false, NULL,                                                       "", NULL }
    };

    static ChatCommand listCommandTable[] =
    {
        { "creature",       SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleListCreatureCommand>,        "", NULL },
        { "item",           SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleListItemCommand>,            "", NULL },
        { "object",         SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleListObjectCommand>,          "", NULL },
        { "auras",          SEC_GAMEMASTER,  false, OldHandler<&ChatHandler::HandleListAurasCommand>,           "", NULL },
        { NULL,             0,               false, NULL,                                                       "", NULL }
    };

    static ChatCommand lookupPlayerCommandTable[] =
    {
        { "ip",             SEC_GAMEMASTER,     true,  OldHandler<&ChatHandler::HandleLookupPlayerIpCommand>,       "", NULL },
        { "account",        SEC_GAMEMASTER,     true,  OldHandler<&ChatHandler::HandleLookupPlayerAccountCommand>,  "", NULL },
        { "email",          SEC_GAMEMASTER,     true,  OldHandler<&ChatHandler::HandleLookupPlayerEmailCommand>,    "", NULL },
        { NULL,             0,                  false, NULL,                                                        "", NULL }
    };

    static ChatCommand lookupCommandTable[] =
    {
        { "area",           SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleLookupAreaCommand>,          "", NULL },
        { "creature",       SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleLookupCreatureCommand>,      "", NULL },
        { "event",          SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleLookupEventCommand>,         "", NULL },
        { "faction",        SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleLookupFactionCommand>,       "", NULL },
        { "item",           SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleLookupItemCommand>,          "", NULL },
        { "itemset",        SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleLookupItemSetCommand>,       "", NULL },
        { "object",         SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleLookupObjectCommand>,        "", NULL },
        { "quest",          SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleLookupQuestCommand>,         "", NULL },
        { "player",         SEC_GAMEMASTER,  true,  NULL,                                   "", lookupPlayerCommandTable },
        { "skill",          SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleLookupSkillCommand>,         "", NULL },
        { "spell",          SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleLookupSpellCommand>,         "", NULL },
        { "taxinode",       SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleLookupTaxiNodeCommand>,      "", NULL },
        { "tele",           SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleLookupTeleCommand>,          "", NULL },
        { "title",          SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleLookupTitleCommand>,         "", NULL },
        { "map",            SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleLookupMapCommand>,           "", NULL },
        { NULL,             0,               false, NULL,                                                       "", NULL }
    };

    static ChatCommand petCommandTable[] =
    {
        { "create",         SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleCreatePetCommand>,           "", NULL },
        { "learn",          SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandlePetLearnCommand>,            "", NULL },
        { "unlearn",        SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandlePetUnlearnCommand>,          "", NULL },
        { NULL,             0,                  false, NULL,                                                       "", NULL }
    };

    static ChatCommand pdumpCommandTable[] =
    {
        { "load",           SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandlePDumpLoadCommand>,           "", NULL },
        { "write",          SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandlePDumpWriteCommand>,          "", NULL },
        { NULL,             0,               false, NULL,                                                       "", NULL }
    };

    static ChatCommand resetCommandTable[] =
    {
        { "achievements",   SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleResetAchievementsCommand>,   "", NULL },
        { "honor",          SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleResetHonorCommand>,          "", NULL },
        { "level",          SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleResetLevelCommand>,          "", NULL },
        { "spells",         SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleResetSpellsCommand>,         "", NULL },
        { "stats",          SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleResetStatsCommand>,          "", NULL },
        { "talents",        SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleResetTalentsCommand>,        "", NULL },
        { "all",            SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleResetAllCommand>,            "", NULL },
        { NULL,             0,               false, NULL,                                                       "", NULL }
    };

    static ChatCommand sendCommandTable[] =
    {
        { "items",          SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleSendItemsCommand>,           "", NULL },
        { "mail",           SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleSendMailCommand>,            "", NULL },
        { "message",        SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleSendMessageCommand>,         "", NULL },
        { "money",          SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleSendMoneyCommand>,           "", NULL },
        { NULL,             0,               false, NULL,                                                       "", NULL }
    };

    static ChatCommand serverIdleRestartCommandTable[] =
    {
        { "cancel",         SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleServerShutDownCancelCommand>, "", NULL },
        { ""   ,            SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleServerIdleRestartCommand>,    "", NULL },
        { NULL,             0,               false, NULL,                                                        "", NULL }
    };

    static ChatCommand serverIdleShutdownCommandTable[] =
    {
        { "cancel",         SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleServerShutDownCancelCommand>, "", NULL },
        { ""   ,            SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleServerIdleShutDownCommand>,   "", NULL },
        { NULL,             0,               false, NULL,                                                        "", NULL }
    };

    static ChatCommand serverRestartCommandTable[] =
    {
        { "cancel",         SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleServerShutDownCancelCommand>, "", NULL },
        { ""   ,            SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleServerRestartCommand>,        "", NULL },
        { NULL,             0,               false, NULL,                                                        "", NULL }
    };

    static ChatCommand serverShutdownCommandTable[] =
    {
        { "cancel",         SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleServerShutDownCancelCommand>, "", NULL },
        { ""   ,            SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleServerShutDownCommand>,       "", NULL },
        { NULL,             0,               false, NULL,                                                        "", NULL }
    };

    static ChatCommand serverSetCommandTable[] =
    {
        { "loglevel",       SEC_CONSOLE,     true,  OldHandler<&ChatHandler::HandleServerSetLogLevelCommand>,     "", NULL },
        { "logfilelevel",   SEC_CONSOLE,     true,  OldHandler<&ChatHandler::HandleServerSetLogFileLevelCommand>, "", NULL },
        { "motd",           SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleServerSetMotdCommand>,         "", NULL },
        { "closed",         SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleServerSetClosedCommand>,       "", NULL },
        { NULL,             0,               false, NULL,                                                         "", NULL }
    };

    static ChatCommand serverCommandTable[] =
    {
        { "corpses",        SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleServerCorpsesCommand>,     "", NULL },
        { "exit",           SEC_CONSOLE,     true,  OldHandler<&ChatHandler::HandleServerExitCommand>,        "", NULL },
        { "idlerestart",    SEC_GAMEMASTER,  true,  NULL,                            "", serverIdleRestartCommandTable },
        { "idleshutdown",   SEC_GAMEMASTER,  true,  NULL,                           "", serverIdleShutdownCommandTable },
        { "info",           SEC_PLAYER,      true,  OldHandler<&ChatHandler::HandleServerInfoCommand>,        "", NULL },
        { "motd",           SEC_PLAYER,      true,  OldHandler<&ChatHandler::HandleServerMotdCommand>,        "", NULL },
        { "plimit",         SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleServerPLimitCommand>,      "", NULL },
        { "restart",        SEC_GAMEMASTER,  true,  NULL,                                "", serverRestartCommandTable },
        { "shutdown",       SEC_GAMEMASTER,  true,  NULL,                               "", serverShutdownCommandTable },
        { "set",            SEC_GAMEMASTER,  true,  NULL,                                    "", serverSetCommandTable },
        { "togglequerylog", SEC_CONSOLE,     true,  OldHandler<&ChatHandler::HandleServerToggleQueryLogging>, "", NULL },
        { NULL,             0,               false, NULL,                                                     "", NULL }
    };

    static ChatCommand unbanCommandTable[] =
    {
        { "account",        SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleUnBanAccountCommand>,       "", NULL },
        { "character",      SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleUnBanCharacterCommand>,     "", NULL },
        { "playeraccount",  SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleUnBanAccountByCharCommand>, "", NULL },
        { "ip",             SEC_GAMEMASTER,  true,  OldHandler<&ChatHandler::HandleUnBanIPCommand>,            "", NULL },
        { NULL,             0,               false, NULL,                                                      "", NULL }
    };

    static ChatCommand commandTable[] =
    {
        { "character",        SEC_GAMEMASTER,    true,  NULL,                                      "", characterCommandTable },
        { "list",             SEC_GAMEMASTER,    true,  NULL,                                           "", listCommandTable },
        { "lookup",           SEC_GAMEMASTER,    true,  NULL,                                         "", lookupCommandTable },
        { "pdump",            SEC_GAMEMASTER,    true,  NULL,                                          "", pdumpCommandTable },
        { "guild",            SEC_GAMEMASTER,    true,  NULL,                                          "", guildCommandTable },
        { "cast",             SEC_GAMEMASTER,    false, NULL,                                           "", castCommandTable },
        { "reset",            SEC_GAMEMASTER,    true,  NULL,                                          "", resetCommandTable },
        { "instance",         SEC_GAMEMASTER,    true,  NULL,                                       "", instanceCommandTable },
        { "server",           SEC_GAMEMASTER,    true,  NULL,                                         "", serverCommandTable },
        { "channel",          SEC_GAMEMASTER,    true,  NULL,                                        "", channelCommandTable },
        { "pet",              SEC_GAMEMASTER,    false, NULL,                                            "", petCommandTable },
        { "aura",             SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleAuraCommand>,                "", NULL },
        { "unaura",           SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleUnAuraCommand>,              "", NULL },
        { "nameannounce",     SEC_GAMEMASTER,    true,  OldHandler<&ChatHandler::HandleNameAnnounceCommand>,        "", NULL },
        { "gmnameannounce",   SEC_GAMEMASTER,    true,  OldHandler<&ChatHandler::HandleGMNameAnnounceCommand>,      "", NULL },
        { "announce",         SEC_GAMEMASTER,    true,  OldHandler<&ChatHandler::HandleAnnounceCommand>,            "", NULL },
        { "gmannounce",       SEC_GAMEMASTER,    true,  OldHandler<&ChatHandler::HandleGMAnnounceCommand>,          "", NULL },
        { "notify",           SEC_GAMEMASTER,    true,  OldHandler<&ChatHandler::HandleNotifyCommand>,              "", NULL },
        { "gmnotify",         SEC_GAMEMASTER,    true,  OldHandler<&ChatHandler::HandleGMNotifyCommand>,            "", NULL },
        { "appear",           SEC_VIP,           false, OldHandler<&ChatHandler::HandleAppearCommand>,              "", NULL },
        { "summon",           SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleSummonCommand>,              "", NULL },
        { "commands",         SEC_PLAYER,        true,  OldHandler<&ChatHandler::HandleCommandsCommand>,            "", NULL },
        { "die",              SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleDieCommand>,                 "", NULL },
        { "revive",           SEC_GAMEMASTER,    true,  OldHandler<&ChatHandler::HandleReviveCommand>,              "", NULL },
        { "dismount",         SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleDismountCommand>,            "", NULL },
        { "guid",             SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleGUIDCommand>,                "", NULL },
        { "help",             SEC_GAMEMASTER,    true,  OldHandler<&ChatHandler::HandleHelpCommand>,                "", NULL },
        { "itemmove",         SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleItemMoveCommand>,            "", NULL },
        { "cooldown",         SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleCooldownCommand>,            "", NULL },
        { "unlearn",          SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleUnLearnCommand>,             "", NULL },
        { "distance",         SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleGetDistanceCommand>,         "", NULL },
        { "recall",           SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleRecallCommand>,              "", NULL },
        { "save",             SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleSaveCommand>,                "", NULL },
        { "saveall",          SEC_GAMEMASTER,    true,  OldHandler<&ChatHandler::HandleSaveAllCommand>,             "", NULL },
        { "kick",             SEC_GAMEMASTER,    true,  OldHandler<&ChatHandler::HandleKickPlayerCommand>,          "", NULL },
        { "ban",              SEC_GAMEMASTER,    true,  NULL,                                            "", banCommandTable },
        { "unban",            SEC_GAMEMASTER,    true,  NULL,                                          "", unbanCommandTable },
        { "baninfo",          SEC_GAMEMASTER,    false, NULL,                                        "", baninfoCommandTable },
        { "banlist",          SEC_GAMEMASTER,    true,  NULL,                                        "", banlistCommandTable },
        { "start",            SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleStartCommand>,               "", NULL },
        { "taxicheat",        SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleTaxiCheatCommand>,           "", NULL },
        { "linkgrave",        SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleLinkGraveCommand>,           "", NULL },
        { "neargrave",        SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleNearGraveCommand>,           "", NULL },
        { "explorecheat",     SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleExploreCheatCommand>,        "", NULL },
        { "hover",            SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleHoverCommand>,               "", NULL },
        { "levelup",          SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleLevelUpCommand>,             "", NULL },
        { "showarea",         SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleShowAreaCommand>,            "", NULL },
        { "hidearea",         SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleHideAreaCommand>,            "", NULL },
        { "additem",          SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleAddItemCommand>,             "", NULL },
        { "additemset",       SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleAddItemSetCommand>,          "", NULL },
        { "bank",             SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleBankCommand>,                "", NULL },
        { "wchange",          SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleChangeWeather>,              "", NULL },
        { "maxskill",         SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleMaxSkillCommand>,            "", NULL },
        { "setskill",         SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleSetSkillCommand>,            "", NULL },
        { "whispers",         SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleWhispersCommand>,            "", NULL },
        { "pinfo",            SEC_GAMEMASTER,    true,  OldHandler<&ChatHandler::HandlePInfoCommand>,               "", NULL },
        { "respawn",          SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleRespawnCommand>,             "", NULL },
        { "send",             SEC_GAMEMASTER,    true,  NULL,                                           "", sendCommandTable },
        { "mute",             SEC_GAMEMASTER,    true,  OldHandler<&ChatHandler::HandleMuteCommand>,                "", NULL },
        { "unmute",           SEC_GAMEMASTER,    true,  OldHandler<&ChatHandler::HandleUnmuteCommand>,              "", NULL },
        { "movegens",         SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleMovegensCommand>,            "", NULL },
        { "cometome",         SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleComeToMeCommand>,            "", NULL },
        { "damage",           SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleDamageCommand>,              "", NULL },
        { "combatstop",       SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleCombatStopCommand>,          "", NULL },
        { "repairitems",      SEC_GAMEMASTER,    true,  OldHandler<&ChatHandler::HandleRepairitemsCommand>,         "", NULL },
        { "waterwalk",        SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleWaterwalkCommand>,           "", NULL },
        { "freeze",           SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleFreezeCommand>,              "", NULL },
        { "unfreeze",         SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleUnFreezeCommand>,            "", NULL },
        { "listfreeze",       SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleListFreezeCommand>,          "", NULL },
        { "possess",          SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandlePossessCommand>,             "", NULL },
        { "unpossess",        SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandleUnPossessCommand>,           "", NULL },
        { "playall",          SEC_GAMEMASTER,    false, OldHandler<&ChatHandler::HandlePlayAllCommand>,             "", NULL },
        { "killallorcs",      SEC_ADMINISTRATOR, false, OldHandler<&ChatHandler::HandleKillAllOrcsCommand>,         "", NULL },
        { "crashclient",      SEC_ADMINISTRATOR, false, OldHandler<&ChatHandler::HandleCrashClientCommand>,         "", NULL },
        { NULL,               0,                 false, NULL,                                                       "", NULL }
    };

    // cache for commands, needed because some commands are loaded dynamically through ScriptMgr
    // cache is never freed and will show as a memory leak in diagnostic tools
    // can't use vector as vector storage is implementation-dependent, eg, there can be alignment gaps between elements
    static ChatCommand* commandTableCache = 0;

    if (LoadCommandTable())
    {
        SetLoadCommandTable(false);

        {
            // count total number of top-level commands
            size_t total = getCommandTableSize(commandTable);
            std::vector<ChatCommand*> const& dynamic = sScriptMgr->GetChatCommands();
            for (std::vector<ChatCommand*>::const_iterator it = dynamic.begin(); it != dynamic.end(); ++it)
                total += getCommandTableSize(*it);
            total += 1; // ending zero

            // cache top-level commands
            commandTableCache = (ChatCommand*)malloc(sizeof(ChatCommand) * total);
            memset(commandTableCache, 0, sizeof(ChatCommand) * total);
            ACE_ASSERT(commandTableCache);
            size_t added = appendCommandTable(commandTableCache, commandTable);
            for (std::vector<ChatCommand*>::const_iterator it = dynamic.begin(); it != dynamic.end(); ++it)
                added += appendCommandTable(commandTableCache + added, *it);
        }

        QueryResult result = WorldDatabase.Query("SELECT name, security, help FROM command");
        if (result)
        {
            do
            {
                Field* fields = result->Fetch();
                std::string name = fields[0].GetString();

                SetDataForCommandInTable(commandTableCache, name.c_str(), fields[1].GetUInt16(), fields[2].GetString(), name);

            } while (result->NextRow());
        }
    }

    return commandTableCache;
}

std::string ChatHandler::PGetParseString(int32 entry, ...) const
{
    const char *format = GetAxiumString(entry);
    char str[1024];
    va_list ap;
    va_start(ap, entry);
    vsnprintf(str, 1024, format, ap);
    va_end(ap);
    return std::string(str);
}

const char *ChatHandler::GetAxiumString(int32 entry) const
{
    return m_session->GetAxiumString(entry);
}

bool ChatHandler::isAvailable(ChatCommand const& cmd) const
{
    // check security level only for simple  command (without child commands)
    return m_session->GetSecurity() >= AccountTypes(cmd.SecurityLevel);
}

bool ChatHandler::HasLowerSecurity(Player* target, uint64 guid, bool strong)
{
    WorldSession* target_session = NULL;
    uint32 target_account = 0;

    if (target)
        target_session = target->GetSession();
    else if (guid)
        target_account = sObjectMgr->GetPlayerAccountIdByGUID(guid);

    if (!target_session && !target_account)
    {
        SendSysMessage(LANG_PLAYER_NOT_FOUND);
        SetSentErrorMessage(true);
        return true;
    }

    return HasLowerSecurityAccount(target_session, target_account, strong);
}

bool ChatHandler::HasLowerSecurityAccount(WorldSession* target, uint32 target_account, bool strong)
{
    uint32 target_sec;

    // allow everything from console and RA console
    if (!m_session)
        return false;

    // ignore only for non-players for non strong checks (when allow apply command at least to same sec level)
    if (!AccountMgr::IsPlayerAccount(m_session->GetSecurity()) && !strong && !sWorld->getBoolConfig(CONFIG_GM_LOWER_SECURITY))
        return false;

    if (target)
        target_sec = target->GetSecurity();
    else if (target_account)
        target_sec = AccountMgr::GetSecurity(target_account, realmID);
    else
        return true;                                        // caller must report error for (target == NULL && target_account == 0)

    AccountTypes target_ac_sec = AccountTypes(target_sec);
    if (m_session->GetSecurity() < target_ac_sec || (strong && m_session->GetSecurity() <= target_ac_sec))
    {
        SendSysMessage(LANG_YOURS_SECURITY_IS_LOW);
        SetSentErrorMessage(true);
        return true;
    }

    return false;
}

bool ChatHandler::hasStringAbbr(const char* name, const char* part)
{
    // non "" command
    if (*name)
    {
        // "" part from non-"" command
        if (!*part)
            return false;

        for (;;)
        {
            if (!*part)
                return true;
            else if (!*name)
                return false;
            else if (tolower(*name) != tolower(*part))
                return false;
            ++name; ++part;
        }
    }
    // allow with any for ""

    return true;
}

void ChatHandler::SendSysMessage(const char *str)
{
    WorldPacket data;

    // need copy to prevent corruption by strtok call in LineFromMessage original string
    char* buf = strdup(str);
    char* pos = buf;

    while (char* line = LineFromMessage(pos))
    {
        FillSystemMessageData(&data, line);
        m_session->SendPacket(&data);
    }

    free(buf);
}

void ChatHandler::SendGlobalSysMessage(const char *str)
{
    // Chat output
    WorldPacket data;

    // need copy to prevent corruption by strtok call in LineFromMessage original string
    char* buf = strdup(str);
    char* pos = buf;

    while (char* line = LineFromMessage(pos))
    {
        FillSystemMessageData(&data, line);
        sWorld->SendGlobalMessage(&data);
    }

    free(buf);
}

void ChatHandler::SendGlobalGMSysMessage(const char *str)
{
    // Chat output
    WorldPacket data;

    // need copy to prevent corruption by strtok call in LineFromMessage original string
    char* buf = strdup(str);
    char* pos = buf;

    while (char* line = LineFromMessage(pos))
    {
        FillSystemMessageData(&data, line);
        sWorld->SendGlobalGMMessage(&data);
     }
    free(buf);
}

void ChatHandler::SendSysMessage(int32 entry)
{
    SendSysMessage(GetAxiumString(entry));
}

void ChatHandler::PSendSysMessage(int32 entry, ...)
{
    const char *format = GetAxiumString(entry);
    va_list ap;
    char str [2048];
    va_start(ap, entry);
    vsnprintf(str, 2048, format, ap);
    va_end(ap);
    SendSysMessage(str);
}

void ChatHandler::PSendSysMessage(const char *format, ...)
{
    va_list ap;
    char str [2048];
    va_start(ap, format);
    vsnprintf(str, 2048, format, ap);
    va_end(ap);
    SendSysMessage(str);
}

void ChatHandler::PSendGlobalGMSysMessage(int32 entry, ...)
{
    const char *format = GetAxiumString(entry);
    va_list ap;
    char str [2048];
    va_start(ap, entry);
    vsnprintf(str, 2048, format, ap);
    va_end(ap);
    SendGlobalGMSysMessage(str);
}

void ChatHandler::PSendGlobalGMSysMessage(const char *format, ...)
{
    va_list ap;
    char str [2048];
    va_start(ap, format);
    vsnprintf(str, 2048, format, ap);
    va_end(ap);
    SendGlobalGMSysMessage(str);
}

bool ChatHandler::ExecuteCommandInTable(ChatCommand* table, const char* text, const std::string& fullcmd)
{
    char const* oldtext = text;
    std::string cmd = "";

    while (*text != ' ' && *text != '\0')
    {
        cmd += *text;
        ++text;
    }

    while (*text == ' ') ++text;

    for (uint32 i = 0; table[i].Name != NULL; ++i)
    {
        if (!hasStringAbbr(table[i].Name, cmd.c_str()))
            continue;

        bool match = false;
        if (strlen(table[i].Name) > cmd.length())
        {
            for (uint32 j = 0; table[j].Name != NULL; ++j)
            {
                if (!hasStringAbbr(table[j].Name, cmd.c_str()))
                    continue;

                if (strcmp(table[j].Name, cmd.c_str()) != 0)
                    continue;
                else
                {
                    match = true;
                    break;
                }
            }
        }
        if (match)
            continue;

        // select subcommand from child commands list
        if (table[i].ChildCommands != NULL)
        {
            if (!ExecuteCommandInTable(table[i].ChildCommands, text, fullcmd))
            {
                if (text && text[0] != '\0')
                    SendSysMessage(LANG_NO_SUBCMD);
                else
                    SendSysMessage(LANG_CMD_SYNTAX);

                ShowHelpForCommand(table[i].ChildCommands, text);
            }

            return true;
        }

        // must be available and have handler
        if (!table[i].Handler || !isAvailable(table[i]))
            continue;

        SetSentErrorMessage(false);
        // table[i].Name == "" is special case: send original command to handler
        if ((table[i].Handler)(this, table[i].Name[0] != '\0' ? text : oldtext))
        {
            if (!AccountMgr::IsPlayerAccount(table[i].SecurityLevel))
            {
                // chat case
                if (m_session)
                {
                    Player* p = m_session->GetPlayer();
                    uint64 sel_guid = p->GetSelection();
                    sLog->outCommand(m_session->GetAccountId(), "Command: %s [Player: %s (Account: %u) X: %f Y: %f Z: %f Map: %u Selected %s: %s (GUID: %u)]",
                        fullcmd.c_str(), p->GetName(), m_session->GetAccountId(), p->GetPositionX(), p->GetPositionY(), p->GetPositionZ(), p->GetMapId(),
                        GetLogNameForGuid(sel_guid), (p->GetSelectedUnit()) ? p->GetSelectedUnit()->GetName() : "", GUID_LOPART(sel_guid));
                }
            }
        }
        // some commands have custom error messages. Don't send the default one in these cases.
        else if (!HasSentErrorMessage())
        {
            if (!table[i].Help.empty())
                SendSysMessage(table[i].Help.c_str());
            else
                SendSysMessage(LANG_CMD_SYNTAX);
        }

        return true;
    }

    return false;
}

bool ChatHandler::SetDataForCommandInTable(ChatCommand* table, const char* text, uint32 security, std::string const& help, std::string const& fullcommand)
{
    std::string cmd = "";

    while (*text != ' ' && *text != '\0')
    {
        cmd += *text;
        ++text;
    }

    while (*text == ' ') ++text;

    for (uint32 i = 0; table[i].Name != NULL; i++)
    {
        // for data fill use full explicit command names
        if (table[i].Name != cmd)
            continue;

        // select subcommand from child commands list (including "")
        if (table[i].ChildCommands != NULL)
        {
            if (SetDataForCommandInTable(table[i].ChildCommands, text, security, help, fullcommand))
                return true;
            else if (*text)
                return false;

            // fail with "" subcommands, then use normal level up command instead
        }
        // expected subcommand by full name DB content
        else if (*text)
        {
            sLog->outErrorDb("Table `command` have unexpected subcommand '%s' in command '%s', skip.", text, fullcommand.c_str());
            return false;
        }

        if (table[i].SecurityLevel != security)
            sLog->outDetail("Table `command` overwrite for command '%s' default security (%u) by %u", fullcommand.c_str(), table[i].SecurityLevel, security);

        table[i].SecurityLevel = security;
        table[i].Help          = help;
        return true;
    }

    // in case "" command let process by caller
    if (!cmd.empty())
    {
        if (table == getCommandTable())
            sLog->outErrorDb("Table `command` have not existed command '%s', skip.", cmd.c_str());
        else
            sLog->outErrorDb("Table `command` have not existed subcommand '%s' in command '%s', skip.", cmd.c_str(), fullcommand.c_str());
    }

    return false;
}

bool ChatHandler::ParseCommands(const char* text)
{
    ASSERT(text);
    ASSERT(*text);

    std::string fullcmd = text;

    if (m_session && AccountMgr::IsPlayerAccount(m_session->GetSecurity()) && !sWorld->getBoolConfig(CONFIG_ALLOW_PLAYER_COMMANDS))
       return false;

    /// chat case (.command or !command format)
    if (m_session)
    {
        if (text[0] != '!' && text[0] != '.')
            return false;
    }

    /// ignore single . and ! in line
    if (strlen(text) < 2)
        return false;
    // original `text` can't be used. It content destroyed in command code processing.

    /// ignore messages staring from many dots.
    if ((text[0] == '.' && text[1] == '.') || (text[0] == '!' && text[1] == '!'))
        return false;

    /// skip first . or ! (in console allowed use command with . and ! and without its)
    if (text[0] == '!' || text[0] == '.')
        ++text;

    if (!ExecuteCommandInTable(getCommandTable(), text, fullcmd))
    {
        if (m_session && AccountMgr::IsPlayerAccount(m_session->GetSecurity()))
            return false;

        SendSysMessage(LANG_NO_CMD);
    }
    return true;
}

bool ChatHandler::isValidChatMessage(const char* message)
{
/*
Valid examples:
|cffa335ee|Hitem:812:0:0:0:0:0:0:0:70|h[Glowing Brightwood Staff]|h|r
|cff808080|Hquest:2278:47|h[The Platinum Discs]|h|r
|cffffd000|Htrade:4037:1:150:1:6AAAAAAAAAAAAAAAAAAAAAAOAADAAAAAAAAAAAAAAAAIAAAAAAAAA|h[Engineering]|h|r
|cff4e96f7|Htalent:2232:-1|h[Taste for Blood]|h|r
|cff71d5ff|Hspell:21563|h[Command]|h|r
|cffffd000|Henchant:3919|h[Engineering: Rough Dynamite]|h|r
|cffffff00|Hachievement:546:0000000000000001:0:0:0:-1:0:0:0:0|h[Safe Deposit]|h|r
|cff66bbff|Hglyph:21:762|h[Glyph of Bladestorm]|h|r

| will be escaped to ||
*/

    if (strlen(message) > 255)
        return false;

    // more simple checks
    if (sWorld->getIntConfig(CONFIG_CHAT_STRICT_LINK_CHECKING_SEVERITY) < 3)
    {
        const char validSequence[6] = "cHhhr";
        const char* validSequenceIterator = validSequence;
        const std::string validCommands = "cHhr|";

        while (*message)
        {
            // find next pipe command
            message = strchr(message, '|');

            if (!message)
                return true;

            ++message;
            char commandChar = *message;
            if (validCommands.find(commandChar) == std::string::npos)
                return false;

            ++message;
            // validate sequence
            if (sWorld->getIntConfig(CONFIG_CHAT_STRICT_LINK_CHECKING_SEVERITY) == 2)
            {
                if (commandChar == *validSequenceIterator)
                {
                    if (validSequenceIterator == validSequence + 4)
                        validSequenceIterator = validSequence;
                    else
                        ++validSequenceIterator;
                }
                else
                    return false;
            }
        }
        return true;
    }

    return LinkExtractor(message).IsValidMessage();
}

bool ChatHandler::ShowHelpForSubCommands(ChatCommand* table, char const* cmd, char const* subcmd)
{
    std::string list;
    for (uint32 i = 0; table[i].Name != NULL; ++i)
    {
        // must be available (ignore handler existence for show command with possible available subcommands)
        if (!isAvailable(table[i]))
            continue;

        // for empty subcmd show all available
        if (*subcmd && !hasStringAbbr(table[i].Name, subcmd))
            continue;

        if (m_session)
            list += "\n    ";
        else
            list += "\n\r    ";

        list += table[i].Name;

        if (table[i].ChildCommands)
            list += " ...";
    }

    if (list.empty())
        return false;

    if (table == getCommandTable())
    {
        SendSysMessage(LANG_AVIABLE_CMD);
        PSendSysMessage("%s", list.c_str());
    }
    else
        PSendSysMessage(LANG_SUBCMDS_LIST, cmd, list.c_str());

    return true;
}

bool ChatHandler::ShowHelpForCommand(ChatCommand* table, const char* cmd)
{
    if (*cmd)
    {
        for (uint32 i = 0; table[i].Name != NULL; ++i)
        {
            // must be available (ignore handler existence for show command with possible available subcommands)
            if (!isAvailable(table[i]))
                continue;

            if (!hasStringAbbr(table[i].Name, cmd))
                continue;

            // have subcommand
            char const* subcmd = (*cmd) ? strtok(NULL, " ") : "";

            if (table[i].ChildCommands && subcmd && *subcmd)
            {
                if (ShowHelpForCommand(table[i].ChildCommands, subcmd))
                    return true;
            }

            if (!table[i].Help.empty())
                SendSysMessage(table[i].Help.c_str());

            if (table[i].ChildCommands)
                if (ShowHelpForSubCommands(table[i].ChildCommands, table[i].Name, subcmd ? subcmd : ""))
                    return true;

            return !table[i].Help.empty();
        }
    }
    else
    {
        for (uint32 i = 0; table[i].Name != NULL; ++i)
        {
            // must be available (ignore handler existence for show command with possible available subcommands)
            if (!isAvailable(table[i]))
                continue;

            if (strlen(table[i].Name))
                continue;

            if (!table[i].Help.empty())
                SendSysMessage(table[i].Help.c_str());

            if (table[i].ChildCommands)
                if (ShowHelpForSubCommands(table[i].ChildCommands, "", ""))
                    return true;

            return !table[i].Help.empty();
        }
    }

    return ShowHelpForSubCommands(table, "", cmd);
}

//Note: target_guid used only in CHAT_MSG_WHISPER_INFORM mode (in this case channelName ignored)
void ChatHandler::FillMessageData(WorldPacket* data, WorldSession* session, uint8 type, uint32 language, const char *channelName, uint64 target_guid, const char *message, Unit* speaker)
{
    uint32 messageLength = (message ? strlen(message) : 0) + 1;

    data->Initialize(SMSG_MESSAGECHAT, 100);                // guess size
    *data << uint8(type);
    if ((type != CHAT_MSG_CHANNEL && type != CHAT_MSG_WHISPER) || language == LANG_ADDON)
        *data << uint32(language);
    else
        *data << uint32(LANG_UNIVERSAL);

    switch (type)
    {
        case CHAT_MSG_SAY:
        case CHAT_MSG_PARTY:
        case CHAT_MSG_PARTY_LEADER:
        case CHAT_MSG_RAID:
        case CHAT_MSG_GUILD:
        case CHAT_MSG_OFFICER:
        case CHAT_MSG_YELL:
        case CHAT_MSG_WHISPER:
        case CHAT_MSG_CHANNEL:
        case CHAT_MSG_RAID_LEADER:
        case CHAT_MSG_RAID_WARNING:
        case CHAT_MSG_BG_SYSTEM_NEUTRAL:
        case CHAT_MSG_BG_SYSTEM_ALLIANCE:
        case CHAT_MSG_BG_SYSTEM_HORDE:
        case CHAT_MSG_BATTLEGROUND:
        case CHAT_MSG_BATTLEGROUND_LEADER:
            target_guid = session ? session->GetPlayer()->GetGUID() : 0;
            break;
        case CHAT_MSG_MONSTER_SAY:
        case CHAT_MSG_MONSTER_PARTY:
        case CHAT_MSG_MONSTER_YELL:
        case CHAT_MSG_MONSTER_WHISPER:
        case CHAT_MSG_MONSTER_EMOTE:
        case CHAT_MSG_RAID_BOSS_WHISPER:
        case CHAT_MSG_RAID_BOSS_EMOTE:
        case CHAT_MSG_BATTLENET:
        {
            *data << uint64(speaker->GetGUID());
            *data << uint32(0);                             // 2.1.0
            *data << uint32(strlen(speaker->GetName()) + 1);
            *data << speaker->GetName();
            uint64 listener_guid = 0;
            *data << uint64(listener_guid);
            if (listener_guid && !IS_PLAYER_GUID(listener_guid))
            {
                *data << uint32(1);                         // string listener_name_length
                *data << uint8(0);                          // string listener_name
            }
            *data << uint32(messageLength);
            *data << message;
            *data << uint8(0);
            return;
        }
        default:
            if (type != CHAT_MSG_WHISPER_INFORM && type != CHAT_MSG_IGNORED && type != CHAT_MSG_DND && type != CHAT_MSG_AFK)
                target_guid = 0;                            // only for CHAT_MSG_WHISPER_INFORM used original value target_guid
            break;
    }

    *data << uint64(target_guid);                           // there 0 for BG messages
    *data << uint32(0);                                     // can be chat msg group or something

    if (type == CHAT_MSG_CHANNEL)
    {
        ASSERT(channelName);
        *data << channelName;
    }

    *data << uint64(target_guid);
    *data << uint32(messageLength);
    *data << message;
    if (session != 0 && type != CHAT_MSG_WHISPER_INFORM && type != CHAT_MSG_DND && type != CHAT_MSG_AFK)
        *data << uint8(session->GetPlayer()->GetChatTag());
    else
        *data << uint8(0);
}

Player* ChatHandler::getSelectedPlayer()
{
    if (!m_session)
        return NULL;

    if (!m_session->GetPlayer())
        return NULL;

    uint64 guid  = m_session->GetPlayer()->GetSelection();

    if (guid == 0)
        return m_session->GetPlayer();

    return ObjectAccessor::FindPlayer(guid);
}

Unit* ChatHandler::getSelectedUnit()
{
    if (!m_session)
        return NULL;

    if (!m_session->GetPlayer())
        return NULL;

    uint64 guid = m_session->GetPlayer()->GetSelection();

    if (guid == 0)
        return m_session->GetPlayer();

    return ObjectAccessor::GetUnit(*m_session->GetPlayer(), guid);
}

WorldObject* ChatHandler::getSelectedObject()
{
    if (!m_session)
        return NULL;

    if (!m_session->GetPlayer())
        return NULL;

    uint64 guid = m_session->GetPlayer()->GetSelection();

    if (guid == 0)
        return GetNearbyGameObject();

    return ObjectAccessor::GetUnit(*m_session->GetPlayer(), guid);
}

Creature* ChatHandler::getSelectedCreature()
{
    if (!m_session)
        return NULL;

    if (!m_session->GetPlayer())
        return NULL;

    return ObjectAccessor::GetCreatureOrPetOrVehicle(*m_session->GetPlayer(), m_session->GetPlayer()->GetSelection());
}

char* ChatHandler::extractKeyFromLink(char* text, char const* linkType, char** something1)
{
    // skip empty
    if (!text)
        return NULL;

    // skip spaces
    while (*text == ' '||*text == '\t'||*text == '\b')
        ++text;

    if (!*text)
        return NULL;

    // return non link case
    if (text[0] != '|')
        return strtok(text, " ");

    // [name] Shift-click form |color|linkType:key|h[name]|h|r
    // or
    // [name] Shift-click form |color|linkType:key:something1:...:somethingN|h[name]|h|r

    char* check = strtok(text, "|");                        // skip color
    if (!check)
        return NULL;                                        // end of data

    char* cLinkType = strtok(NULL, ":");                    // linktype
    if (!cLinkType)
        return NULL;                                        // end of data

    if (strcmp(cLinkType, linkType) != 0)
    {
        strtok(NULL, " ");                                  // skip link tail (to allow continue strtok(NULL, s) use after retturn from function
        SendSysMessage(LANG_WRONG_LINK_TYPE);
        return NULL;
    }

    char* cKeys = strtok(NULL, "|");                        // extract keys and values
    char* cKeysTail = strtok(NULL, "");

    char* cKey = strtok(cKeys, ":|");                       // extract key
    if (something1)
        *something1 = strtok(NULL, ":|");                   // extract something

    strtok(cKeysTail, "]");                                 // restart scan tail and skip name with possible spaces
    strtok(NULL, " ");                                      // skip link tail (to allow continue strtok(NULL, s) use after return from function
    return cKey;
}

char* ChatHandler::extractKeyFromLink(char* text, char const* const* linkTypes, int* found_idx, char** something1)
{
    // skip empty
    if (!text)
        return NULL;

    // skip spaces
    while (*text == ' '||*text == '\t'||*text == '\b')
        ++text;

    if (!*text)
        return NULL;

    // return non link case
    if (text[0] != '|')
        return strtok(text, " ");

    // [name] Shift-click form |color|linkType:key|h[name]|h|r
    // or
    // [name] Shift-click form |color|linkType:key:something1:...:somethingN|h[name]|h|r
    // or
    // [name] Shift-click form |linkType:key|h[name]|h|r

    char* tail;

    if (text[1] == 'c')
    {
        char* check = strtok(text, "|");                    // skip color
        if (!check)
            return NULL;                                    // end of data

        tail = strtok(NULL, "");                            // tail
    }
    else
        tail = text+1;                                      // skip first |

    char* cLinkType = strtok(tail, ":");                    // linktype
    if (!cLinkType)
        return NULL;                                        // end of data

    for (int i = 0; linkTypes[i]; ++i)
    {
        if (strcmp(cLinkType, linkTypes[i]) == 0)
        {
            char* cKeys = strtok(NULL, "|");                // extract keys and values
            char* cKeysTail = strtok(NULL, "");

            char* cKey = strtok(cKeys, ":|");               // extract key
            if (something1)
                *something1 = strtok(NULL, ":|");           // extract something

            strtok(cKeysTail, "]");                         // restart scan tail and skip name with possible spaces
            strtok(NULL, " ");                              // skip link tail (to allow continue strtok(NULL, s) use after return from function
            if (found_idx)
                *found_idx = i;
            return cKey;
        }
    }

    strtok(NULL, " ");                                      // skip link tail (to allow continue strtok(NULL, s) use after return from function
    SendSysMessage(LANG_WRONG_LINK_TYPE);
    return NULL;
}

GameObject* ChatHandler::GetNearbyGameObject()
{
    if (!m_session)
        return NULL;

    Player* pl = m_session->GetPlayer();
    GameObject* obj = NULL;
    Axium::NearestGameObjectCheck check(*pl);
    Axium::GameObjectLastSearcher<Axium::NearestGameObjectCheck> searcher(pl, obj, check);
    pl->VisitNearbyGridObject(SIZE_OF_GRIDS, searcher);
    return obj;
}

GameObject* ChatHandler::GetObjectGlobalyWithGuidOrNearWithDbGuid(uint32 lowguid, uint32 entry)
{
    if (!m_session)
        return NULL;

    Player* pl = m_session->GetPlayer();

    GameObject* obj = pl->GetMap()->GetGameObject(MAKE_NEW_GUID(lowguid, entry, HIGHGUID_GAMEOBJECT));

    if (!obj && sObjectMgr->GetGOData(lowguid))                   // guid is DB guid of object
    {
        // search near player then
        CellCoord p(Axium::ComputeCellCoord(pl->GetPositionX(), pl->GetPositionY()));
        Cell cell(p);

        Axium::GameObjectWithDbGUIDCheck go_check(*pl, lowguid);
        Axium::GameObjectSearcher<Axium::GameObjectWithDbGUIDCheck> checker(pl, obj, go_check);

        TypeContainerVisitor<Axium::GameObjectSearcher<Axium::GameObjectWithDbGUIDCheck>, GridTypeMapContainer > object_checker(checker);
        cell.Visit(p, object_checker, *pl->GetMap(), *pl, pl->GetGridActivationRange());
    }

    return obj;
}

enum SpellLinkType
{
    SPELL_LINK_SPELL   = 0,
    SPELL_LINK_TALENT  = 1,
    SPELL_LINK_ENCHANT = 2,
    SPELL_LINK_TRADE   = 3,
    SPELL_LINK_GLYPH   = 4
};

static char const* const spellKeys[] =
{
    "Hspell",                                               // normal spell
    "Htalent",                                              // talent spell
    "Henchant",                                             // enchanting recipe spell
    "Htrade",                                               // profession/skill spell
    "Hglyph",                                               // glyph
    0
};

uint32 ChatHandler::extractSpellIdFromLink(char* text)
{
    // number or [name] Shift-click form |color|Henchant:recipe_spell_id|h[prof_name: recipe_name]|h|r
    // number or [name] Shift-click form |color|Hglyph:glyph_slot_id:glyph_prop_id|h[%s]|h|r
    // number or [name] Shift-click form |color|Hspell:spell_id|h[name]|h|r
    // number or [name] Shift-click form |color|Htalent:talent_id, rank|h[name]|h|r
    // number or [name] Shift-click form |color|Htrade:spell_id, skill_id, max_value, cur_value|h[name]|h|r
    int type = 0;
    char* param1_str = NULL;
    char* idS = extractKeyFromLink(text, spellKeys, &type, &param1_str);
    if (!idS)
        return 0;

    uint32 id = (uint32)atol(idS);

    switch (type)
    {
        case SPELL_LINK_SPELL:
            return id;
        case SPELL_LINK_TALENT:
        {
            // talent
            TalentEntry const* talentEntry = sTalentStore.LookupEntry(id);
            if (!talentEntry)
                return 0;

            int32 rank = param1_str ? (uint32)atol(param1_str) : 0;
            if (rank >= MAX_TALENT_RANK)
                return 0;

            if (rank < 0)
                rank = 0;

            return talentEntry->RankID[rank];
        }
        case SPELL_LINK_ENCHANT:
        case SPELL_LINK_TRADE:
            return id;
        case SPELL_LINK_GLYPH:
        {
            uint32 glyph_prop_id = param1_str ? (uint32)atol(param1_str) : 0;

            GlyphPropertiesEntry const* glyphPropEntry = sGlyphPropertiesStore.LookupEntry(glyph_prop_id);
            if (!glyphPropEntry)
                return 0;

            return glyphPropEntry->SpellId;
        }
    }

    // unknown type?
    return 0;
}

uint32 ChatHandler::extractItemIdFromLink(char* text)
{
    uint32 itemId = 0;
    if (text[0] == '[') // [name] manual form
    {
        char* citemName = strtok((char*)text, "]");

        if (citemName && citemName[0])
        {
            std::string itemName = citemName + 1;
            WorldDatabase.EscapeString(itemName);
            QueryResult result = WorldDatabase.PQuery("SELECT entry FROM item_template WHERE name = '%s'", itemName.c_str());
            if (!result)
            {
                PSendSysMessage(LANG_COMMAND_COULDNOTFIND, citemName + 1);
                SetSentErrorMessage(true);
                return false;
            }
            itemId = result->Fetch()->GetUInt16();
        }
        else
            return false;
    }
    else // item_id or [name] Shift-click form |color|Hitem:item_id:0:0:0|h[name]|h|r
    {
        char* cId = extractKeyFromLink((char*)text, "Hitem");
        if (!cId)
            return false;

        itemId = atol(cId);
    }

    return itemId;
}

GameTele const* ChatHandler::extractGameTeleFromLink(char* text)
{
    // id, or string, or [name] Shift-click form |color|Htele:id|h[name]|h|r
    char* cId = extractKeyFromLink(text, "Htele");
    if (!cId)
        return NULL;

    // id case (explicit or from shift link)
    if (cId[0] >= '0' || cId[0] >= '9')
        if (uint32 id = atoi(cId))
            return sObjectMgr->GetGameTele(id);

    return sObjectMgr->GetGameTele(cId);
}

enum GuidLinkType
{
    SPELL_LINK_PLAYER     = 0,                              // must be first for selection in not link case
    SPELL_LINK_CREATURE   = 1,
    SPELL_LINK_GAMEOBJECT = 2
};

static char const* const guidKeys[] =
{
    "Hplayer",
    "Hcreature",
    "Hgameobject",
    0
};

uint64 ChatHandler::extractGuidFromLink(char* text)
{
    int type = 0;

    // |color|Hcreature:creature_guid|h[name]|h|r
    // |color|Hgameobject:go_guid|h[name]|h|r
    // |color|Hplayer:name|h[name]|h|r
    char* idS = extractKeyFromLink(text, guidKeys, &type);
    if (!idS)
        return 0;

    switch (type)
    {
        case SPELL_LINK_PLAYER:
        {
            std::string name = idS;
            if (!normalizePlayerName(name))
                return 0;

            if (Player* player = sObjectAccessor->FindPlayerByName(name.c_str()))
                return player->GetGUID();

            if (uint64 guid = sObjectMgr->GetPlayerGUIDByName(name))
                return guid;

            return 0;
        }
        case SPELL_LINK_CREATURE:
        {
            uint32 lowguid = (uint32)atol(idS);

            if (CreatureData const* data = sObjectMgr->GetCreatureData(lowguid))
                return MAKE_NEW_GUID(lowguid, data->id, HIGHGUID_UNIT);
            else
                return 0;
        }
        case SPELL_LINK_GAMEOBJECT:
        {
            uint32 lowguid = (uint32)atol(idS);

            if (GameObjectData const* data = sObjectMgr->GetGOData(lowguid))
                return MAKE_NEW_GUID(lowguid, data->id, HIGHGUID_GAMEOBJECT);
            else
                return 0;
        }
    }

    // unknown type?
    return 0;
}

std::string ChatHandler::extractPlayerNameFromLink(char* text)
{
    // |color|Hplayer:name|h[name]|h|r
    char* name_str = extractKeyFromLink(text, "Hplayer");
    if (!name_str)
        return "";

    std::string name = name_str;
    if (!normalizePlayerName(name))
        return "";

    return name;
}

bool ChatHandler::extractPlayerTarget(char* args, Player** player, uint64* player_guid /*=NULL*/, std::string* player_name /*= NULL*/)
{
    if (args && *args)
    {
        std::string name = extractPlayerNameFromLink(args);
        if (name.empty())
        {
            SendSysMessage(LANG_PLAYER_NOT_FOUND);
            SetSentErrorMessage(true);
            return false;
        }

        Player* pl = sObjectAccessor->FindPlayerByName(name.c_str());

        // if allowed player pointer
        if (player)
            *player = pl;

        // if need guid value from DB (in name case for check player existence)
        uint64 guid = !pl && (player_guid || player_name) ? sObjectMgr->GetPlayerGUIDByName(name) : 0;

        // if allowed player guid (if no then only online players allowed)
        if (player_guid)
            *player_guid = pl ? pl->GetGUID() : guid;

        if (player_name)
            *player_name = pl || guid ? name : "";
    }
    else
    {
        Player* pl = getSelectedPlayer();
        // if allowed player pointer
        if (player)
            *player = pl;
        // if allowed player guid (if no then only online players allowed)
        if (player_guid)
            *player_guid = pl ? pl->GetGUID() : 0;

        if (player_name)
            *player_name = pl ? pl->GetName() : "";
    }

    // some from req. data must be provided (note: name is empty if player not exist)
    if ((!player || !*player) && (!player_guid || !*player_guid) && (!player_name || player_name->empty()))
    {
        SendSysMessage(LANG_PLAYER_NOT_FOUND);
        SetSentErrorMessage(true);
        return false;
    }

    return true;
}

void ChatHandler::extractOptFirstArg(char* args, char** arg1, char** arg2)
{
    char* p1 = strtok(args, " ");
    char* p2 = strtok(NULL, " ");

    if (!p2)
    {
        p2 = p1;
        p1 = NULL;
    }

    if (arg1)
        *arg1 = p1;

    if (arg2)
        *arg2 = p2;
}

char* ChatHandler::extractQuotedArg(char* args)
{
    if (!*args)
        return NULL;

    if (*args == '"')
        return strtok(args+1, "\"");
    else
    {
        char* space = strtok(args, "\"");
        if (!space)
            return NULL;
        return strtok(NULL, "\"");
    }
}

bool ChatHandler::needReportToTarget(Player* chr) const
{
    Player* pl = m_session->GetPlayer();
    return pl != chr && pl->IsVisibleGloballyFor(chr);
}

LocaleConstant ChatHandler::GetSessionDbcLocale() const
{
    return m_session->GetSessionDbcLocale();
}

int ChatHandler::GetSessionDbLocaleIndex() const
{
    return m_session->GetSessionDbLocaleIndex();
}

const char *CliHandler::GetAxiumString(int32 entry) const
{
    return sObjectMgr->GetAxiumStringForDBCLocale(entry);
}

bool CliHandler::isAvailable(ChatCommand const& cmd) const
{
    // skip non-console commands in console case
    return cmd.AllowConsole;
}

void CliHandler::SendSysMessage(const char *str)
{
    m_print(m_callbackArg, str);
    m_print(m_callbackArg, "\r\n");
}

std::string CliHandler::GetNameLink() const
{
    return GetAxiumString(LANG_CONSOLE_COMMAND);
}

bool CliHandler::needReportToTarget(Player* /*chr*/) const
{
    return true;
}

bool ChatHandler::GetPlayerGroupAndGUIDByName(const char* cname, Player* &player, Group* &group, uint64 &guid, bool offline)
{
    player  = NULL;
    guid = 0;

    if (cname)
    {
        std::string name = cname;
        if (!name.empty())
        {
            if (!normalizePlayerName(name))
            {
                PSendSysMessage(LANG_PLAYER_NOT_FOUND);
                SetSentErrorMessage(true);
                return false;
            }

            player = sObjectAccessor->FindPlayerByName(name.c_str());
            if (offline)
                guid = sObjectMgr->GetPlayerGUIDByName(name.c_str());
        }
    }

    if (player)
    {
        group = player->GetGroup();
        if (!guid || !offline)
            guid = player->GetGUID();
    }
    else
    {
        if (getSelectedPlayer())
            player = getSelectedPlayer();
        else
            player = m_session->GetPlayer();

        if (!guid || !offline)
            guid  = player->GetGUID();
        group = player->GetGroup();
    }

    return true;
}

LocaleConstant CliHandler::GetSessionDbcLocale() const
{
    return sWorld->GetDefaultDbcLocale();
}

int CliHandler::GetSessionDbLocaleIndex() const
{
    return sObjectMgr->GetDBCLocaleIndex();
}
