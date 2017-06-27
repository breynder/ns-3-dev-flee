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
#include <ns3/watchdog.h>
#include <map>


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
	// set the broadcast channel;
	void SetBroadcastChannel (uint8_t channel);
	// typedef for our database 
	typedef std::tuple <uint8_t, double, uint8_t, bool,bool, Watchdog*> LinkSpecs;
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
	// finidsh constructing object
	void DoInitialize ();
	// overwrite of deleting elements
  void RemoveFirstTxQElement ();
	// get request for parameters of the PHY layer
  void PlmeGetAttributeConfirm (LrWpanPhyEnumeration status,
                                LrWpanPibAttributeIdentifier id,
                                LrWpanPhyPibAttributes* attribute);
	// interception of request going to the default MAC Layer
  void McpsDataRequest (McpsDataRequestParams params, Ptr<Packet> p);

	// setters of callback functions (see also lr-wpan MAC)
  void SetMcpsDataIndicationCallback (McpsDataIndicationCallback c);
  void SetMcpsDataConfirmCallback (McpsDataConfirmCallback c);
	/**
		* Indicates the start of an MPDU at PHY (receiving)
		*/
  void PdDataStartNotion (void);
private:
	// current channel
	uint8_t m_channelNumber;
	// time out receiving if there is no packety to receive
	void RxTimeOut (void);
	// check queue overwrite
  void CheckQueue (void);
	// overwrite of function to say there is data
  void McpsDataIndication (McpsDataIndicationParams params, Ptr<Packet> pkt);
	// overwrite of function to say data has been successfully transmitted
  void McpsDataConfirm (McpsDataConfirmParams params);
	// callback variables
  McpsDataIndicationCallback m_mcpsDataIndicationCallback;
	McpsDataConfirmCallback m_mcpsDataConfirmCallback;

	// list of all connectable devices with
	// address, channel, time offset on our timer, amount of missed connections, if connected, in TX
	std::map<Address, LinkSpecs > m_connectable;

	// timer for the clock, at this clock, devices can connect to different devices 
	// time is in miliseconds.
	double m_timerLength;
	Timer m_timer;
	// defines if this device is a sink. That one gets less receive slots...
	bool m_sink; 
	// boolean to suppress transmissions for one cycle
	bool m_canTx;
	// how often the device broadcasts with a broadcast message
	double m_broadcastInterval;
	// next broadcast channel
	uint8_t m_broadcastChannel;
	// channel to listen on
	uint8_t m_listenChannel;
	// start of latest received message.
	Time m_latestStart;
	// random variable
	Ptr<UniformRandomVariable> m_var;
	// Reset the timer.
	void EndTimer (void);
	// Schedule the slots in this rotation based on neighbours
	void ScheduleSlots (void);
	// amount of neighbours
	uint8_t m_neighbours;
	// Schedule a transmission for either reception or transmission.
	void ScheduleSlot (const Address& addr);
	// if the request is not acked, remove the connection, because something went wrong.
	void PruneConnection (const Address& addr);
	// incremement the channel of broadcast 
	void IncrementBroadcastChannel (void);
};


} // namespace ns3

#endif /* LR_WPAN_FLEE_MAC_H */
