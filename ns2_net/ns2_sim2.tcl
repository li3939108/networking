# Write ns2.tcl code for specified network
# usage:
# ns ns2.tcl <TCP_VERSION> <CASE_NO>
# example: ns ns2.tcl DropTail 1

set ns [new Simulator]

set f0 [open src1_[lindex $argv 0]_[lindex $argv 1].tr w]
set f1 [open src2_[lindex $argv 0]_[lindex $argv 1].tr w]
set f2 [open src3_[lindex $argv 0]_[lindex $argv 1].tr w]

set nf [open out.nam w]
$ns namtrace-all $nf

set starting_measure_count 3000
set sum_1 0
set sum_2 0
set sum_3 0

set count 0
#set throughput_avg [0]
if {[lindex $argv 1] == 1} {
proc finish {} {
	global f0 f1 argv sum_1 sum_2 count starting_measure_count;
	close $f0
	close $f1
	puts "count = [expr $count-$starting_measure_count] "
	puts "Average throughput (src1) = [expr $sum_1/($count-$starting_measure_count)] Mbps"
	puts "Average throughput (src2) = [expr $sum_2/($count-$starting_measure_count)] Mbps"
	#exec nam out.nam &
	#Call xgraph to display the results
	exec xgraph src1_[lindex $argv 0]_[lindex $argv 1].tr src2_[lindex $argv 0]_[lindex $argv 1].tr -geometry 800x800 &
	exit 0
}
} elseif {[lindex $argv 1] == 2} {
proc finish {} {
	global f0 f1 f2 argv sum_1 sum_2 sum_3 count starting_measure_count;
	close $f0
	close $f1
	close $f2
	puts "count = [expr $count-$starting_measure_count] "
	puts "Average throughput (src1) = [expr $sum_1/($count-$starting_measure_count)] Mbps"
	puts "Average throughput (src2) = [expr $sum_2/($count-$starting_measure_count)] Mbps"
	puts "Average throughput (src3) = [expr $sum_3/($count-$starting_measure_count)] Mbps"
	#exec nam out.nam &
	#Call xgraph to display the results
	exec xgraph src1_[lindex $argv 0]_[lindex $argv 1].tr src2_[lindex $argv 0]_[lindex $argv 1].tr src3_[lindex $argv 0]_[lindex $argv 1].tr -geometry 800x800 &
	exit 0
}
}

if {[lindex $argv 0] == "DROPTAIL"} {
	set queue "DropTail"
} elseif {[lindex $argv 0] == "RED"} {
	set queue "RED"
	Queue/RED set thresh_ 10
	Queue/RED set maxthresh_ 15
	Queue/RED set linterm_ 50
}


if {[lindex $argv 1] == 1} {
	set src1 [$ns node]
	set src2 [$ns node]
	set r1 [$ns node]
	set r2 [$ns node]
	set rec1 [$ns node]
	set rec2 [$ns node]

	$ns duplex-link $src1 $r1 10Mb 1ms $queue 
	$ns duplex-link $r2 $r1 1Mb 10ms $queue
	$ns duplex-link $rec1 $r2 10Mb 1ms $queue
	$ns duplex-link $src2 $r1 10Mb 1ms $queue
	$ns duplex-link $rec2 $r2 10Mb 1ms $queue
} elseif {[lindex $argv 1] == 2} {
	set src1 [$ns node]
	set src2 [$ns node]
	set src3 [$ns node]
	set r1 [$ns node]
	set r2 [$ns node]
	set rec1 [$ns node]
	set rec2 [$ns node]
	set rec3 [$ns node]

	$ns duplex-link $src1 $r1 10Mb 1ms $queue 
	$ns duplex-link $r2 $r1 1Mb 10ms $queue
	$ns duplex-link $rec1 $r2 10Mb 1ms $queue
	$ns duplex-link $src2 $r1 10Mb 1ms $queue
	$ns duplex-link $rec2 $r2 10Mb 1ms $queue
	$ns duplex-link $src3 $r1 10Mb 1ms $queue
	$ns duplex-link $rec3 $r2 10Mb 1ms $queue

}
$ns queue-limit $r1 $r2 20

