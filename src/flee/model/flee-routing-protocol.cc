/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007-2009 Strasbourg University
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
 *
 * Author: Brecht Reynders <brecht.reynders@esat.kuleuven.be>
 */

#include <iomanip>
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/ipv6-route.h"
#include "ns3/net-device.h"
#include "ns3/names.h"
#include "ns3/uinteger.h"
#include "ns3/string.h"
#include "ns3/pointer.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"

#include "ns3/ipv6-l3-protocol.h"
#include <ns3/ipv6-routing-table-entry.h>
#include "flee-routing-protocol.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("FleeRouting");

NS_OBJECT_ENSURE_REGISTERED (FleeRouting);

TypeId FleeRouting::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::FleeRouting")
    .SetParent<Ipv6RoutingProtocol> ()
    .SetGroupName ("Internet")
    .AddConstructor<FleeRouting> ()
    .AddAttribute ("Rand",
                   "Access to the underlying UniformRandomVariable",
                   StringValue ("ns3::UniformRandomVariable"),
                   MakePointerAccessor (&FleeRouting::m_var),
                   MakePointerChecker<UniformRandomVariable> ())
		.AddAttribute ("Sink",
										"Set distance to the sink to 0 to make this sink",
										UintegerValue (100),
										MakeUintegerAccessor (&FleeRouting::m_distanceToSink),
										MakeUintegerChecker<uint8_t> ())
  ;
  return tid;
}

FleeRouting::FleeRouting ()
  : m_ipv6 (0)
{
  NS_LOG_FUNCTION_NOARGS ();

	Simulator::ScheduleNow (&FleeRouting::DoInitialize, this);
}

void FleeRouting::DoInitialize (void)
{
	if (m_distanceToSink == 0)
		Simulator::ScheduleNow ( &FleeRouting::Hello, this);
}


FleeRouting::~FleeRouting ()
{
  NS_LOG_FUNCTION_NOARGS ();
}

void FleeRouting::SetIpv6 (Ptr<Ipv6> ipv6)
{
  NS_LOG_FUNCTION (this << ipv6);
  NS_ASSERT (m_ipv6 == 0 && ipv6 != 0);
  uint32_t i = 0;
  m_ipv6 = ipv6;

  for (i = 0; i < m_ipv6->GetNInterfaces (); i++)
    {
      if (m_ipv6->IsUp (i))
        {
          NotifyInterfaceUp (i);
        }
      else
        {
          NotifyInterfaceDown (i);
        }
    }
}

// Formatted like output of "route -n" command
void
FleeRouting::PrintRoutingTable (Ptr<OutputStreamWrapper> stream) const
{
  NS_LOG_FUNCTION (this << stream);
  std::ostream* os = stream->GetStream ();

  *os << "Node: " << m_ipv6->GetObject<Node> ()->GetId ()
      << ", Time: " << Now().As (Time::S)
      << ", Local time: " << GetObject<Node> ()->GetLocalTime ().As (Time::S)
      << ", FleeRouting table" << std::endl;

  if (GetNRoutes () > 0)
    {
      *os << "Destination                    Next Hop                   Flag Met Ref Use If" << std::endl;
      for (uint32_t j = 0; j < GetNRoutes (); j++)
        {
          std::ostringstream dest, gw, mask, flags;
          Ipv6RoutingTableEntry route = GetRoute (j);
          dest << route.GetDest () << "/" << int(route.GetDestNetworkPrefix ().GetPrefixLength ());
          *os << std::setiosflags (std::ios::left) << std::setw (31) << dest.str ();
          gw << route.GetGateway ();
          *os << std::setiosflags (std::ios::left) << std::setw (27) << gw.str ();
          flags << "U";
          if (route.IsHost ())
            {
              flags << "H";
            }
          else if (route.IsGateway ())
            {
              flags << "G";
            }
          *os << std::setiosflags (std::ios::left) << std::setw (5) << flags.str ();
          *os << std::setiosflags (std::ios::left) << std::setw (4) << GetMetric (j);
          // Ref ct not implemented
          *os << "-" << "   ";
          // Use not implemented
          *os << "-" << "   ";
          if (Names::FindName (m_ipv6->GetNetDevice (route.GetInterface ())) != "")
            {
              *os << Names::FindName (m_ipv6->GetNetDevice (route.GetInterface ()));
            }
          else
            {
              *os << route.GetInterface ();
            }
          *os << std::endl;
        }
    }
  *os << std::endl;
}

