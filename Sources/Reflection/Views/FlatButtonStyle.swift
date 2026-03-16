import SwiftUI

/// A flat, modern button style — solid color fill with no shadows, gradients, or 3D effects.
struct FlatButtonStyle: ButtonStyle {
    let color: Color

    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.system(size: 13, weight: .medium))
            .foregroundStyle(.white)
            .padding(.horizontal, 14)
            .padding(.vertical, 6)
            .background(
                RoundedRectangle(cornerRadius: 6)
                    .fill(configuration.isPressed ? color.opacity(0.7) : color)
            )
    }
}
