#include "Leaderboard.h"

#if defined(RF_IOS)

#import <GameKit/GameKit.h>
#import <UIKit/UIKit.h>

// The leaderboard id must match the one created in App Store Connect.
static NSString* const kLeaderboardId = @"ca.rabidfox.riverflyer.distance";

// Dismisses the Game Center sheet when the player is done with it.
@interface RFBoardDelegate : NSObject <GKGameCenterControllerDelegate>
@end
@implementation RFBoardDelegate
- (void)gameCenterViewControllerDidFinish:(GKGameCenterViewController*)controller
{
    [controller dismissViewControllerAnimated:YES completion:nil];
}
@end

namespace {
bool g_signedIn = false;
RFBoardDelegate* g_delegate = nil;

UIViewController* RootController()
{
    for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
        if (![scene isKindOfClass:UIWindowScene.class]) { continue; }
        for (UIWindow* w in ((UIWindowScene*)scene).windows) {
            if (w.isKeyWindow) { return w.rootViewController; }
        }
    }
    return nil;
}
}   // namespace

namespace leaderboard {

void Init()
{
    // The handler runs again whenever the player signs in or out, so it is
    // the single source of truth for g_signedIn.
    GKLocalPlayer.localPlayer.authenticateHandler = ^(UIViewController* login, NSError* error) {
        if (login != nil) {
            // Game Center wants to show its sign-in sheet; the game keeps
            // running underneath and the player can dismiss it.
            UIViewController* root = RootController();
            if (root != nil) { [root presentViewController:login animated:YES completion:nil]; }
            return;
        }
        g_signedIn = (error == nil) && GKLocalPlayer.localPlayer.isAuthenticated;
    };
}

bool Available() { return g_signedIn; }

void Post(int score)
{
    if (!g_signedIn || score <= 0) { return; }
    [GKLeaderboard submitScore:score
                       context:0
                        player:GKLocalPlayer.localPlayer
          leaderboardIDs:@[kLeaderboardId]
     completionHandler:^(NSError* error) { (void)error; }];   // a failed post is not worth interrupting play
}

void Show()
{
    if (!g_signedIn) { return; }
    UIViewController* root = RootController();
    if (root == nil) { return; }
    GKGameCenterViewController* vc =
        [[GKGameCenterViewController alloc] initWithLeaderboardID:kLeaderboardId
                                                        playerScope:GKLeaderboardPlayerScopeGlobal
                                                          timeScope:GKLeaderboardTimeScopeAllTime];
    if (g_delegate == nil) { g_delegate = [[RFBoardDelegate alloc] init]; }
    vc.gameCenterDelegate = g_delegate;
    [root presentViewController:vc animated:YES completion:nil];
}

} // namespace leaderboard

#endif
