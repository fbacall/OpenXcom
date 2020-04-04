/*
 * Copyright 2010-2020 OpenXcom Developers.
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
#include "RuleSkill.h"
#include "../Engine/Collections.h"
#include "../Engine/ScriptBind.h"
#include "../Battlescape/BattlescapeGame.h"

namespace OpenXcom
{

RuleSkill::RuleSkill(const std::string& type) : _type(type),
	_targetMode(BA_NONE), _compatibleBattleType(BT_NONE), _isPsiRequired(false), _checkHandsOnly(true)
{
}

/**
 * Loads the skill from a YAML file.
 * @param node YAML node.
 * @param mod Mod for the skill.
 */
void RuleSkill::load(const YAML::Node& node, const ModScript& parsers)
{
	if (const YAML::Node& parent = node["refNode"])
	{
		load(parent, parsers);
	}
	_type = node["type"].as<std::string>(_type);

	int targetMode = node["targetMode"].as<int>(_targetMode);
	targetMode = targetMode < 0 ? 0 : targetMode;
	targetMode = targetMode > BA_CQB ? 0 : targetMode;
	_targetMode = static_cast<BattleActionType>(targetMode);

	int compBattleType = node["battleType"].as<int>(_compatibleBattleType);
	compBattleType = compBattleType < 0 ? 0 : compBattleType;
	compBattleType = compBattleType > BT_CORPSE ? 0 : compBattleType;
	_compatibleBattleType = static_cast<BattleType>(compBattleType);

	_isPsiRequired = node["isPsiRequired"].as<bool>(_isPsiRequired);
	_checkHandsOnly = node["checkHandsOnly"].as<bool>(_checkHandsOnly);

	_cost.loadCost(node, "Use");
	_flat.loadPercent(node, "Use");

	_compatibleWeaponNames = node["compatibleWeapons"].as<std::vector<std::string> >(_compatibleWeaponNames);
	_requiredBonusNames = node["requiredBonuses"].as<std::vector<std::string> >(_requiredBonusNames);

	_scriptValues.load(node, parsers.getShared());

	_skillScripts.load(_type, node, parsers.skillScripts);
}

/**
 * Cross link with other Rules.
 */
void RuleSkill::afterLoad(const Mod* mod)
{
	for (auto& itemName : _compatibleWeaponNames)
	{
		_compatibleWeapons.push_back(mod->getItem(itemName, true));
	}
	for (auto& bonusName : _requiredBonusNames)
	{
		_requiredBonuses.push_back(mod->getSoldierBonus(bonusName, true));
	}

	//remove not needed data
	Collections::removeAll(_compatibleWeaponNames);
	Collections::removeAll(_requiredBonusNames);
}

////////////////////////////////////////////////////////////
//					Script binding
////////////////////////////////////////////////////////////

namespace
{

std::string debugDisplayScript(const RuleSkill* rs)
{
	if (rs)
	{
		std::string s;
		s += RuleSkill::ScriptName;
		s += "(name: \"";
		s += rs->getType();
		s += "\")";
		return s;
	}
	else
	{
		return "null";
	}
}

}


/**
 * Register RuleSkill in script parser.
 * @param parser Script parser.
 */
void RuleSkill::ScriptRegister(ScriptParserBase* parser)
{
	Bind<RuleSkill> rs = { parser };
	rs.addScriptValue<&RuleSkill::_scriptValues>();
	rs.addDebugDisplay<&debugDisplayScript>();
}

}
