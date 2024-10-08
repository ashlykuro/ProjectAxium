#include "Language.h"
#include "WorldPacket.h"
#include "Common.h"
#include "ObjectMgr.h"
#include "TicketMgr.h"
#include "Player.h"
#include "World.h"
#include "WorldSession.h"
#include "Util.h"
#include "Chat.h"

void WorldSession::HandleGMTicketCreateOpcode(WorldPacket & recv_data)
{
    // Don't accept tickets if the ticket queue is disabled. (Ticket UI is greyed out but not fully dependable)
    if (sTicketMgr->GetStatus() == GMTICKET_QUEUE_STATUS_DISABLED)
        return;

    if (_player->getLevel() < sWorld->getIntConfig(CONFIG_TICKET_LEVEL_REQ))
    {
        SendNotification(GetAxiumString(LANG_TICKET_REQ), sWorld->getIntConfig(CONFIG_TICKET_LEVEL_REQ));
        return;
    }

    GMTicketResponse response = GMTICKET_RESPONSE_FAILURE;
    GmTicket* ticket = sTicketMgr->GetTicketByPlayer(_player->GetGUID());

    if (ticket && ticket->IsCompleted())
        sTicketMgr->CloseTicket(ticket->GetId(), _player->GetGUID());

    // Player must not have ticket
    if (!ticket || ticket->IsClosed())
    {
        ticket = new GmTicket(_player, recv_data);
        sTicketMgr->AddTicket(ticket);
        sTicketMgr->UpdateLastChange();

        ChatHandler handler = ChatHandler(_player);
        std::string nameLink = handler.playerLink(_player->GetName());
        handler.PSendGlobalGMSysMessage("%s: |cff00ff00Created|r |cffaaffaaTicket|r|cffaaccff(%d)|r", nameLink.c_str(), ticket->GetId());
        response = GMTICKET_RESPONSE_SUCCESS;
    }

    WorldPacket data(SMSG_GMTICKET_CREATE, 4);
    data << uint32(response);
    SendPacket(&data);
}

void WorldSession::HandleGMTicketUpdateOpcode(WorldPacket & recv_data)
{
    std::string message;
    recv_data >> message;

    GMTicketResponse response = GMTICKET_RESPONSE_FAILURE;
    if (GmTicket* ticket = sTicketMgr->GetTicketByPlayer(_player->GetGUID()))
    {
        SQLTransaction trans = SQLTransaction(NULL);
        ticket->SetMessage(message);
        ticket->SaveToDB(trans);

        ChatHandler handler = ChatHandler(_player);
        std::string nameLink = handler.playerLink(_player->GetName());
        handler.PSendGlobalGMSysMessage("%s: |cff00ff00Edited|r |cffaaffaaTicket|r|cffaaccff(%d)|r", nameLink.c_str(), ticket->GetId());
        response = GMTICKET_RESPONSE_SUCCESS;
    }

    WorldPacket data(SMSG_GMTICKET_UPDATETEXT, 4);
    data << uint32(response);
    SendPacket(&data);
}

void WorldSession::HandleGMTicketDeleteOpcode(WorldPacket & /*recv_data*/)
{
    if (GmTicket* ticket = sTicketMgr->GetTicketByPlayer(_player->GetGUID()))
    {
        WorldPacket data(SMSG_GMTICKET_DELETETICKET, 4);
        data << uint32(GMTICKET_RESPONSE_TICKET_DELETED);
        SendPacket(&data);

        ChatHandler handler = ChatHandler(_player);
        std::string nameLink = handler.playerLink(_player->GetName());
        handler.PSendGlobalGMSysMessage("%s: |cff00ff00Abandoned|r |cffaaffaaTicket|r|cffaaccff(%d)|r", nameLink.c_str(), ticket->GetId());
        sTicketMgr->CloseTicket(ticket->GetId(), _player->GetGUID());
        sTicketMgr->SendTicket(this, NULL);
    }
}

