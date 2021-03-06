#!/bin/sh

check_output() {
    cmp .output.$1 .expected_output.$1 2>/dev/null >/dev/null
    if [ $? -eq 0 ]
    then
        echo "PASSED"
        #rm $1 $2
    else
        echo "FAILED"
        echo "Output from tritontalk binary is in file: .output.$1"
        echo "Expected output is in file: .expected_output.$1"
        echo "Debug output from tritontalk binary is in file: .debug_output.$1"
    fi
    echo
}

echo "Rebuilding tritontalk..."
make clean; make -j4 tritontalk
echo "Done rebuilding tritontalk"
echo


## Test Case 1
echo -n "Test case 1: Sending 1 packet and expecting receiver to print it out: "
(sleep 0.5; echo "msg 0 0 Hello world"; sleep 0.5; echo "exit") | ./tritontalk -r 1 -s 1 > .output.1 2> .debug_output.1

cat > .expected_output.1 << EOF
<RECV_0>:[Hello world]
EOF

check_output 1


## Test Case 2
echo -n "Test case 2: Sending 10 packets and expecting receiver to print them out in order: "
(sleep 0.5; for i in `seq 1 10`; do echo "msg 0 0 Packet: $i"; sleep 0.1; done; sleep 1; echo "exit") | ./tritontalk -r 1 -s 1 > .output.2 2> .debug_output.2

(for i in `seq 1 10`; do echo "<RECV_0>:[Packet: $i]"; done) > .expected_output.2

check_output 2


## Test Case 3
echo -n "Test case 3: Sending 10 packets (with corrupt probability of 40%) and expecting receiver to print them out in order: "
(sleep 0.5; for i in `seq 1 10`; do echo "msg 0 0 Packet: $i"; sleep 0.1; done; sleep 5; echo "exit") | ./tritontalk -c 0.4 -r 1 -s 1 > .output.3 2> .debug_output.3

(for i in `seq 1 10`; do echo "<RECV_0>:[Packet: $i]"; done) > .expected_output.3

check_output 3


## Test Case 4
echo -n "Test case 4: Sending 10 packets (with corrupt probability of 20% and drop probability of 20%) and expecting receiver to print them out in order: "
(sleep 0.5; for i in `seq 1 10`; do echo "msg 0 0 Packet: $i"; sleep 0.1; done; sleep 5; echo "exit") | ./tritontalk -d 0.2 -c 0.2 -r 1 -s 1 > .output.4 2> .debug_output.4

(for i in `seq 1 10`; do echo "<RECV_0>:[Packet: $i]"; done) > .expected_output.4

check_output 4

echo -n "Test case 5: Sending 10 packets (with corrupt probability of 50% and drop probability of 50%) and expecting receiver to print them out in order: "
(sleep 0.5; for i in `seq 1 10`; do echo "msg 0 0 Packet: $i"; sleep 0.1; done; sleep 5; echo "exit") | ./tritontalk -c 0.5 -d 0.5 -r 1 -s 1 > .output.5 2> .debug_output.5

(for i in `seq 1 10`; do echo "<RECV_0>:[Packet: $i]"; done) > .expected_output.5

check_output 5

echo -n "Test case 6: Sending 18 packets (with corrupt probability of 60% and drop probability of 60%) and expecting receiver to print them out in order: "
(sleep 0.5; for i in `seq 1 18`; do echo "msg 0 0 Packet: $i"; sleep 0.1; done; sleep 40; echo "exit") | ./tritontalk -c 0.6 -d 0.6 -r 1 -s 1 > .output.6 2> .debug_output.6

(for i in `seq 1 18`; do echo "<RECV_0>:[Packet: $i]"; done) > .expected_output.6

check_output 6

#echo -n "Test case 7: Long msgs test : Sending 10 long packets (with corrupt probability of 30% and drop probability of 50%) and expecting receiver to print them out in order: "
#(sleep 0.5; for i in `seq 1 10`; do echo "msg 0 0 12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890Packet: $i"; sleep 0.1; done; sleep 15; echo "exit") | ./tritontalk -d 0.5 -c 0.3 -r 1 -s 1 > .output.7 2> .debug_output.7

#(for i in `seq 1 10`; do echo "<RECV_0>:[12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890Packet: $i]"; done) > .expected_output.7

#check_output 7

echo -n "Test case 8: SeqNo overflow test : Sending 280 packets (with corrupt probability of 5% and drop probability of 5%) and expecting receiver to print them out in order: "
(sleep 0.5; for i in `seq 1 280`; do echo "msg 0 0 Packet: $i"; sleep 0.1; done; sleep 10; echo "exit") | ./tritontalk -c 0.05 -d 0.05 -r 1 -s 1 > .output.8 2> .debug_output.8

(for i in `seq 1 280`; do echo "<RECV_0>:[Packet: $i]"; done) > .expected_output.8

check_output 8

echo
echo "Completed test cases"
