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

#include "npc_loottable_test.h"
#include "../zone/zone.h"
#include "../zone/zonedb.h"
#include "../zone/zonedump.h"
#include "../zone/common.h"
#include "../common/item_data.h"
#include <fmt/format.h>
#include <sstream>

// Global mocks
MockZone *g_mock_zone = nullptr;
MockDatabase *g_mock_database = nullptr;
MockContentService *g_mock_content_service = nullptr;

// Test Zone helper - accesses Zone's private members
// REQUIRED: Add "friend class TestZoneHelper;" to Zone class in zone/zone.h private section
// OR make m_loottables, m_loottable_entries, m_lootdrops, m_lootdrop_entries protected
class TestZoneHelper
{
public:
	static void AddTestLootTable(Zone *zone, const LoottableRepository::Loottable &lt)
	{
		// Access private member - requires friend declaration in Zone
		// REQUIRED: Add "friend class TestZoneHelper;" to Zone class
		zone->m_loottables.push_back(lt);
	}

	static void AddTestLootTableEntry(Zone *zone, const LoottableEntriesRepository::LoottableEntries &lte)
	{
		zone->m_loottable_entries.push_back(lte);
	}

	static void AddTestLootdrop(Zone *zone, const LootdropRepository::Lootdrop &ld)
	{
		zone->m_lootdrops.push_back(ld);
	}

	static void AddTestLootdropEntry(Zone *zone, const LootdropEntriesRepository::LootdropEntries &lde)
	{
		zone->m_lootdrop_entries.push_back(lde);
	}
};

// Test Database helper - accesses database's protected items_hash
// REQUIRED: Add "friend class TestDatabaseHelper;" to SharedDatabase class in common/shareddb.h protected section
class TestDatabaseHelper
{
public:
	static void AddTestItem(ZoneDatabase *db, const EQ::ItemData &item_data)
	{
		// Access protected member - requires friend declaration in SharedDatabase
		// REQUIRED: Add "friend class TestDatabaseHelper;" to SharedDatabase class
		if (db->items_hash && db->items_hash->max_key() >= item_data.ID)
		{
			try
			{
				db->items_hash->insert(item_data.ID, item_data);
			}
			catch (...)
			{
				// Item may already exist or hash may not be initialized
			}
		}
	}
};

// Helper functions to create repository entities from config
static LoottableRepository::Loottable CreateLootTable(const LootTableTestConfig &config)
{
	LoottableRepository::Loottable lt = LoottableRepository::NewEntity();
	lt.id = config.loottable_id;
	lt.name = config.loottable_name;
	lt.mincash = config.mincash;
	lt.maxcash = config.maxcash;
	lt.avgcoin = config.avgcoin;
	lt.min_expansion = -1.0f;
	lt.max_expansion = -1.0f;
	lt.content_flags = "";
	lt.content_flags_disabled = "";
	return lt;
}

static LoottableEntriesRepository::LoottableEntries CreateLootTableEntry(
	uint32 loottable_id, const LootTableEntryConfig &entry_config)
{
	LoottableEntriesRepository::LoottableEntries entry;
	entry.loottable_id = loottable_id;
	entry.lootdrop_id = entry_config.lootdrop_id;
	entry.multiplier = entry_config.multiplier;
	entry.probability = entry_config.probability;
	entry.droplimit = entry_config.droplimit;
	entry.mindrop = entry_config.mindrop;
	entry.multiplier_min = entry_config.multiplier_min;
	return entry;
}

static LootdropRepository::Lootdrop CreateLootdrop(uint32 lootdrop_id)
{
	LootdropRepository::Lootdrop ld = LootdropRepository::NewEntity();
	ld.id = lootdrop_id;
	ld.name = "Test Lootdrop " + std::to_string(lootdrop_id);
	ld.min_expansion = -1.0f;
	ld.max_expansion = -1.0f;
	ld.content_flags = "";
	ld.content_flags_disabled = "";
	return ld;
}

