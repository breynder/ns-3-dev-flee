/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014 NXP
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
 *
 *
 */

#include "ns3/log.h"
#include "ns3/net-device.h"
#include "ns3/core-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/mobility-module.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/simulator.h"
#include "ns3/energy-module.h"
#include "ns3/applications-module.h"
#include <ns3/udp-socket-factory.h>
#include "ns3/socket.h"
#include <ns3/multi-model-spectrum-channel.h>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/packet-sink.h"
#include <ns3/propagation-delay-model.h>
#include <ns3/isotropic-antenna-model.h>
#include <ns3/spectrum-analyzer.h>
#include <ns3/spectrum-module.h>
#include <ns3/flee-module.h>
#include <ns3/sixlowpan-module.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <math.h>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CSMASimulation");
int sentPackets=0;
int sentPackets1=0;
int sentGwPackets=0;
int receivedPackets=0;
int receivedPackets1=0;
int receivedPackets2=0;
int receivedPackets3=0;
int receivedPackets4=0;
int dropped=0;
int droppedGw=0;
int receivedGwPackets=0;

Time lastCheck;
Time lastCheckRx;
Time receptiontime;
Time transmittime;
bool currentIsTransmission = false;
bool currentIsReception = false;

// Simulation parameters
int pktSize = 90;
double traffic = 200;
bool fullDuplex = false;
bool collisionDetect = false;
bool slotted = false;
int nSensors = 2;
double duration = 20;

Ptr<OutputStreamWrapper> m_waterfall = 0;

// this function prints the position
void printPosition(Vector position){
	NS_LOG_DEBUG("x: "<< position.x <<"  y: " << position.y);
}

void drop (const Ptr<const Packet > packet)
{
	dropped++;
}

void dropGw (const Ptr<const Packet> packet)
{
	droppedGw++;
}

void sent(const Ptr< const Packet > packet){
	sentPackets++;
	//NS_LOG_DEBUG("sent:"<<sentPackets);
}
void sent1(const Ptr< const Packet > packet){
	sentPackets1++;
	//NS_LOG_DEBUG("sent:"<<sentPackets);
}

void sentGw(const Ptr< const Packet > packet){
	sentGwPackets++;
	//NS_LOG_DEBUG("sent:"<<sentPackets);
}

void received(std::string context,const Ptr< const Packet > packet){
	receivedPackets++;
	//NS_LOG_DEBUG("received:"<<receivedPackets);
}
void received1(const Ptr< const Packet > packet){
	receivedPackets1++;
	//NS_LOG_DEBUG("received:"<<receivedPackets);
}
void received2(const Ptr< const Packet > packet){
	receivedPackets2++;
	//NS_LOG_DEBUG("received:"<<receivedPackets);
}
void received3(const Ptr< const Packet > packet){
	receivedPackets3++;
	//NS_LOG_DEBUG("received:"<<receivedPackets);
}
void received4(const Ptr< const Packet > packet){
	receivedPackets4++;
	//NS_LOG_DEBUG("received:"<<receivedPackets);
}

void receivedGw(const Ptr< const Packet > packet){
	receivedGwPackets++;
	//NS_LOG_DEBUG("received:"<<receivedPackets);
}

// this function prints information in the simulation
	void 
PrintMe ()
{
	Simulator::Schedule(Seconds(10),&PrintMe);
	std::cout << Simulator::Now().GetSeconds() << std::endl;
	std::cout << "[ " <<  " " << ((double)receivedPackets) << " " << ((double)sentPackets) << "]"  << std::endl;
}

// this funtion allows to make waterfall curves in MATLAB
	void
ReportPSD (Ptr<const SpectrumValue> sv)
{
	//NS_LOG_DEBUG("Report PSD");
	if(m_waterfall){
		*m_waterfall->GetStream() << "" << Simulator::Now().GetMicroSeconds() << " " << *sv;
	}
	else
	{
		NS_LOG_DEBUG("No stream was set.");
	}
}

