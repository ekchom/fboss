/*
 *  Copyright (c) 2004-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include "fboss/agent/hw/test/HwTestRouteUtils.h"

#include "fboss/agent/hw/bcm/BcmAddressFBConvertors.h"
#include "fboss/agent/hw/bcm/BcmSwitch.h"

extern "C" {
#include <bcm/l3.h>
}

namespace facebook::fboss::utility {

bcm_l3_route_t getBcmRoute(int unit, const folly::CIDRNetwork& cidrNetwork) {
  bcm_l3_route_t route;
  bcm_l3_route_t_init(&route);

  const auto& [networkIP, netmask] = cidrNetwork;
  if (networkIP.isV4()) {
    route.l3a_subnet = networkIP.asV4().toLongHBO();
    route.l3a_ip_mask =
        folly::IPAddressV4(folly::IPAddressV4::fetchMask(netmask)).toLongHBO();
  } else { // IPv6
    ipToBcmIp6(networkIP, &route.l3a_ip6_net);
    memcpy(
        &route.l3a_ip6_mask,
        folly::IPAddressV6::fetchMask(netmask).data(),
        sizeof(route.l3a_ip6_mask));
    route.l3a_flags = BCM_L3_IP6;
  }
  CHECK_EQ(bcm_l3_route_get(unit, &route), 0);
  return route;
}

bool isEgressToIp(
    int unit,
    RouterID rid,
    folly::IPAddress addr,
    bcm_if_t egress) {
  bcm_l3_host_t host;
  bcm_l3_host_t_init(&host);
  if (addr.isV4()) {
    host.l3a_ip_addr = addr.asV4().toLongHBO();
  } else {
    memcpy(
        &host.l3a_ip6_addr,
        addr.asV6().toByteArray().data(),
        sizeof(host.l3a_ip6_addr));
    host.l3a_flags |= BCM_L3_IP6;
  }
  host.l3a_vrf = rid;
  bcm_l3_host_find(unit, &host);
  return egress == host.l3a_intf;
}

std::optional<cfg::AclLookupClass> getHwRouteClassID(
    const HwSwitch* hwSwitch,
    RouterID /*rid*/,
    const folly::CIDRNetwork& cidrNetwork) {
  auto bcmSwitch = static_cast<const BcmSwitch*>(hwSwitch);

  bcm_l3_route_t route = getBcmRoute(bcmSwitch->getUnit(), cidrNetwork);

  return route.l3a_lookup_class == 0
      ? std::nullopt
      : std::optional(cfg::AclLookupClass{route.l3a_lookup_class});
}

bool isHwRouteToCpu(
    const HwSwitch* hwSwitch,
    RouterID /*rid*/,
    const folly::CIDRNetwork& cidrNetwork) {
  auto bcmSwitch = static_cast<const BcmSwitch*>(hwSwitch);

  bcm_l3_route_t route = getBcmRoute(bcmSwitch->getUnit(), cidrNetwork);

  bcm_l3_egress_t egress;
  bcm_l3_egress_t_init(&egress);
  CHECK_EQ(bcm_l3_egress_get(bcmSwitch->getUnit(), route.l3a_intf, &egress), 0);

  return (egress.flags & BCM_L3_L2TOCPU) && (egress.flags | BCM_L3_COPY_TO_CPU);
}

bool isHwRouteMultiPath(
    const HwSwitch* hwSwitch,
    RouterID /*rid*/,
    const folly::CIDRNetwork& cidrNetwork) {
  auto bcmSwitch = static_cast<const BcmSwitch*>(hwSwitch);

  bcm_l3_route_t route = getBcmRoute(bcmSwitch->getUnit(), cidrNetwork);

  return route.l3a_flags & BCM_L3_MULTIPATH;
}

bool isHwRouteToNextHop(
    const HwSwitch* hwSwitch,
    RouterID rid,
    const folly::CIDRNetwork& cidrNetwork,
    folly::IPAddress ip,
    std::optional<uint64_t> weight) {
  auto bcmSwitch = static_cast<const BcmSwitch*>(hwSwitch);

  bcm_l3_route_t route = getBcmRoute(bcmSwitch->getUnit(), cidrNetwork);

  if (route.l3a_flags & BCM_L3_MULTIPATH) {
    // check for member to ip, interface
    bcm_l3_egress_ecmp_t ecmp;
    bcm_l3_egress_ecmp_t_init(&ecmp);
    int count = 0;
    bcm_l3_ecmp_get(bcmSwitch->getUnit(), &ecmp, 0, nullptr, &count);
    std::vector<bcm_l3_ecmp_member_t> members;
    members.resize(count);
    CHECK_EQ(
        bcm_l3_ecmp_get(
            bcmSwitch->getUnit(),
            &ecmp,
            members.size(),
            members.data(),
            &count),
        0);
    bool found = false;
    bcm_l3_ecmp_member_t foundMember;
    bcm_l3_ecmp_member_t_init(&foundMember);
    for (auto member : members) {
      if (!isEgressToIp(bcmSwitch->getUnit(), rid, ip, member.egress_if)) {
        continue;
      }
      found = true;
      foundMember = member;
      break;
    }

    if (!weight) {
      return found;
    }

    return weight.value() ==
        std::count_if(
               members.begin(), members.end(), [foundMember](auto member) {
                 return member.egress_if == foundMember.egress_if;
               });
  }
  // check for next hop
  return isEgressToIp(bcmSwitch->getUnit(), rid, ip, route.l3a_intf);
}

} // namespace facebook::fboss::utility
