/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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


// Network topology
//
//       n1 ------ n2
//            p2p
//
// - UDP flows from n1 to n2

#include <fstream>
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Lab1");


int main (int argc, char *argv[])
{
  // Thay đổi các thông số này cho nhiều lần đo đạc khác nhau (3 lần)
  //
  double lat = 2.0; // độ trễ (in miliseconds)
  uint64_t rate = 5000000; // Data rate in bps 
  double interval = 0.05; // thời gian chờ

  CommandLine cmd;
  cmd.AddValue ("latency", "P2P link Latency in miliseconds", lat);
  cmd.AddValue ("rate", "P2P data rate in bps", rate);
  cmd.AddValue ("interval", "UDP client packet interval", interval);

  cmd.Parse (argc, argv);

//
// Enable logging for UdpClient and UdpServer
//
//  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
//  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);


  NS_LOG_INFO ("Create nodes.");
  NodeContainer n;
// Tạo 3 node 
  n.Create (3);

  NS_LOG_INFO ("Create channels.");
//
// Tạo một kênh kết nối giữa 2 node với các thông số kênh như trên
//
  PointToPointHelper p2p;
  p2p.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (lat)));
  p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (rate)));
  p2p.SetDeviceAttribute ("Mtu", UintegerValue (1400));
// Cài đặt các netDevice trên node0, node1
  NetDeviceContainer dev = p2p.Install (n.Get(0), n.Get(1));
// Cài đặt các netDevice trên node1, node2
  NetDeviceContainer dev2 = p2p.Install (n.Get(1), n.Get(2));

//
// We've got the "hardware" in place.  Now we need to add IP addresses.
//

//
// Install Internet Stack
//
  InternetStackHelper internet;
  internet.Install (n);
  Ipv4AddressHelper ipv4;

  NS_LOG_INFO ("Assign IP Addresses.");
  // Cấp phát vùng địa chỉ 10.1.1.0 đến các Net Devices nằm trong NetDeviceContainer1
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (dev);
 // Cấp phát vùng địa chỉ 10.1.2.0 đến các Net Devices nằm trong NetDeviceContainer2
  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i2 = ipv4.Assign (dev2);


  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  NS_LOG_INFO ("Create Applications.");
//
// Create one udpServer application on node one.
//
  uint16_t port1 = 8000; 
  uint16_t port2 = 8001;
  // Tạo 2 UdpServer trên 2 port 8000 và 8001
  UdpServerHelper server1 (port1);
  UdpServerHelper server2 (port2);
  // Dùng ApplicationContainer để cài đặt 2 server vừa tạo lên Node2 trong mô hình topology phía trên 
  ApplicationContainer apps;
  apps = server1.Install (n.Get (2));
  apps = server2.Install (n.Get (2));

  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

//
// Tạo ứng dụng Udp client để gởi UDP Datagram với số packet == maxPacketCount để gửi từ Node0 --> Node1
//
  uint32_t MaxPacketSize = 1024;
  Time interPacketInterval = Seconds (interval); // thời gia
  uint32_t maxPacketCount = 320; 
  UdpClientHelper client1 (i2.GetAddress (1), port1);
  UdpClientHelper client2 (i2.GetAddress (1), port2);
// Tạo 2 client với các thông số 
  client1.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  client1.SetAttribute ("Interval", TimeValue (interPacketInterval));
  client1.SetAttribute ("PacketSize", UintegerValue (MaxPacketSize));

  client2.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  client2.SetAttribute ("Interval", TimeValue (interPacketInterval));
  client2.SetAttribute ("PacketSize", UintegerValue (MaxPacketSize));

  apps = client1.Install (n.Get (0));
  apps = client2.Install (n.Get (0));

  apps.Start (Seconds (2.0));
  apps.Stop (Seconds (10.0));


//
// Enable tracing trên các net device nằm trong NetDeviceContainer1
//
  AsciiTraceHelper ascii;
  p2p.EnableAscii(ascii.CreateFileStream ("lab-1.tr"), dev);
  p2p.EnablePcap("lab-1", dev, false);

//
// Tính toán lưu lượng bằng flowMonitor
//
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();


//
// Bằng đầu chạy giả lập
//
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds(11.0));
  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      if ((t.sourceAddress=="10.1.1.1" && t.destinationAddress == "10.1.2.2"))
      {
          std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
          std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
          std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      	  std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1024/1024  << " Mbps\n";
      }
     }



  monitor->SerializeToXmlFile("lab-1.flowmon", true, true);

  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
}
