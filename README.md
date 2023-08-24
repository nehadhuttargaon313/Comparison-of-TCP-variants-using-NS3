# Comparison-of-TCP-variants-using-NS3
Analyse and compare TCP Hybla, TCP Westwood+, and TCP YeAH-TCP performance. Select a Dumbbell topology 
with two routers R1 and R2 connected by a (10 Mbps, 50 ms) wired link. Each of the routers is connected to 3 
hosts, i.e. H1, H2, H3 (i.e. senders) are connected to R1, and H4, H5, H6 (i.e. receivers) are connected to R2. The 
hosts are attached with (100 Mbps, 20 ms) links. Both the routers use drop-tail queues with queue size set 
according to bandwidth-delay product. Senders (i.e. H1, H2 and H3) are attached with TCP Hybla, TCP 
Westwood+, and TCP YeAH-TCP agents, respectively. Choose a packet size of 1.3 KB and perform the following
tasks. Make appropriate assumptions wherever necessary.

<img width="532" alt="image" src="https://github.com/nehadhuttargaon313/Comparison-of-TCP-variants-using-NS3/assets/143022872/52c00535-9269-47ff-a647-f19e6552e01e">

1. Start only one flow and analyse the throughput over sufficiently long duration. Mention how you select
the duration. Plot the evolution of congestion window w.r.t. time. Perform this experiment with all the
flows attached to all the three sending agents.
2. In the next experiment, start 2 other flows sharing the bottleneck while the first one is in progress and
measure the throughput(in Kbps) of each flow. Plot the throughput and evolution of the TCP congestion 
window for each of the flow at a steady-state. Report the maximum throughput observed foreach of the
flows.
3. Measure the congestion loss and the goodput over the duration of the experiment for each of the flows.
