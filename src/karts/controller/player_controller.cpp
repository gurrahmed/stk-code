//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2004-2015 Steve Baker <sjbaker1@airmail.net>
//  Copyright (C) 2006-2015 Joerg Henrichs, Steve Baker
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.


#include "karts/controller/player_controller.hpp"

#include "audio/sfx_manager.hpp"
#include "config/user_config.hpp"
#include "input/input.hpp"
#include "input/input_manager.hpp"
#include "items/attachment.hpp"
#include "items/item.hpp"
#include "items/powerup.hpp"
#include "karts/abstract_kart.hpp"
#include "karts/kart_properties.hpp"
#include "karts/rescue_animation.hpp"
#include "karts/skidding.hpp"
#include "modes/world.hpp"
#include "network/game_setup.hpp"
#include "network/network_config.hpp"
#include "network/network_string.hpp"
#include "network/protocols/game_protocol.hpp"
#include "race/history.hpp"
#include "states_screens/race_gui_base.hpp"
#include "states_screens/state_manager.hpp"
#include "utils/constants.hpp"
#include "utils/log.hpp"
#include "utils/translation.hpp"

#include <cmath>
#include <cstdlib>

// ─────────────────────────────────────────────────────────────────────────────
// ctor / dtor
// ─────────────────────────────────────────────────────────────────────────────
PlayerController::PlayerController(AbstractKart* kart) : Controller(kart)
{
    m_penalty_ticks    = 0;
    m_time_since_stuck = 0.0f;
}

PlayerController::~PlayerController() = default;

// ─────────────────────────────────────────────────────────────────────────────
// reset helpers
// ─────────────────────────────────────────────────────────────────────────────
void PlayerController::reset()
{
    m_steer_val_l      = 0;
    m_steer_val_r      = 0;
    m_steer_val        = 0;
    m_prev_brake       = 0;
    m_prev_accel       = 0;
    m_prev_nitro       = false;
    m_penalty_ticks    = 0;
    m_time_since_stuck = 0.0f;
}

void PlayerController::resetInputState()
{
    m_steer_val_l      = 0;
    m_steer_val_r      = 0;
    m_steer_val        = 0;
    m_prev_brake       = 0;
    m_prev_accel       = 0;
    m_prev_nitro       = false;
    m_time_since_stuck = 0.0f;
    m_controls->reset();
}

// ─────────────────────────────────────────────────────────────────────────────
// action() – handle one input event
// ─────────────────────────────────────────────────────────────────────────────
#define SET_OR_TEST(field, new_val)                  \
    do                                               \
    {                                                \
        if (dry_run)                                 \
        {                                            \
            if ((field) != (new_val)) return true;   \
        }                                            \
        else                                         \
            (field) = (new_val);                     \
    } while (0)

