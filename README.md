# datelTool

A tool for dumping/writing to internal flash of certain Datel DS carts.

Current carts supported:

* Action Replay DS (and possibly Action Replay DS Media Edition)
* Datel Games N' Music
* Most if not all Max Media Player carts used for Datel's slot2 products like Max Media Dock. (these would make perfect ntrboot carts given they lack a mSD card slot and aren't useful for much. You can easily just use a flashcart to boot the MMD software anyways. ;) )

This program currently uses SD:/datelTools/gnm-backup.bin (512KB) as the binary path for dumping/writing flash contents of Game n' Music and Max Media Player carts. Action Replay DS carts use dump file name of ards-backup.bin (1MB for now 2MB if we find out why we can't use the second half). Do not run this app from the flashcart you will be reflashing!

This is designed to either run from DSi SD via Unlaunch/HiyaCFW or from a slot2 flashcart on DS/DS Lite!


Credit to edo9300 for figuring out the AUXSPI logic required to make this possible.