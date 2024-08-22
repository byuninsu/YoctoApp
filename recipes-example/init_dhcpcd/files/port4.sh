# Start configuring ports for traffic
# Clear power down bit and reset SERDES P4
echo "Clearing power down bit and resetting SERDES P4"
mdio-tool w eth0 0x1C 0x19 0x2000
mdio-tool w eth0 0x1C 0x18 0x8284
mdio-tool w eth0 0x1C 0x19 0x0A040
mdio-tool w eth0 0x1C 0x18 0x8684

# Fix 1000Base-X AN advertisement
# Write 4.2004.5 to 1 (Address 0x14)
echo "Fixing 1000Base-X AN advertisement"
mdio-tool w eth0 0x1C 0x19 0x2004
mdio-tool w eth0 0x1C 0x18 0x8284
mdio-tool w eth0 0x1C 0x19 0x0020
mdio-tool w eth0 0x1C 0x18 0x8684

# Enable Forwarding on ports
echo "Enabling forwarding on ports"
mdio-tool w eth0 0x04 0x04 0x007F

echo "Configuration complete."
