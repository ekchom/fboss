// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "fboss/agent/state/RouteNextHop.h"

#include <boost/container/flat_map.hpp>

namespace facebook {
namespace fboss {
class SwSwitch;
class ResolvedNextHopProbe;

class ResolvedNexthopProbeScheduler {
  /*
   * manages probes to l3 resolved next hops, for every route delta, resolved
   * next hop monitor triggers scheduler a probe is removed if no route
   * references resolved next hop a probe is added if no probe exists to that
   * resolved next hop
   */
 public:
  explicit ResolvedNexthopProbeScheduler(SwSwitch* sw);
  void processChangedResolvedNexthops(
      std::vector<ResolvedNextHop> added,
      std::vector<ResolvedNextHop> removed);

  boost::container::flat_map<ResolvedNextHop, uint32_t>
  resolvedNextHop2UseCount() const {
    return resolvedNextHop2UseCount_;
  }

 private:
  SwSwitch* sw_{nullptr};
  boost::container::flat_map<ResolvedNextHop, uint32_t>
      resolvedNextHop2UseCount_;
};

} // namespace fboss
} // namespace facebook
