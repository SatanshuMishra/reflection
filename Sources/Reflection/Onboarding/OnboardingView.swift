import SwiftUI

/// Container for the 2-page first-launch onboarding flow.
/// Uses slide transitions, pill-shaped step indicator, and
/// per-step animated window resizing.
struct OnboardingView: View {
    private final class WeakWindowReference {
        weak var window: NSWindow?
    }

    let onComplete: () -> Void

    @State private var currentPage = 0
    @State private var hostWindowReference = WeakWindowReference()

    private let pageCount = Constants.onboardingPageSizes.count

    /// Slide-right-in / slide-left-out transition for page changes.
    private var pageTransition: AnyTransition {
        .asymmetric(
            insertion: .move(edge: .trailing).combined(with: .opacity),
            removal: .move(edge: .leading).combined(with: .opacity)
        )
    }

    var body: some View {
        VStack(spacing: 0) {
            // Pill-shaped step indicator
            pageIndicator

            // Page content
            Group {
                switch currentPage {
                case 0:
                    WelcomePage {
                        advancePage()
                    }
                    .transition(pageTransition)

                default:
                    PermissionPage {
                        onComplete()
                    }
                    .transition(pageTransition)
                }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
        }
        .padding(.vertical, Constants.onboardingVerticalPadding)
        .background(
            Constants.appBackground
                .ignoresSafeArea(.container, edges: .top)
        )
        .clipShape(RoundedRectangle(cornerRadius: Constants.onboardingCornerRadius))
        .background(
            WindowAccessor { window in
                hostWindowReference.window = window
                if let firstPageSize = Constants.onboardingPageSizes.first {
                    resizeWindow(to: firstPageSize)
                }
            }
        )
    }

    // MARK: - Pill Step Indicator

    private var pageIndicator: some View {
        HStack(spacing: 6) {
            ForEach(0..<pageCount, id: \.self) { index in
                Capsule()
                    .fill(
                        index == currentPage
                            ? Color.primary
                            : Color.secondary.opacity(0.3)
                    )
                    .frame(
                        width: index == currentPage
                            ? Constants.onboardingPillWidth
                            : Constants.onboardingDotSize,
                        height: Constants.onboardingDotSize
                    )
                    .animation(.easeInOut(duration: 0.25), value: currentPage)
            }
        }
    }

    // MARK: - Page Navigation

    private func advancePage() {
        let nextPage = currentPage + 1
        guard nextPage < pageCount else { return }
        withAnimation(.easeInOut(duration: 0.35)) {
            currentPage = nextPage
        }
        guard Constants.onboardingPageSizes.indices.contains(nextPage) else { return }
        resizeWindow(to: Constants.onboardingPageSizes[nextPage])
    }

    private func resizeWindow(to size: CGSize) {
        guard let window = hostWindowReference.window else { return }
        let current = window.frame
        let origin = NSPoint(
            x: current.midX - size.width / 2,
            y: current.midY - size.height / 2
        )
        let newFrame = NSRect(origin: origin, size: size)
        window.setFrame(newFrame, display: true, animate: true)
    }
}
