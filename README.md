# GunGame

This BZFlag plugin emulates "gunGame" of Counter Strike and of "Arsenal" of GoldenEye Source fame.

Each player starts with a Laser flag.  On each successive kill, the player gets a "less powerful" flag.  They cannot drop the flag nor pick up others.
The first player to progress through the entire list wins.

The last flag is SR (SteamRoller), and here the gun is disabled (if there are more than 2 players).

As people join or leave the game, the flag list and scores are adjusted accordingly.  For example, SW is disabled for < 3 players.  Bad flags start at 4. At 5
players, everything is on.

Suicide results in going back one level.

## Compiling
Add this plug-in to the BZFlag build system and compile.  From "plugins" directory:

Create support files, check out code & create a symlink.  First time only:

    sh newplug.sh gunGame
    cd gunGame
    git clone https://github.com/danryder/bzGunGameStyle.git src
    ln -s -f src/gunGame.cpp
    cd ..
    make
    
After any change to the source, just rebuild in gunGame directory:

    make
    sudo make install
    
## Setup

### Map Requirements
This plugin handles assigning players flags.  There are 23 flags it can assign, and so any map must have enough of each flag so that a flag give never fails.

In your map have one of each flag for each allowed player.  I have found that up to 12 players currently works, provided you also supply 12 of each of the 23 flags.

This works best if you edit your map to make all the flags spawn out of sight.  For example, in a box off the map or way up high.

### Running
    bzfs -loadplugin /path/to/gunGame.so,ipaddr

 1. [ipaddress] If provided, an IP address from which connecting players will be given debug messages.

### BZDB Variables
These are custom BZDB variables that can be set in game in order to change the plug-in's functionality.

 * _ggSuicidePenalty - how many flags a player forfeits by suicide. defaults to 1 level
 * _ggDetectCheat - whether or not to try to detect drop-shoot cheats. defaults to true
 * _ggCheatPenalty - how many flags a player forfeits if drop-shoot cheat is detected. defaults to 3 levels
 * _ggDebug - if enabled, sends debugging messages to everyone (or to player logged into IP passed in at start). defaults false.
 * _ggJacked  - if enabled, server announces all kills.  defaults false

## Notes

## License

BSD

## Thanks To (no particular order)
 * JeffM
 * blast
 * allejo
 * khonkhortisan
 * optic delusion and the fine folks at Planet Mofo
