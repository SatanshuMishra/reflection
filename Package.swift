// swift-tools-version: 6.0

import PackageDescription

let package = Package(
    name: "Reflection",
    platforms: [
        .macOS(.v13)
    ],
    targets: [
        .executableTarget(
            name: "Reflection",
            path: "Sources/Reflection"
        ),
        .testTarget(
            name: "ReflectionTests",
            dependencies: ["Reflection"],
            path: "Tests/ReflectionTests"
        )
    ]
)