void FleeRouting::AddHostRouteTo (Ipv6Address dst, Ipv6Address nextHop, uint32_t interface, Ipv6Address prefixToUse, uint32_t metric)
{
  NS_LOG_FUNCTION (this << dst << nextHop << interface << prefixToUse << metric);
  if (nextHop.IsLinkLocal())
    {
      NS_LOG_WARN ("FleeRouting::AddHostRouteTo - Next hop should be link-local");
    }

  AddNetworkRouteTo (dst, Ipv6Prefix::GetOnes (), nextHop, interface, prefixToUse, metric);
}

void FleeRouting::AddHostRouteTo (Ipv6Address dst, uint32_t interface, uint32_t metric)
{
  NS_LOG_FUNCTION (this << dst << interface << metric);
  AddNetworkRouteTo (dst, Ipv6Prefix::GetOnes (), interface, metric);
}

void FleeRouting::AddNetworkRouteTo (Ipv6Address network, Ipv6Prefix networkPrefix, Ipv6Address nextHop, uint32_t interface, uint32_t metric)
{
  NS_LOG_FUNCTION (this << network << networkPrefix << nextHop << interface << metric);
  Ipv6RoutingTableEntry* route = new Ipv6RoutingTableEntry ();
  *route = Ipv6RoutingTableEntry::CreateNetworkRouteTo (network, networkPrefix, nextHop, interface);
  m_networkRoutes.push_back (std::make_pair (route, metric));
}

void FleeRouting::AddNetworkRouteTo (Ipv6Address network, Ipv6Prefix networkPrefix, Ipv6Address nextHop, uint32_t interface, Ipv6Address prefixToUse, uint32_t metric)
{
  NS_LOG_FUNCTION (this << network << networkPrefix << nextHop << interface << prefixToUse << metric);
  if (nextHop.IsLinkLocal())
    {
      NS_LOG_WARN ("FleeRouting::AddNetworkRouteTo - Next hop should be link-local");
    }

  Ipv6RoutingTableEntry* route = new Ipv6RoutingTableEntry ();
  *route = Ipv6RoutingTableEntry::CreateNetworkRouteTo (network, networkPrefix, nextHop, interface, prefixToUse);
  m_networkRoutes.push_back (std::make_pair (route, metric));
}

void FleeRouting::AddNetworkRouteTo (Ipv6Address network, Ipv6Prefix networkPrefix, uint32_t interface, uint32_t metric)
{
  NS_LOG_FUNCTION (this << network << networkPrefix << interface);
  Ipv6RoutingTableEntry* route = new Ipv6RoutingTableEntry ();
  *route = Ipv6RoutingTableEntry::CreateNetworkRouteTo (network, networkPrefix, interface);
  m_networkRoutes.push_back (std::make_pair (route, metric));
}

void FleeRouting::SetDefaultRoute (Ipv6Address nextHop, uint32_t interface, Ipv6Address prefixToUse, uint32_t metric)
{
  NS_LOG_FUNCTION (this << nextHop << interface << prefixToUse);
  AddNetworkRouteTo (Ipv6Address ("::"), Ipv6Prefix::GetZero (), nextHop, interface, prefixToUse, metric);
}

bool FleeRouting::HasNetworkDest (Ipv6Address network, uint32_t interfaceIndex)
{
  NS_LOG_FUNCTION (this << network << interfaceIndex);

  /* in the network table */
  for (NetworkRoutesI j = m_networkRoutes.begin (); j != m_networkRoutes.end (); j++)
    {
      Ipv6RoutingTableEntry* rtentry = j->first;
      Ipv6Prefix prefix = rtentry->GetDestNetworkPrefix ();
      Ipv6Address entry = rtentry->GetDestNetwork ();

      if (prefix.IsMatch (network, entry) && rtentry->GetInterface () == interfaceIndex)
        {
          return true;
        }
    }

  /* beuh!!! not route at all */
  return false;
}

