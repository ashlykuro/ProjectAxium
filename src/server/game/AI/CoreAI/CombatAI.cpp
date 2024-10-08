#include "CombatAI.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Vehicle.h"
#include "ObjectAccessor.h"

int AggressorAI::Permissible(const Creature* creature)
{
    // have some hostile factions, it will be selected by IsHostileTo check at MoveInLineOfSight
    if (!creature->isCivilian() && !creature->IsNeutralToAll())
        return PERMIT_BASE_PROACTIVE;

    return PERMIT_BASE_NO;
}

void AggressorAI::UpdateAI(const uint32 /*diff*/)
{
    if (!UpdateVictim())
        return;

    DoMeleeAttackIfReady();
}

// some day we will delete these useless things
int CombatAI::Permissible(const Creature* /*creature*/)
{
    return PERMIT_BASE_NO;
}

int ArcherAI::Permissible(const Creature* /*creature*/)
{
    return PERMIT_BASE_NO;
}

int TurretAI::Permissible(const Creature* /*creature*/)
{
    return PERMIT_BASE_NO;
}

int VehicleAI::Permissible(const Creature* /*creature*/)
{
    return PERMIT_BASE_NO;
}

void CombatAI::InitializeAI()
{
    for (uint8 i = 0; i < CREATURE_MAX_SPELLS; ++i)
        if (me->m_spells[i] && sSpellMgr->GetSpellInfo(me->m_spells[i]))
            spells.push_back(me->m_spells[i]);

    CreatureAI::InitializeAI();

    me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PC);
}

void CombatAI::Reset()
{
    events.Reset();
}

void CombatAI::EnterCombat(Unit* who)
{
    for (SpellVct::iterator i = spells.begin(); i != spells.end(); ++i)
    {
        if (AISpellInfo[*i].condition == AICOND_AGGRO)
            me->CastSpell(who, *i, false);
        else if (AISpellInfo[*i].condition == AICOND_COMBAT)
            events.ScheduleEvent(*i, AISpellInfo[*i].cooldown + rand()%AISpellInfo[*i].cooldown);
    }
}

void CombatAI::SetTarget(Unit* newTarget)
{
    Unit* owner = me->GetCharmerOrOwner();
    Unit* target = me->getVictim();

    if (newTarget != me && newTarget != owner && !me->IsFriendlyTo(newTarget))
        target = newTarget;

    if (!target)
        return;

    for (SpellVct::iterator itr = spells.begin(); itr != spells.end(); ++itr)
    {
        if (AISpellInfo[*itr].condition == AICOND_AGGRO)
            me->CastSpell(target, *itr, false);
        else if (AISpellInfo[*itr].condition == AICOND_COMBAT)
            events.ScheduleEvent(*itr, AISpellInfo[*itr].cooldown + rand() % AISpellInfo[*itr].cooldown);
    }

    AttackStart(target);
}

void CombatAI::JustDied(Unit* killer)
{
    for (SpellVct::iterator itr = spells.begin(); itr != spells.end(); ++itr)
        if (AISpellInfo[*itr].condition == AICOND_DIE)
            me->CastSpell(killer, *itr, true);
}

void CombatAI::UpdateAI(const uint32 diff)
{
    events.Update(diff);

    Unit* target = me->getVictim();
    if (!target)
    {
        me->CombatStop();
        me->CastStop();

        if (me->IsAIEnabled)
            me->AI()->HandleReturnMovement();

        return;
    }

    if (target->HasBreakableCrowdControlAura(me))
    {
        me->CombatStop();
        me->CastStop();

        if (me->IsAIEnabled)
            me->AI()->HandleReturnMovement();

        return;
    }

    if (!me->IsWithinLOSInMap(target))
    {
        me->CastStop();

        if (!me->HasUnitState(UNIT_STATE_CHASE))
        {
            me->GetMotionMaster()->Clear();
            me->GetMotionMaster()->MoveChase(target);
        }

        return;
    }

    Unit* owner = me->GetCharmerOrOwner();
    if (owner && !owner->canSeeOrDetect(target))
        me->CastStop();

    if (me->HasUnitState(UNIT_STATE_CASTING))
        return;

    if (uint32 spellId = events.ExecuteEvent())
    {
        DoCast(spellId);
        events.ScheduleEvent(spellId, AISpellInfo[spellId].cooldown + rand()%AISpellInfo[spellId].cooldown);
    }
    else
        DoMeleeAttackIfReady();
}

void CombatAI::SpellInterrupted(uint32 spellId, uint32 unTimeMs) 
{
    events.RescheduleEvent(spellId, unTimeMs);
}

/////////////////
//CasterAI
/////////////////

void CasterAI::InitializeAI()
{
    for (uint8 i = 0; i < CREATURE_MAX_SPELLS; ++i)
        if (me->m_spells[i] && sSpellMgr->GetSpellInfo(me->m_spells[i]))
            spells.push_back(me->m_spells[i]);

    m_attackDist = 30.0f;
    for (SpellVct::iterator itr = spells.begin(); itr != spells.end(); ++itr)
        if (AISpellInfo[*itr].condition == AICOND_COMBAT && m_attackDist > GetAISpellInfo(*itr)->maxRange)
            m_attackDist = GetAISpellInfo(*itr)->maxRange;

    CreatureAI::InitializeAI();

    me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PC);
}