$ns duplex-link-op $r2 $r1   orient left
$ns duplex-link-op $r1 $src1 orient left-down 
$ns duplex-link-op $r1 $src2 orient left
$ns duplex-link-op $r2 $rec2 orient right
$ns duplex-link-op $r2 $rec1 orient right-down
if {[lindex $argv 1] == 2} {
	$ns duplex-link-op $r1 $src2 orient left-up 
	$ns duplex-link-op $r2 $rec2 orient right-up
}

#Create a TCP agent and attach it to node src1
set tcp0 [new Agent/TCP/Sack1]
$ns attach-agent $src1 $tcp0

# Create a FTP traffic source and attach it to tcp0
set ftp0 [new Application/FTP]
$ftp0 attach-agent $tcp0

#Create a TCP agent and attach it to node src2
set tcp1 [new Agent/TCP/Sack1]
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

if {[lindex $argv 1] == 2} {
	#Create a UDP agent and attach it to node src3
	set udp [new Agent/UDP]
	$ns attach-agent $src3 $udp

	# Create a CBR traffic source and attach it to udp
	set cbr [new Application/Traffic/CBR]
	$cbr set PacketSize_ 100
	$cbr set rate_ 1Mb
	$cbr attach-agent $udp
	
	set sink2 [new Agent/LossMonitor]
	$ns attach-agent $rec3 $sink2
	$ns connect $udp $sink2 
}
if {[lindex $argv 1] == 1} {

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

} elseif {[lindex $argv 1] == 2} {
proc record {} {
	global argv sink0 sink1 sink2 f0 f1 f2 sum_1 sum_2 sum_3 count starting_measure_count 
	#Get an instance of the simulator	
	set ns [Simulator instance]
	#Set the time after which the procedure should be called 		again	
	set time 0.01

	#Get the current time
	set now [$ns now]
  	if { $count > $starting_measure_count } {
		#How many bytes have been received by the traffic sinks?	
		set bw0 [$sink0 set bytes_]
		set bw1 [$sink1 set bytes_]
		set bw2 [$sink2 set bytes_]
		#Calculate the bandwidth (in MBit/s) and write it to the 		files
		set sum_1 [expr $sum_1 + $bw0/$time*8/1000000];
		set sum_2 [expr $sum_2 + $bw1/$time*8/1000000];
		set sum_3 [expr $sum_3 + $bw2/$time*8/1000000];
        puts $f0 "$now [expr $bw0/$time*8/1000000]"
		puts $f1 "$now [expr $bw1/$time*8/1000000]"
		puts $f2 "$now [expr $bw2/$time*8/1000000]"
		#Reset the bytes_ values on the traffic sinks
	}
	$sink0 set bytes_ 0
	$sink1 set bytes_ 0	
	$sink2 set bytes_ 0	
	set count [expr $count + 1];
	#Re-schedule the procedure
	$ns at [expr $now+$time] "record"
}
}

$ns at 0.0 "record"
$ns at 0.0 "$ftp0 start"
$ns at 0.0 "$ftp1 start"
if {[lindex $argv 1] == 2} {
	$ns at 0.0 "$cbr start"
}
$ns at 180.0 "$ftp0 stop"
$ns at 180.0 "$ftp1 stop"
if {[lindex $argv 1] == 2} {
	$ns at 180.0 "$cbr stop"
}

$tcp0 set class_ 1
$tcp1 set class_ 2

$ns color 1 Blue
$ns color 2 Red
if {[lindex $argv 1] == 2} {
	$udp set class_ 3
	$ns color 3 Yellow
}

$ns at 0.01 "$src1 label \"src1\""
$ns at 0.01 "$src2 label \"src2\""
$ns at 0.01 "$rec1 label \"rec1\""
$ns at 0.01 "$rec2 label \"rec2\""

if {[lindex $argv 1] == 2} {
	$ns at 0.01 "$src3 label \"src3\""
	$ns at 0.01 "$rec3 label \"rec3\""
}

$ns at 0.01 "$r1 label \"r1\""
$ns at 0.01 "$r2 label \"r2\""


$ns at 180.0 "finish"

$ns run
