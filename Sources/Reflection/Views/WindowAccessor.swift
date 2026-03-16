import AppKit
import SwiftUI

/// Bridges into the hosting NSWindow so SwiftUI views can configure
/// titlebar appearance and background color without AppKit subclassing.
struct WindowAccessor: NSViewRepresentable {
    let configure: (NSWindow) -> Void

    func makeNSView(context: Context) -> NSView {
        let view = NSView()
        DispatchQueue.main.async {
            guard let window = view.window else { return }
            configure(window)
        }
        return view
    }

    func updateNSView(_ nsView: NSView, context: Context) {}
}
