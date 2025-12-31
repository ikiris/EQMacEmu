/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2013 EQEMu Development Team (http://eqemulator.net)

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; version 2 of the License.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY except by those people which sell it, which
	are required to give you total support for your newly bought product;
	without even the implied warranty of MERCHANTABILITY or FITNESS FOR
	A PARTICULAR PURPOSE. See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#ifndef __EQEMU_TESTS_NPC_LOOTTABLE_H
#define __EQEMU_TESTS_NPC_LOOTTABLE_H

#include "cppunit/cpptest.h"
#include "../zone/npc.h"
#include "../zone/zone.h"
#include "../zone/zonedb.h"
#include "../common/repositories/loottable_repository.h"
#include "../common/repositories/loottable_entries_repository.h"
#include "../common/repositories/lootdrop_repository.h"
#include "../common/repositories/lootdrop_entries_repository.h"
#include "../common/random.h"
#include "../common/content/world_content_service.h"
#include <map>
#include <vector>
#include <algorithm>
#include <memory>
#include <cstring>

// Forward declarations
struct NPCType;
class Zone;
class ZoneDatabase;

// Test configuration structures
struct LootTableEntryConfig
{
	uint32 lootdrop_id;
	uint8 multiplier;
	uint8 probability;
	uint8 droplimit;
	uint8 mindrop;
	uint8 multiplier_min;
};

struct LootDropEntryConfig
{
	uint32 item_id;
	float chance;
	uint8 multiplier;
	int8 item_charges;
	uint8 equip_item;
	int8 minlevel;
	uint8 maxlevel;
};

struct LootTableTestConfig
{
	uint32 loottable_id;
	std::string loottable_name;
	uint32 mincash;
	uint32 maxcash;
	uint32 avgcoin;

	// Loot table entries
	std::vector<LootTableEntryConfig> loottable_entries;

	// Loot drops (keyed by lootdrop_id)
	std::map<uint32, std::vector<LootDropEntryConfig>> lootdrops;

	// Expected percentages: maps complete sorted item ID lists (can include duplicates) to expected percentage
	// Example: {1001, 1001, 1002} -> 15.0 means this exact pattern should occur ~15% of the time
	std::map<std::vector<uint32>, float> expected_percentages;

	float variance_tolerance = 2.0f; // ±2% default
};

// Mock Zone class
class MockZone
{
public:
	EQ::Random random;

	std::map<uint32, LoottableRepository::Loottable> loottables;
	std::map<uint32, std::vector<LoottableEntriesRepository::LoottableEntries>> loottable_entries;
	std::map<uint32, LootdropRepository::Lootdrop> lootdrops;
	std::map<uint32, std::vector<LootdropEntriesRepository::LootdropEntries>> lootdrop_entries;

	void LoadLootTable(uint32 loottable_id)
	{
		// No-op, data provided directly
	}

	LoottableRepository::Loottable *GetLootTable(uint32 loottable_id)
	{
		auto it = loottables.find(loottable_id);
		if (it != loottables.end())
		{
			return &it->second;
		}
		return nullptr;
	}

	std::vector<LoottableEntriesRepository::LoottableEntries> GetLootTableEntries(uint32 loottable_id) const
	{
		auto it = loottable_entries.find(loottable_id);
		if (it != loottable_entries.end())
		{
			return it->second;
		}
		return std::vector<LoottableEntriesRepository::LoottableEntries>();
	}

	LootdropRepository::Lootdrop GetLootdrop(uint32 lootdrop_id) const
	{
		auto it = lootdrops.find(lootdrop_id);
		if (it != lootdrops.end())
		{
			return it->second;
		}
		return LootdropRepository::Lootdrop();
	}

	std::vector<LootdropEntriesRepository::LootdropEntries> GetLootdropEntries(uint32 lootdrop_id) const
	{
		auto it = lootdrop_entries.find(lootdrop_id);
		if (it != lootdrop_entries.end())
		{
			return it->second;
		}
		return std::vector<LootdropEntriesRepository::LootdropEntries>();
	}
};

// Mock Database class
class MockDatabase
{
public:
	struct MockItemData
	{
		uint32 ID;
		std::string Name;
		uint8 MaxCharges;
		EQ::item::ItemType ItemType;
		uint16 Material;
		uint32 Color;
		std::string IDFile;
		uint16 AC;
		uint16 HP;
		uint32 Slots;
		bool NoDrop;
	};

	std::map<uint32, MockItemData> items;

	const EQ::ItemData *GetItem(uint32 item_id)
	{
		auto it = items.find(item_id);
		if (it != items.end())
		{
			// Convert MockItemData to EQ::ItemData
			// For testing, we'll create a minimal ItemData
			static std::map<uint32, std::unique_ptr<EQ::ItemData>> item_cache;
			if (item_cache.find(item_id) == item_cache.end())
			{
				auto item = std::make_unique<EQ::ItemData>();
				memset(item.get(), 0, sizeof(EQ::ItemData));
				item->ID = it->second.ID;
				strncpy(item->Name, it->second.Name.c_str(), sizeof(item->Name) - 1);
				item->MaxCharges = it->second.MaxCharges;
				item->ItemType = it->second.ItemType;
				item->Material = it->second.Material;
				item->Color = it->second.Color;
				strncpy(item->IDFile, it->second.IDFile.c_str(), sizeof(item->IDFile) - 1);
				item->AC = it->second.AC;
				item->HP = it->second.HP;
				item->Slots = it->second.Slots;
				item->NoDrop = it->second.NoDrop;
				item_cache[item_id] = std::move(item);
			}
			return item_cache[item_id].get();
		}
		return nullptr;
	}

	EQ::item::QuantityType ItemQuantityType(uint32 item_id)
	{
		auto it = items.find(item_id);
		if (it != items.end() && it->second.MaxCharges > 0)
		{
			return EQ::item::Quantity_Charges;
		}
		return EQ::item::Quantity_Normal;
	}

	EQ::ItemInstance *CreateItem(uint32 item_id, int8 charges, const QuarmItemData &quarm_item_data)
	{
		// Return nullptr for testing - we don't need actual item instances
		return nullptr;
	}
};

// Mock ContentService
class MockContentService
{
public:
	bool DoesPassContentFiltering(const ContentFlags &flags)
	{
		return true; // Always pass for testing
	}
};

// Helper function to run loot table test
// Implementation is in npc_loottable_test.cpp
void RunLootTableTest(const LootTableTestConfig &config);

class NPCLootTableTest : public Test::Suite
{
	typedef void (NPCLootTableTest::*TestFunction)(void);

public:
	NPCLootTableTest()
	{
		// Test methods will be added here by user
		// Example: TEST_ADD(NPCLootTableTest::TestBasicLootTable);
	}

	~NPCLootTableTest()
	{
	}

private:
	// Test methods can be added here
	// Example:
	// void TestBasicLootTable() {
	//     LootTableTestConfig config;
	//     // ... configure test ...
	//     RunLootTableTest(config);
	// }
};

#endif
