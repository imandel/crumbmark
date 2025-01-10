#!/bin/bash

# Check if IP address is provided
if [ $# -ne 1 ]; then
    echo "Usage: $0 <ip_address>"
    echo "Example: $0 192.168.1.138"
    exit 1
fi

IP_ADDRESS=$1

# Validate IP address format (basic check)
if ! [[ $IP_ADDRESS =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "Error: Invalid IP address format"
    exit 1
fi
echo "Starting benchmark"
curl -X GET "http://$IP_ADDRESS/benchmark"
echo "Starting 10-second toggle test for plug at $IP_ADDRESS..."

for i in {1..10}
do
    # Calculate toggle value (0 or 1)
    toggle=$((i % 2))

    echo -e "\nSecond $i:"

    # Toggle state
    curl -X PUT -H "Content-Type: application/json" \
         -d "{\"state\": $toggle}" \
         "http://$IP_ADDRESS/plug/state"

    # Get state to verify
    echo -e "\nVerifying state:"
    curl -X GET "http://$IP_ADDRESS/plug/state"

    # Wait 1 second before next toggle
    sleep 1
done

echo -e "\nToggle test complete"

echo "Starting basline benchmark"
curl -X GET "http://$IP_ADDRESS/benchmark"
