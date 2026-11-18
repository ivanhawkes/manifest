#pragma once

#include <filesystem>
#include <memory>

#include <jettison/host/Session.h>

namespace Manifest::Host
{
// Starts Manifest on one content source, a directory or a pack, mounted read-only and alone: no
// user mods, no writable overlay, nothing found from the working directory. The source must be the
// Manifest package: a mod.yaml naming it, with its release and engine requirement given, any
// package it requires satisfied, and its definitions loaded and checked as one content set
// (Manifest::Content::LoadContent). The world it starts is empty. Throws Jettison::Host::StartupError
// naming the phase, the source and the cause, having released everything it acquired.
std::unique_ptr<Jettison::Host::Session> Start(const std::filesystem::path &content);
} // namespace Manifest::Host