void CasterAI::Reset()
{
    events.Reset();
}

void CasterAI::EnterCombat(Unit* who)
{
    if (spells.empty())
        return;

    if (!events.empty())
        return;

    uint32 spell = rand()%spells.size();
    uint32 count = 0;
    for (SpellVct::iterator itr = spells.begin(); itr != spells.end(); ++itr, ++count)
    {
        if (AISpellInfo[*itr].condition == AICOND_AGGRO)
            me->CastSpell(who, *itr, false);
        else if (AISpellInfo[*itr].condition == AICOND_COMBAT)
        {
            uint32 cooldown = GetAISpellInfo(*itr)->realCooldown;
            if (count == spell)
            {
                DoCast(spells[spell]);
                cooldown += me->GetCurrentSpellCastTime(*itr);
            }
            events.ScheduleEvent(*itr, cooldown);
        }
    }
}

void CasterAI::SetTarget(Unit* newTarget)
{
    if (spells.empty())
        return;

    Unit* owner = me->GetCharmerOrOwner();
    Unit* target = me->getVictim();

    // Yes, this is on purpose, we do not switch the target if we already have a victim
    if (target)
        return;

    if (newTarget != me && newTarget != owner && !me->IsFriendlyTo(newTarget))
        target = newTarget;

    if (!me->IsWithinDistInMap(target, m_attackDist))
        return;

    uint32 spell = rand() % spells.size();
    uint32 count = 0;

    for (SpellVct::iterator itr = spells.begin(); itr != spells.end(); ++itr, ++count)
    {
        if (AISpellInfo[*itr].condition == AICOND_AGGRO)
            me->CastSpell(target, *itr, false);
        else if (AISpellInfo[*itr].condition == AICOND_COMBAT)
        {
            uint32 cooldown = GetAISpellInfo(*itr)->realCooldown;
            if (count == spell)
            {
                DoCast(spells[spell]);
                cooldown += me->GetCurrentSpellCastTime(*itr);
            }
            events.ScheduleEvent(*itr, cooldown);
        }
    }

    AttackStartCaster(target, m_attackDist);
}

void CasterAI::UpdateAI(const uint32 diff)
{
    events.Update(diff);

    Unit* target = me->getVictim();
    Unit* owner = me->GetCharmerOrOwner();
    if (!target)
    {
        me->CombatStop();
        me->CastStop();

        if (owner && !me->HasUnitState(UNIT_STATE_FOLLOW))
        {
            me->GetMotionMaster()->Clear();
            me->GetMotionMaster()->MoveFollow(owner, PET_FOLLOW_DIST, me->GetFollowAngle());
        }

        return;
    }

    if (target->HasBreakableCrowdControlAura(me))
    {
        me->AttackStop();
        return;
    }

    if (!me->IsWithinLOSInMap(target))
    {
        me->CastStop();

        if (!me->HasUnitState(UNIT_STATE_CHASE))
        {
            me->GetMotionMaster()->Clear();
            me->GetMotionMaster()->MoveChase(target);
        }

        return;
    }

    if (owner && !owner->canSeeOrDetect(target))
    {
        me->AttackStop();
        return;
    }

    if (me->HasUnitState(UNIT_STATE_CASTING))
        return;

    if (uint32 spellId = events.ExecuteEvent())
    {
        DoCast(spellId);
        uint32 cooldown = me->GetCurrentSpellCastTime(spellId) + GetAISpellInfo(spellId)->realCooldown;
        events.ScheduleEvent(spellId, cooldown);
    }
    else if (events.empty())
    {
        uint32 spell = rand() % spells.size();
        uint32 count = 0;

        for (SpellVct::iterator itr = spells.begin(); itr != spells.end(); ++itr, ++count)
        {
            if (AISpellInfo[*itr].condition == AICOND_AGGRO)
                me->CastSpell(target, *itr, false);
            else if (AISpellInfo[*itr].condition == AICOND_COMBAT)
            {
                uint32 cooldown = GetAISpellInfo(*itr)->realCooldown;
                if (count == spell)
                {
                    DoCast(spells[spell]);
                    cooldown += me->GetCurrentSpellCastTime(*itr);
                }
                events.ScheduleEvent(*itr, cooldown);
            }
        }
    }
}

void CasterAI::SpellInterrupted(uint32 spellId, uint32 unTimeMs) 
{
    events.RescheduleEvent(spellId, unTimeMs);
}

//////////////
//ArcherAI
//////////////

