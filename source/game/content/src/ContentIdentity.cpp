#include <manifest/content/ContentIdentity.h>

#include <format>
#include <string>
#include <string_view>

#include <jettison/core/Hash.h>
#include <jettison/serialise/Binary.h>
#include <jettison/sim/DefinitionRegistry.h>

#include <manifest/content/Definitions.h>

namespace Manifest::Content
{
ContentIdentity BuildContentIdentity(
    const Jettison::Sim::DefinitionRegistry &registry, std::string_view scenario)
{
	Jettison::Serialise::BinaryWriter writer{"Manifest.ContentManifest"};
	for (const DefinitionType &definition : DefinitionTypes())
	{
		const std::vector<std::string> names = registry.Names(definition.type);
		const Jettison::Serialise::FormatVersion version = definition.migrations.Current();
		writer.WriteName(definition.migrations.Format());
		writer.WriteUnsigned(version.version, 4);
		writer.WriteUnsigned(names.size(), 4);
		for (const std::string &name : names)
		{
			writer.WriteName(name);
			writer.WriteBytes(
			    Jettison::Serialise::BinaryArchive::Write(
			        *registry.Find(definition.type, name), version));
		}
	}
	writer.WriteName(scenario);

	ContentIdentity identity;
	identity.manifest = writer.Finish(kContentManifestFormat);
	identity.fingerprint = Jettison::Core::Fnv1a64(
	    std::string_view{
	        reinterpret_cast<const char *>(identity.manifest.data()), identity.manifest.size()});
	return identity;
}


std::string FirstDifference(std::span<const std::byte> was, std::span<const std::byte> is)
{
	using Jettison::Serialise::BinaryReader;
	constexpr std::string_view kFormat = "Manifest.ContentManifest";
	BinaryReader before{was, kFormat, kContentManifestFormat};
	BinaryReader after{is, kFormat, kContentManifestFormat};

	// The same build wrote both, so the types are DefinitionTypes' in its order.
	for (std::size_t type = 0; type < DefinitionTypes().size(); ++type)
	{
		std::string name{before.ReadName()};
		if (after.ReadName() != name || before.ReadUnsigned(4) != after.ReadUnsigned(4))
			return name;
		std::uint64_t beforeLeft = before.ReadUnsigned(4);
		std::uint64_t afterLeft = after.ReadUnsigned(4);

		// Each record in name order: the first name one holds and the other does not, or holds
		// otherwise, is the difference.
		while (beforeLeft > 0 && afterLeft > 0)
		{
			const std::string record{before.ReadName()};
			const std::string other{after.ReadName()};
			if (record != other)
				return record < other
				           ? std::format("{} {}, which only the world's content has", name, record)
				           : std::format("{} {}, which only this content has", name, other);
			if (before.ReadBytes() != after.ReadBytes())
				return std::format("{} {}", name, record);
			--beforeLeft;
			--afterLeft;
		}
		if (beforeLeft > 0)
			return std::format("{} {}, which only the world's content has", name, before.ReadName());
		if (afterLeft > 0)
			return std::format("{} {}, which only this content has", name, after.ReadName());
	}
	const std::string scenario{before.ReadName()};
	return after.ReadName() == scenario ? std::string{} : std::string{"the scenario"};
}
} // namespace Manifest::Content
