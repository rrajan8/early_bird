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

using namespace ns3;
using namespace std;

#define LINK1MBPS       "1Mbps"	
#define LINK5MBPS       "5Mbps"
#define LINK10MBPS		"10Mbps"
#define DELAY1MS		"1ms"
#define DELAY10MS		"10ms"
#define DELAY20MS		"20ms"
// Seed for the 
#define SEEDVALUE  		01234
// Probability a child Node is vulnerable to the worm app
#define PVUL			0.5
#define FANOUT1			10
#define FANOUT2			20

Ptr<UniformRandomVariable> uv;
NodeContainer root_node;
//vector <NetDeviceContainer> global_ndc;


class treeNode {
	private:
		int vulnerable;
		Ptr <Node> root; // This node is the root of the subtree
		NodeContainer children; // This keeps track of the children of root
		vector <treeNode> treeChildren; // This keeps track of the children's children
	
	public:
		
		treeNode(Ptr<Node> in) { // Assumes that root should have no children
			root = in;
			//treeChildren.reserve(FANOUT2);
		};

		void createChild(uint32_t subnet, int level) {
			uint32_t index = children.GetN();
			PointToPointHelper p2p;
			InternetStackHelper stack;
			Ipv4AddressHelper ipv4;
			ostringstream address;
			ostringstream base;

			// Set up ipv4
			int a_ind = level+ index*2;
			if(level == 1 ) a_ind = 1;

			address.str("");
			base.str("");
			address << "192.168."<<subnet<<".0"; 
			base << "0.0.0." << a_ind;
			//cout <<"a_ind="<<a_ind<< " : "<<address.str().c_str() << ":"<<base.str().c_str()<<endl;
			ipv4.SetBase (address.str().c_str (), "255.255.255.0", base.str().c_str());
		

			
			
			// Set up p2p
			p2p.SetDeviceAttribute ("DataRate", StringValue (LINK1MBPS));
			p2p.SetChannelAttribute ("Delay", StringValue (DELAY20MS));

			children.Create(1); // Creates the Node instance for the child
			Ptr <Node> newborn = children.Get(index);
			treeChildren.push_back(newborn); // Create a treeNode for the child
			NetDeviceContainer ndc = p2p.Install(root, newborn); // Link the root to the child
			stack.Install(newborn); // Install IPv4 stack to newborn using InternetStackHelper 
			ipv4.Assign(ndc); 
  			root_node.Add(newborn);
  			//global_ndc.push_back(ndc);
  			double random = uv->GetValue();
  			double vuln_rate = PVUL;

  			if(random < vuln_rate) { // Install the Worm application here
  				vulnerable = 0;
  			}
  			else{
  				vulnerable = 1;	
  			}   			
  			//cout << newborn->GetId() << " \t: " << a_ind << "\t: "<< vulnerable<<endl;
		}

		// GetChild returns the child at position index
		Ptr <Node> GetChildNode(int index){
			return children.Get(index);
		}

		treeNode* GetChildTree(unsigned index){
			if(index > treeChildren.size() ){
				cout<< "ERROR: index for treeChildren out of bounds: " << index << endl;
			}
			return &treeChildren[index];
		}

		int numChildren(){
			return children.GetN();
		}

		Ptr <Node> GetNode(){
			return root;
		}

		int isVulnerable(){
			return vulnerable;
		}
};


int main(int argc, char* argv[])
{
	RngSeedManager::SetSeed (SEEDVALUE);	//seed can be user defined
	uv = CreateObject<UniformRandomVariable> ();
	uv->SetAttribute ("Stream", IntegerValue (6110));
	uv->SetAttribute ("Min", DoubleValue (0.0));
	uv->SetAttribute ("Max", DoubleValue (1.0));

	cout << "===== Setting Up Worm Simulator v2.0 =====" << endl<<endl;

	InternetStackHelper stack;
	Simulator::Stop (Seconds (10.0));
	// Create the root Node & some children
	root_node.Create(1);
	stack.Install(root_node.Get(0));
	treeNode root_tree(root_node.Get(0));
	treeNode* child;
	for(int i = 0 ; i < FANOUT1; ++i ) {
		int address = i +2;
		root_tree.createChild(address,1);
		child = root_tree.GetChildTree(i);
		for(int j = 0; j < FANOUT2 ; ++j) {
			child -> createChild(address,3);
		}
 	}


	Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

	// Set up NetAnim 
	MobilityHelper mobility;
  	mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  	mobility.Install (root_node);
  	treeNode * child2;
  	double center = (FANOUT2*FANOUT1)/2.0;
  	double height = 0;
  	double i_x = FANOUT2/2.0, j_x = 0;
    double i_y = 10, j_y = 20;
  	AnimationInterface::SetConstantPosition(root_node.Get(0), center, height);
  	AnimationInterface anim ("worm.xml"); // Mandatory
  	for(int i = 0; i<root_tree.numChildren(); ++i){
  		child = root_tree.GetChildTree(i);
  		AnimationInterface::SetConstantPosition(child->GetNode(),i_x , i_y);
		if(child->isVulnerable()==0)
			cout<< child->GetNode()->GetId()<< "\t : vul = " << child->isVulnerable() <<endl;	
  		if(child->isVulnerable()){
  			anim.UpdateNodeColor (child->GetNode(), 0, 0, 255); // Set vulnerable nodes to blue
  		}
  		else 
  			anim.UpdateNodeColor (child->GetNode(), 0, 255, 0); // Set unvulnerable nodes to green

  		for(int j = 0; j < child->numChildren(); ++j){
  			child2 = child->GetChildTree(j);
  			j_x = i_x + j - FANOUT2/2.0;
  			anim.SetConstantPosition(child2->GetNode(),j_x, j_y );
  			cout<< child2->GetNode()->GetId()<< "\t : vul = " << child2->isVulnerable() <<endl;
  			if(child2->isVulnerable()){
  				anim.UpdateNodeColor (child2->GetNode(), 0, 0, 255); // Set vulnerable nodes to blue
	  		}
	  		else 
	  			anim.UpdateNodeColor (child2->GetNode(), 0, 255, 0); // Set unvulnerable nodes to green
	  		}
  		i_x+=FANOUT2;
  	}
	
	

	cout << "===== Starting Worm Simulator v2.0 =====" << endl<<endl;
	// for(int i = 0; i < root_node.GetN(); ++i){
	// 	cout << i <<" : "<<root_node.Get(i)->GetId() << endl;
	// }
	Simulator::Run ();
	Simulator::Destroy ();
	cout <<endl<< "===== Exiting Worm Simulator v2.0 =====" << endl;
	return 0;

}