static LootdropEntriesRepository::LootdropEntries CreateLootdropEntry(
	uint32 lootdrop_id, const LootDropEntryConfig &de_config)
{
	LootdropEntriesRepository::LootdropEntries lde = LootdropEntriesRepository::NewNpcEntity();
	lde.lootdrop_id = lootdrop_id;
	lde.item_id = de_config.item_id;
	lde.chance = de_config.chance;
	lde.multiplier = de_config.multiplier;
	lde.item_charges = de_config.item_charges;
	lde.equip_item = de_config.equip_item;
	lde.minlevel = de_config.minlevel;
	lde.maxlevel = de_config.maxlevel;
	lde.min_expansion = -1.0f;
	lde.max_expansion = -1.0f;
	lde.content_flags = "";
	lde.content_flags_disabled = "";
	lde.min_looter_level = 0;
	lde.item_loot_lockout_timer = 0;
	return lde;
}

static EQ::ItemData CreateItemData(const LootDropEntryConfig &de_config)
{
	EQ::ItemData item_data;
	memset(&item_data, 0, sizeof(EQ::ItemData));
	item_data.ID = de_config.item_id;
	strncpy(item_data.Name, ("Test Item " + std::to_string(de_config.item_id)).c_str(), sizeof(item_data.Name) - 1);
	item_data.MaxCharges = de_config.item_charges > 0 ? de_config.item_charges : 1;
	item_data.ItemType = EQ::item::ItemTypeCommon;
	item_data.NoDrop = 1;
	item_data.min_expansion = -1.0f;
	item_data.max_expansion = -1.0f;
	return item_data;
}

static NPCType CreateTestNPCType(uint32 loottable_id)
{
	NPCType npc_type;
	memset(&npc_type, 0, sizeof(NPCType));
	strncpy(npc_type.name, "Test NPC", sizeof(npc_type.name) - 1);
	npc_type.npc_id = 1;
	npc_type.level = 1;
	npc_type.max_hp = 100;
	npc_type.cur_hp = 100;
	npc_type.loottable_id = loottable_id;
	npc_type.size = 6.0f;
	npc_type.runspeed = 1.3f;
	npc_type.walkspeed = 0.7f;
	npc_type.gender = 2;
	npc_type.race = 1;
	npc_type.class_ = 1;
	npc_type.bodytype = 1;
	npc_type.deity = 1;
	npc_type.STR = npc_type.STA = npc_type.DEX = npc_type.AGI = npc_type.INT = npc_type.WIS = npc_type.CHA = 75;
	npc_type.min_dmg = 1;
	npc_type.max_dmg = 2;
	npc_type.attack_count = 1;
	npc_type.attack_delay = 30;
	npc_type.prim_melee_type = npc_type.sec_melee_type = 28;
	npc_type.ranged_type = 7;
	npc_type.maxlevel = 1;
	npc_type.spellscale = npc_type.healscale = 1.0f;
	return npc_type;
}

static void InitializeItemsHash(uint32 max_item_id, size_t estimated_item_count)
{
	static std::vector<uint8> test_items_buffer;
	if (!database.items_hash && max_item_id > 0)
	{
		uint32 item_count = static_cast<uint32>(estimated_item_count);
		if (item_count == 0)
			item_count = 100;
		uint32 size = static_cast<uint32>(EQ::FixedMemoryHashSet<EQ::ItemData>::estimated_size(item_count, max_item_id + 100));
		test_items_buffer.resize(size);
		memset(test_items_buffer.data(), 0, size);
		database.items_hash = std::make_unique<EQ::FixedMemoryHashSet<EQ::ItemData>>(
			test_items_buffer.data(), size, item_count, max_item_id + 100);
	}
}

static uint32 FindMaxItemID(const LootTableTestConfig &config)
{
	uint32 max_item_id = 0;
	for (const auto &drop_pair : config.lootdrops)
	{
		for (const auto &de_config : drop_pair.second)
		{
			if (de_config.item_id > max_item_id)
			{
				max_item_id = de_config.item_id;
			}
		}
	}
	return max_item_id;
}