Ptr<Ipv6Route> FleeRouting::LookupStatic (Ipv6Address dst, Ptr<NetDevice> interface)
{
  NS_LOG_FUNCTION (this << dst << interface);
  Ptr<Ipv6Route> rtentry = 0;
  uint16_t longestMask = 0;
  uint32_t shortestMetric = 0xffffffff;

  for (NetworkRoutesI it = m_networkRoutes.begin (); it != m_networkRoutes.end (); it++)
    {
      Ipv6RoutingTableEntry* j = it->first;
      uint32_t metric = it->second;
      Ipv6Prefix mask = j->GetDestNetworkPrefix ();
      uint16_t maskLen = mask.GetPrefixLength ();
      Ipv6Address entry = j->GetDestNetwork ();

      NS_LOG_LOGIC ("Searching for route to " << dst << ", mask length " << maskLen << ", metric " << metric);

      if (mask.IsMatch (dst, entry))
        {
          NS_LOG_LOGIC ("Found global network route " << *j << ", mask length " << maskLen << ", metric " << metric);

          /* if interface is given, check the route will output on this interface */
          if (!interface || interface == m_ipv6->GetNetDevice (j->GetInterface ()))
            {
              if (maskLen < longestMask)
                {
                  NS_LOG_LOGIC ("Previous match longer, skipping");
                  continue;
                }

              if (maskLen > longestMask)
                {
                  shortestMetric = 0xffffffff;
                }

              longestMask = maskLen;
              if (metric > shortestMetric)
                {
                  NS_LOG_LOGIC ("Equal mask length, but previous metric shorter, skipping");
                  continue;
                }

              shortestMetric = metric;
              Ipv6RoutingTableEntry* route = j;
              uint32_t interfaceIdx = route->GetInterface ();
              rtentry = Create<Ipv6Route> ();

              if (route->GetGateway ().IsAny ())
                {
                  rtentry->SetSource (m_ipv6->SourceAddressSelection (interfaceIdx, route->GetDest ()));
                }
              else if (route->GetDest ().IsAny ()) /* default route */
                {
                  rtentry->SetSource (m_ipv6->SourceAddressSelection (interfaceIdx, route->GetPrefixToUse ().IsAny () ? dst : route->GetPrefixToUse ()));
                }
              else
                {
                  rtentry->SetSource (m_ipv6->SourceAddressSelection (interfaceIdx, route->GetGateway ()));
                }

              rtentry->SetDestination (route->GetDest ());
              rtentry->SetGateway (route->GetGateway ());
              rtentry->SetOutputDevice (m_ipv6->GetNetDevice (interfaceIdx));
              if (maskLen == 128)
                {
                  break;
                }
            }
        }
    }

  if (rtentry)
    {
      NS_LOG_LOGIC ("Matching route via " << rtentry->GetDestination () << " (Through " << rtentry->GetGateway () << ") at the end");
    }
  return rtentry;
}

void FleeRouting::DoDispose ()
{
  NS_LOG_FUNCTION_NOARGS ();

  for (NetworkRoutesI j = m_networkRoutes.begin ();  j != m_networkRoutes.end (); j = m_networkRoutes.erase (j))
    {
      delete j->first;
    }
  m_networkRoutes.clear ();

  m_ipv6 = 0;
  Ipv6RoutingProtocol::DoDispose ();
}

uint32_t FleeRouting::GetNRoutes () const
{
  return m_networkRoutes.size ();
}

Ipv6RoutingTableEntry FleeRouting::GetDefaultRoute ()
{
  NS_LOG_FUNCTION_NOARGS ();
  Ipv6Address dst ("::");
  uint32_t shortestMetric = 0xffffffff;
  Ipv6RoutingTableEntry* result = 0;

  for (NetworkRoutesI it = m_networkRoutes.begin (); it != m_networkRoutes.end (); it++)
    {
      Ipv6RoutingTableEntry* j = it->first;
      uint32_t metric = it->second;
      Ipv6Prefix mask = j->GetDestNetworkPrefix ();
      uint16_t maskLen = mask.GetPrefixLength ();
      Ipv6Address entry = j->GetDestNetwork ();

      if (maskLen)
        {
          continue;
        }

      if (metric > shortestMetric)
        {
          continue;
        }
      shortestMetric = metric;
      result = j;
    }

  if (result)
    {
      return result;
    }
  else
    {
      return Ipv6RoutingTableEntry ();
    }
}

