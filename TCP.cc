#include <string>
#include <fstream>
#include <cstdlib>
#include <unordered_map>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/gnuplot.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/gnuplot.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-helper.h"
#include "ns3/constant-position-mobility-model.h"

typedef uint32_t uint;

using namespace ns3;

#define ERROR 0.00001
#define PART 2

class ClientApp : public Application 
{
public:

  //ClientApp ();
  //virtual ~ClientApp();

  //void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);

  ClientApp ()
  : m_socket (0), 
    m_peer (), 
    m_packetSize (0), 
    m_nPackets (0), 
    m_dataRate (0), 
    m_sendEvent (), 
    m_running (false), 
    m_packetsSent (0) {}

  ~ClientApp(){
    m_socket = 0;
  }

  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate){
    m_socket = socket;
    m_peer = address;
    m_packetSize = packetSize;
    m_nPackets = nPackets;
    m_dataRate = dataRate;
  }
  

private:
  // virtual void StartApplication (void);
  // virtual void StopApplication (void);

  // void ScheduleTx (void);
  // void SendPacket (void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  DataRate        m_dataRate;
  EventId         m_sendEvent;
  bool            m_running;
  uint32_t        m_packetsSent;

  void ScheduleTx (){
    if (m_running){
      Time tNext (Seconds (m_packetSize * 8 / static_cast<double> (m_dataRate.GetBitRate ())));
      m_sendEvent = Simulator::Schedule (tNext, &ClientApp::SendPacket, this);
    }
  }

  void SendPacket (){
    Ptr<Packet> packet = Create<Packet> (m_packetSize);
    m_socket->Send (packet);

    if (++m_packetsSent < m_nPackets)
      ScheduleTx ();  
  }

  void StartApplication (){
    m_running = true;
    m_packetsSent = 0;
    m_socket->Bind ();
    m_socket->Connect (m_peer);
    SendPacket ();
  }

  void StopApplication (){
    m_running = false;

    if (m_sendEvent.IsRunning ())
      Simulator::Cancel (m_sendEvent);

    if (m_socket)
      m_socket->Close ();
  }

};

std::map<Address, double>  mapBytesReceivedAppLayer, mapMaxGoodput;
std::map<std::string, double> mapBytesReceivedIPV4, mapMaxThroughput;
std::unordered_map<uint, uint> c_loss;
static void CwndChange(Ptr<OutputStreamWrapper> stream, double startTime, uint id, uint oldCwnd, uint newCwnd) {

  if(newCwnd < oldCwnd)
    c_loss[id] ++;
	*stream->GetStream() << Simulator::Now ().GetSeconds () - startTime << "\t" << newCwnd << std::endl;
}

void PacketRcvdIPV4(Ptr<OutputStreamWrapper> stream, double startTime, std::string context, Ptr<const Packet> p, Ptr<Ipv4> ipv4, uint interface) {
	double timeNow = Simulator::Now().GetSeconds();

	if(mapBytesReceivedIPV4.find(context) == mapBytesReceivedIPV4.end())
		mapBytesReceivedIPV4[context] = 0;
	if(mapMaxThroughput.find(context) == mapMaxThroughput.end())
		mapMaxThroughput[context] = 0;
	mapBytesReceivedIPV4[context] += p->GetSize();
	double kbps = (((mapBytesReceivedIPV4[context] * 8.0) / 1024)/(timeNow-startTime));
	*stream->GetStream() << timeNow-startTime << "\t" <<  kbps << std::endl;
	if(mapMaxThroughput[context] < kbps)
		mapMaxThroughput[context] = kbps;
}

void PacketRcvdAppLayer(Ptr<OutputStreamWrapper> stream, double startTime, std::string context, Ptr<const Packet> p, const Address& addr){
	double timeNow = Simulator::Now().GetSeconds();

	if(mapBytesReceivedAppLayer.find(addr) == mapBytesReceivedAppLayer.end())
		mapBytesReceivedAppLayer[addr] = 0;
	mapBytesReceivedAppLayer[addr] += p->GetSize();
	double kbps = (((mapBytesReceivedAppLayer[addr] * 8.0) / 1024)/(timeNow-startTime));
	*stream->GetStream() << timeNow-startTime << "\t" <<  kbps << std::endl;
  if(mapMaxGoodput[addr] < kbps)
		mapMaxGoodput[addr] = kbps;
}

