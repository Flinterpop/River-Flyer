// River Flyer for iPad and iPhone: a full-screen WKWebView hosting the game's
// WebAssembly build, which is copied into the app bundle under web/ by
// make-ios.sh. No scene manifest in Info.plist, so UIKit hands the window to
// this delegate on every iOS version the app targets.
import UIKit

@main
final class AppDelegate: UIResponder, UIApplicationDelegate {
    var window: UIWindow?

    func application(_ application: UIApplication,
                     didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]?) -> Bool {
        let window = UIWindow(frame: UIScreen.main.bounds)
        window.backgroundColor = .black
        window.rootViewController = GameViewController()
        window.makeKeyAndVisible()
        self.window = window
        return true
    }
}
