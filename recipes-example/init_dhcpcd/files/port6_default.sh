#!/bin/bash

# Check if the interface name is provided as an argument
if [ -z "$1" ]; then
    echo "Usage: $0 <interface>"
    exit 1
fi

INTERFACE=$1

# Version 2.3
# 3-18-16 jpannell

# Force CMODE, Use SGMII AN results
# P6
echo "Forcing CMODE, Using SGMII AN results for P6"
mdio-tool w $INTERFACE 0x05 0x1A 0xA100
mdio-tool w $INTERFACE 0x04 0x1A 0xFCC0

# Set Ports in SGMII mode
echo "Setting ports in SGMII mode for P6"
mdio-tool w $INTERFACE 0x06 0x00 0x000A

# Force SGMII PHY Mode, SPEED 1000Mbps
# P6
echo "Forcing SGMII PHY mode, Speed 1000Mbps for P6"
mdio-tool w $INTERFACE 0x05 0x1A 0x0A02
mdio-tool w $INTERFACE 0x04 0x1A 0xFCC2

# Reset SERDES
# Port 6
echo "Resetting SERDES for Port 6"
mdio-tool w $INTERFACE 0x1C 0x19 0x2000
mdio-tool w $INTERFACE 0x1C 0x18 0x82C4
mdio-tool w $INTERFACE 0x1C 0x19 0x9340
mdio-tool w $INTERFACE 0x1C 0x18 0x86C4

# Set port state to forwarding
echo "Setting port state to forwarding for P6"
mdio-tool w $INTERFACE 0x06 0x04 0x007F

echo "Configuration complete."

