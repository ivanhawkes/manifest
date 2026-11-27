#include <doctest/doctest.h>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <jettison/ui/StringTable.h>
#include <jettison/vfs/FileSystem.h>

namespace
{
using Jettison::Ui::StringTable;

// A language's texts by key, across its files and mounts.
using Table = std::map<std::string, std::string, std::less<>>;
using Tables = std::map<std::string, Table, std::less<>>;

// The languages the client ships (docs/client.md, "Languages"), each with every key English has.
constexpr std::array<std::string_view, 4> kLanguages{"en", "fr", "de", "ja"};

// Counts enough to meet every plural category of a language: the CLDR's rules for a whole number
// repeat within the first two hundred.
constexpr std::int64_t kPluralCounts = 200;

constexpr std::array<std::string_view, 6> kPluralCategories{"zero", "one",  "two",
                                                            "few",  "many", "other"};

void Flatten(const YAML::Node &node, const std::string &prefix, Table &table)
{
	for (const auto &entry : node)
	{
		const auto name = entry.first.as<std::string>();
		const std::string key = prefix.empty() ? name : std::format("{}.{}", prefix, name);
		if (entry.second.IsMap())
			Flatten(entry.second, key, table);
		else
			table[key] = entry.second.as<std::string>();
	}
}

// Every language's tables under strings/<language>/, each file read in every mount, as StringTable
// reads them.
Tables Read(const Jettison::Vfs::FileSystem &files)
{
	const std::string directory = std::string{StringTable::kDirectory} + "/";
	Tables tables;
	for (const std::string &path : files.Enumerate(StringTable::kDirectory))
	{
		const std::string language =
		    path.substr(directory.size(), path.find('/', directory.size()) - directory.size());
		for (const Jettison::Vfs::Layer &layer : files.ReadLayers(path))
		{
			YAML::Node root = YAML::Load(
			    std::string(reinterpret_cast<const char *>(layer.bytes.data()), layer.bytes.size()));
			root.remove("version");
			Flatten(root, "", tables[language]);
		}
	}
	return tables;
}

// A text's format arguments, {0} and {1:>8}, and its data expressions, {{ship.shipId}}.
std::set<std::string> Placeholders(const std::string &text)
{
	static const std::regex kPlaceholder{R"(\{\{[^}]*\}\}|\{[0-9][^}]*\})"};
	std::set<std::string> found;
	for (std::sregex_iterator match{text.begin(), text.end(), kPlaceholder};
	     match != std::sregex_iterator{}; ++match)
		found.insert(match->str());
	return found;
}

// The plural categories a key with plural forms needs in the language.
std::set<std::string, std::less<>> PluralForms(std::string_view language)
{
	std::set<std::string, std::less<>> forms{"other"};
	for (std::int64_t count = 0; count <= kPluralCounts; ++count)
		forms.emplace(StringTable::PluralCategory(language, count));
	return forms;
}

std::string Join(const std::set<std::string> &placeholders)
{
	std::string joined;
	for (const std::string &placeholder : placeholders)
		joined += (joined.empty() ? "" : " ") + placeholder;
	return joined.empty() ? "none" : joined;
}

// What the language's table lacks or has beyond the base's, and each text whose placeholders differ
// from the base's, as lines naming the language and the key. A key with plural forms (key.one,
// key.other) needs the forms of the language's plural rule, each with placeholders the base's forms
// have.
std::vector<std::string> Compare(const Table &base, std::string_view language, const Table &table)
{
	// The base's keys with plural forms, each with the placeholders its forms name.
	std::map<std::string, std::set<std::string>, std::less<>> plurals;
	const auto pluralOf = [&base](std::string_view key) -> std::optional<std::string>
	{
		const std::size_t dot = key.rfind('.');
		if (dot == std::string_view::npos ||
		    std::ranges::find(kPluralCategories, key.substr(dot + 1)) == kPluralCategories.end())
			return std::nullopt;
		std::string group{key.substr(0, dot)};
		if (!base.contains(group + ".other"))
			return std::nullopt;
		return group;
	};
	for (const auto &[key, text] : base)
		if (const std::optional<std::string> group = pluralOf(key))
			plurals[*group].merge(Placeholders(text));

	std::vector<std::string> problems;
	const auto forms = PluralForms(language);
	for (const auto &[key, text] : base)
		if (!pluralOf(key) && !table.contains(key))
			problems.push_back(std::format("{}: {} is missing", language, key));
	for (const auto &[group, placeholders] : plurals)
		for (const std::string &form : forms)
			if (!table.contains(std::format("{}.{}", group, form)))
				problems.push_back(std::format("{}: {}.{} is missing", language, group, form));
	for (const auto &[key, text] : table)
	{
		const std::set<std::string> named = Placeholders(text);
		if (const std::optional<std::string> group = pluralOf(key))
		{
			if (!forms.contains(key.substr(group->size() + 1)))
				problems.push_back(
				    std::format("{}: {} is not a plural form of the language", language, key));
			else if (!std::ranges::includes(plurals[*group], named))
				problems.push_back(
				    std::format(
				        "{}: {} names {}, its base's forms {}", language, key, Join(named),
				        Join(plurals[*group])));
			continue;
		}
		const auto found = base.find(key);
		if (found == base.end())
			problems.push_back(std::format("{}: {} is not in the base's table", language, key));
		else if (const std::set<std::string> expected = Placeholders(found->second); named != expected)
			problems.push_back(
			    std::format(
			        "{}: {} names {}, the base's {}", language, key, Join(named), Join(expected)));
	}
	return problems;
}
} // namespace


