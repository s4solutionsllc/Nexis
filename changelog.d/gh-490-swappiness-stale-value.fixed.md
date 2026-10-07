Helpers → Swappiness (GH#490): returning to the Swappiness tool after
navigating away now re-reads the live `vm.swappiness` value instead of
redisplaying whatever was last loaded, which could show a stale "Current
value" after the setting changed outside of Nexis.
