// SB Audio Installer – a small SwiftUI app that shows every plugin as a card
// (screenshot + short description) and installs the ones the user picks.
//
// Built by scripts/make-installer-app.sh. The app bundle contains:
//   Contents/Resources/plugins.plist        – list of plugins (name, tagline, description)
//   Contents/Resources/Images/<Name>.png     – screenshot per plugin
//   Contents/Resources/Plugins/<Name>.component / <Name>.vst3

import SwiftUI
import AppKit

// MARK: - Model

struct PluginInfo: Decodable, Identifiable {
    let name: String
    let tagline: String
    let summary: String
    let hasAU: Bool
    let hasVST3: Bool
    var id: String { name }
}

struct Manifest: Decodable {
    let version: String
    let plugins: [PluginInfo]
}

enum Paths {
    static let resources = Bundle.main.resourceURL!
    static let systemComponents = URL(fileURLWithPath: "/Library/Audio/Plug-Ins/Components")
    static let systemVST3 = URL(fileURLWithPath: "/Library/Audio/Plug-Ins/VST3")
    static let userComponents = FileManager.default.homeDirectoryForCurrentUser
        .appendingPathComponent("Library/Audio/Plug-Ins/Components")
    static let userVST3 = FileManager.default.homeDirectoryForCurrentUser
        .appendingPathComponent("Library/Audio/Plug-Ins/VST3")

    static func bundled(_ p: PluginInfo, ext: String) -> URL {
        resources.appendingPathComponent("Plugins/\(p.name).\(ext)")
    }
    static func image(_ p: PluginInfo) -> NSImage? {
        NSImage(contentsOf: resources.appendingPathComponent("Images/\(p.name).png"))
    }
}

enum Phase: Equatable {
    case choosing
    case installing
    case done([String])
    case failed(String)
}

final class InstallerModel: ObservableObject {
    @Published var manifest = Manifest(version: "?", plugins: [])
    @Published var selected: Set<String> = []
    @Published var installAU = true
    @Published var installVST3 = true
    @Published var phase: Phase = .choosing
    @Published var removeUserCopies = true

    init() {
        let url = Paths.resources.appendingPathComponent("plugins.plist")
        if let data = try? Data(contentsOf: url),
           let m = try? PropertyListDecoder().decode(Manifest.self, from: data) {
            manifest = m
            selected = Set(m.plugins.map(\.name))
        }
    }

    func isInstalled(_ p: PluginInfo) -> Bool {
        let fm = FileManager.default
        return fm.fileExists(atPath: Paths.systemComponents.appendingPathComponent("\(p.name).component").path)
            || fm.fileExists(atPath: Paths.systemVST3.appendingPathComponent("\(p.name).vst3").path)
    }

    /// Plugins previously installed by scripts/build.sh into ~/Library – Logic would see two copies.
    var userCopies: [URL] {
        let fm = FileManager.default
        return manifest.plugins.filter { selected.contains($0.name) }.flatMap { p -> [URL] in
            [Paths.userComponents.appendingPathComponent("\(p.name).component"),
             Paths.userVST3.appendingPathComponent("\(p.name).vst3")]
        }.filter { fm.fileExists(atPath: $0.path) }
    }

    var canInstall: Bool { !selected.isEmpty && (installAU || installVST3) }

    func toggle(_ p: PluginInfo) {
        if selected.contains(p.name) { selected.remove(p.name) } else { selected.insert(p.name) }
    }

