// Zhe Cheng Lee
// ECE 6110 - Project 1

#include <iostream>
#include <vector>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-dumbbell.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/tcp-l4-protocol.h"
#include "ns3/tcp-socket.h"
#include "ns3/queue.h"
#include "ns3/bulk-send-application.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;
using namespace std;



int main (int argc, char **argv)
{
	LogComponentEnable("WormClientApplication", LOG_LEVEL_INFO);
	LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
	//WormClient::init_URV();
	// Random number generator between 0 and 0.1
	RngSeedManager::SetSeed (11223344);
	Ptr<UniformRandomVariable> U = CreateObject<UniformRandomVariable> ();
	U->SetAttribute ("Stream", IntegerValue (6110));
	U->SetAttribute ("Min", DoubleValue (0.0));
	U->SetAttribute ("Max", DoubleValue (0.1));	
	Ptr<UniformRandomVariable> U_worm = CreateObject<UniformRandomVariable> ();
	U->SetAttribute ("Stream", IntegerValue (6110));
	U->SetAttribute ("Min", DoubleValue (0.0));
	U->SetAttribute ("Max", DoubleValue (10));	
	// Parse command arguments for number of flows, segment sizes, queue length, and max 
	// receiver window size
	uint32_t nFlows = 2, segSize = 512, queueSize = 64000, windowSize = 2000;
	// CommandLine cmd;
	// cmd.AddValue ("nFlows", "Number of TCP flows", nFlows);
	// cmd.AddValue ("segSize", "TCP segment size", segSize);
	// cmd.AddValue ("queueSize", "Drop tail queue length", queueSize);
	// cmd.AddValue ("windowSize", "Maximum receiver window size", windowSize);
	// cmd.Parse (argc, argv);
	
	Time::SetResolution (Time::NS);

	// Attributes for simulation
	Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpTahoe"));
	Config::SetDefault ("ns3::TcpSocketBase::MaxWindowSize", UintegerValue (windowSize));
	Config::SetDefault ("ns3::TcpSocketBase::WindowScaling", BooleanValue (false));
	Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (segSize));
	Config::SetDefault ("ns3::DropTailQueue::MaxBytes", UintegerValue (queueSize)); 
	Config::SetDefault ("ns3::DropTailQueue::Mode", StringValue ("QUEUE_MODE_BYTES"));
	Config::SetDefault ("ns3::WormClient::MaxPackets", UintegerValue(4));

	// Build dumbbell topology
	PointToPointHelper p2pLeaf, p2pNeck;
	p2pLeaf.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
	p2pLeaf.SetChannelAttribute ("Delay", StringValue ("10ms"));
	p2pNeck.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
	p2pNeck.SetChannelAttribute ("Delay", StringValue ("20ms"));
	PointToPointDumbbellHelper dumbbell(nFlows, p2pLeaf, nFlows, p2pLeaf, p2pNeck);

	// Install internet stack in topology
	InternetStackHelper stack;
	dumbbell.InstallStack (stack);

	// Assign addresses to nodes
	Ipv4AddressHelper ipLeft, ipRight, ipRouter;
	ipLeft.SetBase ("10.1.1.0", "255.255.255.0");
	ipRight.SetBase ("10.2.1.0", "255.255.255.0");
	ipRouter.SetBase ("10.3.1.0", "255.255.255.0");
	dumbbell.AssignIpv4Addresses (ipLeft, ipRight, ipRouter);	
	
	// Populate routing table
	Ipv4GlobalRoutingHelper::PopulateRoutingTables();	

	uint16_t port = 9;

	// Source and sink nodes
	ApplicationContainer sourceApps;
	ApplicationContainer sinkApps;
	vector<double> timeStarts; 

	for (uint32_t i = 0; i < nFlows; ++i) {
		WormClientHelper src ( dumbbell.GetRightIpv4Address (i), port);
		//WormClientHelper src ( Ipv4Address("10.2.2.4"), port);
		//src.SetAttribute ("MaxBytes", UintegerValue (0));
		//src.SetAttribute ("SendSize", UintegerValue (512));  		
		ApplicationContainer srca = src.Install (dumbbell.GetLeft (i));
		timeStarts.push_back (U->GetValue ());
		srca.Start (Seconds (timeStarts.back ()));
		sourceApps.Add (srca);
		Ptr<WormClient> worm = DynamicCast<WormClient> (sourceApps.Get (i));
		worm->setURV(U_worm);

	}
	
	Ptr<WormClient> sink1 = DynamicCast<WormClient> (sourceApps.Get (0));
	sink1->setInfected(true);

	for (uint32_t i = 0; i < nFlows; ++i) {
		WormClientHelper sink (dumbbell.GetLeftIpv4Address(nFlows-i-1), port);
		ApplicationContainer sinka = sink.Install (dumbbell.GetRight (i));
		sinka.Start (Seconds (0.0));
		sinkApps.Add (sinka);
		Ptr<WormClient> worm = DynamicCast<WormClient> (sinkApps.Get (i));
		worm->setURV(U_worm);
	}

	// Set the bounding box for animation
	dumbbell.BoundingBox (0, 0, 800, 800);
	AnimationInterface anim ("example.xml");
	anim.EnablePacketMetadata (); // Optional
	anim.EnableIpv4L3ProtocolCounters (Seconds (0), Seconds (10)); // Optional

	for(int i = 0; i < 2; i++)
		cout << "address " <<
                   Ipv4Address::ConvertFrom (dumbbell.GetRightIpv4Address(i)) << endl;

 	// Run simulation. Stop simulation after 10 seconds
	Simulator::Stop (Seconds (10.0));
	Simulator::Run ();
	Simulator::Destroy ();

	return 0;
}