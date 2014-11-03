# Write ns2.tcl code for specified network
# usage:
# ns ns2.tcl <TCP_VERSION> <CASE_NO>
# example: ns ns2.tcl Vegas 2

set ns [new Simulator]

set f0 [open src1_[lindex $argv 0]_[lindex $argv 1].tr w]
set f1 [open src2_[lindex $argv 0]_[lindex $argv 1].tr w]

set nf [open out.nam w]
$ns namtrace-all $nf

set sum_1 0
set sum_2 0
set count 0
#set throughput_avg [0]

proc finish {} {
	global f0 f1 argv sum_1 sum_2 count
	close $f0
	close $f1
	puts "Average throughput (src1) = [expr $sum_1/$count] MBits/s "
	puts "Average throughput (src2) = [expr $sum_2/$count] MBits/s "
	puts "Ratio of Average throughput (src1/src2) = [expr $sum_1/$sum_2] MBits/s \n"
	#exec nam out.nam &
	#Call xgraph to display the results
        exec xgraph src1_[lindex $argv 0]_[lindex $argv 1].tr src2_[lindex $argv 0]_[lindex $argv 1].tr -geometry 800x400 &
	exit 0
}

set n0 [$ns node]
set n1 [$ns node]
set n2 [$ns node]
set n3 [$ns node]
set n4 [$ns node]
set n5 [$ns node]

$ns duplex-link $n0 $n2 10Mb 5ms DropTail
$ns duplex-link $n3 $n2 1Mb 5ms DropTail
$ns duplex-link $n4 $n3 10Mb 5ms DropTail

if {[lindex $argv 1] == 1} {
$ns duplex-link $n1 $n2 10Mb 12.5ms DropTail
$ns duplex-link $n5 $n3 10Mb 12.5ms DropTail
} elseif {[lindex $argv 1] == 2} {
$ns duplex-link $n1 $n2 10Mb 20.0ms DropTail
$ns duplex-link $n5 $n3 10Mb 20.0ms DropTail
} else {
$ns duplex-link $n1 $n2 10Mb 27.5ms DropTail
$ns duplex-link $n5 $n3 10Mb 27.5ms DropTail
}

#Create a TCP agent and attach it to node n0
set tcp0 [new Agent/TCP/[lindex $argv 0]]
$ns attach-agent $n0 $tcp0

# Create a FTP traffic source and attach it to tcp0
set ftp0 [new Application/FTP]
$ftp0 attach-agent $tcp0

#Create a TCP agent and attach it to node n1
set tcp1 [new Agent/TCP/[lindex $argv 0]]
$ns attach-agent $n1 $tcp1

# Create a FTP traffic source and attach it to tcp1
set ftp1 [new Application/FTP]
$ftp1 attach-agent $tcp1

set sink0 [new Agent/TCPSink]
set sink1 [new Agent/TCPSink]
$ns attach-agent $n4 $sink0
$ns attach-agent $n5 $sink1

$ns connect $tcp0 $sink0
$ns connect $tcp1 $sink1

proc record {} {
	global sink0 sink1 f0 f1 sum_1 sum_2 count 
	#Get an instance of the simulator	
	set ns [Simulator instance]
	#Set the time after which the procedure should be called 		again	
	set time 0.01
	#How many bytes have been received by the traffic 		sinks?	
	set bw0 [$sink0 set bytes_]
	set bw1 [$sink1 set bytes_]
	#Get the current time
	set now [$ns now]
	#Calculate the bandwidth (in MBit/s) and write it to the 		files
	set sum_1 [expr $sum_1 + $bw0/$time*8/1000000];
	set sum_2 [expr $sum_2 + $bw1/$time*8/1000000];
	set count [expr $count + 1];
        puts $f0 "$now [expr $bw0/$time*8/1000000]"
        puts $f1 "$now [expr $bw1/$time*8/1000000]"
	#Reset the bytes_ values on the traffic sinks
        $sink0 set bytes_ 0
        $sink1 set bytes_ 0	
	#Re-schedule the procedure
        $ns at [expr $now+$time] "record"
}

$ns at 100.0 "record"
$ns at 0.0 "$ftp0 start"
$ns at 0.0 "$ftp1 start"
$ns at 400.0 "$ftp0 stop"
$ns at 400.0 "$ftp1 stop"

$tcp0 set class_ 1
$tcp1 set class_ 2
$ns color 1 Blue
$ns color 2 Red

$ns at 400.0 "finish"

$ns run