// The main function implementation
int mainBody (int argc, char **argv)
{
	// print some information
	if (true)
	{
		std::cout << "% " << nSensors << " ";
		if (fullDuplex)
			std::cout << "Full Duplex";
		else if (collisionDetect)
			std::cout << "Collision Detect";
		else
			std::cout << "Half Duplex";

		if(slotted)
			std::cout << " in slotted mode" << std::endl;
		else
			std::cout << " in unslotted mode" << std::endl;
	}
	//Create Nodes
	NodeContainer sensors;
	sensors.Create (nSensors);
	NodeContainer pan;
	pan.Create(1);
	NodeContainer lrwpanNodes;
	lrwpanNodes.Add(pan);
	lrwpanNodes.Add(sensors);

	//Install stack
	LrWpanHelper lrWpanHelper (true);
	
	//Create Channel
	Ptr<SpectrumChannel> channel = CreateObject<MultiModelSpectrumChannel>();
	Ptr<PropagationLossModel> lossModel = CreateObject<LogDistancePropagationLossModel> ();
	Ptr<PropagationDelayModel> delayModel = CreateObject<ConstantSpeedPropagationDelayModel> ();
	channel->AddPropagationLossModel (lossModel);
	channel->SetPropagationDelayModel (delayModel);
	lrWpanHelper.SetChannel (channel);

	// Install stack (2)
	NetDeviceContainer netdev = lrWpanHelper.InstallFlee (lrwpanNodes); // uses Friss propagation

	// Finish the nodes : fd-hd, (un)slotted CSMA,...
	for (int inode = 0; inode<1+nSensors; inode++){
		netdev.Get(inode)->GetObject<LrWpanNetDevice> ()->GetMac()->GetCsmaCa()->AssignStreams(inode);
	}
	for(int inode=0; inode<1+nSensors;inode++){
		netdev.Get(inode)->GetObject<LrWpanNetDevice> ()->GetMac()->SetFullDuplex(fullDuplex);
		netdev.Get(inode)->GetObject<LrWpanNetDevice> ()->GetMac()->SetCollisionDetect(collisionDetect);
		netdev.Get(inode)->GetObject<LrWpanNetDevice> ()->GetMac()->GetCsmaCa()->SetSlotted(slotted);
	}

	// Set mobility
	for(int inode=0; inode<nSensors;inode++){
		Ptr<ConstantPositionMobilityModel> sensorMobility = CreateObject<ConstantPositionMobilityModel> ();
		sensorMobility->SetPosition (Vector (cos(((double)inode)/((double)nSensors)*2*3.14)*inode,sin(((double)inode)/((double)nSensors)*2*3.14)*inode,0));
		netdev.Get(1+inode)->GetObject<LrWpanNetDevice> ()->GetPhy ()->SetMobility (sensorMobility);
	}

	//Mobility for PAN
	Ptr<ConstantPositionMobilityModel> panMobility = CreateObject<ConstantPositionMobilityModel> ();
	panMobility->SetPosition (Vector (0,0,0));
	netdev.Get(0)->GetObject<LrWpanNetDevice> ()->GetPhy ()->SetMobility (panMobility);

	//Show positions of nodes
	for(int i =0; i<=nSensors; i++){
		printPosition(netdev.Get(i)->GetObject<LrWpanNetDevice> ()->GetPhy()->GetMobility()->GetPosition());
	}

	//PCAP tracing
	//lrWpanHelper.EnablePcapAll (std::dstring ("csmaFD"), true);

	// Create callbacks
	lrWpanHelper.AssociateToPan(netdev,0);//devices,empty slots
	for(int i = 1; i<=nSensors; i++){
		netdev.Get(i)->GetObject<LrWpanNetDevice>()->GetMac()->TraceConnectWithoutContext("MacTx", MakeCallback(&sent));
		netdev.Get(i)->GetObject<LrWpanNetDevice>()->GetMac()->TraceConnectWithoutContext("MacTxDrop", MakeCallback(&drop));
		std::ostringstream oss;
		oss << i;
		netdev.Get(i)->GetObject<LrWpanNetDevice>()->GetMac()->TraceConnect("MacTxOk", oss.str(), MakeCallback(&received));
	}

	//hook up for the gateway
	netdev.Get(0)->GetObject<LrWpanNetDevice>()->GetMac()->TraceConnectWithoutContext("MacTx", MakeCallback(&sentGw));
	netdev.Get(0)->GetObject<LrWpanNetDevice>()->GetMac()->TraceConnectWithoutContext("MacTxDrop", MakeCallback(&dropGw));
	netdev.Get(0)->GetObject<LrWpanNetDevice>()->GetMac()->TraceConnectWithoutContext("MacTxOk", MakeCallback(&receivedGw));

	/* energy source */
	LrWpanEnergySourceHelper LrWpanEnergySourceHelper;
	// configure energy source
	LrWpanEnergySourceHelper.Set ("LrWpanEnergySourceInitialEnergyJ", DoubleValue (10));
	LrWpanEnergySourceHelper.Set ("LrWpanUnlimitedEnergy",BooleanValue(false));
	LrWpanEnergySourceHelper.Set ("LrWpanEnergySupplyVoltageV", DoubleValue(2));
	LrWpanEnergySourceHelper.Set ("PeriodicEnergyUpdateInterval", TimeValue(Seconds(0.4)));
	// install source
	EnergySourceContainer energySources = LrWpanEnergySourceHelper.Install (lrwpanNodes);
	/* device energy model */
	LrWpanRadioEnergyModelHelper radioEnergyHelper;
	// configure radio energy model
	radioEnergyHelper.Set ("TxCurrentA", DoubleValue (0.0174));
	radioEnergyHelper.Set ("RxCurrentA", DoubleValue (0.0174));
	// install device model
	DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install (netdev, energySources);
	/***************************************************************************/

	Simulator::Stop(Seconds(duration));

	SixLowPanHelper sixHelper;
	NetDeviceContainer dev2 = sixHelper.Install (netdev);

	InternetStackHelper internet;
	internet.SetIpv4StackInstall (false);
	FleeHelper flee;
	internet.SetRoutingHelper (flee);
	internet.InstallFlee (lrwpanNodes);

	pan.Get(0)->GetObject<Ipv6> ()->GetRoutingProtocol ()->SetAttribute("Sink",UintegerValue (0));

	Ipv6AddressHelper ad;
  Ipv6InterfaceContainer interfaces = ad.Assign(dev2);

  UdpEchoServerHelper echoServer (9);

  ApplicationContainer serverApps = echoServer.Install (pan.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (20.0));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (1,1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (sensors.Get (0));
  clientApps.Start (Seconds (15.0));
  clientApps.Stop (Seconds (18.0));

	//Start de simulator
	Simulator::Run ();
	Simulator::Destroy ();
	double remainingEnergy =0.0;

	// Calculate remaining energy
	for(int i =1; i<=nSensors; i++){
		remainingEnergy += 10-energySources.Get(i)->GetRemainingEnergy();
	}
	// Print results
	if (true)
	{
		std::cout << "[" << nSensors << " " << " " << remainingEnergy << " " << ((double)receivedPackets) << " " << (double)receivedGwPackets << " " <<  ((double)sentPackets) << " " << (double)sentGwPackets <<  " " << dropped << " " << droppedGw << "]"  << std::endl;
		receivedPackets=0;
		receivedGwPackets=0;
		sentPackets=0;
		sentGwPackets=0;
	}
	return 0;
}


// main function
int main (int argc, char **argv){ 
	//Simulator::ScheduleNow(&PrintMe);


	fullDuplex = false;
	collisionDetect = false;
	slotted = false;

	// read input parameters, this is easier for starting scripts. 
	CommandLine cmd;
	cmd.AddValue ("fullDuplex", "set in full duplex mode",fullDuplex);
	cmd.AddValue ("collisionDetect","set in collision detection mode",collisionDetect);
	cmd.AddValue ("slotted","set the csma-ca protocol in slotted mode",slotted);
	cmd.AddValue ("nSensors","number of extra sensors",nSensors);

	cmd.Parse (argc,argv);

	mainBody(argc, argv);
}
