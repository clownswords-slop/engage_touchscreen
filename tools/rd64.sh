#!/bin/bash
# rd64.sh ADDR : the u64 at ADDR (debug channel), printed as 0x...
l=$("$(dirname "$0")/cmd.sh" "dump $1 16" | sed -n 2p); lo=$(echo "$l" | awk '{print $2}'); hi=$(echo "$l" | awk '{print $3}')
printf '0x%x\n' $(( (hi<<32) | lo ))