    func install() {
        let chosen = manifest.plugins.filter { selected.contains($0.name) }
        var lines = ["#!/bin/bash", "set -e"]
        func q(_ s: String) -> String { "'" + s.replacingOccurrences(of: "'", with: "'\\''") + "'" }
        func copy(_ src: URL, to dir: URL) {
            let dest = dir.appendingPathComponent(src.lastPathComponent).path
            lines.append("mkdir -p \(q(dir.path))")
            lines.append("rm -rf \(q(dest))")
            lines.append("/usr/bin/ditto \(q(src.path)) \(q(dest))")
            lines.append("/usr/bin/xattr -dr com.apple.quarantine \(q(dest)) 2>/dev/null || true")
            lines.append("chown -R root:wheel \(q(dest)); chmod -R a+rX \(q(dest))")
        }
        var installed: [String] = []
        for p in chosen {
            var formats: [String] = []
            if installAU && p.hasAU { copy(Paths.bundled(p, ext: "component"), to: Paths.systemComponents); formats.append("AU") }
            if installVST3 && p.hasVST3 { copy(Paths.bundled(p, ext: "vst3"), to: Paths.systemVST3); formats.append("VST3") }
            if !formats.isEmpty { installed.append("\(p.name) (\(formats.joined(separator: " + ")))") }
        }
        if removeUserCopies {
            for u in userCopies { lines.append("rm -rf \(q(u.path))") }
        }
        // Make macOS rescan Audio Units
        lines.append("killall -9 AudioComponentRegistrar 2>/dev/null || true")

        let script = FileManager.default.temporaryDirectory.appendingPathComponent("sbaudio-install.sh")
        do {
            try lines.joined(separator: "\n").write(to: script, atomically: true, encoding: .utf8)
        } catch {
            phase = .failed(error.localizedDescription); return
        }

        phase = .installing
        DispatchQueue.global(qos: .userInitiated).async {
            let apple = "do shell script \"/bin/bash \" & quoted form of \"\(script.path)\" with administrator privileges"
            let proc = Process()
            proc.executableURL = URL(fileURLWithPath: "/usr/bin/osascript")
            proc.arguments = ["-e", apple]
            let err = Pipe()
            proc.standardError = err
            var result: Phase
            do {
                try proc.run()
                proc.waitUntilExit()
                let msg = String(data: err.fileHandleForReading.readDataToEndOfFile(), encoding: .utf8) ?? ""
                if proc.terminationStatus == 0 {
                    result = .done(installed)
                } else if msg.contains("-128") {
                    result = .choosing // user cancelled the password prompt
                } else {
                    result = .failed(msg.trimmingCharacters(in: .whitespacesAndNewlines))
                }
            } catch {
                result = .failed(error.localizedDescription)
            }
            try? FileManager.default.removeItem(at: script)
            DispatchQueue.main.async { self.phase = result }
        }
    }
}

// MARK: - Look

extension Color {
    init(hex: UInt32) {
        self.init(.sRGB, red: Double((hex >> 16) & 0xff) / 255, green: Double((hex >> 8) & 0xff) / 255,
                  blue: Double(hex & 0xff) / 255, opacity: 1)
    }
    static let sbBg = Color(hex: 0x0f1115)
    static let sbPanel = Color(hex: 0x181b21)
    static let sbEdge = Color(hex: 0x262a33)
    static let sbWell = Color(hex: 0x0b0d10)
    static let sbText = Color(hex: 0xc9ced6)
    static let sbDim = Color(hex: 0x6d7480)
    static let sbAccent = Color(hex: 0x3fd0c9)
    static let sbRed = Color(hex: 0xe0566b)
    static let sbGreen = Color(hex: 0x3fbf6a)
}

struct CheckMark: View {
    let on: Bool
    var body: some View {
        ZStack {
            RoundedRectangle(cornerRadius: 5)
                .fill(on ? Color.sbAccent : Color.sbWell)
                .overlay(RoundedRectangle(cornerRadius: 5).stroke(on ? Color.sbAccent : Color.sbDim, lineWidth: 1.5))
            if on {
                Image(systemName: "checkmark").font(.system(size: 11, weight: .heavy)).foregroundColor(.sbBg)
            }
        }
        .frame(width: 20, height: 20)
    }
}