std::unordered_map<uint, uint> mapDrop;
static void packetDrop(Ptr<OutputStreamWrapper> stream, double startTime, uint myId) {
	*stream->GetStream() << Simulator::Now ().GetSeconds () - startTime << "\t" << std::endl;
	if(mapDrop.find(myId) == mapDrop.end()) {
		mapDrop[myId] = 0;
	}
	mapDrop[myId]++;
}


Ptr<Socket> singleFlow(Address sinkAddress, 
					uint sinkPort, 
					std::string tcpVariant, 
					Ptr<Node> hostNode, 
					Ptr<Node> sinkNode, 
					double startTime, 
					double stopTime,
					uint packetSize,
					uint numPackets,
					std::string dataRate,
					double appStartTime,
					double appStopTime) {

	if(tcpVariant.compare("TcpHybla") == 0) 
		Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpHybla::GetTypeId()));
	else if(tcpVariant.compare("TcpWestwood") == 0) 
		Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpWestwood::GetTypeId()));
  else if(tcpVariant.compare("TcpYeah") == 0) 
		Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpYeah::GetTypeId()));
  else {
    std::cout << tcpVariant << std::endl;
		fprintf(stderr, "Invalid TCP version\n");
		exit(EXIT_FAILURE);
	}

	PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
	ApplicationContainer sinkApps = packetSinkHelper.Install(sinkNode);
	sinkApps.Start(Seconds(startTime));
	sinkApps.Stop(Seconds(stopTime));

	Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(hostNode, TcpSocketFactory::GetTypeId());
	
	Ptr<ClientApp> app = CreateObject<ClientApp>();
	app->Setup(ns3TcpSocket, sinkAddress, packetSize, numPackets, DataRate(dataRate));
	hostNode->AddApplication(app);
	app->SetStartTime(Seconds(appStartTime));
	app->SetStopTime(Seconds(appStopTime));

	return ns3TcpSocket;
}


