/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2007 University of Washington
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
 */
#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/address-utils.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/socket.h"
#include "ns3/udp-socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "worm-client.h"
#include <stdio.h>
#include <cstdio>
#include <stdlib.h>
#include <string>
#include <sstream>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("WormClientApplication");
NS_OBJECT_ENSURE_REGISTERED (WormClient);

uint32_t WormClient::num_infected = 0;

TypeId
WormClient::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::WormClient")
    .SetParent<Application> ()
    .AddConstructor<WormClient> ()
    .AddAttribute ("MaxPackets", 
                   "The maximum number of packets the application will send",
                   UintegerValue (100),
                   MakeUintegerAccessor (&WormClient::m_count),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Interval", 
                   "The time to wait between packets",
                   TimeValue (Seconds (1.0)),
                   MakeTimeAccessor (&WormClient::m_interval),
                   MakeTimeChecker ())
    .AddAttribute ("RemoteAddress", 
                   "The destination Address of the outbound packets",
                   AddressValue (),
                   MakeAddressAccessor (&WormClient::m_peerAddress),
                   MakeAddressChecker ())
    .AddAttribute ("RemotePort", 
                   "The destination port of the outbound packets",
                   UintegerValue (0),
                   MakeUintegerAccessor (&WormClient::m_peerPort),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("PacketSize", "Size of echo data in outbound packets",
                   UintegerValue (100),
                   MakeUintegerAccessor (&WormClient::SetDataSize,
                                         &WormClient::GetDataSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("mask", "Range to scan",
                   UintegerValue (0),
                   MakeUintegerAccessor (&WormClient::m_mask),
                   MakeUintegerChecker<uint32_t> ())
    .AddTraceSource ("Tx", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&WormClient::m_txTrace))
  ;
  return tid;
}

WormClient::WormClient ()
{
  NS_LOG_FUNCTION (this);
  m_sent = 0;
  m_socket = 0;
  m_sendEvent = EventId ();
  m_data = 0;
  m_dataSize = 0;
  m_infected = false;

}

WormClient::~WormClient()
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;

  delete [] m_data;
  m_data = 0;
  m_dataSize = 0;
}

void 
WormClient::SetRemote (Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_peerAddress = ip;
  m_peerPort = port;
}

void 
WormClient::SetRemote (Ipv4Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_peerAddress = Address (ip);
  m_peerPort = port;
}

void 
WormClient::SetRemote (Ipv6Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_peerAddress = Address (ip);
  m_peerPort = port;
}

void
WormClient::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  Application::DoDispose ();
}

void 
WormClient::StartApplication (void)
{
  NS_LOG_FUNCTION (this);
  //printf("starting worm\n");
  if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);
      InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_peerPort);
      m_socket->Bind (local);
      
      if (addressUtils::IsMulticast (m_local))
        {
          Ptr<UdpSocket> udpSocket = DynamicCast<UdpSocket> (m_socket);
          if (udpSocket)
            {
              // equivalent to setsockopt (MCAST_JOIN_GROUP)
              udpSocket->MulticastJoinGroup (0, m_local);
            }
          else
            {
              NS_FATAL_ERROR ("Error: Failed to join multicast group");
            }
        }

      // if (Ipv4Address::IsMatchingType(m_peerAddress) == true)
      //   {
      //     m_socket->Bind();
      //     m_socket->Connect (InetSocketAddress (Ipv4Address::ConvertFrom(m_peerAddress), m_peerPort));
      //   }
      // else if (Ipv6Address::IsMatchingType(m_peerAddress) == true)
      //   {
      //     m_socket->Bind6();
      //     m_socket->Connect (Inet6SocketAddress (Ipv6Address::ConvertFrom(m_peerAddress), m_peerPort));
      //   }
    }

  m_socket->SetRecvCallback (MakeCallback (&WormClient::HandleRead, this));
  if(m_infected)
    ScheduleTransmit (Seconds (0.));
}

void 
WormClient::StopApplication ()
{
  NS_LOG_FUNCTION (this);

  if (m_socket != 0) 
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
      m_socket = 0;
    }

  Simulator::Cancel (m_sendEvent);
}

void 
WormClient::SetDataSize (uint32_t dataSize)
{
  NS_LOG_FUNCTION (this << dataSize);

  //
  // If the client is setting the echo packet data size this way, we infer
  // that she doesn't care about the contents of the packet at all, so 
  // neither will we.
  //
  delete [] m_data;
  m_data = 0;
  m_dataSize = 0;
  m_size = dataSize;
}

uint32_t 
WormClient::GetDataSize (void) const
{
  NS_LOG_FUNCTION (this);
  return m_size;
}

void 
WormClient::SetFill (std::string fill)
{
  NS_LOG_FUNCTION (this << fill);

  uint32_t dataSize = fill.size () + 1;

  if (dataSize != m_dataSize)
    {
      delete [] m_data;
      m_data = new uint8_t [dataSize];
      m_dataSize = dataSize;
    }

  memcpy (m_data, fill.c_str (), dataSize);

  //
  // Overwrite packet size attribute.
  //
  m_size = dataSize;
}

void 
WormClient::SetFill (uint8_t fill, uint32_t dataSize)
{
  NS_LOG_FUNCTION (this << fill << dataSize);
  if (dataSize != m_dataSize)
    {
      delete [] m_data;
      m_data = new uint8_t [dataSize];
      m_dataSize = dataSize;
    }

  memset (m_data, fill, dataSize);

  //
  // Overwrite packet size attribute.
  //
  m_size = dataSize;
}