ArcherAI::ArcherAI(Creature* c) : CreatureAI(c)
{
    if (!me->m_spells[0])
        sLog->outError("ArcherAI set for creature (entry = %u) with spell1=0. AI will do nothing", me->GetEntry());

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(me->m_spells[0]);
    m_minRange = spellInfo ? spellInfo->GetMinRange(false) : 0;

    if (!m_minRange)
        m_minRange = MELEE_RANGE;
    me->m_CombatDistance = spellInfo ? spellInfo->GetMaxRange(false) : 0;
    me->m_SightDistance = me->m_CombatDistance;
}

void ArcherAI::AttackStart(Unit* who)
{
    if (!who)
        return;

    if (me->IsWithinCombatRange(who, m_minRange))
    {
        if (me->Attack(who, true) && !who->IsFlying())
            me->GetMotionMaster()->MoveChase(who);
    }
    else
    {
        if (me->Attack(who, false) && !who->IsFlying())
            me->GetMotionMaster()->MoveChase(who, me->m_CombatDistance);
    }

    if (who->IsFlying())
        me->GetMotionMaster()->MoveIdle();
}

void ArcherAI::UpdateAI(const uint32 /*diff*/)
{
    if (!UpdateVictim())
        return;

    if (!me->IsWithinCombatRange(me->getVictim(), m_minRange))
        DoSpellAttackIfReady(me->m_spells[0]);
    else
        DoMeleeAttackIfReady();
}

//////////////
//TurretAI
//////////////

TurretAI::TurretAI(Creature* c) : CreatureAI(c)
{
    if (!me->m_spells[0])
        sLog->outError("TurretAI set for creature (entry = %u) with spell1=0. AI will do nothing", me->GetEntry());

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(me->m_spells[0]);
    m_minRange = spellInfo ? spellInfo->GetMinRange(false) : 0;
    me->m_CombatDistance = spellInfo ? spellInfo->GetMaxRange(false) : 0;
    me->m_SightDistance = me->m_CombatDistance;
}

bool TurretAI::CanAIAttack(const Unit* /*who*/) const
{
    // TODO: use one function to replace it
    if (!me->IsWithinCombatRange(me->getVictim(), me->m_CombatDistance)
        || (m_minRange && me->IsWithinCombatRange(me->getVictim(), m_minRange)))
        return false;
    return true;
}

void TurretAI::AttackStart(Unit* who)
{
    if (who)
        me->Attack(who, false);
}

void TurretAI::UpdateAI(const uint32 /*diff*/)
{
    if (!UpdateVictim())
        return;

    DoSpellAttackIfReady(me->m_spells[0]);
}

//////////////
//VehicleAI
//////////////

VehicleAI::VehicleAI(Creature* c) : CreatureAI(c), m_vehicle(c->GetVehicleKit()), m_IsVehicleInUse(false), m_ConditionsTimer(VEHICLE_CONDITION_CHECK_TIME)
{
    LoadConditions();
    m_DoDismiss = false;
    m_DismissTimer = VEHICLE_DISMISS_TIME;
}

//NOTE: VehicleAI::UpdateAI runs even while the vehicle is mounted
void VehicleAI::UpdateAI(const uint32 diff)
{
    CheckConditions(diff);

    if (m_DoDismiss)
    {
        if (m_DismissTimer < diff)
        {
            m_DoDismiss = false;
            me->SetVisible(false);
            me->DespawnOrUnsummon();
        }else m_DismissTimer -= diff;
    }
}

void VehicleAI::Reset()
{
    me->SetVisible(true);
}

void VehicleAI::OnCharmed(bool apply)
{
    if (m_IsVehicleInUse && !apply && !conditions.empty())//was used and has conditions
    {
        m_DoDismiss = true;//needs reset
        me->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_PLAYER_VEHICLE);
        me->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_SPELLCLICK);
    }
    else if (apply)
        m_DoDismiss = false;//in use again
    m_DismissTimer = VEHICLE_DISMISS_TIME;//reset timer
    m_IsVehicleInUse = apply;
}

void VehicleAI::LoadConditions()
{
    conditions = sConditionMgr->GetConditionsForNotGroupedEntry(CONDITION_SOURCE_TYPE_CREATURE_TEMPLATE_VEHICLE, me->GetEntry());
    if (!conditions.empty())
        sLog->outDebug(LOG_FILTER_CONDITIONSYS, "VehicleAI::LoadConditions: loaded %u conditions", uint32(conditions.size()));
}

void VehicleAI::CheckConditions(const uint32 diff)
{
    if (m_ConditionsTimer < diff)
    {
        if (!conditions.empty())
        {
            for (SeatMap::iterator itr = m_vehicle->Seats.begin(); itr != m_vehicle->Seats.end(); ++itr)
                if (Unit* passenger = ObjectAccessor::GetUnit(*m_vehicle->GetBase(), itr->second.Passenger))
                {
                    if (Player* player = passenger->ToPlayer())
                    {
                        if (!sConditionMgr->IsPlayerMeetToConditions(player, conditions))
                        {
                            player->ExitVehicle();
                            return;//check other pessanger in next tick
                        }
                    }
                }
        }
        m_ConditionsTimer = VEHICLE_CONDITION_CHECK_TIME;
    } else m_ConditionsTimer -= diff;
}