int main(){
  uint32_t    nLeftLeaf = 3;
  uint32_t    nRightLeaf = 3;
  std::string rateHR = "100Mbps";
	std::string latencyHR = "20ms";
	std::string rateRR = "10Mbps";
	std::string latencyRR = "50ms";
  uint packetSize = 1.3*1000;		//1.3KB
  //uint qs = 10;

  //Config::SetDefault ("ns3::DropTailQueue<Packet>::MaxSize", StringValue ("40p"));
  PointToPointHelper pointToPointAccess, pointToPointBottleneck;
  pointToPointAccess.SetDeviceAttribute("DataRate", StringValue(rateHR));
	pointToPointAccess.SetChannelAttribute("Delay", StringValue(latencyHR));
	pointToPointAccess.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", StringValue("190p"));//QueueSizeValue (QueueSize ("190p"))); //b*d = 100*10^6 /8 B/s * 20 * 10^-3 s = 250000 B = 193 packets
  //pointToPointAccess.SetQueue("ns3::DropTailQueue<Packet>");
  pointToPointBottleneck.SetDeviceAttribute("DataRate", StringValue(rateRR));
	pointToPointBottleneck.SetChannelAttribute("Delay", StringValue(latencyRR));
	pointToPointBottleneck.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", StringValue("49p"));// QueueSizeValue (QueueSize ("49p")));
  //pointToPointBottleneck.SetQueue("ns3::DropTailQueue<Packet>");

  Ptr<RateErrorModel> em = CreateObjectWithAttributes<RateErrorModel> ("ErrorRate", DoubleValue (ERROR));

  NodeContainer routers, senders, receivers;
	//Create n nodes and append pointers to them to the end of this NodeContainer. 
	routers.Create(2);
	senders.Create(nLeftLeaf);
	receivers.Create(nRightLeaf);

  NetDeviceContainer routerDevices = pointToPointBottleneck.Install(routers);
  NetDeviceContainer leftRouterDevices, rightRouterDevices, senderDevices, receiverDevices;
  
  for(uint i = 0; i < nLeftLeaf; ++i) {
		NetDeviceContainer cleft = pointToPointAccess.Install(routers.Get(0), senders.Get(i));
		leftRouterDevices.Add(cleft.Get(0));
		senderDevices.Add(cleft.Get(1));
		cleft.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em));

		NetDeviceContainer cright = pointToPointAccess.Install(routers.Get(1), receivers.Get(i));
		rightRouterDevices.Add(cright.Get(0));
		receiverDevices.Add(cright.Get(1));
		cright.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em));
	}

  // Install Stack
  InternetStackHelper stack;
	stack.Install(routers);
	stack.Install(senders);
	stack.Install(receivers);


  // Assign IP Addresses
  Ipv4AddressHelper routerIP = Ipv4AddressHelper("10.3.0.0", "255.255.255.0");	//(network, mask)
	Ipv4AddressHelper senderIP = Ipv4AddressHelper("10.1.0.0", "255.255.255.0");
	Ipv4AddressHelper receiverIP = Ipv4AddressHelper("10.2.0.0", "255.255.255.0");
	

	Ipv4InterfaceContainer routerIFC, senderIFCs, receiverIFCs, leftRouterIFCs, rightRouterIFCs;
  routerIFC = routerIP.Assign(routerDevices);

  double simTime = 100;
	double start = 0;
	uint port = 18000;
	uint numPackets = 10000000;
	std::string transferSpeed = "100Mbps";	
  AsciiTraceHelper asciiTraceHelper;
  
  std::string tcpVariant[nLeftLeaf] = {"TcpHybla", "TcpWestwood", "TcpYeah"};
  std::string nodeInd[nLeftLeaf] = {"5", "6", "7"};
  for(uint i = 0; i < nLeftLeaf; ++i) {
		NetDeviceContainer senderDevice;
		senderDevice.Add(senderDevices.Get(i));
		senderDevice.Add(leftRouterDevices.Get(i));
		Ipv4InterfaceContainer senderIFC = senderIP.Assign(senderDevice);
		senderIFCs.Add(senderIFC.Get(0));
		leftRouterIFCs.Add(senderIFC.Get(1));
		senderIP.NewNetwork();

		NetDeviceContainer receiverDevice;
		receiverDevice.Add(receiverDevices.Get(i));
		receiverDevice.Add(rightRouterDevices.Get(i));
		Ipv4InterfaceContainer receiverIFC = receiverIP.Assign(receiverDevice);
		receiverIFCs.Add(receiverIFC.Get(0));
		rightRouterIFCs.Add(receiverIFC.Get(1));
		receiverIP.NewNetwork();
  }

  if(PART==1){
    std::string trace[nLeftLeaf] = {"singleFlow1to4", "singleFlow2to5", "singleFlow3to6"};
    for(uint i = 0; i < nLeftLeaf; ++i){
      Ptr<OutputStreamWrapper> streamCWND = asciiTraceHelper.CreateFileStream(trace[i] + ".cwnd");
      Ptr<OutputStreamWrapper> streamTP = asciiTraceHelper.CreateFileStream(trace[i] + ".tp");
      Ptr<OutputStreamWrapper> streamGP = asciiTraceHelper.CreateFileStream(trace[i] + ".gp");
      Ptr<OutputStreamWrapper> streamPD = asciiTraceHelper.CreateFileStream(trace[i] + ".pd");
      Ptr<Socket> ns3TcpSocket1 = singleFlow(InetSocketAddress(receiverIFCs.GetAddress(i), port), port, tcpVariant[i], senders.Get(i), receivers.Get(i), start, start+simTime, packetSize, numPackets, transferSpeed, start, start+simTime);
	    ns3TcpSocket1->TraceConnectWithoutContext("Drop", MakeBoundCallback (&packetDrop, streamPD, 0, i+1));
      ns3TcpSocket1->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback (&CwndChange, streamCWND, start, i+1));
      std::string sink = "/NodeList/" + nodeInd[i] + "/$ns3::Ipv4L3Protocol/Rx";
      std::string appSink = "/NodeList/" + nodeInd[i] + "/ApplicationList/0/$ns3::PacketSink/Rx";
	    Config::Connect(sink, MakeBoundCallback(&PacketRcvdIPV4, streamTP, start));
      Config::Connect(appSink, MakeBoundCallback(&PacketRcvdAppLayer, streamGP, start));
      start += simTime;
    }
  }

  if(PART==2){
      std::string trace[nLeftLeaf] = {"multiFlow1to4", "multiFlow2to5", "multiFlow3to6"};
      Ptr<OutputStreamWrapper> streamCWND = asciiTraceHelper.CreateFileStream(trace[0] + ".cwnd");
      Ptr<OutputStreamWrapper> streamTP = asciiTraceHelper.CreateFileStream(trace[0] + ".tp");
      Ptr<OutputStreamWrapper> streamGP = asciiTraceHelper.CreateFileStream(trace[0] + ".gp");
      Ptr<OutputStreamWrapper> streamPD = asciiTraceHelper.CreateFileStream(trace[0] + ".pd");
      Ptr<Socket> ns3TcpSocket1 = singleFlow(InetSocketAddress(receiverIFCs.GetAddress(0), port), port, tcpVariant[0], senders.Get(0), receivers.Get(0), start, start+simTime, packetSize, numPackets, transferSpeed, start, start+simTime);
	    ns3TcpSocket1->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback (&CwndChange, streamCWND, start, 1));
      ns3TcpSocket1->TraceConnectWithoutContext("Drop", MakeBoundCallback (&packetDrop, streamPD, 0, 1));
      std::string sink = "/NodeList/" + nodeInd[0] + "/$ns3::Ipv4L3Protocol/Rx";
      std::string appSink = "/NodeList/" + nodeInd[0] + "/ApplicationList/0/$ns3::PacketSink/Rx";
	    Config::Connect(sink, MakeBoundCallback(&PacketRcvdIPV4, streamTP, start));
      Config::Connect(appSink, MakeBoundCallback(&PacketRcvdAppLayer, streamGP, start));
      start = 20;
    for(uint i = 1; i < nLeftLeaf; ++i){
      Ptr<OutputStreamWrapper> streamCWND = asciiTraceHelper.CreateFileStream(trace[i] + ".cwnd");
      Ptr<OutputStreamWrapper> streamTP = asciiTraceHelper.CreateFileStream(trace[i] + ".tp");
      Ptr<OutputStreamWrapper> streamGP = asciiTraceHelper.CreateFileStream(trace[i] + ".gp");
      Ptr<OutputStreamWrapper> streamPD = asciiTraceHelper.CreateFileStream(trace[i] + ".pd");
      Ptr<Socket> ns3TcpSocket1 = singleFlow(InetSocketAddress(receiverIFCs.GetAddress(i), port), port, tcpVariant[i], senders.Get(i), receivers.Get(i), start, start+simTime, packetSize, numPackets, transferSpeed, start, start+simTime);
	    ns3TcpSocket1->TraceConnectWithoutContext("Drop", MakeBoundCallback (&packetDrop, streamPD, 0, i+1));
      ns3TcpSocket1->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback (&CwndChange, streamCWND, start, i+1));
      std::string sink = "/NodeList/" + nodeInd[i] + "/$ns3::Ipv4L3Protocol/Rx";
      std::string appSink = "/NodeList/" + nodeInd[i] + "/ApplicationList/0/$ns3::PacketSink/Rx";
	    Config::Connect(sink, MakeBoundCallback(&PacketRcvdIPV4, streamTP, start));
      Config::Connect(appSink, MakeBoundCallback(&PacketRcvdAppLayer, streamGP, start));
      //start += simTime;
    }
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Ptr<FlowMonitor> flowmon;
	FlowMonitorHelper flowmonHelper;
	flowmon = flowmonHelper.InstallAll();
  
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (routers);
  mobility.Install (senders);
  mobility.Install (receivers);
  //pointToPointAccess.BoundingBox (1, 1, 100, 100);
  AnimationInterface anim ("anim.xml");

  Ptr<ConstantPositionMobilityModel> s1 = routers.Get (0)->GetObject<ConstantPositionMobilityModel> ();
  Ptr<ConstantPositionMobilityModel> s2 = routers.Get (1)->GetObject<ConstantPositionMobilityModel> ();
  s1->SetPosition (Vector ( 33.0, 50.0, 0  ));
  s2->SetPosition (Vector ( 66.0, 50.0, 0  ));
  Ptr<ConstantPositionMobilityModel> s3 = senders.Get (0)->GetObject<ConstantPositionMobilityModel> ();
  Ptr<ConstantPositionMobilityModel> s4 = senders.Get (1)->GetObject<ConstantPositionMobilityModel> ();
  Ptr<ConstantPositionMobilityModel> s5 = senders.Get (2)->GetObject<ConstantPositionMobilityModel> ();
  Ptr<ConstantPositionMobilityModel> s6 = receivers.Get (0)->GetObject<ConstantPositionMobilityModel> ();
  Ptr<ConstantPositionMobilityModel> s7 = receivers.Get (1)->GetObject<ConstantPositionMobilityModel> ();
  Ptr<ConstantPositionMobilityModel> s8 = receivers.Get (2)->GetObject<ConstantPositionMobilityModel> ();
  s3->SetPosition (Vector ( 0, 25.0, 0  ));
  s4->SetPosition (Vector ( 0, 50.0, 0  ));
  s5->SetPosition (Vector ( 0, 75.0, 0  ));
  s6->SetPosition (Vector ( 100.0, 25.0, 0  ));
  s7->SetPosition (Vector ( 100.0, 50.0, 0  ));
  s8->SetPosition (Vector ( 100.0, 75.0, 0  ));
  Simulator::Stop (Seconds (350.0));
  Simulator::Run ();
  flowmon->CheckForLostPackets();
  Simulator::Destroy ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
	std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats();
	for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
		Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
    //std::cout << i->second.bytesDropped.size () << "size\n";
		if(t.sourceAddress == "10.1.0.1") {
			std::cout << "TcpReno Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "Packets sent " << i->second.txPackets << "\n";
      std::cout << "Packets received " << i->second.rxPackets << "\n";
			std::cout  << "Congestion loss: " << c_loss[1] << "\n";
			std::cout << "Max throughput: " << mapMaxThroughput["/NodeList/5/$ns3::Ipv4L3Protocol/Rx"] << std::endl << std::endl;
		} 
    else if(t.sourceAddress == "10.1.1.1") {
			std::cout << "TcpWestwood Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "Packets sent " << i->second.txPackets << "\n";
      std::cout << "Packets received " << i->second.rxPackets << "\n";
			std::cout  << "Congestion loss: "  << c_loss[2]  << "\n";
			std::cout << "Max throughput: " << mapMaxThroughput["/NodeList/6/$ns3::Ipv4L3Protocol/Rx"] << std::endl << std::endl;
		} 
    else if(t.sourceAddress == "10.1.2.1") {
			std::cout << "TcpFack Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "Packets sent " << i->second.txPackets << "\n";
      std::cout << "Packets received " << i->second.rxPackets << "\n";
			std::cout  << "Congestion loss: " << " " << c_loss[3]   << "\n";
			std::cout << "Max throughput: " << mapMaxThroughput["/NodeList/7/$ns3::Ipv4L3Protocol/Rx"] << std::endl << std::endl;
		}
	}
  
  return 0;
}
