import os

extension Logger {
    private static let subsystem = "com.reflection.app"

    static let capture = Logger(subsystem: subsystem, category: "Capture")
    static let discovery = Logger(subsystem: subsystem, category: "Discovery")
    static let session = Logger(subsystem: subsystem, category: "Session")
    static let ui = Logger(subsystem: subsystem, category: "UI")
}
