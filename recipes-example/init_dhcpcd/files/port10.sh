# Set Ports in 1000Base-X
echo "Setting Port 10 in 1000Base-X mode"
mdio-tool w eth0 0x0A 0x00 0x0009

# Start configuring ports for traffic
echo "Clearing power down bit and resetting SERDES P10"
mdio-tool w eth0 0x1C 0x19 0x2000
mdio-tool w eth0 0x1C 0x18 0x8144
mdio-tool w eth0 0x1C 0x19 0x0A040
mdio-tool w eth0 0x1C 0x18 0x8544

# Fix 1000Base-X AN advertisement (Address 0x10)
echo "Fixing 1000Base-X AN advertisement"
mdio-tool w eth0 0x1C 0x19 0x2004
mdio-tool w eth0 0x1C 0x18 0x8144
mdio-tool w eth0 0x1C 0x19 0x0020
mdio-tool w eth0 0x1C 0x18 0x8544

# Enable Forwarding on ports
echo "Enabling forwarding on port 10"
mdio-tool w eth0 0x0A 0x04 0x007F

echo "Configuration complete."