// The four languages' check (L43): every key in every language the client ships, the engine's own
// texts (read from the engine's assets) with Manifest's, naming the same placeholders as English.
TEST_CASE("Every language the client ships has every key, with the base's placeholders")
{
	Jettison::Vfs::FileSystem files;
	files.AddMount({"engine", MANIFEST_ENGINE_ASSETS});
	files.AddMount({"manifest", MANIFEST_MOD_DIRECTORY});
	const Tables tables = Read(files);
	for (const std::string_view language : kLanguages)
	{
		CAPTURE(language);
		REQUIRE(tables.contains(language));
	}
	const Table &base = tables.find(StringTable::kBaseLanguage)->second;
	CHECK(base.contains("manifest.start.provision"));
	CHECK(base.contains("saves.title"));
	for (const std::string_view language : kLanguages)
		for (const std::string &problem : Compare(base, language, tables.find(language)->second))
			FAIL_CHECK(problem);
}


// The engine ships its own text (the saves screen, the number format and its reasons) in all four
// languages, so no game carries a copy: its assets alone, with no content mounted, are complete.
TEST_CASE("The engine's own tables are in every language the client ships, with no content mounted")
{
	Jettison::Vfs::FileSystem files;
	files.AddMount({"engine", MANIFEST_ENGINE_ASSETS});
	const Tables tables = Read(files);
	for (const std::string_view language : kLanguages)
	{
		CAPTURE(language);
		REQUIRE(tables.contains(language));
		CHECK(tables.find(language)->second.contains("saves.title"));
	}
	const Table &base = tables.find(StringTable::kBaseLanguage)->second;
	for (const std::string_view language : kLanguages)
		for (const std::string &problem : Compare(base, language, tables.find(language)->second))
			FAIL_CHECK(problem);
}


TEST_CASE("The languages check names a missing key, a placeholder changed and a plural form")
{
	const Table base{
	    {"market.docked", "Docked at {0}"},
	    {"market.title", "Market"},
	    {"market.crates.one", "{0} crate"},
	    {"market.crates.other", "{0} crates"},
	    {"market.ship", "{{ship.shipId}}"}};
	CHECK(Compare(base, "en", base).empty());

	// Japanese has one plural category, so other alone.
	const Table japanese{
	    {"market.docked", "{0}に停泊中"},
	    {"market.title", "市場"},
	    {"market.crates.other", "{0}箱"},
	    {"market.ship", "{{ship.shipId}}"}};
	CHECK(Compare(base, "ja", japanese).empty());

	const Table french{{"market.docked", "Amarré à {1}"},      {"market.crates.one", "{0} caisse"},
	                   {"market.crates.other", "{0} caisses"}, {"market.crates.few", "{0} caisses"},
	                   {"market.ship", "{{ship.hullId}}"},     {"market.extra", "En trop"}};
	CHECK(
	    Compare(base, "fr", french) ==
	    std::vector<std::string>{
	        "fr: market.title is missing", "fr: market.crates.few is not a plural form of the language",
	        "fr: market.docked names {1}, the base's {0}",
	        "fr: market.extra is not in the base's table",
	        "fr: market.ship names {{ship.hullId}}, the base's {{ship.shipId}}"});

	// A language with more categories than the base's needs each of them.
	const Table russian{
	    {"market.docked", "{0}"},
	    {"market.title", "Рынок"},
	    {"market.crates.one", "{0} ящик"},
	    {"market.crates.other", "{0} ящика"},
	    {"market.ship", "{{ship.shipId}}"}};
	CHECK(
	    Compare(base, "ru", russian) ==
	    std::vector<std::string>{
	        "ru: market.crates.few is missing", "ru: market.crates.many is missing"});
}
