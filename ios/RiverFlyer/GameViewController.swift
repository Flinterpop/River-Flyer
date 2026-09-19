// The one screen: a WKWebView filling the display, loading the game from the
// bundle through a custom URL scheme. A scheme handler rather than file:// so
// that the Emscripten loader's fetch() of index.wasm works and arrives with
// the application/wasm type WebAssembly.instantiateStreaming insists on.
import UIKit
import WebKit

final class GameViewController: UIViewController, WKNavigationDelegate {
    private static let scheme = "riverflyer"
    private static let origin = "riverflyer://app/"
    private var webView: WKWebView!

    // A game owns the whole screen: no status bar, home indicator fades, and a
    // swipe near the bottom edge needs a second swipe before iOS takes it.
    override var prefersStatusBarHidden: Bool { true }
    override var prefersHomeIndicatorAutoHidden: Bool { true }
    override var preferredScreenEdgesDeferringSystemGestures: UIRectEdge { .all }
    override var supportedInterfaceOrientations: UIInterfaceOrientationMask { .portrait }

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .black

        let config = WKWebViewConfiguration()
        config.setURLSchemeHandler(BundleSchemeHandler(), forURLScheme: Self.scheme)
        config.allowsInlineMediaPlayback = true
        config.mediaTypesRequiringUserActionForPlayback = []   // the Play tap is the gesture that unlocks audio

        webView = WKWebView(frame: view.bounds, configuration: config)
        webView.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        webView.navigationDelegate = self
        webView.isOpaque = false
        webView.backgroundColor = .black
        webView.scrollView.isScrollEnabled = false
        webView.scrollView.bounces = false
        webView.scrollView.contentInsetAdjustmentBehavior = .never
        view.addSubview(webView)

        guard let url = URL(string: Self.origin + "index.html") else { preconditionFailure("bad origin") }
        webView.load(URLRequest(url: url))
    }

    func webView(_ webView: WKWebView, didFail navigation: WKNavigation!, withError error: Error) {
        NSLog("River Flyer: navigation failed: %@", error.localizedDescription)
    }

    func webView(_ webView: WKWebView, didFailProvisionalNavigation navigation: WKNavigation!, withError error: Error) {
        NSLog("River Flyer: load failed: %@", error.localizedDescription)
    }
}

// Serves riverflyer://app/<file> from the bundle's web/ folder.
final class BundleSchemeHandler: NSObject, WKURLSchemeHandler {
    private static let types = ["html": "text/html; charset=utf-8", "js": "text/javascript", "wasm": "application/wasm"]

    func webView(_ webView: WKWebView, start task: WKURLSchemeTask) {
        guard let url = task.request.url else { return }
        var name = url.path
        if name.isEmpty || name == "/" { name = "/index.html" }
        let file = String(name.dropFirst())   // "index.wasm"

        guard let fileURL = Bundle.main.url(forResource: file, withExtension: nil, subdirectory: "web"),
              let data = try? Data(contentsOf: fileURL) else {
            NSLog("River Flyer: no such bundled file: %@", file)
            task.didFailWithError(NSError(domain: NSURLErrorDomain, code: NSURLErrorFileDoesNotExist))
            return
        }
        let type = Self.types[fileURL.pathExtension] ?? "application/octet-stream"
        guard let response = HTTPURLResponse(url: url, statusCode: 200, httpVersion: "HTTP/1.1",
                                             headerFields: ["Content-Type": type, "Content-Length": String(data.count)]) else {
            task.didFailWithError(NSError(domain: NSURLErrorDomain, code: NSURLErrorUnknown))
            return
        }
        task.didReceive(response)
        task.didReceive(data)
        task.didFinish()
    }

    func webView(_ webView: WKWebView, stop task: WKURLSchemeTask) {}
}
