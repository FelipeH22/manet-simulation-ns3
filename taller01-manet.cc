/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Este archivo es un ejemplo basado en mixed-wired-wireless.cc de NS-3,
 * modificado para ilustrar la creación de:
 *   - Dos clústeres inalámbricos en el primer nivel (Cluster1 y Cluster2)
 *   - Un clúster adicional (Cluster3) en un segundo nivel, 
 *     enlazado a uno de los primeros.
 *
 * Se usa enrutamiento ad hoc (AODV) y un ejemplo de tráfico UDP (UdpEcho).
 * Además, se muestra cómo habilitar trazas Pcap y animaciones XML.
 */

#include <iostream>
#include <cmath>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MixedWiredWirelessHierarchical");

static void
CourseChangeCallback (std::string context, Ptr<const MobilityModel> mobility)
{
  Vector pos = mobility->GetPosition ();
  Vector vel = mobility->GetVelocity ();
  std::cout << Simulator::Now ().GetSeconds () 
            << "s, " << context
            << " -> POS: x=" << pos.x << ", y=" << pos.y
            << ", z=" << pos.z << "; VEL: x=" << vel.x
            << ", y=" << vel.y << ", z=" << vel.z << std::endl;
}

int main(int argc, char *argv[])
{
  // Parametrización desde línea de comandos
  bool useCourseChangeCallback = false;
  uint32_t nNodesC1 = 3; // Número de nodos en el Cluster1 (primer nivel)
  uint32_t nNodesC2 = 3; // Número de nodos en el Cluster2 (primer nivel)
  uint32_t nNodesC3 = 2; // Número de nodos extra en Cluster3 (segundo nivel)
  double stopTime = 30.0; // Tiempo de simulación (segundos)

  CommandLine cmd (__FILE__);
  cmd.AddValue("useCourseChangeCallback",
               "Habilitar callback para el cambio de curso (movilidad).",
               useCourseChangeCallback);
  cmd.AddValue("nNodesC1",
               "Cantidad de nodos en el cluster1 (nivel 1).",
               nNodesC1);
  cmd.AddValue("nNodesC2",
               "Cantidad de nodos en el cluster2 (nivel 1).",
               nNodesC2);
  cmd.AddValue("nNodesC3",
               "Cantidad de nodos extra en cluster3 (nivel 2).",
               nNodesC3);
  cmd.AddValue("stopTime",
               "Tiempo de simulación en segundos.",
               stopTime);
  cmd.Parse(argc, argv);

  NS_LOG_INFO("Creando nodos de la simulación...");

  // -----------------------------------------------------------
  // 1) Crear los nodos para cada cluster
  // -----------------------------------------------------------
  NodeContainer cluster1;
  cluster1.Create(nNodesC1);

  NodeContainer cluster2;
  cluster2.Create(nNodesC2);

  // Para el "segundo nivel", creamos un set de nodos extra
  NodeContainer cluster3;
  cluster3.Create(nNodesC3);

  // Nodo puente
  Ptr<Node> gatewayNode = cluster1.Get(0);

  // -----------------------------------------------------------
  // 2) Configurar PHY y MAC para redes Wi-Fi ad hoc
  //    - Crearemos dos Helpers distintos para cluster1 y cluster2 
  //      (aunque podrían compartir la config).
  // -----------------------------------------------------------
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n);
  WifiMacHelper mac;
  mac.SetType("ns3::AdhocWifiMac"); // Ad hoc

  // Cluster 1
  NetDeviceContainer devicesC1 = wifi.Install(phy, mac, cluster1);

  // Cluster 2
  YansWifiPhyHelper phy2;
  phy2.SetChannel(channel.Create());
  WifiHelper wifi2;
  wifi2.SetStandard(WIFI_STANDARD_80211n);
  WifiMacHelper mac2;
  mac2.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devicesC2 = wifi2.Install(phy2, mac2, cluster2);

  // -----------------------------------------------------------
  // 3) Enlazar el cluster3 con el gatewayNode a través de
  //    (a) segunda interfaz Wi-Fi
  //    (b) o un enlace CSMA
  // Para fines ilustrativos, usaremos una segunda red Wi-Fi ad hoc
  // en gatewayNode. 
  // Esto implica que gatewayNode tendrá 2 interfaces Wi-Fi.
  // -----------------------------------------------------------

  // Creamos un nuevo WifiPhy/Mac para cluster3
  YansWifiPhyHelper phy3;
  phy3.SetChannel(channel.Create());  
  WifiHelper wifi3;
  wifi3.SetStandard(WIFI_STANDARD_80211n);
  WifiMacHelper mac3;
  mac3.SetType("ns3::AdhocWifiMac");

  // Contenedor que incluye gatewayNode + los nodos de cluster3
  NodeContainer cluster3All;
  cluster3All.Add(gatewayNode);   // el primer nodo es el gateway
  cluster3All.Add(cluster3);      // los nodos "nuevos" de nivel 2

  NetDeviceContainer devicesC3 = wifi3.Install(phy3, mac3, cluster3All);

  
  // 4) Instalar capa de Internet con enrutamiento ad hoc 
  //    (AODV) en todos los nodos.

  AodvHelper aodv;
  Ipv4ListRoutingHelper list;
  list.Add(aodv, 100);

  InternetStackHelper stack;
  stack.SetRoutingHelper(list);

  // Instalamos la pila de protocolos en TODOS los nodos 
  // (cluster1, cluster2, cluster3)
  // Recordar que "gatewayNode" es parte de cluster1, 
  // y cluster3All incluye gatewayNode + cluster3.
  stack.Install(cluster1);
  stack.Install(cluster2);
  stack.Install(cluster3);

  
  // 5) Asignar direcciones IP (subredes diferentes)
  //    - Cluster1 en 10.1.1.x
  //    - Cluster2 en 10.1.2.x
  //    - Cluster3 en 10.1.3.x
  
  Ipv4AddressHelper address;

  // Cluster 1
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifacesC1 = address.Assign(devicesC1);

  // Cluster 2
  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer ifacesC2 = address.Assign(devicesC2);

  // Cluster3 (gatewayNode + nodos extras)
  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer ifacesC3 = address.Assign(devicesC3);

  // 6) Configurar movilidad

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));

  mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                            "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
                            "Pause", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
                            "PositionAllocator", StringValue("ns3::RandomRectanglePositionAllocator"));

  // Instalar movilidad en cluster1, cluster2 y cluster3All
  mobility.Install(cluster1);
  mobility.Install(cluster2);
  mobility.Install(cluster3All);

  
  // 7) Crear tráfico de prueba
  //    Ejemplo: un servidor en cluster3 y un cliente en cluster2.
  
  uint16_t port = 9; // Puerto UDP
  UdpEchoServerHelper echoServer(port);

  // Instalar Server en el primer nodo de cluster3 (por ejemplo)
  ApplicationContainer serverApps = echoServer.Install(cluster3.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(stopTime - 1));

  // Cliente en cluster2
  UdpEchoClientHelper echoClient(ifacesC3.GetAddress(1), port); 
  // ifacesC3.GetAddress(1) = IP del cluster3.Get(0), 
  // asumiendo que 0 es gatewayNode y 1 es cluster3.Get(0). 
  // Ajusta si difiere.

  echoClient.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(2.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(512));

  ApplicationContainer clientApps = echoClient.Install(cluster2.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(stopTime - 1));

  
  // 8) Habilitar Pcap 

  phy.EnablePcap("mixed-wireless", devicesC1.Get(0), true);

  if (useCourseChangeCallback)
    {
      Config::Connect("/NodeList/*/$ns3::MobilityModel/CourseChange",
                      MakeCallback(&CourseChangeCallback));
    }

  // -----------------------------------------------------------
  // 9) Animación NetAnim (XML)
  // -----------------------------------------------------------
  AnimationInterface anim("mixed-wireless.xml");
  // Podemos etiquetar nodos (opcional)
  anim.SetConstantPosition(cluster1.Get(0), 10, 10); // etc.

  // -----------------------------------------------------------
  // 10) Ejecutar la simulación
  // -----------------------------------------------------------
  NS_LOG_INFO("Run Simulation.");
  Simulator::Stop(Seconds(stopTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}

