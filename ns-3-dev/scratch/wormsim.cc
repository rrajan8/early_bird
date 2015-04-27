#include <iostream>
#include <stdio.h>
#include <string>
#include <sys/time.h>
#include <vector>
#include <time.h>
#include <iomanip>
#include <assert.h>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/stats-module.h"
#include "ns3/mobility-module.h"

#define ROOTCHILDS   2
#define DEPT1CHILDS  4
#define DEPT2CHILDS  8
#define CHILDPROB    0.75

#define Z22Z3BW      "1Mbps"	//zone2 to zone3 bandwidth
#define Z12Z2BW      "10Mbps"	//zone1 to zone2 bandwidth
#define RT2Z1BW 	 "100Mbps"	//root to zone1 bandwidth

#define SEEDVALUE    11223344
#define INFECTPER    0.75


using namespace ns3;
using namespace std;


int main(int argc, char* argv[])
{
	
	uint32_t num_root_childs = ROOTCHILDS;
	uint32_t fanoutdept1 = DEPT1CHILDS;
	uint32_t fanoutdept2 = DEPT2CHILDS;
	string rootbw  = RT2Z1BW;
	string z1bw = Z12Z2BW;
	string z2bw = Z22Z3BW;
	uint32_t seed = SEEDVALUE;
	double prob = CHILDPROB;
	double infectper = INFECTPER;

	RngSeedManager::SetSeed (seed);	//seed can be user defined
	Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable> ();
	uv->SetAttribute ("Stream", IntegerValue (6110));
	uv->SetAttribute ("Min", DoubleValue (0.0));
	uv->SetAttribute ("Max", DoubleValue (1.0));

	//printf("here1\n");
	//create 2 nodes for dept 1 for now...can be changed later as user defined
	uint32_t num_hosts = 1;
	vector<NodeContainer> dept1_nodes(2);
	vector<NodeContainer> dept2_nodes;
	//printf("here1\n");
	for (uint32_t i = 0; i < num_root_childs; ++i)
	{
			//printf("here2\n");
		num_hosts++;
		for (uint32_t j = 0; j < fanoutdept1; ++j)
		{
			//printf("here3\n");
			double uv1 = uv->GetValue() ;
			//printf("here7\n");
			if (uv1 <= prob)	// this is probability of child creation
			{
				// Create a fanout node for this tree
				dept1_nodes[i].Create(1);
				dept2_nodes.push_back(NodeContainer ());
				num_hosts++;
				for (uint32_t k = 0; k < fanoutdept2; ++k)
				{
					//printf("here4\n");
					double uv2 = uv-> GetValue();
					if (uv2 <= prob)
					{
						// Create a fanout node for this fanout node
						dept2_nodes[dept2_nodes.size()-1].Create(1);
						num_hosts++;
					}
				}
	
			}
		}
	}

	//create array of all hosts and find which hosts will be infectable

	 	// CAYLEN
	//int num_hosts = 10; // Number of hosts
	//double infectablePer = 0.45; // Fraction of hosts that are infectable

	uint32_t num_infectable = infectper * num_hosts; // Number of infectable hosts
	cout << "number of hosts = " << num_hosts << endl;
	cout << "infectable: " << num_infectable << endl;

	// Variable hosts is an array of node indices.
	uint32_t* hosts = new uint32_t[num_hosts];
	for (uint32_t i = 0; i < num_hosts; ++i)
		hosts[i] = i;

	// Indices of infectable hosts are on right portion of the array, starting at 
	// index = num_hosts-num_infectable. Indices of noninfectable hosts are on left 
	// portion up to index = num_hosts-num_infectable-1.
	for (uint32_t i = num_hosts-1; i >= (num_hosts - num_infectable); --i) {
		uint32_t j = rand() % (i + 1);
		uint32_t tmp = hosts[i];
		hosts[i] = hosts[j];
		hosts[j] = tmp;
	}


	cout << "Noninfectable: ";
	for (uint32_t i = 0; i < num_hosts-num_infectable; ++i)
		cout << hosts[i] << " ";

	cout << endl << "Infectable: ";
	for (uint32_t i = num_hosts-num_infectable; i < num_hosts; ++i)
		cout << hosts[i] << " ";

	cout << endl;

	// create topology

	//printf("here1\n");
	NodeContainer topology;
	NodeContainer d1_nodes;
	topology.Create(1);	//root
	d1_nodes.Create(num_root_childs);	//user defined
	topology.Add(d1_nodes);
	for(uint32_t x = 0; x < num_root_childs; x++)
	{
		topology.Add(dept1_nodes[x]);
	}
	for(uint32_t x = 0; x < dept2_nodes.size(); x++)
	{
		topology.Add(dept2_nodes[x]);
	}

	// Install nodes on internet stack.
	InternetStackHelper stack;
	stack.Install (topology);

	// create and set data rates and devices.
	PointToPointHelper topp2p;
	topp2p.SetDeviceAttribute ("DataRate", StringValue (rootbw));
	topp2p.SetChannelAttribute ("Delay", StringValue ("20ms"));
	vector<NetDeviceContainer> topdevices(num_root_childs);

	PointToPointHelper d1p2p;
	d1p2p.SetDeviceAttribute ("DataRate", StringValue (z1bw));
	d1p2p.SetChannelAttribute ("Delay", StringValue ("20ms"));
	vector<NetDeviceContainer> d1devices;

	PointToPointHelper d2p2p;
	d2p2p.SetDeviceAttribute ("DataRate", StringValue (z2bw));
	d2p2p.SetChannelAttribute ("Delay", StringValue ("20ms"));
	vector<NetDeviceContainer> d2devices;

	//now to install the p2ps and devices

	uint32_t j_int = 0;
	for (uint32_t i = 0; i < num_root_childs; ++i)
	{
		// Connect the root node to each tree node
		topdevices[i] = topp2p.Install (topology.Get(0), d1_nodes.Get(i));

		for (uint32_t j = 0; j < dept1_nodes[i].GetN(); ++j)
		{
			// Connect each tree node to its fanout nodes
			d1devices.push_back (d1p2p.Install (d1_nodes.Get(i),
			                                         dept1_nodes[i].Get(j)));

			for (uint32_t k = 0; k < dept2_nodes[j_int+j].GetN(); ++k)
			{
		  		d2devices.push_back (d2p2p.Install (dept1_nodes[i].Get(j),
		  									dept2_nodes[j_int+j].Get(k)));
			}
		}
		j_int += dept1_nodes[i].GetN();
	}

	// Assign IP addresses
	ostringstream ostr;
	Ipv4AddressHelper ipv4;
	vector<Ipv4InterfaceContainer> ipv4top(num_root_childs);
	vector<Ipv4InterfaceContainer> ipv4d1;
	vector<Ipv4InterfaceContainer> ipv4d2;
	uint32_t countj = 0;
	uint32_t countk = 0;
	//printf("here1\n");
	for (uint32_t i = 0; i < num_root_childs; ++i)
	{
		ostr.str ("");
		ostr << i + 1 << ".1.0.0";
		ipv4.SetBase (ostr.str().c_str (), "255.255.255.0");
		ipv4top[i] = ipv4.Assign(topdevices[i]);
		//printf("here2\n");
		for (uint32_t j = 0; j < dept1_nodes[i].GetN(); ++j)
		{
			ostr.str ("");
			ostr << "192." << 1 + countj + j << ".1.0";
			ipv4.SetBase (ostr.str().c_str (), "255.255.255.0");
			ipv4d1.push_back(ipv4.Assign(d1devices[countj+j]));
			//printf("here3\n");
			for (uint32_t k = countk; k < countk + dept2_nodes[countj+j].GetN(); ++k)
			{
				ostr.str ("");
				ostr << "192." << 1 + countj + j << "." << k - countk + 2   << ".0";
				ipv4.SetBase (ostr.str().c_str (), "255.255.255.0");
				ipv4d2.push_back(ipv4.Assign(d2devices[k]));
				//printf("here4\n");
			}
			countk += dept2_nodes[countj+j].GetN();
		}
		countj += dept1_nodes[i].GetN();
	}

	// Populate routing tables.
	Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

	//shit for netanim...

	MobilityHelper mobility;
  	mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  	mobility.Install (topology);

	// Set the bounding box for animation
  	//topology.BoundingBox (1, 1, 100, 100);
	AnimationInterface anim ("wormtopology.xml"); // Mandatory

	//anim root node
    char buffer[255];
	sprintf(buffer, "%d", 0);
	anim.UpdateNodeDescription (topology.Get (0), buffer); // Optional
	for (uint32_t h = num_hosts-num_infectable; h < num_hosts; h++)
	{
		//host[i] are infectable hosts - set those blue
		if(0 == hosts[h])
		{
			//this host infectable
			anim.UpdateNodeColor (topology.Get(0), 0, 255, 0); // blue - infectable
			cout << 0 << " infected!" << endl;
			break;
		}
		else
		{
			anim.UpdateNodeColor (topology.Get(0), 255, 0, 0); // red - uninfectable
		}
	}
	//anim.UpdateNodeColor (topology.Get (0), 255, 0, 0); // Optional
	anim.SetConstantPosition (topology.Get(0), 0, 0);
		

    uint32_t j_interval = 0;
    uint32_t z1 = 0;
    uint32_t z2 = 0;
    uint32_t z3 = 0;
    uint32_t a = 1;
    for (uint32_t i = 0; i < num_root_childs; i++)
    {	
    	char buffer1[255];
		sprintf(buffer1, "%d", a);
		anim.UpdateNodeDescription (d1_nodes.Get(i), buffer1); // Optional
		for (uint32_t h = num_hosts-num_infectable; h < num_hosts; h++)
		{
			//host[i] are infectable hosts - set those blue
			if(a == hosts[h])
			{
				//this host infectable
				anim.UpdateNodeColor (d1_nodes.Get(i), 0, 255, 0); // blue - infectable
				cout << a << " infected!" << endl;
				break;
			}
			else
			{
				anim.UpdateNodeColor (d1_nodes.Get(i), 255, 0, 0); // red - uninfectable
			}
		}
		//anim.UpdateNodeColor (d1_nodes.Get(i), 255, 0, 0); // red - non infectable
		anim.SetConstantPosition (d1_nodes.Get(i), z1, 100);
		z1 += 10;
		a++;
    	for(uint32_t j = 0; j< dept1_nodes[i].GetN(); j++)
    	{
	    	char buffer2[255];
			sprintf(buffer2, "%d", a);
			anim.UpdateNodeDescription (dept1_nodes[i].Get(j), buffer2); // Optional
			for (uint32_t h = num_hosts-num_infectable; h < num_hosts; h++)
			{
				//host[i] are infectable hosts - set those blue
				if(a == hosts[h])
				{
					//this host infectable
					anim.UpdateNodeColor (dept1_nodes[i].Get(j), 0, 255, 0); // blue - infectable
					cout << a << " infected!" << endl;
					break;
				}
				else
				{
					anim.UpdateNodeColor (dept1_nodes[i].Get(j), 255, 0, 0); // red - uninfectable
				}
			}
			//anim.UpdateNodeColor (dept1_nodes[i].Get(j), 255, 0, 0); // Optional
			anim.SetConstantPosition (dept1_nodes[i].Get(j), z2, 200);
			z2+=10;
			a++;
    		for(uint32_t k = 0; k < dept2_nodes[j_interval+j].GetN(); k++)
    		{
				char buffer3[255];
				sprintf(buffer3, "%d", a);
				anim.UpdateNodeDescription (dept2_nodes[j_interval+j].Get(k), buffer3); // Optional
				for (uint32_t h = num_hosts-num_infectable; h < num_hosts; h++)
				{
					//host[i] are infectable hosts - set those blue
					if(a == hosts[h])
					{
						//this host infectable
						anim.UpdateNodeColor (dept2_nodes[j_interval+j].Get(k), 0, 255, 0); // blue - infectable
						cout << a << " infected!" << endl;
						break;
					}
					else
					{
						anim.UpdateNodeColor (dept2_nodes[j_interval+j].Get(k), 255, 0, 0); // red - uninfectable
					}
				}
				//anim.UpdateNodeColor (dept2_nodes[next+j].Get(k), 255, 0, 0); // Optional
				anim.SetConstantPosition (dept2_nodes[j_interval+j].Get(k), z3, 300);
				z3+=10;
				a++;
    		}
    	}	
    	j_interval += dept1_nodes[i].GetN();
    }

}