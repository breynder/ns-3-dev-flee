/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 University of Washington
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <vector>

#include "ns3/log.h"
#include "ns3/ptr.h"
#include "ns3/names.h"
#include "ns3/node.h"
#include "ns3/ipv6.h"
#include "ns3/ipv6-route.h"
#include "ns3/ipv6-list-routing.h"
#include "ns3/assert.h"
#include "ns3/ipv6-address.h"
#include "ns3/ipv6-routing-protocol.h"
#include "ns3/flee-routing-protocol.h"

#include "flee-helper.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("FleeHelper");

FleeHelper::FleeHelper ()
{
}

FleeHelper::FleeHelper (const FleeHelper &o)
{
}

FleeHelper*
FleeHelper::Copy (void) const
{
  return new FleeHelper (*this);
}

Ptr<Ipv6RoutingProtocol>
FleeHelper::Create (Ptr<Node> node) const
{
  return CreateObject<FleeRouting> ();
}

Ptr<Ipv6RoutingProtocol>
FleeHelper::GetRouting (Ptr<Ipv6> ipv6) const
{
  NS_LOG_FUNCTION (this);
  Ptr<Ipv6RoutingProtocol> ipv6rp = ipv6->GetRoutingProtocol ();
  NS_ASSERT_MSG (ipv6rp, "No routing protocol associated with Ipv6");
  if (DynamicCast<FleeRouting> (ipv6rp))
    {
      NS_LOG_LOGIC ("Static routing found as the main IPv4 routing protocol.");
      return DynamicCast<FleeRouting> (ipv6rp); 
    } 
  if (DynamicCast<Ipv6ListRouting> (ipv6rp))
    {
      Ptr<Ipv6ListRouting> lrp = DynamicCast<Ipv6ListRouting> (ipv6rp);
      int16_t priority;
      for (uint32_t i = 0; i < lrp->GetNRoutingProtocols ();  i++)
        {
          NS_LOG_LOGIC ("Searching for static routing in list");
          Ptr<Ipv6RoutingProtocol> temp = lrp->GetRoutingProtocol (i, priority);
          if (DynamicCast<FleeRouting> (temp))
            {
              NS_LOG_LOGIC ("Found static routing in list");
              return DynamicCast<FleeRouting> (temp);
            }
        }
    }
  NS_LOG_LOGIC ("Static routing not found");
  return 0;
}

} // namespace ns3