#define SET_OR_TEST_GETTER(func, ...)                                \
    do                                                               \
    {                                                                \
        if (dry_run)                                                 \
        {                                                            \
            if (m_controls->get##func() != (__VA_ARGS__)) return true; \
        }                                                            \
        else                                                         \
            m_controls->set##func(__VA_ARGS__);                      \
    } while (0)

bool PlayerController::action(PlayerAction action, int value, bool dry_run)
{
    switch (action)
    {
    case PA_STEER_LEFT:
        SET_OR_TEST(m_steer_val_l, value);
        if (value)
        {
            SET_OR_TEST(m_steer_val, value);
            if (m_controls->getSkidControl() == KartControl::SC_NO_DIRECTION)
                SET_OR_TEST_GETTER(SkidControl, KartControl::SC_LEFT);
        }
        else
            SET_OR_TEST(m_steer_val, m_steer_val_r);
        break;

    case PA_STEER_RIGHT:
        SET_OR_TEST(m_steer_val_r, -value);
        if (value)
        {
            SET_OR_TEST(m_steer_val, -value);
            if (m_controls->getSkidControl() == KartControl::SC_NO_DIRECTION)
                SET_OR_TEST_GETTER(SkidControl, KartControl::SC_RIGHT);
        }
        else
            SET_OR_TEST(m_steer_val, m_steer_val_l);
        break;

    case PA_ACCEL:
    {
        uint16_t v16 = static_cast<uint16_t>(value);
        SET_OR_TEST(m_prev_accel, v16);
        SET_OR_TEST_GETTER(Accel, v16 / static_cast<float>(Input::MAX_VALUE));
        break;
    }

    case PA_NITRO:
        SET_OR_TEST(m_prev_nitro, value != 0);
        SET_OR_TEST_GETTER(Nitro, (value != 0) && m_controls->getAccel());
        break;

    case PA_RESCUE:
        SET_OR_TEST_GETTER(Rescue, value != 0);
        break;

    case PA_PAUSE_RACE:
        if (value) StateManager::get()->escapePressed();
        break;

    default:
        return !dry_run;   // ignore other actions
    }

    if (dry_run) return false;
    return true;
}

#undef SET_OR_TEST
#undef SET_OR_TEST_GETTER

void PlayerController::actionFromNetwork(PlayerAction p_action,
                                         int value, int value_l, int value_r)
{
    m_steer_val_l = value_l;
    m_steer_val_r = value_r;
    action(p_action, value, /*dry_run=*/false);
}

// ─────────────────────────────────────────────────────────────────────────────
// steer smoothing
// ─────────────────────────────────────────────────────────────────────────────
void PlayerController::steer(int /*ticks*/, int steer_val)
{
    float steer = m_controls->getSteer();
    if (stk_config->m_disable_steer_while_unskid &&
        m_kart->getSkidding()->isSkidding())
    {
        // keep previous steering while recovering from a skid
    }
    else
    {
        steer = steer_val / static_cast<float>(Input::MAX_VALUE);
    }
    m_controls->setSteer(steer);
}

// ─────────────────────────────────────────────────────────────────────────────
// per-frame update
// ─────────────────────────────────────────────────────────────────────────────
void PlayerController::update(int ticks)
{
    // Block controls during the READY countdown
    if (World::getWorld()->isStartPhase())
    {
        if ((m_controls->getAccel() || m_controls->getBrake() ||
             m_controls->getNitro()) &&
            !NetworkConfig::get()->isNetworking())
        {
            if (m_penalty_ticks == 0 &&
                World::getWorld()->getPhase() == WorldStatus::READY_PHASE)
            {
                displayPenaltyWarning();
            }
            m_controls->setBrake(false);
        }
        return;
    }

    // False-start penalty
    if (m_penalty_ticks &&
        World::getWorld()->getTicksSinceStart() < m_penalty_ticks)
    {
        m_controls->setBrake(false);
        m_controls->setAccel(0.0f);
        return;
    }

    // Auto-rescue if stuck > 2 s
    if (!World::getWorld()->isStartPhase())
    {
        const float dt = stk_config->ticks2Time(ticks);

        if (m_kart->getSpeed() < 2.0f && !m_kart->getKartAnimation())
        {
            m_time_since_stuck += dt;

            if (m_time_since_stuck > 2.0f)
            {
                if (NetworkConfig::get()->isNetworking())
                {
                    if (auto gp = GameProtocol::lock())
                        gp->controllerAction(m_kart->getWorldKartId(),
                                             PA_RESCUE, Input::MAX_VALUE,
                                             m_steer_val_l, m_steer_val_r);
                }
                else
                {
                    RescueAnimation::create(m_kart);
                }
                m_time_since_stuck = 0.0f;
            }
        }
        else
        {
            m_time_since_stuck = 0.0f;
        }
    }

    // Manual rescue
    if (m_controls->getRescue() && !m_kart->getKartAnimation())
    {
        RescueAnimation::create(m_kart);
        m_controls->setRescue(false);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void PlayerController::handleZipper(bool /*play_sound*/)
{
    m_kart->showZipperFire();
}

// ─────────────────────────────────────────────────────────────────────────────
// v-table helpers missing from your previous edit
// ─────────────────────────────────────────────────────────────────────────────
void PlayerController::skidBonusTriggered()
{
    if (isLocalPlayerController())
        SFXManager::get()->quickSound("skid_bonus");
}

void PlayerController::displayPenaltyWarning()
{
    if (!isLocalPlayerController()) return;

    core::stringw msg = _("False start!  Brakes locked for two seconds.");
    RaceGUIBase::displayGeneralRaceMessage(msg, 2.0f);
    m_penalty_ticks = World::getWorld()->getTicksSinceStart() +
                      stk_config->seconds2Ticks(2.0f);
}

void PlayerController::rewindTo(BareNetworkString* buffer)
{
    m_steer_val  = static_cast<int>(buffer->getUInt16());
    m_prev_accel = buffer->getUInt16();
    m_prev_brake = buffer->getUInt16();
    m_prev_nitro = buffer->getUInt8() != 0;
}

core::stringw PlayerController::getName(bool /*short_name*/) const
{
    return m_kart->getDriverName();
}

// ─────────────────────────────────────────────────────────────────────────────
// save-state for rewind / net-play
// ─────────────────────────────────────────────────────────────────────────────
bool PlayerController::saveState(BareNetworkString* buffer) const
{
    const int steer_abs = std::abs(m_steer_val);
    buffer->addUInt16(static_cast<uint16_t>(steer_abs))
          .addUInt16(m_prev_accel)
          .addUInt16(m_prev_brake)
          .addUInt8 (m_prev_nitro ? 1 : 0);
    return true;
}
