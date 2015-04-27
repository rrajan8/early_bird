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
	vector<NodeContainer> dept1_nodes(num_root_childs);
	//that says make room for num_root_childs in that vector - set by user
	vector<NodeContainer> dept2_nodes;
	//going to populate the node containers
	//the num_root_childs is the number of children directly from the root node
	//so start iterating from those nodes 
	for (uint32_t i = 0; i < num_root_childs; ++i)
	{
		//simple count to find total number of hosts
		num_hosts++;
		//for each of these nodes we are going to spawn their children
		//the fandout dept is an indication of how many children nodes specified
		//by the user - fanoutdept1 is simply how many child nodes to spwn in 
		//next zone
		//so iterate through that many times to create that many nodes
		for (uint32_t j = 0; j < fanoutdept1; ++j)
		{
			//this is how I generate the random value, based on the probability
			//set by the user, to determine if to create this nodecontainer/not
			double uv1 = uv->GetValue() ;
			//if the random value (0,1) is less than the probability specified
			//then go ahead and spawn that nodecontainer
			if (uv1 <= prob)	// this is probability of child creation
			{
				// Create a fanout node for this zone
				//here I access the vector of zone 1 nodecontainers and create 
				//a node inside the ith container of the zone 1
				dept1_nodes[i].Create(1);
				//for each zone 1 node you create, there has to be a zone 2 
				//container to represent it, hence the next thing is to add containers
				//to the dept2 zone vector correspondingly
				dept2_nodes.push_back(NodeContainer ());
				//another simple way to keep track of all the hosts we have created
				num_hosts++;
				//now, for each of the nodes created in this zone 1, we have to 
				//create their children nodes, also determined by another fanout
				//vairiable user defined -fanoutdept2
				//so for each node in zone 1 created, create its children
				for (uint32_t k = 0; k < fanoutdept2; ++k)
				{
					//yet again determine if we are going to create this child
					//based on the probability defined - read above
					double uv2 = uv-> GetValue();
					if (uv2 <= prob)
					{
						//remember above where we created a corresponding node
						//container for each zone 1 node, well here is where we
						//create nodes for each of those containers, to represent
						//the children of the nodes created in zone 1
						// hence the zone 2 children - dept 2 nodes
						dept2_nodes[dept2_nodes.size()-1].Create(1);
						//another simple increase in hosts count after we create
						//a node
						num_hosts++;
					}
				}
	
			}
		}
	}

	//create array of all hosts and find which hosts will be infectable

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
	
	//now we put everything together and link them up

	NodeContainer topology;	//main topology - everything connected to this
	NodeContainer d1_nodes;	//the dept 1 nodes
	//those will be connected to the root node
	topology.Create(1);	//create root node now
	d1_nodes.Create(num_root_childs);	//user defined
	//append the dept1 nodes to root
	topology.Add(d1_nodes);
	for(uint32_t x = 0; x < num_root_childs; x++)
	{
		//remember above we created a vector of dept1 node containers
		//now we add the all nodes from each container to dept 1 nodes created
		topology.Add(dept1_nodes[x]);
	}
	for(uint32_t x = 0; x < dept2_nodes.size(); x++)
	{
		//similarly, to each of the nodes added to the dept 1 nodes above, we add
		//their children to each of them
		topology.Add(dept2_nodes[x]);
	}
	
	//the nodes are added now, but not linked into the tree  as we are picturing
	// we now make the final p2p links and create our desired tree like structure

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
	//here is where all the magic happens
	
	// j_int is simply used to keep a counter on the j iteration to prevent 
	// overlapping when connecting the zone 2 nodes to their parent zone 1 nodes
	uint32_t j_int = 0;
	for (uint32_t i = 0; i < num_root_childs; ++i)
	{
		// Connect the root node to zone 1 nodes
		topdevices[i] = topp2p.Install (topology.Get(0), d1_nodes.Get(i));

		for (uint32_t j = 0; j < dept1_nodes[i].GetN(); ++j)
		{
			// Connect zone 1 nodes to it's children in dept 1 nodeContainers
			d1devices.push_back (d1p2p.Install (d1_nodes.Get(i), dept1_nodes[i].Get(j)));

			for (uint32_t k = 0; k < dept2_nodes[j_int+j].GetN(); ++k)
			{
				//connect dept 1 nodes to its children in dept 2 nodeContainers
				// dont get confused with the j_int, its a simple way to keep 
				// of where the j iteration left when assigning the dept 2 
				// children to its parents
		  		d2devices.push_back (d2p2p.Install (dept1_nodes[i].Get(j), dept2_nodes[j_int+j].Get(k)));
			}
		}
		j_int += dept1_nodes[i].GetN();
	}
	
	//alright, thats it, the tree is complete
	//the next step is assigning the ip addresses to each node
	//I tried to give each level in the tree its own unique way of assigning
	//thier level IPs but if you followed how I iterated through the tree in the
	//previous 2 iterations above, then its the same iteration here
	//so you all can assign the IPs as you see best suited for the worm app

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
	//here is the same as above in terms of iterating the levels in the tree
	//all I do extra is based on the hosts list create by Caylen, I changed the
	//node colors correspondingly

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