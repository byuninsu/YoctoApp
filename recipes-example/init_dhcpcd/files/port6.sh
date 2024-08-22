# Start configuring ports for traffic
# Clear power down bit and reset SERDES P6
echo "Clearing power down bit and resetting SERDES P6"
mdio-tool w eth0 0x1C 0x19 0x2000
mdio-tool w eth0 0x1C 0x18 0x82C4
mdio-tool w eth0 0x1C 0x19 0x0A040
mdio-tool w eth0 0x1C 0x18 0x86C4

# Fix 1000Base-X AN advertisement
# Write 4.2004.5 to 1 (Address 0x16)
echo "Fixing 1000Base-X AN advertisement"
mdio-tool w eth0 0x1C 0x19 0x2004
mdio-tool w eth0 0x1C 0x18 0x82C4
mdio-tool w eth0 0x1C 0x19 0x0020
mdio-tool w eth0 0x1C 0x18 0x86C4

# Enable Forwarding on ports
echo "Enabling forwarding on ports"
mdio-tool w eth0 0x06 0x04 0x007F

echo "Configuration complete."
