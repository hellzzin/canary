/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019-2022 OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.org/
*/

#include "pch.hpp"

#include "game/game.h"
#include "creatures/players/management/waitlist.h"

namespace {

struct Wait
{
	constexpr Wait(std::size_t initTimeout, uint32_t initPlayerGUID) :
			timeout(initTimeout), playerGUID(initPlayerGUID) {}

	std::size_t timeout;
	uint32_t playerGUID;
};

using WaitList = std::list<Wait>;

void cleanupList(WaitList& list)
{
	int64_t time = OTSYS_TIME();

	auto it = list.begin(), end = list.end();
	while (it != end) {
		if ((it->timeout - time) <= 0) {
			it = list.erase(it);
		} else {
			++it;
		}
	}
}

std::size_t getTimeout(std::size_t slot)
{
	//timeout is set to 15 seconds longer than expected retry attempt
	return WaitingList::getTime(slot) + 15;
}

} // namespace

struct WaitListInfo
{
	WaitList priorityWaitList;
	WaitList waitList;

	std::pair<WaitList::iterator, WaitList::size_type> findClient(const Player *player) {
		std::size_t slot = 1;
		for (auto it = priorityWaitList.begin(), end = priorityWaitList.end(); it != end; ++it, ++slot) {
			if (it->playerGUID == player->getGUID()) {
				return {it, slot};
			}
		}

		for (auto it = waitList.begin(), end = waitList.end(); it != end; ++it, ++slot) {
			if (it->playerGUID == player->getGUID()) {
				return {it, slot};
			}
		}
		return {waitList.end(), slot};
	}
};

WaitingList& WaitingList::getInstance()
{
	static WaitingList waitingList;
	return waitingList;
}

std::size_t WaitingList::getTime(std::size_t slot)
{
	if (slot < 5) {
		return 5;
	} else if (slot < 10) {
		return 10;
	} else if (slot < 20) {
		return 20;
	} else if (slot < 50) {
		return 60;
	} else {
		return 120;
	}
}

bool WaitingList::clientLogin(const Player* player)
{
	if (player->hasFlag(PlayerFlags_t::CanAlwaysLogin) ||
		player->getAccountType() >= account::ACCOUNT_TYPE_GAMEMASTER) {
		return true;
	}

	auto maxPlayers = static_cast<uint32_t>(g_configManager().getNumber(MAX_PLAYERS));
	if (maxPlayers == 0 || (info->priorityWaitList.empty() && info->waitList.empty() && g_game().getPlayersOnline() < maxPlayers)) {
		return true;
	}

	cleanupList(info->priorityWaitList);
	cleanupList(info->waitList);

	WaitList::iterator it;
	WaitList::size_type slot;
	std::tie(it, slot) = info->findClient(player);
	if (it != info->waitList.end()) {
		if ((g_game().getPlayersOnline() + slot) <= maxPlayers) {
			//should be able to login now
			info->waitList.erase(it);
			return true;
		}

		//let them wait a bit longer
		it->timeout = OTSYS_TIME() + (getTimeout(slot) * 1000);
		return false;
	}

	slot = info->priorityWaitList.size();
	if (player->isPremium()) {
		info->priorityWaitList.emplace_back(OTSYS_TIME() + (getTimeout(slot + 1) * 1000), player->getGUID());
	} else {
		slot += info->waitList.size();
		info->waitList.emplace_back(OTSYS_TIME() + (getTimeout(slot + 1) * 1000), player->getGUID());
	}
	return false;
}

std::size_t WaitingList::getClientSlot(const Player* player)
{
	WaitList::iterator it;
	WaitList::size_type slot;
	std::tie(it, slot) = info->findClient(player);
	if (it == info->waitList.end()) {
		return 0;
	}
	return slot;
}

WaitingList::WaitingList() : info(new WaitListInfo) {}