void WorldSession::HandleGMTicketGetTicketOpcode(WorldPacket & /*recv_data*/)
{
    SendQueryTimeResponse();

    if (GmTicket* ticket = sTicketMgr->GetTicketByPlayer(GetPlayer()->GetGUID()))
    {
        if (ticket->IsCompleted())
            ticket->SendResponse(this);
        else
            sTicketMgr->SendTicket(this, ticket);
    }
    else
        sTicketMgr->SendTicket(this, NULL);
}

void WorldSession::HandleGMTicketSystemStatusOpcode(WorldPacket & /*recv_data*/)
{
    // Note: This only disables the ticket UI at client side and is not fully reliable
    // are we sure this is a uint32? Should ask Zor
    WorldPacket data(SMSG_GMTICKET_SYSTEMSTATUS, 4);
    data << uint32(sTicketMgr->GetStatus() ? GMTICKET_QUEUE_STATUS_ENABLED : GMTICKET_QUEUE_STATUS_DISABLED);
    SendPacket(&data);
}

void WorldSession::HandleGMSurveySubmit(WorldPacket& recv_data)
{
    uint32 nextSurveyID = sTicketMgr->GetNextSurveyID();
    // just put the survey into the database
    uint32 mainSurvey; // GMSurveyCurrentSurvey.dbc, column 1 (all 9) ref to GMSurveySurveys.dbc
    recv_data >> mainSurvey;

    // sub_survey1, r1, comment1, sub_survey2, r2, comment2, sub_survey3, r3, comment3, sub_survey4, r4, comment4, sub_survey5, r5, comment5, sub_survey6, r6, comment6, sub_survey7, r7, comment7, sub_survey8, r8, comment8, sub_survey9, r9, comment9, sub_survey10, r10, comment10,
    for (uint8 i = 0; i < 10; i++)
    {
        uint32 subSurveyId; // ref to i'th GMSurveySurveys.dbc field (all fields in that dbc point to fields in GMSurveyQuestions.dbc)
        recv_data >> subSurveyId;
        if (!subSurveyId)
            break;

        uint8 rank; // probably some sort of ref to GMSurveyAnswers.dbc
        recv_data >> rank;
        std::string comment; // comment ("Usage: GMSurveyAnswerSubmit(question, rank, comment)")
        recv_data >> comment;

        PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_GM_SUBSURVEY);
        stmt->setUInt32(0, nextSurveyID);
        stmt->setUInt32(1, subSurveyId);
        stmt->setUInt32(2, rank);
        stmt->setString(3, comment);
        CharacterDatabase.Execute(stmt);
    }

    std::string comment; // just a guess
    recv_data >> comment;

    PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_GM_SURVEY);
    stmt->setUInt32(0, GUID_LOPART(GetPlayer()->GetGUID()));
    stmt->setUInt32(1, nextSurveyID);
    stmt->setUInt32(2, mainSurvey);
    stmt->setString(3, comment);

    CharacterDatabase.Execute(stmt);
}

void WorldSession::HandleReportLag(WorldPacket& recv_data)
{
    // just put the lag report into the database...
    // can't think of anything else to do with it
    uint32 lagType, mapId;
    recv_data >> lagType;
    recv_data >> mapId;
    float x, y, z;
    recv_data >> x;
    recv_data >> y;
    recv_data >> z;
}

void WorldSession::HandleGMResponseResolve(WorldPacket& /*recvPacket*/)
{
    // empty packet
    if (GmTicket* ticket = sTicketMgr->GetTicketByPlayer(GetPlayer()->GetGUID()))
    {
        uint8 getSurvey = 0;
        if (float(rand_chance()) < sWorld->getFloatConfig(CONFIG_CHANCE_OF_GM_SURVEY))
            getSurvey = 1;

        WorldPacket data(SMSG_GMRESPONSE_STATUS_UPDATE, 4);
        data << uint8(getSurvey);
        SendPacket(&data);

        WorldPacket data2(SMSG_GMTICKET_DELETETICKET, 4);
        data2 << uint32(GMTICKET_RESPONSE_TICKET_DELETED);
        SendPacket(&data2);

        sTicketMgr->CloseTicket(ticket->GetId(), GetPlayer()->GetGUID());
        sTicketMgr->SendTicket(this, NULL);
    }
}
