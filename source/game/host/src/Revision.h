#pragma once

#include <string_view>

namespace Manifest::Host
{
// Written into the build tree at every build (cmake/Revision.cmake).
extern const std::string_view kGameRevision;
extern const std::string_view kSdkRevision;
} // namespace Manifest::Host
