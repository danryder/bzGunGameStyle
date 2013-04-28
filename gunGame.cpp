/*
Copyright (c) 2013, Dan Ryder
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/

// gunGame.cpp
// for updates, see: https://github.com/danryder/bzGunGameStyle

#include "bzfsAPI.h"

#include <stdio.h>
#include <strings.h>
#include <map>
#include <utility>
#include <functional>
#include <string>
#include <list>

using namespace std;

#define DELAYSEC 0.25
#define RETRYSEC 0.1
#define MINORWARN 3
#define MAJORWARN 1
#define DETECTCHEAT 1
#define CHEATPENALTY 3
#define SUICIDEPENALTY 1

// enable this if playing sounds from a plugin crashing clients is fixed
// #define PLAYSOUNDS

// ORDERED LIST OF ALL THE FLAGS WE MIGHT USE
// along with the #players required to enable them
typedef struct {
    const char *flagName;
    size_t playersRequired;
} FlagOption;

FlagOption possibleFlags[] = {
        {"L", 2},
        {"GM", 2},
        {"SW", 3},
        {"CL", 3},
        {"F", 2},
        {"IB", 3},
        {"A", 2},
        {"MG", 2},
        {"ST", 2},
        {"T", 2},
        {"SB", 2},
        {"V", 2},
        {"BU", 3},
        {"WG", 3},
        {"QT", 2},
        {"M", 4},
        {"B", 4},
        {"O", 4},
        {"RT", 5},
        {"LT", 5},
        {"WA", 3},
        {"JM", 4},
        {"NJ", 4},
        {"RC", 3},
        {"SR", 2},
        {NULL, 0}
};

class FlagManager
{
private:

    typedef map<int, int> AssignedFlagsType;
    typedef map<size_t, int> FlagLevelsType;
    typedef map<int, int> WinnersListType;
    typedef multimap<int, int, greater<int> > LeaderboardType;
    struct DelayedFlagType{
        DelayedFlagType(double t=0.0, const char *f=NULL)
               : givetime(t), flag(f) {}
        double givetime;
        const char *flag;
    };

    AssignedFlagsType AssignedFlags; // flag# assigned by player ID
    FlagLevelsType FlagLevels;       // flag levels of all enabled flags (by flag#)
    WinnersListType WinnersList;     // total wins by player ID
    LeaderboardType Leaderboard; // sorts winners at end of game

    size_t numTotalFlags;        // #flags that could be enabled
    size_t numEnabledFlags;      // #flags actually enabled
    size_t minPlayers;           // min players for a game
    int firstFlag;               // first flag enabled
    int lastFlag;                // last flag enabled

    // if #flags enabled changes (as players come and go), update info about
    // enabled flags
    void recalcFlags()
    {
        firstFlag = -1;
        lastFlag = -1;
        FlagLevels.clear();
        numEnabledFlags = 0;
        for (size_t f = 0; f < numTotalFlags; ++f)
        {
            if (possibleFlags[f].playersRequired <= numPlayers)
            {
                if (firstFlag < 0) firstFlag = f;
                lastFlag = f;
                numEnabledFlags++;
                FlagLevels[f] = numEnabledFlags;
                bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Flag %3d (slot %u): %s", numEnabledFlags, f, possibleFlags[f].flagName);
            }
        }
    }

    // if #flags enabled changes (as players come and go), update player scores
    // handle case where a player has a flag now removed from circulation
    void recalcScores()
    {
        for (AssignedFlagsType::iterator i = AssignedFlags.begin(); i != AssignedFlags.end(); ++i)
        {
            int playerID = i->first;
            int flag = i->second;
            FlagLevelsType::iterator j = FlagLevels.find(flag);
            if (j != FlagLevels.end())
            {
                // current flag was still in the list
                // just update the score with the new position
                bz_setPlayerWins(playerID, j->second);
            }
            else
            {
                // player had a flag not in new list
                // replace it with preceeding valid flag
                int decr = 0;
                int newFlagNo = getPrevFlag(flag, decr, 1);
                j = FlagLevels.find(newFlagNo);
                if (j != FlagLevels.end())
                {
                    i->second = newFlagNo; // update what flag the player *should* have
                    bz_setPlayerWins(playerID, j->second);
                    const char *newFlag = possibleFlags[newFlagNo].flagName;
                    replaceFlagIfAlive(playerID, newFlag, "flag deactivated");
                }
                else
                {
                    // this should never happen
                    bz_sendTextMessagef(BZ_SERVER, debuggerID, "ERROR: PREVIOUS FLAG NOT DEFINED");
                }
            }
        }
    }

    // flag progression
    int getNextFlag(int oldFlag, int &delta, int count=1)
    {
        int c = 0;
        delta = 0;
        if (oldFlag >= lastFlag) return lastFlag;
        for (int f = oldFlag+1; f < numTotalFlags; ++f)
        {
            if (possibleFlags[f].playersRequired <= numPlayers)
            {
                delta += 1;
                if ((++c==count) || (f == lastFlag))
                {
                    return f;
                }
            }
        }
        return -1;
    }

    // flag regression
    int getPrevFlag(int oldFlag, int &delta, int count=1)
    {
        int c = 0;
        delta = 0;
        if (oldFlag <= firstFlag) return firstFlag;
        for (int f = oldFlag-1; f >= 0; --f)
        {
            if (possibleFlags[f].playersRequired <= numPlayers)
            {
                delta += 1;
                if ((++c==count) || (f == firstFlag))
                {
                    return f;
                }
            }
        }
        return -1;
    }

    // sometimes we don't care how many we've traversed
    int getNextFlag(int oldFlag, int count=1) {
        int foo = 0;
        return getNextFlag(oldFlag, foo, count);
    }
    int getPrevFlag(int oldFlag, int count=1) {
        int foo = 0;
        return getPrevFlag(oldFlag, foo, count);
    }

    int numPlayersNeeded()
    {
        return (minPlayers - numPlayers);
    }

    bool gameOn()
    {
        return (numPlayersNeeded() <= 0);
    }

public:
    // members accessed in plugin class
    typedef map<int, DelayedFlagType> DelayedFlagsType;
    DelayedFlagsType DelayedFlags;
    size_t numPlayers;
    int debuggerID;

    FlagManager()
         : minPlayers(0),
           numPlayers(0),
           numEnabledFlags(0),
           debuggerID(BZ_ALLUSERS)
    {
        // count all flags, determine min #Players required
        numTotalFlags = 0;
        FlagOption *f = &possibleFlags[0];
        while (f && f->flagName)
        {
            numTotalFlags++;
            if (!minPlayers || (f->playersRequired < minPlayers))
                minPlayers = f->playersRequired;
            f++;
        }
    }

    // return true if this starts a game
    bool addPlayer(const bz_PlayerJoinPartEventData_V1 *joinData)
    {
        bool start = false;
        const char *newGuy = bz_getPlayerCallsign(joinData->playerID);
        if (numPlayers)
        {
            bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, 
                                "Everyone say \"Hi %s!\", player #%d (ID %d)",
                                newGuy, numPlayers, joinData->playerID);
        }
        bz_sendTextMessagef(BZ_SERVER, joinData->playerID, 
                            "Welcome to \"GunGame Style\", %s...",
                            newGuy);

        bool wasGameOn = gameOn();
        int oldNumFlags = numEnabledFlags;

        numPlayers++;
        recalcFlags();

        if (gameOn())
        {
            int numFlags = numEnabledFlags;
            if (!wasGameOn)
            {
                Begin();
                start = true;
                bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, 
                            "\"GunGame Style\" started with %d players %d flags",
                            numPlayers, numFlags);
            }
            else if (oldNumFlags < numFlags)
            {
                bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS,
                                    "\"GunGame Style\" added %d flags to win...",
                                    numFlags - oldNumFlags);
                // adjust scores to map to new flags
                recalcScores();
            }
            AssignedFlags[joinData->playerID] = firstFlag;
            bz_setPlayerWins(joinData->playerID, 1);
            bz_setPlayerLosses(joinData->playerID, 0);
            bz_setPlayerTKs(joinData->playerID, 0);
        }
        else
        {
            // no game yet
            AssignedFlags[joinData->playerID] = -1;
            int needed = numPlayersNeeded();
            bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, 
                                "\"GunGame Style\" awaiting %d more player%s...",
                                needed, (needed > 1) ? "s": "");
        }

        return start;
    }

    // return true if this suspends a game
    bool removePlayer(const bz_PlayerJoinPartEventData_V1 *partData)
    {
    
        bool end = false;
        bool wasGameOn = gameOn();
        int oldNumFlags = numEnabledFlags;
        if (numPlayersNeeded() == 0) AnnounceLeaders();

        numPlayers--;
        recalcFlags();

        AssignedFlags.erase(partData->playerID);
        if (!gameOn())
        {
            if (wasGameOn)
            {
                bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, 
                                   "\"GunGame Style\" suspended - thanks a lot \"%s\"!",
                                   bz_getPlayerCallsign(partData->playerID));
                End();
                end = true;
                bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, 
                                   "\"GunGame Style\" needs one more player to restart...");
            }
            else
            {
                bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS,
                                    "\"GunGame Style\" now needs %d more players to restart...",
                                    numPlayersNeeded());
            }
        }
        else
        {
            // game continues
            int numFlags = numEnabledFlags;
            if (oldNumFlags > numFlags)
            {
                bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS,
                                    "\"GunGame Style\" removed %d flags to win...",
                                    oldNumFlags - numFlags);
                // adjust scores to map to new flags
                recalcScores();
            }
        }
        return end;
    }

    void givePlayerFlagDelayed(int playerID, const char *flagName)
    {
        double now = bz_getCurrentTime();
        DelayedFlags[playerID].flag = flagName;
        DelayedFlags[playerID].givetime = now + DELAYSEC;
    }

    bool givePlayerFlagNow(int playerID, const char *flagName)
    {
        return bz_givePlayerFlag(playerID, flagName, true);
    }

    // returns True if flag give succeeded immediately
    // otherwise queues it up and returns False
    bool givePlayerFlag(int playerID, const char *flagName)
    {
        bool now = givePlayerFlagNow(playerID, flagName);
        if (now) return true;
        givePlayerFlagDelayed(playerID, flagName);
        return false;
    }

    void replaceFlagIfAlive(int playerID, const char *flagName, const char *reason, bool tryFast=false)
    {
         bz_BasePlayerRecord *pr = bz_getPlayerByIndex(playerID);
         if (pr)
         {
             if (pr->spawned) 
             {
                 bz_removePlayerFlag(playerID);
                 if (flagName)
                 {
                     if (tryFast) 
                         givePlayerFlag(playerID, flagName);
                     else
                         givePlayerFlagDelayed(playerID, flagName);
                 }
             }
         }
         else
         {
             bz_sendTextMessagef(BZ_SERVER, debuggerID, 
             "ERROR: NO PLAYER RECORD (for '%s', player ID %d) can't replace %s's flag with %s",
             reason,
             playerID, bz_getPlayerCallsign(playerID),
             (flagName) ? flagName : "nothing");
         }
         bz_freePlayerRecord(pr);
    }

    void Begin()
    {
        // new game -- pass out flags to anyone spawned
        const char *firstFlagName = possibleFlags[firstFlag].flagName;
#ifdef PLAYSOUNDS
        bz_sendPlayCustomLocalSound(BZ_ALLUSERS, "gungame/gungame_start");
#endif
            
        bz_BasePlayerRecord *pr = NULL;
        for (AssignedFlagsType::iterator i = AssignedFlags.begin(); i != AssignedFlags.end(); ++i)
        {
            int playerID = i->first;
            i->second = firstFlag;
            bz_setPlayerWins(playerID, 1);
            bz_setPlayerLosses(playerID, 0);
            bz_setPlayerTKs(playerID, 0);
            replaceFlagIfAlive(playerID, firstFlagName, "game begin", true);
        }
    }

    void End()
    {
        for (AssignedFlagsType::iterator i = AssignedFlags.begin(); i != AssignedFlags.end(); ++i)
        {
            // reset flag and scores
            int playerID = i->first;
            i->second = -1;
            bz_setPlayerWins(playerID, 0);
            bz_setPlayerLosses(playerID, 0);
            bz_setPlayerTKs(playerID, 0);
            
            replaceFlagIfAlive(playerID, NULL, "game end");
        }
    }

    void AnnounceLeaders()
    {
        int maxFlag = -1;
        list<int> quasiWinners;
        for (AssignedFlagsType::const_iterator i = AssignedFlags.begin(); i != AssignedFlags.end(); ++i)
        {
            int playerID = i->first;
            int flag = i->second;
            if (flag > maxFlag)
            {
                quasiWinners.clear();
                quasiWinners.push_back(playerID);
                maxFlag = flag;
            }
            else if (flag == maxFlag)
            {
                quasiWinners.push_back(playerID);
            }
        }
    
        // announce "quasi winners"
        if (maxFlag > firstFlag)
        {
            string qwinners = "";
            for(list<int>::const_iterator i = quasiWinners.begin();
                i != quasiWinners.end(); 
                ++i)
            {
                qwinners += bz_getPlayerCallsign(*i);
                qwinners += ", ";
            }
            qwinners = qwinners.substr(0, qwinners.length() - 2);
            
            const char *lastFlag = possibleFlags[maxFlag].flagName;
            bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS,
                                "A semi-sincere congratulations to: %s with %s",
                                qwinners.c_str(), lastFlag);
        }
    }

    const char *getAssignedFlag(const int playerID)
    {
        return possibleFlags[AssignedFlags[playerID]].flagName;
    }

    // suicide or natural causes
    void handleSuicide(const bz_PlayerDieEventData_V1 *dieData)
    {
        const char *victimName = bz_getPlayerCallsign(dieData->playerID);
        int victimFlagNo = AssignedFlags[dieData->playerID];
        const char *victimFlag = possibleFlags[victimFlagNo].flagName;

        int decr = 0;
        int newFlagNo = getPrevFlag(victimFlagNo, decr, bz_getBZDBInt("_ggSuicidePenalty"));
        if (newFlagNo < 0)
        {
            newFlagNo = victimFlagNo;
        }
        const char *newFlag = possibleFlags[newFlagNo].flagName;
#ifdef PLAYSOUNDS
        bz_sendPlayCustomLocalSound(BZ_ALLUSERS, "flag_lost");
#endif
        bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Ha-Ha!  %s suicided with %s.  %s %s",
                            victimName, victimFlag,
                            (newFlagNo <= firstFlag) ? "starting over with"
                                                                 : "demoted to",
                            newFlag);
        AssignedFlags[dieData->playerID] = newFlagNo;

        // reduce player score on suicide
        if (decr)
        {
            bz_setPlayerWins(dieData->playerID, FlagLevels[newFlagNo]);
        }
    }

    void handleHomicide(bz_PlayerDieEventData_V1 *dieData)
    {
        // on noes! a homicide occurred
        int killerID = dieData->killerID;
        int victimID = dieData->playerID;
        const char *victimName = bz_getPlayerCallsign(victimID);
        const char *killerName = bz_getPlayerCallsign(killerID);
        int victimFlagNo = AssignedFlags[victimID];
        int killerFlagNo = AssignedFlags[dieData->killerID];
        const char *victimFlag = possibleFlags[victimFlagNo].flagName;
        const char *killerFlag = possibleFlags[killerFlagNo].flagName;

        // detect possible cheater drop-kill, or inconsistent flag holder
        // allow valid cases where flagKilledWith can be empty:
        //  1 SR (squish)
        //  2 BU (squish) - could also check data->state.pos[2] >= 0
        if (bz_getBZDBBool("_ggDetectCheat") &&
            (dieData->flagKilledWith.size() == 0) &&
            (string(killerFlag) != "SR") && 
            (string(victimFlag) != "BU"))
        {
            if (bz_getBZDBBool("_ggDebug"))
            {
                bz_sendTextMessagef(BZ_SERVER, debuggerID, "Possible cheating? Killed without flag, not SR or BU.");
            }
            // if someone spams the drop flag key and squeezes of a shot with no flag
            // ... and if we don't succeed in making the bullet PZ
            // ... and this results in another player dying
            // demote and publicly shame the cheater
            int decr = 0;
            int newFlagNo = getPrevFlag(killerFlagNo, decr, bz_getBZDBInt("_ggCheatPenalty"));
            if (newFlagNo < 0)
            {
                newFlagNo = firstFlag;
            }
            const char *newFlag = possibleFlags[newFlagNo].flagName;
            bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "%s killed %s ... WITHOUT holding %s!  Booted to %s",
                                killerName, victimName, killerFlag, newFlag);
            AssignedFlags[killerID] = newFlagNo;
            // negate cheater score increase
            // and roll it back 
            bz_setPlayerWins(killerID, FlagLevels[newFlagNo] - 1);
            // if cheater died, do nothing - new flag will be given on spawn
            replaceFlagIfAlive(killerID, newFlag, "suspected cheat", true);
        }
        else
        {
            // legit kill!
            if (bz_getBZDBBool("_ggJacked"))
            {
                bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "%s got JACKED by %s with %s",
                                    victimName, killerName, killerFlag);
            }

            int killerLevel = FlagLevels[killerFlagNo];
            int maxLevel = numEnabledFlags;
            int remainLevels = maxLevel - killerLevel;

            // advance the killer... detect win case, etc
            if (killerLevel < maxLevel)
            {
                if (remainLevels <= MAJORWARN)
                {
#ifdef PLAYSOUNDS
                    bz_sendPlayCustomLocalSound(BZ_ALLUSERS, "flag_alert");
#endif
                    bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, 
                                        "--->>> ATTENTION: %s is ABOUT TO WIN!!! <<<---",
                                        killerName);
                                        
                }
                else if (remainLevels <= MINORWARN)
                {
#ifdef PLAYSOUNDS
                    bz_sendPlayCustomLocalSound(BZ_ALLUSERS, "lock");
#endif
                    bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS,
                                        "-> ATTENTION: %s has %d KILLS REMAINING!!! <-",
                                        killerName, remainLevels);
                }

                int adv = 0;
                int newFlagNo = getNextFlag(killerFlagNo, adv, 1);
                if (bz_getBZDBBool("_ggDebug"))
                {
                    bz_sendTextMessagef(BZ_SERVER, debuggerID,
                                    "-> ATTENTION: %s made a legit kill. new flag is %d which is level %d",
                                    killerName, newFlagNo, FlagLevels[newFlagNo]);
                }
                AssignedFlags[killerID] = newFlagNo;

                // set score to new level (minus one to account for pending increment)
                bz_setPlayerWins(killerID, FlagLevels[newFlagNo] - 1);
                const char *newFlag = possibleFlags[newFlagNo].flagName;
                bz_sendTextMessagef(BZ_SERVER, killerID, "\"Upgraded\" from %s to %s (%d/%d)", killerFlag, newFlag, killerLevel+1, maxLevel);
#ifdef PLAYSOUNDS
                bz_sendPlayCustomLocalSound(killerID, "gungame/gungame_boost");
#endif
                replaceFlagIfAlive(killerID, newFlag, "advancing");
            }
            else
            {
                // winner!
                bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "---===>>> WINNER: %s <<<===---",
                                    killerName);
                WinnersListType::iterator i = WinnersList.find(killerID);
                if (i == WinnersList.end())
                {
                    // first time winning
                    WinnersList[killerID] = 1;
                }
                else
                {
                    i->second += 1;
                }

                Leaderboard.clear();
                for (i = WinnersList.begin(); i != WinnersList.end(); ++i)
                {
                    Leaderboard.insert(pair<int, int>(i->second, i->first));
                }
                for (LeaderboardType::const_iterator j = Leaderboard.begin(); j != Leaderboard.end(); ++j)
                {
                    bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "%d wins - %s",
                                        j->first, 
                                        bz_getPlayerCallsign(j->second));
                }

                // reset game
                int winnerFlag = AssignedFlags[killerID];
                int firstFlag = getNextFlag(-1);
                const char *firstFlagName = possibleFlags[firstFlag].flagName;
                for (AssignedFlagsType::iterator i = AssignedFlags.begin(); i!= AssignedFlags.end(); ++i)
                {
                    int playerID = i->first;
                    int flag = i->second;
                    // reset scores
                    bz_setPlayerWins(i->first, 0); // will increment to 1 due to kill
                    bz_setPlayerLosses(i->first, 0);
                    bz_setPlayerTKs(i->first, 0);
                    i->second = firstFlag;
                    if (playerID != killerID)
                    {
                        if (flag == winnerFlag)
                        {
#ifdef PLAYSOUNDS
                            bz_sendPlayCustomLocalSound(playerID, "phantom");
                            bz_sendPlayCustomLocalSound(playerID, "flag_won");
#endif
                            bz_sendTextMessagef(BZ_SERVER, playerID, "So close!  %s just beat you", killerName);
                        }
                        else
                        {
#ifdef PLAYSOUNDS
                            bz_sendPlayCustomLocalSound(playerID, "flag_won");
#endif
                            bz_sendTextMessagef(BZ_SERVER, playerID, "Bow to %s", killerName);
                        }
                        // kill all non-winners
                        // spawn will reset their flag
                        bz_killPlayer(playerID, false);
                    }
                    else
                    {
#ifdef PLAYSOUNDS
                        bz_sendPlayCustomLocalSound(playerID, "gungame/gungame_sexy");
                        bz_sendPlayCustomLocalSound(playerID, "flag_won");
#endif
                        bz_sendTextMessagef(BZ_SERVER, playerID, "Nice game %s!", killerName);
                        replaceFlagIfAlive(playerID, firstFlagName, "winning");
                    }
                }
            }
        }
    }
};

class GunGame : public bz_Plugin
{
private:
    FlagManager *flagManager;
    const char *debuggerIP;
    bool savedHideFlagsOnRadar;

public:
    virtual const char* Name (){return "GunGame";}
    virtual void Init ( const char* config)
    {
        savedHideFlagsOnRadar = bz_getBZDBBool("_hideFlagsOnRadar");
        bz_setBZDBBool("_hideFlagsOnRadar", true, 0, false);

        bz_setBZDBBool("_ggJacked", false, 0, false);
        bz_setBZDBBool("_ggDebug", false, 0, false);
        bz_setBZDBBool("_ggDetectCheat", true, 0, false);
        bz_setBZDBInt("_ggSuicidePenalty", SUICIDEPENALTY, 0, false);
        bz_setBZDBInt("_ggCheatPenalty", CHEATPENALTY, 0, false);

        debuggerIP = config;
        flagManager = new FlagManager();

        Register(bz_ePlayerJoinEvent);
        Register(bz_ePlayerPartEvent);
    }

    void Cleanup()
    {
        if (flagManager) 
        {
            delete flagManager;
            flagManager = NULL;
        }
        bz_Plugin::Cleanup();
        bz_setBZDBBool("_hideFlagsOnRadar", savedHideFlagsOnRadar, 0, false);
    }

    virtual void Event ( bz_EventData *eventData );
};

BZ_PLUGIN(GunGame)

void GunGame::Event ( bz_EventData *eventData )
{
    // sometimes a flag give fails (for example if we just took it)
    // handle this with a delayed give
    if (eventData->eventType == bz_eTickEvent)
    {
        bz_TickEventData_V1 *tickData = (bz_TickEventData_V1*)eventData;
        for (FlagManager::DelayedFlagsType::iterator i = flagManager->DelayedFlags.begin();
             i != flagManager->DelayedFlags.end(); ++i)
        {
            if (tickData->eventTime > i->second.givetime)
            {
                // timer has expired - try to give the flag now
                // if that still fails, reset timer
                if (i->second.flag)
                {
                    if (flagManager->givePlayerFlagNow(i->first, i->second.flag))
                    {
                        // i->second.flag = NULL;
                        flagManager->DelayedFlags.erase(i);
                    }
                    else
                    {
                        i->second.givetime = tickData->eventTime + RETRYSEC;
                    }
                }
            }
        }
    }

    else if (eventData->eventType == bz_eShotFiredEvent)
    {
        bz_ShotFiredEventData_V1 *shotData = (bz_ShotFiredEventData_V1*)eventData;
        const char *shootingPlayer = bz_getPlayerCallsign(shotData->playerID);
        
        bz_BasePlayerRecord *pr = bz_getPlayerByIndex(shotData->playerID);
        if (pr)
        {
            if(!pr->currentFlag.size())
            {
                // if no flag - change bullet to PZ so its harmless to others
                // not sure how well this actually works
                // so we catch ill-gotten kills elsewhere and dispense justice there
                shotData->type = "PZ";
                shotData->changed = true;
                if (bz_getBZDBBool("_ggDebug"))
                {
                    bz_sendTextMessagef(BZ_SERVER, flagManager->debuggerID,
                                        ">>>>>>> %s fired a shot... but has no flag.. converted to PZ <<<<<",
                                        shootingPlayer);
                }
            }
            else
            {
                // check flag type
                const char *shouldHave = flagManager->getAssignedFlag(shotData->playerID);
                if (shotData->type != shouldHave)
                {
                    // shooter had a flag...  was it the right one?
                    // I've never seen this actually happen
                    shotData->type = "PZ";
                    shotData->changed = true;
                    if (bz_getBZDBBool("_ggDebug"))
                    {
                        bz_sendTextMessagef(BZ_SERVER, 
                                        flagManager->debuggerID,
                                        ">>>>>>> %s shot type: %s should have been: %s converted to PZ <<<<<<",
                                        shootingPlayer, shotData->type.c_str(), shouldHave);
                    }
                }
                // also if SR - end game situation - disable gun in same way
                else if((flagManager->numPlayers > 2) && (pr->currentFlag == "SteamRoller (+SR)"))
                {
                    shotData->type = "PZ";
                    shotData->changed = true;
                    bz_sendTextMessagef(BZ_SERVER, shotData->playerID, "Nope - you gotta smush 'em to win!");
                }
            }
        }
        else
        {
            // example: a world weapon
            // do nothing
            if (bz_getBZDBBool("_ggDebug"))
            {
                bz_sendTextMessagef(BZ_SERVER, 
                                flagManager->debuggerID,
                                ">>>>>>> No Player ID for shot - world weapon?");
            }
        }
        bz_freePlayerRecord(pr);
    }

    // drops happen twice when you die - once spawned before the DieEvent and once after
    else if (eventData->eventType == bz_eFlagDroppedEvent)
    {
        bz_FlagDroppedEventData_V1 *playerData = (bz_FlagDroppedEventData_V1*)eventData;
        const char *dropPlayer = bz_getPlayerCallsign(playerData->playerID);
        bz_BasePlayerRecord *pr = bz_getPlayerByIndex(playerData->playerID);
        if (pr)
        {
            if (pr->spawned) 
            {
                // player dropped while alive
                const char *droppedFlag = bz_getFlagName(playerData->flagID).c_str();
                const char *shouldHave = flagManager->getAssignedFlag(playerData->playerID);
                if (0 == strncmp(droppedFlag, shouldHave, strlen(droppedFlag)))
                {
                    if (bz_getBZDBBool("_ggDebug"))
                    {
                        bz_sendTextMessagef(BZ_SERVER, flagManager->debuggerID, "%s dropped %s while alive", dropPlayer, droppedFlag);
                    }
                    // happens if a player dies (before die event)
                    // OR if they try to drop their flag
                    // either way, give them that flag back
                    bool res = flagManager->givePlayerFlag(playerData->playerID, droppedFlag);
                    if (bz_getBZDBBool("_ggDebug"))
                    {
                        bz_sendTextMessagef(BZ_SERVER, flagManager->debuggerID, "Immediate re-gift success? %s", res ? "yes" : "no");
                    }
                }
                else if (bz_getBZDBBool("_ggDebug"))
                {
                    // happens if plugin removed their flag
                    // after upgrading state to a new one
                    // plugin will also assign next flag
                    bz_sendTextMessagef(BZ_SERVER, flagManager->debuggerID,
                                        "%s dropped: %s to upgrade to: %s",
                                        dropPlayer, droppedFlag, shouldHave);
                }
            }
        }
        else
        {
            // seems to be resolved as of commit 22715 - thanks JeffM!
            bz_sendTextMessagef(BZ_SERVER, flagManager->debuggerID, "ERROR: PLAYER RECORD at DROP (player %s)", dropPlayer);
        }
        bz_freePlayerRecord(pr);
    }

    else if (eventData->eventType == bz_ePlayerDieEvent)
    {
        // handle suicides (go back one flag)
        // and extra taunting
        bz_PlayerDieEventData_V1 *dieData = (bz_PlayerDieEventData_V1*)eventData;

        // losses score will have been incremented... undo that
        bz_setPlayerLosses(dieData->playerID, bz_getPlayerLosses(dieData->playerID) - 1);

        if ((dieData->playerID == dieData->killerID) ||
            (dieData->killerID < 0))
        {
            flagManager->handleSuicide(dieData);
        }
        else
        {
            flagManager->handleHomicide(dieData);
        }
    }

    else if (eventData->eventType == bz_ePlayerSpawnEvent)
    {
        bz_PlayerSpawnEventData_V1 *playerData = (bz_PlayerSpawnEventData_V1*)eventData;
        const char *shouldHave = flagManager->getAssignedFlag(playerData->playerID);
        flagManager->givePlayerFlag(playerData->playerID, shouldHave);
        bz_sendTextMessagef(BZ_SERVER, playerData->playerID, "Spawned with %s", shouldHave);
#ifdef PLAYSOUNDS
        // bz_sendPlayCustomLocalSound(playerData->playerID, "gungame/gungame_boost");
#endif
    }

    else if (eventData->eventType == bz_ePlayerJoinEvent)
    {
        bz_PlayerJoinPartEventData_V1 *joinData = (bz_PlayerJoinPartEventData_V1*)eventData;

        // if configured with a special IP, send extra debugging messages
        // in-game to this clown
        if (debuggerIP)
        {
            if (0 == strncmp(joinData->record->ipAddress.c_str(), debuggerIP, strlen(debuggerIP)))
            {
                flagManager->debuggerID = joinData->playerID;
                bz_sendTextMessagef(BZ_SERVER, flagManager->debuggerID, "Welcome debug overlord");
            }
        }
         
        if (flagManager->addPlayer(joinData))
        {
            Register(bz_ePlayerSpawnEvent);
            Register(bz_ePlayerDieEvent);
            Register(bz_eFlagDroppedEvent);
            Register(bz_eShotFiredEvent);
            Register(bz_eTickEvent);
        }
    }

    else if (eventData->eventType == bz_ePlayerPartEvent)
    {
        bz_PlayerJoinPartEventData_V1 *partData = (bz_PlayerJoinPartEventData_V1*)eventData;
        if (flagManager->removePlayer(partData))
        {
            Remove(bz_ePlayerSpawnEvent);
            Remove(bz_ePlayerDieEvent);
            Remove(bz_eFlagDroppedEvent);
            Remove(bz_eShotFiredEvent);
            Remove(bz_eTickEvent);
        }
    }
}