Ipv6RoutingTableEntry FleeRouting::GetRoute (uint32_t index) const
{
  NS_LOG_FUNCTION (this << index);
  uint32_t tmp = 0;

  for (NetworkRoutesCI it = m_networkRoutes.begin (); it != m_networkRoutes.end (); it++)
    {
      if (tmp == index)
        {
          return it->first;
        }
      tmp++;
    }
  NS_ASSERT (false);
  // quiet compiler.
  return 0;
}

uint32_t FleeRouting::GetMetric (uint32_t index) const
{
  NS_LOG_FUNCTION_NOARGS ();
  uint32_t tmp = 0;

  for (NetworkRoutesCI it = m_networkRoutes.begin (); it != m_networkRoutes.end (); it++)
    {
      if (tmp == index)
        {
          return it->second;
        }
      tmp++;
    }
  NS_ASSERT (false);
  // quiet compiler.
  return 0;
}

void FleeRouting::RemoveRoute (uint32_t index)
{
  NS_LOG_FUNCTION (this << index);
  uint32_t tmp = 0;

  for (NetworkRoutesI it = m_networkRoutes.begin (); it != m_networkRoutes.end (); it++)
    {
      if (tmp == index)
        {
          delete it->first;
          m_networkRoutes.erase (it);
          return;
        }
      tmp++;
    }
  NS_ASSERT (false);
}

void FleeRouting::RemoveRoute (Ipv6Address network, Ipv6Prefix prefix, uint32_t ifIndex, Ipv6Address prefixToUse)
{
  NS_LOG_FUNCTION (this << network << prefix << ifIndex);

  for (NetworkRoutesI it = m_networkRoutes.begin (); it != m_networkRoutes.end (); it++)
    {
      Ipv6RoutingTableEntry* rtentry = it->first;
      if (network == rtentry->GetDest () && rtentry->GetInterface () == ifIndex
          && rtentry->GetPrefixToUse () == prefixToUse)
        {
          delete it->first;
          m_networkRoutes.erase (it);
          return;
        }
    }
}

Ptr<Ipv6Route> FleeRouting::RouteOutput (Ptr<Packet> p, const Ipv6Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr)
{
  NS_LOG_FUNCTION (this << header << oif);
  Ipv6Address destination = header.GetDestinationAddress ();
  Ptr<Ipv6Route> rtentry = 0;
	if (destination.IsMulticast ())
	{
		rtentry = Create<Ipv6Route> ();
		rtentry->SetDestination (destination);
		rtentry->SetOutputDevice (oif);
		return rtentry;
	}
		

  rtentry = LookupStatic (destination, oif);
  if (rtentry)
    {
      sockerr = Socket::ERROR_NOTERROR;
    }
  else
    {
      sockerr = Socket::ERROR_NOROUTETOHOST;
    }
  return rtentry;
}

bool FleeRouting::RouteInput (Ptr<const Packet> p, const Ipv6Header &header, Ptr<const NetDevice> idev,
                                    UnicastForwardCallback ucb, MulticastForwardCallback mcb,
                                    LocalDeliverCallback lcb, ErrorCallback ecb)
{
  NS_LOG_FUNCTION (this << p << header << header.GetSourceAddress () << header.GetDestinationAddress () << idev);
  NS_ASSERT (m_ipv6 != 0);
  // Check if input device supports IP
  NS_ASSERT (m_ipv6->GetInterfaceForDevice (idev) >= 0);
  uint32_t iif = m_ipv6->GetInterfaceForDevice (idev);
  Ipv6Address dst = header.GetDestinationAddress ();

  // Check if input device supports IP forwarding
  if (m_ipv6->IsForwarding (iif) == false)
    {
      NS_LOG_LOGIC ("Forwarding disabled for this interface");
      if (!ecb.IsNull ())
        {
          ecb (p, header, Socket::ERROR_NOROUTETOHOST);
        }
      return false;
    }
  // Next, try to find a route
  NS_LOG_LOGIC ("Unicast destination");
  Ptr<Ipv6Route> rtentry = LookupStatic (header.GetDestinationAddress ());

  if (rtentry != 0)
    {
      NS_LOG_LOGIC ("Found unicast destination- calling unicast callback");
      ucb (idev, rtentry, p, header);  // unicast forwarding callback
      return true;
    }
  else
    {
      NS_LOG_LOGIC ("Did not find unicast destination- returning false");
      return false; // Let other routing protocols try to handle this
    }
}

