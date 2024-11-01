#!/bin/bash

# Check if the interface name is provided as an argument
if [ -z "$1" ]; then
    echo "Usage: $0 <interface>"
    exit 1
fi

INTERFACE=$1

# Version 2.4
# 3-20-17 jpannell

# Force CMODE, Use SGMII AN results
# P9
echo "Forcing CMODE, Using SGMII AN results for P9"
mdio-tool w $INTERFACE 0x05 0x1A 0xA100
mdio-tool w $INTERFACE 0x04 0x1A 0xFD20

# Set Ports in SGMII mode
echo "Setting ports in SGMII mode for P9"
mdio-tool w $INTERFACE 0x09 0x00 0x000A

# Force SGMII PHY Mode, SPEED 1000Mbps
# P9
echo "Forcing SGMII PHY mode, Speed 1000Mbps for P9"
mdio-tool w $INTERFACE 0x05 0x1A 0x0A02
mdio-tool w $INTERFACE 0x04 0x1A 0xFD22

# Reset SERDES
# Port 9
echo "Resetting SERDES for Port 9"
mdio-tool w $INTERFACE 0x1C 0x19 0x2000
mdio-tool w $INTERFACE 0x1C 0x18 0x8124
mdio-tool w $INTERFACE 0x1C 0x19 0x9340
mdio-tool w $INTERFACE 0x1C 0x18 0x8524

# Set port state to forwarding
echo "Setting port state to forwarding for P9"
mdio-tool w $INTERFACE 0x09 0x04 0x007F

echo "Configuration complete."

