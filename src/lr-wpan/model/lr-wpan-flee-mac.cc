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
 *  Brecht Reynders <brecht.reynders@esat.kuleuven.be>
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
#include <ns3/watchdog.h>
#include <ns3/llc-snap-header.h>
#include <tuple>

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
		m_canTx = true;
		m_var = CreateObject<UniformRandomVariable>();
	}

	LrWpanFleeMac::~LrWpanFleeMac ()
	{
	}

	void
		LrWpanFleeMac::DoInitialize ()
		{
			NS_LOG_FUNCTION(this);
			// set timerlength to 100 ms
			m_timerLength = 100;
			// set slotlength to 10 ms
			m_broadcastInterval = 10;
			// create timer
			m_timer = Timer(Timer::REMOVE_ON_DESTROY);
			m_timer.SetDelay(MilliSeconds(m_timerLength));
			m_timer.SetFunction(&LrWpanFleeMac::EndTimer,this);
			// randomize timers
			Simulator::Schedule (MilliSeconds(m_var->GetInteger(0,m_timerLength*2)),&LrWpanFleeMac::EndTimer, this);
			// intercept callbacks
			LrWpanMac::SetMcpsDataIndicationCallback (MakeCallback(&LrWpanFleeMac::McpsDataIndication, this));
			LrWpanMac::SetMcpsDataConfirmCallback (MakeCallback(&LrWpanFleeMac::McpsDataConfirm, this));
			// turn on RX always
			SetRxOnWhenIdle (true);
			// set the PHY layer attributes
			LrWpanPhyPibAttributes *attributes = new LrWpanPhyPibAttributes ();
			attributes->phyCCAMode = 4; // turn off CCA!
			m_phy->PlmeSetAttributeRequest (LrWpanPibAttributeIdentifier::phyCCAMode,attributes);
			m_phy->SetPlmeGetAttributeConfirmCallback (MakeCallback(&LrWpanFleeMac::PlmeGetAttributeConfirm,this));

			// configure default CSMA MAC layer
			LrWpanMac::DoInitialize ();
		}

	//Intercepting functions
	void
		LrWpanFleeMac::SetMcpsDataIndicationCallback (McpsDataIndicationCallback c)
		{
			m_mcpsDataIndicationCallback = c;
		}

	void
		LrWpanFleeMac::SetMcpsDataConfirmCallback (McpsDataConfirmCallback c)
		{
			m_mcpsDataConfirmCallback = c;
		}

	void 
	LrWpanFleeMac::RemoveFirstTxQElement ()
	{
		NS_LOG_FUNCTION (this);
		// csma always deletes first packet
		// here we pick packet to transmit
		DeleteFromQueue(m_currentTxPkt);
	}

	void
		LrWpanFleeMac::CheckQueue ()
		{
			// left empty on purpuse.
		}

	void
		LrWpanFleeMac::RxTimeOut (void)
		{
			// leave empty for now.
			// \TODO this could save power
			//NS_LOG_FUNCTION(this);
			//SetRxOnWhenIdle (false);	
		}

	void
		LrWpanFleeMac::PlmeGetAttributeConfirm (LrWpanPhyEnumeration status,
				LrWpanPibAttributeIdentifier id,
				LrWpanPhyPibAttributes* attribute)
		{
			NS_LOG_FUNCTION (this << status << id << attribute);
			// send from phy 
			// if success, we read the value
			if (status == IEEE_802_15_4_PHY_SUCCESS)
				// current channel we are transmitting on
				if (id == LrWpanPibAttributeIdentifier::phyCurrentChannel)
					//store this value
					m_channelNumber = attribute->phyCurrentChannel;
		}

	void
		LrWpanFleeMac::McpsDataRequest (McpsDataRequestParams params, Ptr<Packet> p)
		{
			NS_LOG_FUNCTION(this);
			if (params.m_dstAddr == Mac16Address ("ff:ff"))
			{
				// send it on all frequencies (so copy packet 16 times)
				for (uint8_t i = 0; i< 16; i++)
					LrWpanMac::McpsDataRequest (params,p->Copy());
			}
			else
				//if not broadcast just pass this request
					LrWpanMac::McpsDataRequest (params,p);
		}
 
	//Passing ACK from MAC to netdevice
	void LrWpanFleeMac::McpsDataConfirm (McpsDataConfirmParams params)
	{
		// Check the current packet being transmitted
		LrWpanMacHeader mh;
		m_currentTxPkt->PeekHeader(mh);
		Address addr = mh.GetShortDstAddr();
		//and increment the timeout event
		if (addr != Mac16Address ("ff:ff"))
			std::get<5>(m_connectable[addr])->Ping(MilliSeconds(10*m_timerLength));
		// and pass it on to wherever
		if (!m_mcpsDataConfirmCallback.IsNull ())
			m_mcpsDataConfirmCallback(params);
	}
  
	void 
		LrWpanFleeMac::McpsDataIndication (McpsDataIndicationParams params, Ptr<Packet> pkt)
		{
			NS_LOG_FUNCTION (this);
			// check current channel
			m_phy->PlmeGetAttributeRequest(LrWpanPibAttributeIdentifier::phyCurrentChannel);
			std::map<Address, LinkSpecs >::iterator it;
			// check if we know the destination
			it = m_connectable.find (params.m_srcAddr);
			if (it==m_connectable.end())
			{
				// we dont know it
				NS_LOG_LOGIC ("New connection does not appear in the list");
				LlcSnapHeader llc;
				pkt->PeekHeader (llc);

				if (params.m_dstAddr != m_shortAddress)
				{
					// it is a broadcast message!
					NS_LOG_LOGIC("Not dedicated to us");
					// create watchdog timer for timeout or desynchronization
					Watchdog* wd = new Watchdog ();
					wd->SetFunction (&LrWpanFleeMac::PruneConnection,this);
					wd->SetArguments ( (Address)params.m_srcAddr);
					// initialize only 2 cycles, maybe we or they don't want to connect
					wd->Ping (MilliSeconds (2*m_timerLength));
					// add it to database
					m_connectable[params.m_srcAddr]=std::make_tuple(m_channelNumber,m_latestStart.GetMilliSeconds(),0,false,true, wd);
				}
				else
				{
					NS_LOG_LOGIC ("this case");
					// create watchdog timer for timeout or desynchronization
					Watchdog* wd = new Watchdog ();
					wd->SetFunction (&LrWpanFleeMac::PruneConnection,this);
					wd->SetArguments ((Address)params.m_srcAddr);
					wd->Ping (MilliSeconds (10*m_timerLength)); 
					// add it to database, it is official
					m_connectable[params.m_srcAddr]=std::make_tuple(m_channelNumber,m_latestStart.GetMilliSeconds (),0,true,true,wd);
				}
			}
			else
			{
				// we do know it
				// increment timeout
				std::get<5> (it->second)->Ping(MilliSeconds(10*m_timerLength));
				// set that connection is confirmed
				std::get<3> (it->second) = true;
				// save
				m_connectable[params.m_srcAddr]=it->second;
			}

			// pass it for default beheaviour to higher layers
			m_mcpsDataIndicationCallback (params, pkt);
		}

	void 
		LrWpanFleeMac::EndTimer (void)
		{
			NS_LOG_FUNCTION(this);
			// reschedule timer
			Simulator::ScheduleNow (&LrWpanFleeMac::ScheduleSlots,this);
			m_timer.Schedule();
		}


	/*
	   Schedule slots in the entire timer duration based on the received packets.
	   */
	void 
		LrWpanFleeMac::ScheduleSlots (void)
		{
			//NS_LOG_FUNCTION(this);
			if (m_connectable.size() > 0)
				// schedule all connections in DB
				for (std::map<Address,LinkSpecs >::iterator it = m_connectable.begin(); it != m_connectable.end(); ++it)
				{
					Simulator::Schedule (MilliSeconds(std::get<1>(it->second)),&LrWpanFleeMac::ScheduleSlot, this, it->first);
					double mindiff = m_timerLength;
					for (std::map<Address,LinkSpecs >::iterator it2 = m_connectable.begin(); it2 != m_connectable.end(); ++it2)
					{
						if (it2->first!=it->first)
						{
							uint8_t diff = ((uint8_t)(std::get<1> (it2->second) - std::get<1>(it->second)+m_timerLength))%(uint8_t)m_timerLength;
							if (diff < mindiff)
								mindiff = diff;
						}
					}
					if (mindiff > 2*m_broadcastInterval)
					{
						for (double i = std::get<1> (it->second)+m_broadcastInterval; i < std::get<1> (it->second)+mindiff; i+=m_broadcastInterval)
							if (m_canTx)
							{
								NS_LOG_DEBUG("scheduling new transmission 1 at " << (uint8_t)i%(uint8_t)m_timerLength << " ms");
								Simulator::Schedule (MilliSeconds((uint8_t)i%(uint8_t)m_timerLength),&LrWpanFleeMac::ScheduleSlot, this, Mac16Address ("ff:ff"));
							}
					}
				}
			else
				// if ther are no cons, just schedule broadcasts
				for (double i = 0; i < m_timerLength-m_broadcastInterval; i+=m_broadcastInterval)
					if (m_canTx)
					{
						NS_LOG_DEBUG("scheduling new transmission at " << i << " ms");
						Simulator::Schedule (MilliSeconds(i),&LrWpanFleeMac::ScheduleSlot, this, Mac16Address ("ff:ff"));
					}
			if (CheckQueueFor(Mac16Address ("ff:ff")))
				m_canTx = !m_canTx;
			else
			{
				m_canTx = true;
				IncrementBroadcastChannel();
			}
		}


	void 
		LrWpanFleeMac::ScheduleSlot (const Address& addr)
		{
			LinkSpecs settings; 
			if (addr != Mac16Address ("ff:ff"))
				settings = m_connectable[addr];
			else
			{
				if (CheckQueueFor(Mac16Address ("ff:ff")))
				{
					NS_LOG_LOGIC ("there is something in the queue" << !m_canTx );
					if (!m_canTx)
						IncrementBroadcastChannel ();
					settings = std::make_tuple (m_broadcastChannel,0,0,true,!m_canTx, new Watchdog());
				}
				else
					settings = std::make_tuple (m_broadcastChannel,0,0,true,false, new Watchdog ());
			}
			// check if this connection has been pruned
			if (std::get<0> (settings) == 0)
			{
				// prune it again to create more slots
				std::get<5>(settings)->Ping (Seconds(0));
				return;
			}

			if (std::get<4> (settings))
			{
				// Check if there are packets to send.
				if (CheckQueueFor(addr))
				{
					NS_LOG_FUNCTION(this << addr << std::get<4>(settings) << (uint32_t)std::get<0>(settings));
					// prepare receiving slot frequency
					LrWpanPhyPibAttributes *attributes = new LrWpanPhyPibAttributes ();
					attributes->phyCurrentChannel = std::get<0>(settings); 
					// turn RX on when IDLE
					m_phy->PlmeSetAttributeRequest (LrWpanPibAttributeIdentifier::phyCurrentChannel,attributes);
					m_currentTxPkt = m_txPkt;
					Simulator::ScheduleNow(&LrWpanFleeMac::ChangeMacState,this,MAC_SENDING);
					m_setMacState = Simulator::ScheduleNow (&LrWpanPhy::PlmeSetTRXStateRequest, m_phy, IEEE_802_15_4_PHY_TX_ON);
					if (addr == Mac16Address ("ff:ff"))
					{
						// Schedule a receive slot on this channel for devices to connect to
						Simulator::Schedule(MilliSeconds(m_timerLength),&LrWpanFleeMac::SetBroadcastChannel, this, m_broadcastChannel);
						Simulator::Schedule(MilliSeconds(m_timerLength),&LrWpanFleeMac::ScheduleSlot, this, addr);
					}
				}
				else
				{
					// else just turn listening on.
					if (addr == Mac16Address ("ff:ff"))
						std::get<4>(settings) = false;
				}
			}

			if (!std::get<4> (settings))
			{
				NS_LOG_FUNCTION(this << addr << std::get<4>(settings) << (uint32_t)std::get<0>(settings));
				// prepare receiving slot frequency
				LrWpanPhyPibAttributes *attributes = new LrWpanPhyPibAttributes ();
				attributes->phyCurrentChannel = std::get<0>(settings); 
				// turn RX on when IDLE
				Simulator::ScheduleNow ( &LrWpanPhy::PlmeSetAttributeRequest, m_phy, LrWpanPibAttributeIdentifier::phyCurrentChannel,attributes);
				Simulator::ScheduleNow ( &LrWpanMac::SetRxOnWhenIdle, this, true);
				// prepare timeout
				Simulator::Schedule (MilliSeconds(5), &LrWpanFleeMac::RxTimeOut, this);
			}
			// turn around TX and RX
			std::get<4> (settings) = !std::get<4> (settings);
			// store the new information
			if (addr != Mac16Address("ff:ff"))
			{
				m_connectable[addr] = settings;
			}

		}

	void LrWpanFleeMac::PdDataStartNotion (void)
	{
		// save the start of the last packet
		m_latestStart = MilliSeconds(m_timerLength) - m_timer.GetDelayLeft ();
		NS_LOG_FUNCTION(this << m_latestStart);
		LrWpanMac::PdDataStartNotion();
	}

	void LrWpanFleeMac::PruneConnection (const Address& addr)
	{
		NS_LOG_FUNCTION (this << addr <<  m_txPkt);
		NS_LOG_DEBUG ("The connection is lost...");
		m_connectable.erase(addr);
		m_txPkt = 0;
			
	}

	void LrWpanFleeMac::IncrementBroadcastChannel (void)
	{
		m_broadcastChannel++;
		if (m_broadcastChannel > 26)
			m_broadcastChannel = 11;
	}

	void LrWpanFleeMac::SetBroadcastChannel (uint8_t channel)
	{
		NS_ASSERT ( channel >= 11 && channel <= 26);
		m_broadcastChannel = channel;
	}

} // namespace ns3
