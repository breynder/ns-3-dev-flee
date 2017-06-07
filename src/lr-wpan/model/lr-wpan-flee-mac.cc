/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 The Boeing Company
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
 * Authors:
 *  Gary Pei <guangyu.pei@boeing.com>
 *  kwong yin <kwong-sang.yin@boeing.com>
 *  Tom Henderson <thomas.r.henderson@boeing.com>
 *  Sascha Alexander Jopen <jopen@cs.uni-bonn.de>
 *  Erwan Livolant <erwan.livolant@inria.fr>
 */
#include "lr-wpan-flee-mac.h"
#include "lr-wpan-mac.h"
#include "lr-wpan-csmaca.h"
#include "lr-wpan-mac-header.h"
#include "lr-wpan-phy.h"
#include <ns3/simulator.h>
#include <ns3/log.h>
#include <ns3/uinteger.h>
#include <ns3/node.h>
#include <ns3/packet.h>
#include <ns3/random-variable-stream.h>
#include <ns3/double.h>
#include <ns3/timer.h>

namespace ns3 {

	NS_LOG_COMPONENT_DEFINE ("LrWpanFleeMac");

	NS_OBJECT_ENSURE_REGISTERED (LrWpanFleeMac);

	TypeId
		LrWpanFleeMac::GetTypeId (void)
		{
			static TypeId tid = TypeId ("ns3::LrWpanFleeMac")
				.SetParent<LrWpanMac> ()
				.SetGroupName ("LrWpan")
				.AddConstructor<LrWpanFleeMac> ()
				;
			return tid;
		}

	LrWpanFleeMac::LrWpanFleeMac ()
		:LrWpanMac()
	{
		m_channelNumber = 11;
		m_broadcastChannel = 11;
		m_var = CreateObject<UniformRandomVariable>();
	}

	LrWpanFleeMac::~LrWpanFleeMac ()
	{
	}

	void
		LrWpanFleeMac::DoInitialize ()
		{
			NS_LOG_FUNCTION(this);
			//m_connectable.push_back(std::make_tuple(Mac16Address("ff:ff"),11,0,0));
			m_timerLength = 100;
			m_RXTX = false;
			m_broadcastInterval = 10;
			m_timer = Timer(Timer::REMOVE_ON_DESTROY);
			m_timer.SetDelay(MilliSeconds(m_timerLength));
			m_timer.SetFunction(&LrWpanFleeMac::EndTimer,this);
			Simulator::Schedule (MilliSeconds(m_var->GetInteger(0,m_timerLength*2)),&LrWpanFleeMac::EndTimer, this);
			LrWpanMac::SetMcpsDataIndicationCallback (MakeCallback(&LrWpanFleeMac::McpsDataIndication, this));
			SetRxOnWhenIdle (true);
			LrWpanPhyPibAttributes *attributes = new LrWpanPhyPibAttributes ();
			attributes->phyCCAMode = 4; // turn off CCA!
			m_phy->PlmeSetAttributeRequest (LrWpanPibAttributeIdentifier::phyCCAMode,attributes);
			m_phy->SetPlmeGetAttributeConfirmCallback (MakeCallback(&LrWpanFleeMac::PlmeGetAttributeConfirm,this));


			LrWpanMac::DoInitialize ();
		}
	
	void
		LrWpanFleeMac::SetMcpsDataIndicationCallback (McpsDataIndicationCallback c)
		{
			m_mcpsDataIndicationCallback = c;
		}

	void 
	LrWpanFleeMac::RemoveFirstTxQElement ()
	{
		NS_LOG_FUNCTION (this);
		DeleteFromQueue(m_currentTxPkt);
	}

	void
		LrWpanFleeMac::CheckQueue ()
		{
			//NS_LOG_FUNCTION (this);

			// Pull a packet from the queue and start sending, if we are not already sending.
			//NS_LOG_DEBUG(m_lrWpanMacState << " " << m_currentTxPkt << " "  << m_txPkt << " " << m_setMacState.IsRunning () << " " << m_txQueue.size());
			//if (m_lrWpanMacState == MAC_IDLE && !m_txQueue.empty () && m_txPkt == 0 && !m_setMacState.IsRunning ())
			//{
			//	NS_LOG_DEBUG("TAKE NEW PACKET TO TRANSMIT");
			//	TxQueueElement *txQElement = m_txQueue.front ();
			//	m_txPkt = txQElement->txQPkt;
			//	if (m_currentTxPkt != m_txPkt)
			//	{
			//		m_new = true;
			//		m_currentTxPkt = m_txPkt;
			//	}
				//LrWpanMacHeader header;
				//m_txPkt->PeekHeader(header);
				//if (!(header.GetDstAddrMode()==2 && header.GetShortDstAddr() == Mac16Address ("ff:ff")))
				//	m_setMacState = Simulator::Schedule (MilliSeconds(8),&LrWpanMac::SetLrWpanMacState, this, MAC_CSMA);
				//else
				//	m_setMacState = Simulator::ScheduleNow (&LrWpanMac::SetLrWpanMacState, this, MAC_CSMA);
			//}
			//else if (m_lrWpanMacState == MAC_IDLE && m_txPkt && !m_setMacState.IsRunning ())
			//{
			//	m_setMacState = Simulator::ScheduleNow (&LrWpanMac::SetLrWpanMacState, this, MAC_CSMA);
			//}
		}

	void
		LrWpanFleeMac::RxTimeOut (void)
		{
			//NS_LOG_FUNCTION(this);
			//SetRxOnWhenIdle (false);	
		}

