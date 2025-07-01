//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2009-2015 Marianne Gagnon
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

#include "states_screens/online/online_screen.hpp"

#include "config/player_manager.hpp"
#include "config/user_config.hpp"
#include "guiengine/message_queue.hpp"
#include "guiengine/widgets/label_widget.hpp"
#include "guiengine/widgets/ribbon_widget.hpp"
#include "guiengine/widgets/text_box_widget.hpp"
#include "input/device_manager.hpp"
#include "io/file_manager.hpp"
#include "network/event.hpp"
#include "network/network_config.hpp"
#include "network/server.hpp"
#include "network/server_config.hpp"
#include "network/socket_address.hpp"
#include "network/stk_host.hpp"
#include "network/stk_peer.hpp"
#include "online/link_helper.hpp"
#include "online/profile_manager.hpp"
#include "online/request_manager.hpp"
#include "states_screens/dialogs/enter_address_dialog.hpp"
#include "states_screens/dialogs/general_text_field_dialog.hpp"
#include "states_screens/dialogs/message_dialog.hpp"
#include "states_screens/main_menu_screen.hpp"
#include "states_screens/online/networking_lobby.hpp"
#include "states_screens/online/online_lan.hpp"
#include "states_screens/online/online_profile_achievements.hpp"
#include "states_screens/online/online_profile_servers.hpp"
#include "states_screens/options/user_screen.hpp"
#include "states_screens/state_manager.hpp"
#include "utils/string_utils.hpp"
#include "utils/translation.hpp"
#include <string>

using namespace GUIEngine;
using namespace Online;

// ----------------------------------------------------------------------------

OnlineScreen::OnlineScreen() : Screen("online/online.stkgui") {} // OnlineScreen

// ----------------------------------------------------------------------------

void OnlineScreen::loadedFromFile() {} // loadedFromFile

// ----------------------------------------------------------------------------

void OnlineScreen::unloaded() {}

// ----------------------------------------------------------------------------

void OnlineScreen::init() {
  Screen::init();

  RibbonWidget *r = getWidget<RibbonWidget>("menu_toprow");
  r->setFocusForPlayer(PLAYER_ID_GAME_MASTER);
} // init

// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------

void OnlineScreen::onUpdate(float delta) {
  // In case for entering server address finished
  if (m_entered_server) {
    NetworkConfig::get()->setIsLAN();
    NetworkConfig::get()->setIsServer(false);
    ServerConfig::m_private_server_password = "";
    STKHost::create();
    NetworkingLobby::getInstance()->setJoinedServer(m_entered_server);
    m_entered_server = nullptr;
    StateManager::get()->resetAndSetStack(
        NetworkConfig::get()->getResetScreens(true /*lobby*/).data());
  }
} // onUpdate

// ----------------------------------------------------------------------------

void OnlineScreen::eventCallback(Widget *widget, const std::string &name,
                                 const int playerID) {
  if (name == "back") {
    StateManager::get()->escapePressed();
    return;
  }

  RibbonWidget *ribbon = dynamic_cast<RibbonWidget *>(widget);
  if (ribbon == NULL)
    return; // what's that event??

  // ---- A ribbon icon was clicked
  std::string selection = ribbon->getSelectionIDString(PLAYER_ID_GAME_MASTER);

  if (selection == "lan") {
    OnlineLanScreen::getInstance()->push();
  } else if (selection == "enter-address") {
    m_entered_server = nullptr;
    new EnterAddressDialog(&m_entered_server);
  }
} // eventCallback

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
/** Also called when pressing the back button. It resets the flags to indicate
 *  a networked game.
 */
bool OnlineScreen::onEscapePressed() {
  NetworkConfig::get()->cleanNetworkPlayers();
  NetworkConfig::get()->unsetNetworking();
  return true;
} // onEscapePressed
