#!/bin/bash

####################
# Lab 1 Exercise 5
# Name: Tan Kai Qun, Jeremy
# Student No: A0136134N
# Lab Group: 18
####################

# Fill the below up
hostname=$(uname -n)
machine_hardware=$(echo "$(uname) $(uname -m)")
max_user_process_count=$(ulimit -u)
user_process_count=$(ps -U $(whoami) --no-headers | wc -l)
user_with_most_processes=$(ps -eo user | sort | uniq -c | sort -nr | head -1 | awk '{print $2}')
mem_free_percentage=$(free | awk '/Mem/ {print $4/$2 * 100.0}')
swap_free_percentage=$(free | awk '/Swap/ {print $4/$2 * 100.0}')

echo "Hostname: $hostname"
echo "Machine Hardware: $machine_hardware"
echo "Max User Processes: $max_user_process_count"
echo "User Processes: $user_process_count"
echo "User With Most Processes: $user_with_most_processes"
echo "Memory Free (%): $mem_free_percentage"
echo "Swap Free (%): $swap_free_percentage"
