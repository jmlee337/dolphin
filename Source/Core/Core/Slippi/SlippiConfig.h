#pragma once

namespace Melee
{
enum class Version
{
  NTSC,
  TwentyXX,
  UPTM,
  MEX,
  OTHER,
};
}

namespace Slippi
{
enum class Chat
{
  ON,
  DIRECT_ONLY,
  OFF
};

enum class PortMapping
{
  OFF,
  UPNP,
  NATPMP
};

struct Config
{
  Melee::Version melee_version;
  bool oc_enable = true;
  float oc_factor = 1.0f;
  std::string slippi_input = "";
};
}  // namespace Slippi
