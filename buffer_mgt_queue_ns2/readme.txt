ECEN602 HW5 NS2 Simulation 
(implementation of a small network in ns2)
-----------------------------------------------------------------

Team Number: 30
Member 1 # Raghavan S V (UIN: 722009006)
Member 2 # Hari Kishan Srikonda (UIN: 924002529)
Member 3 # Jiaju Shi (UIN: 823001803)
---------------------------------------

Description/Comments:
--------------------
1. Created eight nodes n0 and n1 correspond to src1 and src2; n2 and n3 correspond to R1 and R2; n4 and n5 correspond to rcv1 and rcv2. n6 to src3 and n7 to rcv3
2. Established appropriate links(duplex) wih DropTail and RED queue mechanism 
3. Created TCP agents and attached it to n0 and n1, UDP agents to n6
4. Added FTP traffic source over these TCP agents, CBR traffic over UDP
5. Created two TCP sinks and attached them to n4 and n5, UDP sink to n7
6. Recorded bandwidth data using set bw0 and set bw1 at the two sinks, set bw2 to the third sink.
7. Used four global variables sum_1, sum_2 sum_3 and count to calculate the average throughput of src1, src2 and src3
4. Printed these results using puts. 

Usage Syntax:
-------------------
ns ns2.tcl <QUEUE STRATEGY> <SCENARIO_NO>

<QUEUE STRATEGY> is either of { DROPTAIL, RED }
<SCENARIO_NO> is either of { 1, 2}

