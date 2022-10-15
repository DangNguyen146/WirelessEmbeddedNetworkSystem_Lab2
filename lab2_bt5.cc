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

#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ThirdScriptExample");

int 
main (int argc, char *argv[])
{
  bool verbose = true;
  uint32_t nWifi = 5;

  CommandLine cmd;
  cmd.AddValue ("nWifi", "Number of wifi STA devices", nWifi);
  cmd.Parse (argc,argv);

  // Check for valid number of csma or wifi nodes
  // 250 should be enough, otherwise IP addresses 
  // soon become an issue
  if (nWifi > 250)
    {
      std::cout << "Too many wifi nodes, no more than 250 each." << std::endl;
      return 1;
    }

  if (verbose)
  {
      LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
  }

//////////////////////////////////////////////////////////////////////////////////////////
  // ******* 1. Đầu tiên tạo các Node và lấy một node đầu tiên (Node0) làm AP *******

  // Access Point(AP) là một thiết bị có khả năng tạo ra WLAN, hay còn gọi là mạng không dây
  // cục bộ. Access Point thường được dùng tại môi trường công sở, nhà hàng, tiệc cưới
  // hay các tòa nhà lớn nhằm tạo ra không gian sử dụng mạng rộng rãi mà không làm
  // suy giảm tốc độ của mạng. Ngoài ra, Access Point còn khả năng chuyển đổi mạng có dây
  // thành mạng không dây

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (nWifi);
  // Dùng node0 làm AP
  NodeContainer wifiApNode = wifiStaNodes.Get (0);

//////////////////////////////////////////////////////////////////////////////////////////
  // ******* 2. Tạo kênh kết nối WLAN (YansWifiChannel trong ns-3 *******
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");
  // Cài đặt các thông số cho kênh truyền Wifi như SSID
  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-ssid");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));


//////////////////////////////////////////////////////////////////////////////////////////
  // ******* 3. Cài đặt kênh kết nối WLAN lên các NetDevice của các node và AP *******
  // Cài đặt kênh truyền WLAN lên các node 
  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevices;
  // Cài đặt kênh truyền WLAN lên node AP
  apDevices = wifi.Install (phy, mac, wifiApNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
  mobility.Install (wifiStaNodes);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode);

//////////////////////////////////////////////////////////////////////////////////////////
  // ******* 4. Cài đặt IPv4_Stack lên tất các nodes và cấp phát IP Addr cho các NetDevice 
  // Interfaces vừa cài đặt ở bước 3 *******

  // Cài đặt IPv4 Stack cho các node
  InternetStackHelper stack;
  stack.Install (wifiStaNodes);
  Ipv4AddressHelper address;
  // Cấp IP cho các nodes 
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer wifiInterfaces;
  wifiInterfaces = address.Assign (staDevices);
  address.Assign (apDevices);

//////////////////////////////////////////////////////////////////////////////////////////
  // ******* 5. Cài đặt UDP Echo Client và Server *******

  // Cài đặt UDPEchoServer trên port 9000 ở node4
  UdpEchoServerHelper echoServer (9000);
  ApplicationContainer serverApps = echoServer.Install (wifiStaNodes.Get (nWifi - 1));  // Cài lên node4
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Cài đặt UDPEchoClient trên một node thuộc WLAN
  UdpEchoClientHelper echoClient (wifiInterfaces.GetAddress (nWifi - 1), 9000);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (100));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.5)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  UdpEchoClientHelper echoClient2 (wifiInterfaces.GetAddress (nWifi - 1), 9002);
  echoClient2.SetAttribute ("MaxPackets", UintegerValue (100));
  echoClient2.SetAttribute ("Interval", TimeValue (Seconds (0.5)));
  echoClient2.SetAttribute ("PacketSize", UintegerValue (1024));

  // Cài lên node1
  ApplicationContainer clientApps = 
  echoClient.Install (wifiStaNodes.Get (1));

  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  AsciiTraceHelper ascii;
  phy.EnableAscii(ascii.CreateFileStream("./lab2/lab2.tr"), staDevices);
  // access point
  phy.EnablePcap("./lab2/lab2_ap", staDevices.Get (0), true);
  // updclient
  phy.EnablePcap("./lab2/lab2_udpclient", staDevices.Get (1), true);
  // udpserver
  phy.EnablePcap("./lab2/lab2_udpserver", staDevices.Get (4), true);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  Simulator::Destroy ();
  return 0;
}