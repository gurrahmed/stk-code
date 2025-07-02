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

#include "config/user_config.hpp"
#include "input/input_manager.hpp"
#include "input/input.hpp"
#include "items/attachment.hpp"
#include "items/item.hpp"
#include "items/powerup.hpp"
#include "karts/abstract_kart.hpp"
#include "karts/kart_properties.hpp"
#include "karts/skidding.hpp"
#include "karts/rescue_animation.hpp"
#include "modes/world.hpp"
#include "network/game_setup.hpp"
#include "network/rewind_manager.hpp"
#include "network/network_config.hpp"
#include "network/network_player_profile.hpp"
#include "network/network_string.hpp"
#include "network/protocols/game_protocol.hpp"
#include "race/history.hpp"
#include "states_screens/race_gui_base.hpp"
#include "utils/constants.hpp"
#include "utils/log.hpp"
#include "utils/string_utils.hpp"
#include "utils/translation.hpp"

#include <cstdlib>

PlayerController::PlayerController(AbstractKart *kart)
                : Controller(kart)
{
    m_penalty_ticks   = 0;
    m_time_since_stuck = 0.0f;
}   // PlayerController

//-----------------------------------------------------------------------------
/** Destructor for a player kart.
 */
PlayerController::~PlayerController()
{
}   // ~PlayerController

//-----------------------------------------------------------------------------
/** Resets the player kart for a new or restarted race.
 */
void PlayerController::reset()
{
    m_steer_val_l   = 0;
    m_steer_val_r   = 0;
    m_steer_val     = 0;
    m_prev_brake    = 0;
    m_prev_accel    = 0;
    m_prev_nitro    = false;
    m_penalty_ticks = 0;
    m_time_since_stuck = 0.0f;
}   // reset

// ----------------------------------------------------------------------------
/** Resets the state of control keys. This is used after the in-game menu to
 *  avoid that any keys pressed at the time the menu is opened are still
 *  considered to be pressed.
 */
void PlayerController::resetInputState()
{
    m_steer_val_l           = 0;
    m_steer_val_r           = 0;
    m_steer_val             = 0;
    m_prev_brake            = 0;
    m_prev_accel            = 0;
    m_prev_nitro            = false;
    m_time_since_stuck      = 0.0f;
    m_controls->reset();
}   // resetInputState

// ----------------------------------------------------------------------------
/** This function interprets a kart action and value, and set the corresponding
 *  entries in the kart control data structure. This function handles esp.
 *  cases like 'press left, press right, release right' - in this case after
 *  releasing right, the steering must switch to left again. Similarly it
 *  handles 'press left, press right, release left' (in which case still
 *  right must be selected). Similarly for braking and acceleration.
 *  This function can be run in two modes: first, if 'dry_run' is set,
 *  it will return true if this action will cause a state change. This
 *  is sued in networking to avoid sending events to the server (and then
 *  to other clients) if they are just (e.g. auto) repeated events/
 *  \param action  The action to be executed.
 *  \param value   If 32768, it indicates a digital value of 'fully set'
 *                 if between 1 and 32767, it indicates an analog value,
 *                 and if it's 0 it indicates that the corresponding button
 *                 was released.
 *  \param dry_run If set, it will only test if the parameter will trigger
 *                 a state change. If not set, the appropriate actions
 *                 (i.e. input state change) will be done.
 *  \return        If dry_run is set, will return true if this action will
 *                 cause a state change. If dry_run is not set, will return
 *                 false.
@@ -152,122 +156,73 @@ bool PlayerController::action(PlayerAction action, int value, bool dry_run)
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
        // Handle throttle input so that online games receive an acceleration
        // event once the race begins.  The value is expected to be in the
        // range [0, Input::MAX_VALUE].
        uint16_t v16 = (uint16_t)value;
        SET_OR_TEST(m_prev_accel, v16);
        SET_OR_TEST_GETTER(Accel, v16 / (float)Input::MAX_VALUE);
        break;
    }
    case PA_NITRO:
        // This basically keeps track whether the button still is being pressed
        SET_OR_TEST(m_prev_nitro, value != 0 );
        // Enable nitro only when also accelerating
        SET_OR_TEST_GETTER(Nitro, ((value!=0) && m_controls->getAccel()) );
        break;
    case PA_RESCUE:
        SET_OR_TEST_GETTER(Rescue, value != 0);
        break;
    case PA_PAUSE_RACE:
        if (value != 0) StateManager::get()->escapePressed();
        break;
    default:
        // Ignore all other actions so that only steering and nitro are active
        return !dry_run;
    }
    if (dry_run) return false;
    return true;
#undef SET_OR_TEST
#undef SET_OR_TEST_GETTER
}   // action

//-----------------------------------------------------------------------------
void PlayerController::actionFromNetwork(PlayerAction p_action, int value,
                                         int value_l, int value_r)
{
    m_steer_val_l = value_l;
    m_steer_val_r = value_r;
    // Reuse the action handler but avoid dispatching additional network
    // events by passing 'false' directly for the dry_run parameter.
    PlayerController::action(p_action, value, false);
}   // actionFromNetwork

//-----------------------------------------------------------------------------
/** Handles steering for a player kart.
 */
