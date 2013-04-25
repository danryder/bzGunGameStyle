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

    bzfs -loadplugin /path/to/gunGame.so,ipaddr

### Parameters
 1. [ipaddress] If provided, an IP address from which connecting players will be given debug messages.

### BZDB Variables
These are custom BZDB variables that can be set in game in order to change the plug-in's functionality.

 * _ggSuicidePenalty - how many flags a player forfeits by suicide
 * _ggDetectCheat - whether or not to try to detect drop-shoot cheats
 * _ggCheatPenalty - how many flags a player forfeits if drop-shoot cheat is detected
 * _ggDebug - corresponds with parameter #3
 * _ggJacked  - corresponds with parameter #4

## Notes

This works best if you edit your map to make all the flags spawn out of sight.  For example, in a box off the map or way up high.

You will need to have 'n' of each flag defined in the map, where 'n' is the #players you intend to have.

This will allow all the players to be on the same level at the same time.

## License

lGPL

## Thanks To (no particular order)
 * JeffM
 * blast
 * allejo
 * khonkhortisan
 * optic delusion and the fine folks at Planet Mofo