struct PluginCard: View {
    let plugin: PluginInfo
    let selected: Bool
    let installed: Bool
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            VStack(alignment: .leading, spacing: 0) {
                ZStack {
                    Color.sbWell
                    if let img = Paths.image(plugin) {
                        Image(nsImage: img).resizable().aspectRatio(contentMode: .fit).padding(8)
                    } else { // no screenshot yet – show a name plate
                        VStack(spacing: 6) {
                            Image(systemName: "waveform").font(.system(size: 34)).foregroundColor(.sbAccent)
                            Text(plugin.name.uppercased()).font(.system(size: 22, weight: .heavy))
                                .tracking(4).foregroundColor(.sbText)
                        }
                    }
                }
                .overlay(Group {
                    if installed {
                        Text("INSTALLERT")
                            .font(.system(size: 9, weight: .bold)).tracking(1)
                            .padding(.horizontal, 6).padding(.vertical, 3)
                            .background(Color.sbGreen.opacity(0.85)).foregroundColor(.sbBg)
                            .cornerRadius(4).padding(10)
                    }
                }, alignment: .topTrailing)
                .frame(height: 170)
                .clipped()

                VStack(alignment: .leading, spacing: 6) {
                    HStack(alignment: .center, spacing: 10) {
                        CheckMark(on: selected)
                        VStack(alignment: .leading, spacing: 1) {
                            Text(plugin.name).font(.system(size: 16, weight: .semibold)).foregroundColor(.sbText)
                            Text(plugin.tagline).font(.system(size: 11)).foregroundColor(.sbAccent)
                        }
                    }
                    Text(plugin.summary)
                        .font(.system(size: 12)).foregroundColor(.sbText.opacity(0.8))
                        .fixedSize(horizontal: false, vertical: true)
                        .lineSpacing(2)
                }
                .padding(14)
                Spacer(minLength: 0)
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
            .background(Color.sbPanel)
            .cornerRadius(10)
            .overlay(RoundedRectangle(cornerRadius: 10)
                .stroke(selected ? Color.sbAccent : Color.sbEdge, lineWidth: selected ? 2 : 1))
            .opacity(selected ? 1 : 0.6)
            .contentShape(Rectangle())
        }
        .buttonStyle(PlainButtonStyle())
    }
}

struct FormatToggle: View {
    let title: String
    @Binding var on: Bool
    var body: some View {
        Button(action: { on.toggle() }) {
            HStack(spacing: 6) { CheckMark(on: on); Text(title).foregroundColor(.sbText) }
        }.buttonStyle(PlainButtonStyle())
    }
}

struct AccentButton: View {
    let title: String
    let enabled: Bool
    let action: () -> Void
    var body: some View {
        Button(action: action) {
            Text(title).font(.system(size: 13, weight: .semibold))
                .padding(.horizontal, 18).padding(.vertical, 8)
                .background(enabled ? Color.sbAccent : Color.sbEdge)
                .foregroundColor(enabled ? .sbBg : .sbDim)
                .cornerRadius(7)
        }
        .buttonStyle(PlainButtonStyle())
        .disabled(!enabled)
    }
}

// MARK: - Screens

struct ContentView: View {
    @StateObject private var model = InstallerModel()

    var body: some View {
        VStack(spacing: 0) {
            header
            Divider().background(Color.sbEdge)
            switch model.phase {
            case .choosing, .installing: chooser
            case .done(let list): finished(list)
            case .failed(let msg): failure(msg)
            }
        }
        .background(Color.sbBg)
        .frame(minWidth: 860, minHeight: 640)
    }

    var header: some View {
        HStack(alignment: .firstTextBaseline) {
            Text("SB AUDIO").font(.system(size: 20, weight: .heavy)).tracking(3).foregroundColor(.sbText)
            Text("Installer \(model.manifest.version)").font(.system(size: 13)).foregroundColor(.sbDim)
            Spacer()
            Text("Velg pluginene du vil installere").font(.system(size: 13)).foregroundColor(.sbDim)
        }
        .padding(.horizontal, 24).padding(.vertical, 16)
    }