void FleeRouting::NotifyInterfaceUp (uint32_t i)
{
  NS_LOG_FUNCTION (this << m_ipv6->GetAddress (i, 0).GetAddress ());
  Ptr<Ipv6L3Protocol> l3 = m_ipv6->GetObject<Ipv6L3Protocol> ();

  Ipv6InterfaceAddress iface = l3->GetAddress (i, 0);
  if (iface.GetAddress () == Ipv6Address ("::1"))
    return;

  // Create a socket to listen only on this interface
  Ptr<Socket> socket = Socket::CreateSocket (m_ipv6->GetObject <Node> (), 
                                             UdpSocketFactory::GetTypeId ());
  NS_ASSERT (socket != 0);
  socket->SetRecvCallback (MakeCallback (&FleeRouting::RecvFlee, this));
	
	socket->Bind (Inet6SocketAddress (iface.GetAddress (), FLEE_PORT));
  socket->BindToNetDevice (m_ipv6->GetObject <Node> ()->GetDevice (i));
  socket->SetAllowBroadcast (true);
  socket->SetAttribute ("IpTtl", UintegerValue (1));
  m_socketAddresses.insert (std::make_pair (socket, iface));
  
	// Create a socket to listen only on this interface
  socket = Socket::CreateSocket (m_ipv6->GetObject <Node> (), 
                                             UdpSocketFactory::GetTypeId ());
  NS_ASSERT (socket != 0);
  socket->SetRecvCallback (MakeCallback (&FleeRouting::RecvFlee, this));
	
	socket->Bind (Inet6SocketAddress (Ipv6Address::GetAny (), FLEE_PORT));
  socket->BindToNetDevice (m_ipv6->GetObject <Node> ()->GetDevice (i));
  socket->SetAllowBroadcast (true);
  m_broadcastSocketAddresses.insert (std::make_pair (socket, iface));
}

void FleeRouting::NotifyInterfaceDown (uint32_t i)
{
  NS_LOG_FUNCTION (this << i);

  /* remove all static routes that are going through this interface */
  for (NetworkRoutesI it = m_networkRoutes.begin (); it != m_networkRoutes.end (); )
    {
      if (it->first->GetInterface () == i)
        {
          delete it->first;
          it = m_networkRoutes.erase (it);
        }
      else
        {
          it++;
        }
    }
}

void FleeRouting::NotifyAddAddress (uint32_t interface, Ipv6InterfaceAddress address)
{
  if (!m_ipv6->IsUp (interface))
    {
      return;
    }

  Ipv6Address networkAddress = address.GetAddress ().CombinePrefix (address.GetPrefix ());
  Ipv6Prefix networkMask = address.GetPrefix ();

  if (address.GetAddress () != Ipv6Address () && address.GetPrefix () != Ipv6Prefix ())
    {
      AddNetworkRouteTo (networkAddress, networkMask, interface);
    }
}

void FleeRouting::NotifyRemoveAddress (uint32_t interface, Ipv6InterfaceAddress address)
{
  if (!m_ipv6->IsUp (interface))
    {
      return;
    }

  Ipv6Address networkAddress = address.GetAddress ().CombinePrefix (address.GetPrefix ());
  Ipv6Prefix networkMask = address.GetPrefix ();

  // Remove all static routes that are going through this interface
  // which reference this network
  for (NetworkRoutesI it = m_networkRoutes.begin (); it != m_networkRoutes.end (); )
    {
      if (it->first->GetInterface () == interface
          && it->first->IsNetwork ()
          && it->first->GetDestNetwork () == networkAddress
          && it->first->GetDestNetworkPrefix () == networkMask)
        {
          delete it->first;
          it = m_networkRoutes.erase (it);
        }
      else
        {
          it++;
        }
    }
}

