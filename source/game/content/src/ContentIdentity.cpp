#include <manifest/content/ContentIdentity.h>

#include <string>
#include <string_view>

#include <jettison/core/Hash.h>
#include <jettison/serialise/Binary.h>
#include <jettison/sim/DefinitionRegistry.h>

#include <manifest/content/Definitions.h>

namespace Manifest::Content
{
ContentIdentity BuildContentIdentity(const Jettison::Sim::DefinitionRegistry &registry)
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

	ContentIdentity identity;
	identity.manifest = writer.Finish(kContentManifestFormat);
	identity.fingerprint = Jettison::Core::Fnv1a64(
	    std::string_view{
	        reinterpret_cast<const char *>(identity.manifest.data()), identity.manifest.size()});
	return identity;
}
} // namespace Manifest::Content