    var chooser: some View {
        VStack(spacing: 0) {
            ScrollView {
                LazyVGrid(columns: [GridItem(.adaptive(minimum: 380), spacing: 16)], spacing: 16) {
                    ForEach(model.manifest.plugins) { p in
                        PluginCard(plugin: p, selected: model.selected.contains(p.name),
                                   installed: model.isInstalled(p)) { model.toggle(p) }
                    }
                }
                .padding(24)
            }
            if !model.userCopies.isEmpty {
                HStack {
                    FormatToggle(title: "Fjern gamle kopier i ~/Library/Audio/Plug-Ins (\(model.userCopies.count) stk.), så Logic ikke ser to versjoner",
                                 on: $model.removeUserCopies)
                    Spacer()
                }
                .font(.system(size: 12))
                .padding(.horizontal, 24).padding(.vertical, 10)
                .background(Color.sbPanel)
            }
            Divider().background(Color.sbEdge)
            footer
        }
    }

    var footer: some View {
        HStack(spacing: 18) {
            Button("Velg alle") { model.selected = Set(model.manifest.plugins.map(\.name)) }
            Button("Velg ingen") { model.selected = [] }
            Divider().frame(height: 18)
            FormatToggle(title: "Audio Unit (Logic Pro)", on: $model.installAU)
            FormatToggle(title: "VST3", on: $model.installVST3)
            Spacer()
            if model.phase == .installing {
                ProgressView().scaleEffect(0.6).frame(width: 20, height: 20)
                Text("Installerer …").foregroundColor(.sbDim)
            } else {
                Text("\(model.selected.count) av \(model.manifest.plugins.count) valgt").foregroundColor(.sbDim)
                AccentButton(title: "Installer", enabled: model.canInstall) { model.install() }
            }
        }
        .font(.system(size: 12))
        .padding(.horizontal, 24).padding(.vertical, 14)
        .background(Color.sbPanel)
    }

    func finished(_ list: [String]) -> some View {
        VStack(spacing: 14) {
            Spacer()
            Image(systemName: "checkmark.circle.fill").font(.system(size: 54)).foregroundColor(.sbGreen)
            Text("Ferdig").font(.system(size: 22, weight: .semibold)).foregroundColor(.sbText)
            VStack(spacing: 4) {
                ForEach(list, id: \.self) { Text($0).foregroundColor(.sbText.opacity(0.85)) }
            }
            Text("Start Logic Pro på nytt. Pluginene ligger under Audio Units › SB Audio.")
                .foregroundColor(.sbDim).padding(.top, 6)
            HStack(spacing: 12) {
                Button("Tilbake") { model.phase = .choosing }
                AccentButton(title: "Avslutt", enabled: true) { NSApp.terminate(nil) }
            }.padding(.top, 10)
            Spacer()
        }
        .font(.system(size: 13))
        .frame(maxWidth: .infinity)
    }

    func failure(_ msg: String) -> some View {
        VStack(spacing: 14) {
            Spacer()
            Image(systemName: "exclamationmark.triangle.fill").font(.system(size: 50)).foregroundColor(.sbRed)
            Text("Installasjonen feilet").font(.system(size: 20, weight: .semibold)).foregroundColor(.sbText)
            Text(msg).foregroundColor(.sbDim).multilineTextAlignment(.center).frame(maxWidth: 560)
            Button("Prøv igjen") { model.phase = .choosing }
            Spacer()
        }
        .font(.system(size: 13))
        .frame(maxWidth: .infinity)
    }
}

final class AppDelegate: NSObject, NSApplicationDelegate {
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }
    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.appearance = NSAppearance(named: .darkAqua)
    }
}

@main
struct SBAudioInstallerApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) var delegate
    var body: some Scene {
        WindowGroup("SB Audio Installer") { ContentView() }
    }
}
