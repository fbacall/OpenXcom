/*
 * Copyright 2010-2019 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "RuleStartingCondition.h"
#include "RuleItem.h"
#include "../Engine/Logger.h"
#include "../Mod/Mod.h"
#include "../Savegame/Craft.h"
#include "../Savegame/ItemContainer.h"
#include "../Savegame/WeightedOptions.h"
#include <algorithm>

namespace OpenXcom
{

/**
 * Creates a blank ruleset for a certain type of Starting Condition.
 * @param type String defining the type.
 */
RuleStartingCondition::RuleStartingCondition(const std::string& type) : _type(type), _destroyRequiredItems(false)
{
}

/**
 *
 */
RuleStartingCondition::~RuleStartingCondition()
{
}

/**
 * Loads the Starting Conditions from a YAML file.
 * @param node YAML node.
 */
void RuleStartingCondition::load(const YAML::Node& node)
{
	if (const YAML::Node& parent = node["refNode"])
	{
		load(parent);
	}
	_type = node["type"].as<std::string>(_type);
	_defaultArmor = node["defaultArmor"].as< std::map<std::string, std::map<std::string, int> > >(_defaultArmor);
	_allowedArmors = node["allowedArmors"].as< std::vector<std::string> >(_allowedArmors);
	_forbiddenArmors = node["forbiddenArmors"].as< std::vector<std::string> >(_forbiddenArmors);
	_allowedVehicles = node["allowedVehicles"].as< std::vector<std::string> >(_allowedVehicles);
	_forbiddenVehicles = node["forbiddenVehicles"].as< std::vector<std::string> >(_forbiddenVehicles);
	_allowedItems = node["allowedItems"].as< std::vector<std::string> >(_allowedItems);
	_forbiddenItems = node["forbiddenItems"].as< std::vector<std::string> >(_forbiddenItems);
	_allowedItemCategories = node["allowedItemCategories"].as< std::vector<std::string> >(_allowedItemCategories);
	_forbiddenItemCategories = node["forbiddenItemCategories"].as< std::vector<std::string> >(_forbiddenItemCategories);
	_allowedCraft = node["allowedCraft"].as< std::vector<std::string> >(_allowedCraft);
	_forbiddenCraft = node["forbiddenCraft"].as< std::vector<std::string> >(_forbiddenCraft);
	_allowedSoldierTypes = node["allowedSoldierTypes"].as< std::vector<std::string> >(_allowedSoldierTypes);
	_forbiddenSoldierTypes = node["forbiddenSoldierTypes"].as< std::vector<std::string> >(_forbiddenSoldierTypes);
	_requiredItems = node["requiredItems"].as< std::map<std::string, int> >(_requiredItems);
	_destroyRequiredItems = node["destroyRequiredItems"].as<bool>(_destroyRequiredItems);

	if (node["environmentalConditions"] || node["paletteTransformations"] || node["armorTransformations"]
		|| node["mapBackgroundColor"] || node["inventoryShockIndicator"] || node["mapShockIndicator"])
	{
		Log(LOG_ERROR) << "There are invalid/obsolete attributes in starting condition " << _type << ". Please review the ruleset.";
	}
}

/**
 * Checks if the craft type is permitted.
 * @param craftType Craft type name.
 * @return True if permitted, false otherwise.
 */
bool RuleStartingCondition::isCraftPermitted(const std::string& craftType) const
{
	if (!_forbiddenCraft.empty())
	{
		return (std::find(_forbiddenCraft.begin(), _forbiddenCraft.end(), craftType) == _forbiddenCraft.end());
	}
	else if (!_allowedCraft.empty())
	{
		return (std::find(_allowedCraft.begin(), _allowedCraft.end(), craftType) != _allowedCraft.end());
	}
	return true;
}

/**
 * Checks if the soldier type is permitted.
 * @param soldierType Soldier type name.
 * @return True if permitted, false otherwise.
 */
bool RuleStartingCondition::isSoldierTypePermitted(const std::string& soldierType) const
{
	if (!_forbiddenSoldierTypes.empty())
	{
		return (std::find(_forbiddenSoldierTypes.begin(), _forbiddenSoldierTypes.end(), soldierType) == _forbiddenSoldierTypes.end());
	}
	else if (!_allowedSoldierTypes.empty())
	{
		return (std::find(_allowedSoldierTypes.begin(), _allowedSoldierTypes.end(), soldierType) != _allowedSoldierTypes.end());
	}
	return true;
}

/**
 * Gets the replacement armor.
 * @param soldierType Soldier type name.
 * @param armorType Existing/old armor type name.
 * @return Replacement armor type name (or empty string if no replacement is needed).
 */