void FleeRouting::NotifyAddRoute (Ipv6Address dst, Ipv6Prefix mask, Ipv6Address nextHop, uint32_t interface, Ipv6Address prefixToUse)
{
  NS_LOG_INFO (this << dst << mask << nextHop << interface << prefixToUse);
  if (dst != Ipv6Address::GetZero ())
    {
      AddNetworkRouteTo (dst, mask, nextHop, interface);
    }
  else /* default route */
    {
      /* this case is mainly used by configuring default route following RA processing,
       * in case of multiple prefix in RA, the first will configured default route
       */

      /* for the moment, all default route has the same metric
       * so according to the longest prefix algorithm,
       * the default route chosen will be the last added
       */
      SetDefaultRoute (nextHop, interface, prefixToUse);
    }
}

void FleeRouting::NotifyRemoveRoute (Ipv6Address dst, Ipv6Prefix mask, Ipv6Address nextHop, uint32_t interface, Ipv6Address prefixToUse)
{
  NS_LOG_FUNCTION (this << dst << mask << nextHop << interface);
  if (dst != Ipv6Address::GetZero ())
    {
      for (NetworkRoutesI j = m_networkRoutes.begin (); j != m_networkRoutes.end ();)
        {
          Ipv6RoutingTableEntry* rtentry = j->first;
          Ipv6Prefix prefix = rtentry->GetDestNetworkPrefix ();
          Ipv6Address entry = rtentry->GetDestNetwork ();

          if (dst == entry && prefix == mask && rtentry->GetInterface () == interface)
            {
              delete j->first;
              j = m_networkRoutes.erase (j);
            }
          else
            {
              ++j;
            }
        }
    }
  else
    {
      /* default route case */
      RemoveRoute (dst, mask, interface, prefixToUse);
    }
}

void 
FleeRouting::Hello (void)
{
	NS_LOG_FUNCTION (this);
  for (std::map<Ptr<Socket>, Ipv6InterfaceAddress>::const_iterator j =
         m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
	{
		uint8_t payload[1];
		payload[0] = m_distanceToSink;
		Ptr<Packet> pkt = Create<Packet> (payload,1);
		Ipv6Address destination = j->second.GetAddress ();
		destination = Ipv6Address ("ff02::1");
		j->first->SendTo(pkt,0,Inet6SocketAddress(destination,FLEE_PORT));
	}
	if (m_distanceToSink == 0)
		Simulator::Schedule (Seconds(5), &FleeRouting::Hello, this);
}

void 
FleeRouting::RecvFlee (Ptr<Socket> socket)
{
	Ptr<Packet> pkt;
	Address address;
	// Read all messages from the current socket
	while (pkt = socket->RecvFrom(address))
	{
		uint8_t payload[1];
		pkt->CopyData(payload,1);
		NS_LOG_DEBUG ("We can see a device " << payload[1] << "hops away from the sink");
		m_distanceToSink = std::min (payload[1]+1,m_distanceToSink);
		// check if the address is a ipv6 socket address
		if (Inet6SocketAddress::IsMatchingType (address))
		{
			// convert it and get IPv6 address
			Ipv6Address add = Inet6SocketAddress::ConvertFrom (address).GetIpv6 ();
			// if we do not have a route yet, ...
			if (!HasNetworkDest (add, m_ipv6->GetInterfaceForDevice(socket->GetBoundNetDevice ())))
			{
				// ... add the address to our list, ...
				AddHostRouteTo (add, m_ipv6->GetInterfaceForDevice (socket->GetBoundNetDevice ())); 
				// ... and send a message back to finalize.
				Simulator::ScheduleNow (&FleeRouting::SendHelloResp,this,socket,Create<Packet> (9), 0, Inet6SocketAddress (add,FLEE_PORT));
				// Broadcast the new message.
				Simulator::Schedule (MilliSeconds (100),&FleeRouting::Hello, this);
			}
		}
	}
}

void 
FleeRouting::SendHelloResp (Ptr<Socket> socket, Ptr<Packet> pkt, uint8_t flags, Address address)
{
  for (std::map<Ptr<Socket>, Ipv6InterfaceAddress>::const_iterator j =
         m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
	{
		if (socket->GetBoundNetDevice() == j->first->GetBoundNetDevice ())
		{
			j->first->SendTo (pkt,flags,address);
			NS_LOG_DEBUG("Sending Response");
		}
	}	
}

} /* namespace ns3 */

