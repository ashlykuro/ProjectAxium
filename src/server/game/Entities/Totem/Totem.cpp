#include "Totem.h"
#include "WorldPacket.h"
#include "Log.h"
#include "Group.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "SpellInfo.h"

Totem::Totem(SummonPropertiesEntry const* properties, Unit* owner) : Minion(properties, owner, false)
{
    m_unitTypeMask |= UNIT_MASK_TOTEM;
    m_duration = 0;
    m_type = TOTEM_PASSIVE;
}

void Totem::Update(uint32 time)
{
    if (!m_owner->isAlive() || !isAlive())
    {
        UnSummon();                                         // remove self
        return;
    }

    if (m_duration <= time)
    {
        UnSummon();                                         // remove self
        return;
    }
    else
        m_duration -= time;

    Creature::Update(time);
}

void Totem::InitStats(uint32 duration)
{
    // client requires SMSG_TOTEM_CREATED to be sent before adding to world and before removing old totem
    if (m_owner->GetTypeId() == TYPEID_PLAYER
        && m_Properties->Slot >= SUMMON_SLOT_TOTEM
        && m_Properties->Slot < MAX_TOTEM_SLOT)
    {
        WorldPacket data(SMSG_TOTEM_CREATED, 1 + 8 + 4 + 4);
        data << uint8(m_Properties->Slot - 1);
        data << uint64(GetGUID());
        data << uint32(duration);
        data << uint32(GetUInt32Value(UNIT_CREATED_BY_SPELL));
        m_owner->ToPlayer()->SendDirectMessage(&data);

        // set display id depending on caster's race
        SetDisplayId(m_owner->GetModelForTotem(PlayerTotemType(m_Properties->Id)));
    }

    Minion::InitStats(duration);

    // Get spell cast by totem
    if (SpellInfo const* totemSpell = sSpellMgr->GetSpellInfo(GetSpell()))
        if (totemSpell->CalcCastTime())   // If spell has cast time -> its an active totem
            m_type = TOTEM_ACTIVE;

    if (GetEntry() == SENTRY_TOTEM_ENTRY)
        SetReactState(REACT_AGGRESSIVE);

    m_duration = duration;

    SetLevel(m_owner->getLevel());
}

void Totem::InitSummon()
{
    if (m_type == TOTEM_PASSIVE)
        CastSpell(this, GetSpell(), true);

    // Some totems can have both instant effect and passive spell
    if (GetSpell(1))
        CastSpell(this, GetSpell(1), true);
}

void Totem::UnSummon()
{
    CombatStop();
    RemoveAurasDueToSpell(GetSpell());

    // clear owner's totem slot
    for (uint8 i = SUMMON_SLOT_TOTEM; i < MAX_TOTEM_SLOT; ++i)
    {
        if (m_owner->m_SummonSlot[i] == GetGUID())
        {
            m_owner->m_SummonSlot[i] = 0;
            break;
        }
    }

    m_owner->RemoveAurasDueToSpell(GetSpell());

    //remove aura all party members too
    if (Player* owner = m_owner->ToPlayer())
    {
        owner->SendAutoRepeatCancel(this);

        if (SpellInfo const* spell = sSpellMgr->GetSpellInfo(GetUInt32Value(UNIT_CREATED_BY_SPELL)))
            owner->SendCooldownEvent(spell, 0, NULL, false);

        if (Group* group = owner->GetGroup())
        {
            for (GroupReference* itr = group->GetFirstMember(); itr != NULL; itr = itr->next())
            {
                Player* target = itr->getSource();
                if (target && group->SameSubGroup(owner, target))
                    target->RemoveAurasDueToSpell(GetSpell());
            }
        }
    }

    AddObjectToRemoveList();
}

bool Totem::IsImmunedToSpellEffect(SpellInfo const* spellInfo, uint32 index) const
{
    // This check should not apply to Grounding Totem
    if (GetEntry() == 5925)
        return false;

    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        // Immunity for all negative auras
        if (spellInfo->Effects[i].Effect == SPELL_EFFECT_APPLY_AURA
            && (spellInfo->Effects[i].Effect != SPELL_EFFECT_NORMALIZED_WEAPON_DMG
            || spellInfo->Effects[i].Effect != SPELL_EFFECT_WEAPON_PERCENT_DAMAGE
            || spellInfo->Effects[i].Effect != SPELL_EFFECT_SCHOOL_DAMAGE))
        {
            if (!spellInfo->HasEffect(SPELL_EFFECT_SCHOOL_DAMAGE))
                if (!spellInfo->IsPositiveEffect(i))
                    return true;
        }
    }

    return Creature::IsImmunedToSpellEffect(spellInfo, index);
}