// Helper to format vector as string for error messages
std::string FormatItemList(const std::vector<uint32> &items)
{
	if (items.empty())
	{
		return "[]";
	}
	std::ostringstream oss;
	oss << "[";
	for (size_t i = 0; i < items.size(); ++i)
	{
		if (i > 0)
			oss << ", ";
		oss << items[i];
	}
	oss << "]";
	return oss.str();
}

void RunLootTableTest(const LootTableTestConfig &config)
{
	Zone *original_zone = zone;

	// Initialize database items hash
	uint32 max_item_id = FindMaxItemID(config);
	InitializeItemsHash(max_item_id, config.lootdrops.size() * 10);

	// Create test zone
	Zone test_zone(1, "testzone", 0xFFFFFFFF);
	zone = &test_zone;

	// Configure loot table
	TestZoneHelper::AddTestLootTable(&test_zone, CreateLootTable(config));

	// Configure loot table entries
	for (const auto &entry_config : config.loottable_entries)
	{
		TestZoneHelper::AddTestLootTableEntry(&test_zone, CreateLootTableEntry(config.loottable_id, entry_config));
	}

	// Configure loot drops and items
	for (const auto &drop_pair : config.lootdrops)
	{
		TestZoneHelper::AddTestLootdrop(&test_zone, CreateLootdrop(drop_pair.first));

		for (const auto &de_config : drop_pair.second)
		{
			TestZoneHelper::AddTestLootdropEntry(&test_zone, CreateLootdropEntry(drop_pair.first, de_config));
			TestDatabaseHelper::AddTestItem(&database, CreateItemData(de_config));
		}
	}

	// Create test NPC type
	NPCType npc_type = CreateTestNPCType(config.loottable_id);

	// Collection map: sorted item list -> count
	std::map<std::vector<uint32>, uint32> result_counts;

	// Run 10000 iterations
	const int iterations = 10000;
	for (int i = 0; i < iterations; ++i)
	{
		// Create NPC for each iteration
		glm::vec4 position(0.0f, 0.0f, 0.0f, 0.0f);
		NPC *npc = new NPC(&npc_type, nullptr, position, GravityBehavior::Ground);

		// Clear any existing loot
		npc->ClearLootItems();

		// Call AddLootTable
		npc->AddLootTable(config.loottable_id, false);

		// Extract item IDs from loot (including duplicates) and sort them
		std::vector<uint32> item_ids;
		const auto &loot_items = npc->GetLootItems();
		for (const auto *item : loot_items)
		{
			if (item)
			{
				item_ids.push_back(item->item_id);
			}
		}
		std::sort(item_ids.begin(), item_ids.end());

		// Increment count for this pattern
		result_counts[item_ids]++;

		// Clean up NPC
		delete npc;
	}

	// Restore original zone
	zone = original_zone;

	// Calculate observed percentages and validate
	for (const auto &expected_pair : config.expected_percentages)
	{
		const auto &expected_pattern = expected_pair.first;
		float expected_percent = expected_pair.second;

		uint32 observed_count = 0;
		auto it = result_counts.find(expected_pattern);
		if (it != result_counts.end())
		{
			observed_count = it->second;
		}

		float observed_percent = (static_cast<float>(observed_count) / static_cast<float>(iterations)) * 100.0f;
		float diff = std::abs(observed_percent - expected_percent);

		std::string pattern_str = FormatItemList(expected_pattern);
		std::string msg = fmt::format(
			"Pattern {}: Expected {:.2f}%, Observed {:.2f}%, Difference {:.2f}% (tolerance: {:.2f}%)",
			pattern_str,
			expected_percent,
			observed_percent,
			diff,
			config.variance_tolerance);

		TEST_ASSERT_MSG(
			diff <= config.variance_tolerance,
			msg.c_str());
	}
}
