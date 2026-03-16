import SwiftUI

struct EmptyStateView: View {
    var onRefresh: (() -> Void)?

    var body: some View {
        VStack(spacing: 16) {
            Image(systemName: "ipad.slash")
                .font(.system(size: 48))
                .foregroundStyle(.secondary)

            Text("No iPad Detected")
                .font(.title2)
                .fontWeight(.semibold)

            Text("Connect your iPad via USB cable to begin mirroring.")
                .font(.body)
                .foregroundStyle(.secondary)
                .multilineTextAlignment(.center)

            if let onRefresh {
                Button {
                    onRefresh()
                } label: {
                    Label("Refresh", systemImage: "arrow.clockwise")
                }
                .buttonStyle(.bordered)
                .padding(.top, 8)
            }
        }
        .padding(40)
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }
}