std::string RuleStartingCondition::getArmorReplacement(const std::string& soldierType, const std::string& armorType) const
{
	bool allowed = true;
	if (!_forbiddenArmors.empty())
	{
		allowed = (std::find(_forbiddenArmors.begin(), _forbiddenArmors.end(), armorType) == _forbiddenArmors.end());
	}
	else if (!_allowedArmors.empty())
	{
		allowed = (std::find(_allowedArmors.begin(), _allowedArmors.end(), armorType) != _allowedArmors.end());
	}

	if (!allowed)
	{
		std::map<std::string, std::map<std::string, int> >::const_iterator j = _defaultArmor.find(soldierType);
		if (j != _defaultArmor.end())
		{
			WeightedOptions w = WeightedOptions();
			for (std::map<std::string, int>::const_iterator k = (j->second).begin(); k != (j->second).end(); ++k)
			{
				w.set(k->first, k->second);
			}
			std::string pick = w.choose();
			return pick == "noChange" ? "" : pick;
		}
	}

	return "";
}

/**
 * Checks if the vehicle type is permitted.
 * @param vehicleType Vehicle type name.
 * @return True if permitted, false otherwise.
 */
bool RuleStartingCondition::isVehiclePermitted(const std::string& vehicleType) const
{
	if (!_forbiddenVehicles.empty())
	{
		return (std::find(_forbiddenVehicles.begin(), _forbiddenVehicles.end(), vehicleType) == _forbiddenVehicles.end());
	}
	else if (!_allowedVehicles.empty())
	{
		return (std::find(_allowedVehicles.begin(), _allowedVehicles.end(), vehicleType) != _allowedVehicles.end());
	}
	return true;
}

/**
 * Checks if the item type is permitted.
 * @param itemType Item type name.
 * @return True if permitted, false otherwise.
 */
bool RuleStartingCondition::isItemPermitted(const std::string& itemType, Mod* mod, Craft* craft) const
{
	bool itemCheckSubResult = true; // if both item lists are empty, item is OK

	if (!_forbiddenItems.empty())
	{
		if (std::find(_forbiddenItems.begin(), _forbiddenItems.end(), itemType) != _forbiddenItems.end())
		{
			return false; // item explicitly forbidden
		}
		else
		{
			itemCheckSubResult = true; // item is OK, unless category rules below say otherwise
		}
	}
	else if (!_allowedItems.empty())
	{
		if (std::find(_allowedItems.begin(), _allowedItems.end(), itemType) != _allowedItems.end())
		{
			return true; // item explicitly allowed
		}
		else
		{
			itemCheckSubResult = false; // item is NOT OK, unless category rules below say otherwise
		}
	}

	bool checkForbiddenCategories = !_forbiddenItemCategories.empty();
	bool checkAllowedCategories = !checkForbiddenCategories && !_allowedItemCategories.empty();

	bool categoryCheckSubResult = true;

	if (checkForbiddenCategories)
		categoryCheckSubResult = true;
	else if (checkAllowedCategories)
		categoryCheckSubResult = false;

	if (checkForbiddenCategories || checkAllowedCategories)
	{
		RuleItem* item = mod->getItem(itemType);
		if (item)
		{
			// primary categories
			std::vector<std::string> itemCategories = item->getCategories();

			// secondary categories ("inherited" from equipped ammo)
			if (mod->getShareAmmoCategories() && item->getBattleType() == BT_FIREARM)
			{
				for (auto& compatibleAmmoName : *item->getPrimaryCompatibleAmmo())
				{
					if (craft->getItems()->getItem(compatibleAmmoName) > 0)
					{
						RuleItem *ammoRule = mod->getItem(compatibleAmmoName);
						if (ammoRule)
						{
							for (auto& cat : ammoRule->getCategories())
							{
								itemCategories.push_back(cat);
							}
						}
					}
				}
			}

			if (!itemCategories.empty())
			{
				// check all categories of the item
				for (std::vector<std::string>::iterator i = itemCategories.begin(); i != itemCategories.end(); ++i)
				{
					if (checkForbiddenCategories)
					{
						if (std::find(_forbiddenItemCategories.begin(), _forbiddenItemCategories.end(), (*i)) != _forbiddenItemCategories.end())
						{
							return false; // found a category that is forbidden
						}
					}
					else if (checkAllowedCategories)
					{
						if (std::find(_allowedItemCategories.begin(), _allowedItemCategories.end(), (*i)) != _allowedItemCategories.end())
						{
							return true; // found a category that is allowed
						}
					}
				}
			}
		}
	}

	return itemCheckSubResult && categoryCheckSubResult;
}

}
