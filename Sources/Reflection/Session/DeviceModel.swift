import AVFoundation

public struct DeviceModel: Identifiable, Equatable, Sendable {
    public let id: String
    public let name: String
    public let modelID: String
    public let isConnected: Bool

    public init(id: String, name: String, modelID: String, isConnected: Bool) {
        self.id = id
        self.name = name
        self.modelID = modelID
        self.isConnected = isConnected
    }

    public init(from device: AVCaptureDevice) {
        self.id = device.uniqueID
        self.name = device.localizedName
        self.modelID = device.modelID
        self.isConnected = device.isConnected
    }
}