void PlayerController::steer(int ticks, int steer_val)
{
    // Get the old value, compute the new steering value,
    // and set it at the end of this function
    float steer = m_controls->getSteer();
    if(stk_config->m_disable_steer_while_unskid &&
@@ -333,50 +288,76 @@ void PlayerController::update(int ticks)
        if ((m_controls->getAccel() || m_controls->getBrake()||
            m_controls->getNitro()) && !NetworkConfig::get()->isNetworking())
        {
            // Only give penalty time in READY_PHASE.
            // Penalty time check makes sure it doesn't get rendered on every
            // update.
            if (m_penalty_ticks == 0 &&
                World::getWorld()->getPhase() == WorldStatus::READY_PHASE)
            {
                displayPenaltyWarning();
            }   // if penalty_time = 0
            m_controls->setBrake(false);
        }   // if key pressed

        return;
    }   // if isStartPhase

    if (m_penalty_ticks != 0 &&
        World::getWorld()->getTicksSinceStart() < m_penalty_ticks)
    {
        m_controls->setBrake(false);
        m_controls->setAccel(0.0f);
        return;
    }

    // Once the race has started check for the kart being stuck and trigger

    // an automatic rescue if it doesn't move for too long. In online races
    // a rescue request is sent to the server instead so every client stays
    // in sync.
    if (!World::getWorld()->isStartPhase() && isLocalPlayerController())
    {

    // an automatic rescue if it doesn't move for too long. In network games
    // the rescue request is sent to the server so all clients stay in sync.

    if (!World::getWorld()->isStartPhase() &&
        !NetworkConfig::get()->isNetworking())
    if (!World::getWorld()->isStartPhase())
    {
        // Track how long the kart has been stationary and trigger an
        // automatic rescue if necessary.

        float dt = stk_config->ticks2Time(ticks);
        if (m_kart->getSpeed() < 2.0f && !m_kart->getKartAnimation())
        {
            m_time_since_stuck += dt;
            if (m_time_since_stuck > 2.0f)
            {

                if (NetworkConfig::get()->isNetworking())
                {
                    if (auto gp = GameProtocol::lock())
                    {
                        gp->controllerAction(m_kart->getWorldKartId(),
                                             PA_RESCUE, Input::MAX_VALUE,
                                             m_steer_val_l, m_steer_val_r);
                    }
                }
                else
                {
                    RescueAnimation::create(m_kart);
                }


                RescueAnimation::create(m_kart);

                if (NetworkConfig::get()->isNetworking())
                    action(PA_RESCUE, Input::MAX_VALUE);
                else
                    RescueAnimation::create(m_kart);


                m_time_since_stuck = 0.0f;
            }
        }
        else
        {
            m_time_since_stuck = 0.0f;
        }
    }

    // Only accept rescue if there is no kart animation is already playing
    // (e.g. if an explosion happens, wait till the explosion is over before
    // starting any other animation).
    if ( m_controls->getRescue() && !m_kart->getKartAnimation() )
    {
        RescueAnimation::create(m_kart);
        m_controls->setRescue(false);
    }
}   // update

//-----------------------------------------------------------------------------
/** Called when a kart hits or uses a zipper.
 */
void PlayerController::handleZipper(bool play_sound)
{
    m_kart->showZipperFire();
}   // handleZipper

//-----------------------------------------------------------------------------
bool PlayerController::saveState(BareNetworkString *buffer) const
{
    // NOTE: when the size changes, the AIBaseController::saveState and
    // restore state MUST be adjusted!!
    int steer_abs = std::abs(m_steer_val);
    buffer->addUInt16((uint16_t)steer_abs).addUInt16(m_prev_accel)