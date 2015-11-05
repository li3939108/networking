# Write ns2.tcl code for specified network
# usage:
# ns ns2.tcl <TCP_VERSION> <CASE_NO>
# example: ns ns2.tcl Vegas 2

set ns [new Simulator]

set f0 [open src1_[lindex $argv 0]_[lindex $argv 1].tr w]
set f1 [open src2_[lindex $argv 0]_[lindex $argv 1].tr w]

set nf [open out.nam w]
$ns namtrace-all $nf

set starting_measure_count 10000
set sum_1 0
set sum_2 0
set count 0
#set throughput_avg [0]

proc finish {} {
	global f0 f1 argv sum_1 sum_2 count starting_measure_count;
	close $f0
	close $f1
  puts "count = [expr $count-$starting_measure_count] "
	puts "Average throughput (src1) = [expr $sum_1/($count-$starting_measure_count)] Mbps"
	puts "Average throughput (src2) = [expr $sum_2/($count-$starting_measure_count)] Mbps"
	puts "Ratio of Average throughput (src1/src2) = [expr $sum_1/$sum_2] \n"
	#exec nam out.nam &
	#Call xgraph to display the results
        exec xgraph src1_[lindex $argv 0]_[lindex $argv 1].tr src2_[lindex $argv 0]_[lindex $argv 1].tr -geometry 800x800 &
	exit 0
}

set src1 [$ns node]
set src2 [$ns node]
set r1 [$ns node]
set r2 [$ns node]
set rec1 [$ns node]
set rec2 [$ns node]

$ns duplex-link $src1 $r1 10Mb 5ms RED
$ns duplex-link $r2 $r1 1Mb 5ms RED
$ns duplex-link $rec1 $r2 10Mb 5ms RED

if {[lindex $argv 1] == 1} {
$ns duplex-link $src2 $r1 10Mb 12.5ms RED
$ns duplex-link $rec2 $r2 10Mb 12.5ms RED
} elseif {[lindex $argv 1] == 2} {
$ns duplex-link $src2 $r1 10Mb 20.0ms RED
$ns duplex-link $rec2 $r2 10Mb 20.0ms RED
} else {
$ns duplex-link $src2 $r1 10Mb 27.5ms RED
$ns duplex-link $rec2 $r2 10Mb 27.5ms RED
}
$ns duplex-link-op $r2 $r1   orient left
$ns duplex-link-op $r1 $src1 orient left-down 
$ns duplex-link-op $r1 $src2 orient left-up 
$ns duplex-link-op $r2 $rec2 orient right-up
$ns duplex-link-op $r2 $rec1 orient right-down

#Create a TCP agent and attach it to node src1
set tcp0 [new Agent/TCP/[lindex $argv 0]]
$ns attach-agent $src1 $tcp0

# Create a FTP traffic source and attach it to tcp0
set ftp0 [new Application/FTP]
$ftp0 attach-agent $tcp0

#Create a TCP agent and attach it to node src2
set tcp1 [new Agent/TCP/[lindex $argv 0]]
$ns attach-agent $src2 $tcp1

# Create a FTP traffic source and attach it to tcp1
set ftp1 [new Application/FTP]
$ftp1 attach-agent $tcp1

set sink0 [new Agent/TCPSink]
set sink1 [new Agent/TCPSink]
$ns attach-agent $rec1 $sink0
$ns attach-agent $rec2 $sink1

$ns connect $tcp0 $sink0
$ns connect $tcp1 $sink1

proc record {} {
	global sink0 sink1 f0 f1 sum_1 sum_2 count starting_measure_count 
	#Get an instance of the simulator	
	set ns [Simulator instance]
	#Set the time after which the procedure should be called 		again	
	set time 0.01

	#Get the current time
	set now [$ns now]
  if { $count > $starting_measure_count } {
	#How many bytes have been received by the traffic 		sinks?	
	set bw0 [$sink0 set bytes_]
	set bw1 [$sink1 set bytes_]
	#Calculate the bandwidth (in MBit/s) and write it to the 		files
	set sum_1 [expr $sum_1 + $bw0/$time*8/1000000];
	set sum_2 [expr $sum_2 + $bw1/$time*8/1000000];
        puts $f0 "$now [expr $bw0/$time*8/1000000]"
        puts $f1 "$now [expr $bw1/$time*8/1000000]"
	#Reset the bytes_ values on the traffic sinks
  }
        $sink0 set bytes_ 0
        $sink1 set bytes_ 0	
	      set count [expr $count + 1];
	#Re-schedule the procedure
        $ns at [expr $now+$time] "record"
}

$ns at 0.0 "record"
$ns at 0.0 "$ftp0 start"
$ns at 0.0 "$ftp1 start"
$ns at 400.0 "$ftp0 stop"
$ns at 400.0 "$ftp1 stop"

$tcp0 set class_ 1
$tcp1 set class_ 2
$ns color 1 Blue
$ns color 2 Red

$ns at 0.01 "$src1 label \"src1\""
$ns at 0.01 "$src2 label \"src2\""
$ns at 0.01 "$rec1 label \"rec1\""
$ns at 0.01 "$rec2 label \"rec2\""
$ns at 0.01 "$r1 label \"r1\""
$ns at 0.01 "$r2 label \"r2\""
$ns at 400.0 "finish"

$ns run

