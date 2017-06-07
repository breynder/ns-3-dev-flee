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
 */
#ifndef LR_WPAN_FLEE_MAC_H
#define LR_WPAN_FLEE_MAC_H

#include <ns3/lr-wpan-mac.h>
#include <ns3/lr-wpan-phy.h>
#include <ns3/timer.h>


namespace ns3 {

class Packet;
class LrWpanCsmaCa;
class UniformRandomVariable;

/**
 * \defgroup lr-wpan LR-WPAN models
 *
 * This section documents the API of the IEEE 802.15.4-related models.  For a generic functional description, please refer to the ns-3 manual.
 */

/**
 * \ingroup lr-wpan
 *
 * Tx options
 */


/**
 * \ingroup lr-wpan
 *
 * Class that implements the LR-WPAN Mac state machine
 */
class LrWpanFleeMac : public LrWpanMac
{
public:
  /**
   * Get the type ID.
   *
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  /**
   * Default constructor.
   */
  LrWpanFleeMac (void);
  ~LrWpanFleeMac (void);
	void DoInitialize ();
  void RemoveFirstTxQElement ();
  void PlmeGetAttributeConfirm (LrWpanPhyEnumeration status,
                                LrWpanPibAttributeIdentifier id,
                                LrWpanPhyPibAttributes* attribute);
  void McpsDataRequest (McpsDataRequestParams params, Ptr<Packet> p);
  void SetMcpsDataIndicationCallback (McpsDataIndicationCallback c);
private:
	uint8_t m_channelNumber;
	void RxTimeOut (void);
  void CheckQueue (void);
  void McpsDataIndication (McpsDataIndicationParams params, Ptr<Packet> pkt);
  McpsDataIndicationCallback m_mcpsDataIndicationCallback;

	// list of all connectable devices with
	// address, channel, time offset on our timer and amount of missed connections
	std::list<std::tuple <Address, uint8_t, double, uint8_t> > m_connectable;

	// timer for the clock, at this clock, devices can connect to different devices 
	// time is in miliseconds.
	double m_timerLength;
	Timer m_timer;
	// defines if this cycle is RX or TX
	bool m_RXTX; 
	// defines if this device is a sink. That one gets less receive slots...
	bool m_sink; 
	// how often the device broadcasts with a broadcast message
	double m_broadcastInterval;
	// next broadcast channel
	uint8_t m_broadcastChannel;
	uint8_t m_listenChannel;
	// Reset the timer.
	void EndTimer (void);
	// Schedule the slots in this rotation based on neighbours
	void ScheduleSlots (void);
	// amount of neighbours
	uint8_t m_neighbours;
	// Schedule a transmission for either reception or transmission.
	void ScheduleSlot (const Address& addr, bool RXTX, uint8_t channel);

	Ptr<UniformRandomVariable> m_var;

};


} // namespace ns3

#endif /* LR_WPAN_FLEE_MAC_H */
