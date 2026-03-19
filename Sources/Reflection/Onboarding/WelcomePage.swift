import SwiftUI

/// Onboarding page 1: Welcome introduction.
///
/// Left-aligned layout matching the design mockup:
///   "Welcome to"  (small)
///   "Reflection"   (large, bold)
///   subtitle text
///   [Get Started] button (small, left-aligned)
struct WelcomePage: View {
    let onContinue: () -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            Spacer()

            VStack(alignment: .leading, spacing: 4) {
                Text("Welcome to")
                    .font(.system(size: 20))
                    .foregroundStyle(.white)

                Text("Reflection")
                    .font(.system(size: 60, weight: .bold))
                    .foregroundStyle(.primary)
            }

            Text("Share your iPad's reflection to the rest of the world!")
                .font(.system(size: 20))
                .foregroundStyle(.white)
                .padding(.top, 12)

            Button(action: onContinue) {
                Text("Get Started")
                    .font(.system(size: 18, weight: .medium))
                    .padding(.horizontal, 12)
                    .padding(.vertical, 4)
            }
            .buttonStyle(.borderedProminent)
            .padding(.top, 24)

            Spacer()
        }
        .padding(.horizontal, 64)
        .padding(.vertical, 24)
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .leading)
    }
}

// MARK: - Preview

struct WelcomePage_Previews: PreviewProvider {
    static var previews: some View {
        WelcomePage(onContinue: {})
            .frame(width: 800, height: 600)
            .background(Constants.appBackground)
            .preferredColorScheme(.dark)
    }
}