void 
WormClient::SetFill (uint8_t *fill, uint32_t fillSize, uint32_t dataSize)
{
  NS_LOG_FUNCTION (this << fill << fillSize << dataSize);
  if (dataSize != m_dataSize)
    {
      delete [] m_data;
      m_data = new uint8_t [dataSize];
      m_dataSize = dataSize;
    }

  if (fillSize >= dataSize)
    {
      memcpy (m_data, fill, dataSize);
      m_size = dataSize;
      return;
    }

  //
  // Do all but the final fill.
  //
  uint32_t filled = 0;
  while (filled + fillSize < dataSize)
    {
      memcpy (&m_data[filled], fill, fillSize);
      filled += fillSize;
    }

  //
  // Last fill may be partial
  //
  memcpy (&m_data[filled], fill, dataSize - filled);

  //
  // Overwrite packet size attribute.
  //
  m_size = dataSize;
}

void WormClient::setInfected(bool a)
{
  m_infected = a;
  num_infected++;
  NS_LOG_INFO("number of infected nodes: " << m_infected);
}

void 
WormClient::ScheduleTransmit (Time dt)
{
  NS_LOG_FUNCTION (this << dt);
  m_sendEvent = Simulator::Schedule (dt, &WormClient::Send, this);
}

void 
WormClient::Send (void)
{
  NS_LOG_FUNCTION (this);

  NS_ASSERT (m_sendEvent.IsExpired ());

  Ptr<Packet> p;
  if (m_dataSize)
    {
      //
      // If m_dataSize is non-zero, we have a data buffer of the same size that we
      // are expected to copy and send.  This state of affairs is created if one of
      // the Fill functions is called.  In this case, m_size must have been set
      // to agree with m_dataSize
      //
      NS_ASSERT_MSG (m_dataSize == m_size, "WormClient::Send(): m_size and m_dataSize inconsistent");
      NS_ASSERT_MSG (m_data, "WormClient::Send(): m_dataSize but no m_data");
      p = Create<Packet> (m_data, m_dataSize);
    }
  else
    {
      //
      // If m_dataSize is zero, the client has indicated that it doesn't care
      // about the data itself either by specifying the data size by setting
      // the corresponding attribute or by not calling a SetFill function.  In
      // this case, we don't worry about it either.  But we do allow m_size
      // to have a value different from the (zero) m_dataSize.
      //
      p = Create<Packet> (m_size);
    }
  // call to the trace sinks before the packet is actually sent,
  // so that tags added to the packet can be sent as well
  m_txTrace (p);
  //printf("sending pkts\n");
  Ipv4Address dest = genIP();
  //std::cout << "Random Address used :" << dest << std::endl;
  m_socket->Connect (InetSocketAddress(dest, m_peerPort));
  int error = m_socket->Send (p);

  NS_LOG_INFO("send error code " << error);

  ++m_sent;

  if (Ipv4Address::IsMatchingType (m_peerAddress))
    {
      NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s client sent " << m_size << " bytes to " <<
                   dest << " port " << m_peerPort);
    }
  else if (Ipv6Address::IsMatchingType (m_peerAddress))
    {
      NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s client sent " << m_size << " bytes to " <<
                   Ipv6Address::ConvertFrom (m_peerAddress) << " port " << m_peerPort);
    }

  //if (m_sent < m_count) 
    //{
      ScheduleTransmit (m_interval);
    //}
}

  void
  WormClient::HandleRead (Ptr<Socket> socket)
  {
    NS_LOG_FUNCTION (this << socket);
    Ptr<Packet> packet;
    Address from;
    if(!m_infected){
      m_infected = true;
      num_infected++;
      NS_LOG_DEBUG("At time " << Simulator::Now ().GetSeconds () << " infected nodes: " << num_infected);
      
      ScheduleTransmit (m_interval);
      return;
    }
    else {
      NS_LOG_INFO("I am already infected :c");
      return;
    }
    
    
    
    // Here we check if it's a UDP or TCP worm
    
    while ((packet = socket->RecvFrom (from)))
      {
        if (InetSocketAddress::IsMatchingType (from))
          {
            NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s client received " << packet->GetSize () << " bytes from " <<
                         InetSocketAddress::ConvertFrom (from).GetIpv4 () << " port " <<
                         InetSocketAddress::ConvertFrom (from).GetPort ());
          }
        else if (Inet6SocketAddress::IsMatchingType (from))
          {
            NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s client received " << packet->GetSize () << " bytes from " <<
                         Inet6SocketAddress::ConvertFrom (from).GetIpv6 () << " port " <<
                         Inet6SocketAddress::ConvertFrom (from).GetPort ());
          }

        //packet->RemoveAllPacketTags ();
        //packet->RemoveAllByteTags ();

        //NS_LOG_LOGIC ("Echoing packet");
        //socket->SendTo (packet, 0, from);
      }
  }

  Ipv4Address WormClient::genIP()
  {
    std::string temp;
    std::stringstream ss;
    ss << Ipv4Address::ConvertFrom (m_peerAddress);
    ss >> temp;

    
    std::string address = "";
    
    int t[4];
    char * pch;
    pch = strtok (const_cast<char*>(temp.c_str()),".");
    int i = 3;
    while (pch != NULL)
    {
      t[i] = atoi(pch);
      pch = strtok (NULL, ".");
      i--;
    }
    
    switch(m_mask)
    {
      case 3:
        t[3] = URV->GetInteger();
      case 2:
        t[2] = URV->GetInteger();
      case 1:
        t[1] = URV->GetInteger();
      case 0:
        t[0] = URV->GetInteger();
        address = std::to_string(t[3]) + "."+std::to_string(t[2]) + "."+std::to_string(t[1]) + "."+std::to_string(t[0]);
        break;
    }
    return Ipv4Address(address.c_str());
  }

} // Namespace ns3
