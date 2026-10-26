// xcloud_client.h: Xbox remote play (xHome) and xCloud cloud gaming client.
// Login, browsing your consoles and cloud games, and the WebRTC stream. Auth
// is the Microsoft device code flow, then the Xbox Live token chain, same as
// Greenlight and xal-node use.

#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct XcloudConsole {
    std::string serverId;    // id we hand to the stream session later
    std::string name;        // "Living Room Xbox"
    std::string powerState;  // "On", "ConnectedStandby", "Off"
    std::string consoleType; // "XboxSeriesX", "XboxOne", ...
    bool        isDevKit;
};

struct XcloudGame {
    std::string titleId;
    std::string name;
    std::string productId;
    std::string artUrl;      // box art, empty if none
    bool        playable;    // owned, free, or in the user's Game Pass
};

// Lifecycle
void Xcloud_Init();
void Xcloud_Shutdown();
bool Xcloud_HasToken();   // true once we have a saved refresh token
void Xcloud_SignOut();

// Device code login. Show the user code and let them approve it on the link.
void        Xcloud_StartLogin();
void        Xcloud_CancelLogin();
bool        Xcloud_LoginInFlight();
std::string Xcloud_GetUserCode();          // "ABCD-EFGH"
std::string Xcloud_GetVerificationUri();   // "https://microsoft.com/link"
std::string Xcloud_GetLoginStatus();

// Session. Runs the token chain from the saved refresh token so the browse
// calls have a host and a bearer token to use. Safe to call every frame.
void        Xcloud_StartSession();
bool        Xcloud_SessionReady();
std::string Xcloud_SessionPhase();
std::string Xcloud_GetGamertag();

// Browse
void                       Xcloud_FetchConsoles();
std::vector<XcloudConsole> Xcloud_GetConsoles();
bool                       Xcloud_ConsolesReady();

void                       Xcloud_FetchGames();
std::vector<XcloudGame>    Xcloud_GetGames();
bool                       Xcloud_GamesReady();

// Box-art cache. QueueArtDownload fetches url to disk once (no-op if already
// cached); ArtCachePath is the E:\ path the XAP loads the texture from.
std::string Xcloud_ArtCachePath(const std::string& titleId);
void        Xcloud_QueueArtDownload(const std::string& titleId, const std::string& url);

// Stream session. Provisions a play session against the offering host and
// walks it to ReadyToConnect. type is "cloud" (xCloud) or "home" (remote
// play); id is the titleId for cloud or the serverId for home. This is the
// signaling half; the WebRTC transport hangs off it next.
void        Xcloud_StartStreamSession(const std::string& type, const std::string& id);
void        Xcloud_StopStreamSession();

// Xbox controller state (SDL_GameController already normalizes any pad to the
// Xbox layout). Buttons are 0/1; sticks are -1..1 in SDL convention (Y is +down,
// negated on the wire); triggers are 0..1. Sent over the input datachannel.
struct XcloudGamepad {
    bool  a, b, x, y;
    bool  up, down, left, right;
    bool  lb, rb, ls, rs;
    bool  view, menu, nexus;
    float lx, ly, rx, ry;
    float lt, rt;
};
void        Xcloud_SendGamepad(const XcloudGamepad& gp);
bool        Xcloud_StreamRunning();
bool        Xcloud_StreamReady();       // reached ReadyToConnect
uint64_t    Xcloud_StreamGeneration();  // bumps on each new stream; UI resets on change
std::string Xcloud_StreamPhase();       // human readable progress line
