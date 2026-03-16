import SwiftUI

struct StatusIndicatorView: View {
    let isConnected: Bool

    var body: some View {
        Circle()
            .fill(isConnected ? Color.green : Color.gray)
            .frame(width: 10, height: 10)
            .animation(.easeInOut(duration: Constants.animationDuration), value: isConnected)
    }
}
