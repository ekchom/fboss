// Copyright 2004-present Facebook. All Rights Reserved.

#include "fboss/agent/platforms/sai/SaiBcmWedge100PlatformPort.h"

#include "fboss/agent/platforms/common/utils/Wedge100LedUtils.h"

#include <tuple>

namespace facebook::fboss {

void SaiBcmWedge100PlatformPort::linkStatusChanged(bool up, bool adminUp) {
  // TODO(pshaikh): add support for compact mode LED
  uint32_t phyPortId = getPhysicalPortId();
  auto ledAndIndex = getLedAndIndex(phyPortId);
  if (!ledAndIndex) {
    return;
  }

  auto [led, index] = ledAndIndex.value();
  auto lanes = getHwPortLanes(getCurrentProfile());

  uint32_t status = static_cast<uint32_t>(Wedge100LedUtils::getLEDColor(
      static_cast<PortID>(phyPortId),
      static_cast<int>(lanes.size()),
      up,
      adminUp));
  setLEDState(led, index, status);
}

void SaiBcmWedge100PlatformPort::externalState(PortLedExternalState lfs) {
  uint32_t phyPortId = getPhysicalPortId();
  auto ledAndIndex = getLedAndIndex(phyPortId);
  if (!ledAndIndex) {
    return;
  }

  auto [led, index] = ledAndIndex.value();

  uint32_t status = static_cast<uint32_t>(Wedge100LedUtils::getLEDColor(
      lfs, static_cast<Wedge100LedUtils::LedColor>(getCurrentLedState())));
  setLEDState(led, index, status);
}

std::optional<std::tuple<uint32_t, uint32_t>>
SaiBcmWedge100PlatformPort::getLedAndIndex(uint32_t phyPortId) {
  PortID port = static_cast<PortID>(phyPortId);
  int index = Wedge100LedUtils::getPortIndex(port);
  auto led = Wedge100LedUtils::getLEDProcessorNumber(port);
  if (!led) {
    return std::nullopt;
  }
  return std::make_tuple(led.value(), index);
}
} // namespace facebook::fboss