	void
		LrWpanFleeMac::PlmeGetAttributeConfirm (LrWpanPhyEnumeration status,
				LrWpanPibAttributeIdentifier id,
				LrWpanPhyPibAttributes* attribute)
		{
			NS_LOG_FUNCTION (this << status << id << attribute);
			if (status == IEEE_802_15_4_PHY_SUCCESS)
				if (id = LrWpanPibAttributeIdentifier::phyCurrentChannel);
					m_channelNumber = attribute->phyCurrentChannel;
		}

	void
		LrWpanFleeMac::McpsDataRequest (McpsDataRequestParams params, Ptr<Packet> p)
		{
			NS_LOG_FUNCTION(this);
			if (params.m_dstAddr == Mac16Address ("ff:ff"))
				for (uint8_t i = 0; i< 16; i++)
					LrWpanMac::McpsDataRequest (params,p->Copy());
			else
					LrWpanMac::McpsDataRequest (params,p);
		}
  
	void 
		LrWpanFleeMac::McpsDataIndication (McpsDataIndicationParams params, Ptr<Packet> pkt)
		{
			NS_LOG_FUNCTION (this);
			m_phy->PlmeGetAttributeRequest(LrWpanPibAttributeIdentifier::phyCurrentChannel);
			bool found = false;
			for (std::list<std::tuple<Address, uint8_t, double, uint8_t> >::iterator it = m_connectable.begin(); it != m_connectable.end(); ++it)
			{
				if (std::get<0> (*it) == params.m_srcAddr)
					found = true;
			}
			if (!found)
				m_connectable.push_back(std::make_tuple(params.m_srcAddr,m_channelNumber,0,0));
			// add timeout when they are not listening

			m_mcpsDataIndicationCallback (params, pkt);
		}

	void 
		LrWpanFleeMac::EndTimer (void)
		{
			NS_LOG_FUNCTION(this);
			m_RXTX = !m_RXTX;
			Simulator::ScheduleNow (&LrWpanFleeMac::ScheduleSlots,this);
			m_timer.Schedule();
		}

	void 
		LrWpanFleeMac::ScheduleSlots (void)
		{
			//NS_LOG_FUNCTION(this);
			if (m_connectable.size() > 0)
				for (std::list<std::tuple<Address, uint8_t, double, uint8_t> >::iterator it = m_connectable.begin(); it != m_connectable.end(); ++it)
				{
					Simulator::Schedule (MilliSeconds(std::get<2>(*it)),&LrWpanFleeMac::ScheduleSlot, this, std::get<0> (*it), m_RXTX, std::get<1>(*it));
					double mindiff = m_timerLength;
					for (std::list<std::tuple<Address, uint8_t, double, uint8_t> >::iterator it2 = m_connectable.begin(); it2 != m_connectable.end(); ++it2)
					{
						uint8_t diff = ((uint8_t)(std::get<2> (*it2) - std::get<2>(*it)+m_timerLength))%(uint8_t)m_timerLength;
						if (diff < mindiff)
							mindiff = diff;
					}
					if (mindiff > 2*m_broadcastInterval)
					{
						for (double i = std::get<2> (*it)+m_broadcastInterval; i < std::get<2> (*it)+mindiff; i+=m_broadcastInterval)
							Simulator::Schedule (MilliSeconds(i),&LrWpanFleeMac::ScheduleSlot, this, Mac16Address ("ff:ff"), m_RXTX, m_broadcastChannel);
					}
				}
			else
				for (double i = 0; i < m_timerLength-m_broadcastInterval; i+=m_broadcastInterval)
					Simulator::Schedule (MilliSeconds(i),&LrWpanFleeMac::ScheduleSlot, this, Mac16Address ("ff:ff"), m_RXTX, m_broadcastChannel);
		}
	void 
		LrWpanFleeMac::ScheduleSlot (const Address& addr, bool RXTX, uint8_t channel)
		{
			if (RXTX)
				if (CheckQueueFor(addr))
				{
					NS_LOG_FUNCTION(this << addr << RXTX << (uint32_t)channel);
					// prepare receiving slot frequency
					LrWpanPhyPibAttributes *attributes = new LrWpanPhyPibAttributes ();
					attributes->phyCurrentChannel = channel; 
					// turn RX on when IDLE
					m_phy->PlmeSetAttributeRequest (LrWpanPibAttributeIdentifier::phyCurrentChannel,attributes);
					m_currentTxPkt = m_txPkt;
					ChangeMacState (MAC_SENDING);
					m_setMacState = Simulator::ScheduleNow (&LrWpanPhy::PlmeSetTRXStateRequest, m_phy, IEEE_802_15_4_PHY_TX_ON);
				}
				else
				{
				}
			else
			{
				NS_LOG_FUNCTION(this << addr << RXTX << (uint32_t)channel);
				// prepare receiving slot frequency
				LrWpanPhyPibAttributes *attributes = new LrWpanPhyPibAttributes ();
				attributes->phyCurrentChannel = channel; 
				// turn RX on when IDLE
				Simulator::ScheduleNow ( &LrWpanPhy::PlmeSetAttributeRequest, m_phy, LrWpanPibAttributeIdentifier::phyCurrentChannel,attributes);
				Simulator::ScheduleNow ( &LrWpanMac::SetRxOnWhenIdle, this, true);
				// prepare timeout
				Simulator::Schedule (MilliSeconds(5), &LrWpanFleeMac::RxTimeOut, this);
			}

		}

} // namespace ns3
