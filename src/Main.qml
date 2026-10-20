import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Prompter

ApplicationWindow {
    id: win
    width: 1280
    height: 760
    minimumWidth: 900
    minimumHeight: 560
    visible: true
    title: "Prompter"
    color: "#0e0e10"

    Material.theme: Material.Dark
    Material.accent: theme.accent

    property bool helpVisible: false
    // Children with a property of the same name can't see the context
    // property "backend", so hand it to them under this name instead.
    readonly property QtObject prompter: backend
    // Side by side when there's room; stacked, preview first, in a narrow tile.
    readonly property bool wide: width >= 1100
    // Single-key shortcuts would eat typing, so they only work outside the editor.
    readonly property bool keys: !editor.activeFocus

    Shortcut { sequence: "?"; enabled: win.keys; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Q"; enabled: win.keys; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    Shortcut { sequences: ["Space", "PgDown"]; enabled: win.keys; context: Qt.ApplicationShortcut; onActivated: backend.togglePlay() }
    Shortcut { sequence: "Ctrl+Space"; context: Qt.ApplicationShortcut; onActivated: backend.togglePlay() }
    Shortcut { sequence: "PgUp"; enabled: win.keys; context: Qt.ApplicationShortcut; onActivated: backend.nudge(-3) }
    Shortcut { sequence: "Up"; enabled: win.keys; autoRepeat: true; context: Qt.ApplicationShortcut; onActivated: backend.nudge(-1) }
    Shortcut { sequence: "Down"; enabled: win.keys; autoRepeat: true; context: Qt.ApplicationShortcut; onActivated: backend.nudge(1) }
    Shortcut { sequence: "Right"; enabled: win.keys; context: Qt.ApplicationShortcut; onActivated: backend.faster() }
    Shortcut { sequence: "Left"; enabled: win.keys; context: Qt.ApplicationShortcut; onActivated: backend.slower() }
    Shortcut { sequence: "Home"; enabled: win.keys; context: Qt.ApplicationShortcut; onActivated: backend.toTop() }
    Shortcut { sequences: ["+", "="]; enabled: win.keys; context: Qt.ApplicationShortcut; onActivated: backend.fontSize += 4 }
    Shortcut { sequence: "-"; enabled: win.keys; context: Qt.ApplicationShortcut; onActivated: backend.fontSize -= 4 }
    Shortcut { sequence: "]"; enabled: win.keys; context: Qt.ApplicationShortcut; onActivated: backend.margin += 16 }
    Shortcut { sequence: "["; enabled: win.keys; context: Qt.ApplicationShortcut; onActivated: backend.margin -= 16 }
    Shortcut { sequence: "M"; enabled: win.keys; context: Qt.ApplicationShortcut; onActivated: backend.mirror = !backend.mirror }
    Shortcut { sequences: ["E", "Return"]; enabled: win.keys; context: Qt.ApplicationShortcut; onActivated: editor.forceActiveFocus() }
    Shortcut {
        sequence: "Esc"
        context: Qt.ApplicationShortcut
        onActivated: win.helpVisible ? win.helpVisible = false : focusSink.forceActiveFocus()
    }

    // Takes focus away from the editor so the single-key shortcuts work.
    Item { id: focusSink; focus: true }

    DropArea {
        anchors.fill: parent
        onDropped: drop => { if (drop.hasUrls) backend.openFile(drop.urls[0]) }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        // Header: name and the Prompter's state, said plainly.
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Label { text: "Prompter"; font.pixelSize: 20; font.bold: true }
            Rectangle {
                width: 10; height: 10; radius: 5
                color: backend.outputConnected ? "#3ecf6e" : "#e5534b"
            }
            Label {
                Layout.fillWidth: true
                text: backend.outputStatus
                elide: Text.ElideRight
                color: backend.outputConnected ? "#9a9aa2" : "#e5534b"
            }
            Label { text: "? shortcuts"; color: "#6c6c74" }
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columnSpacing: 16
            rowSpacing: 16

            // Script editor.
            Rectangle {
                Layout.row: win.wide ? 0 : 1
                Layout.column: 0
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#16161a"
                radius: 8
                border.width: editor.activeFocus ? 1 : 0
                border.color: theme.accent

                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 4
                    TextArea {
                        id: editor
                        text: backend.text
                        onTextChanged: if (text !== backend.text) backend.text = text
                        wrapMode: TextArea.Wrap
                        font.pixelSize: 16
                        placeholderText: "Type or paste your script, or drop a .txt or .md file here.\n\n"
                            + "# Headings and **emphasis** work.\n[Stage directions] in brackets are highlighted.\n\n"
                            + "Press Esc to leave the editor, then Space to roll."
                        background: null
                        selectByMouse: true
                    }
                }
            }

            ColumnLayout {
                Layout.row: 0
                Layout.column: win.wide ? 1 : 0
                Layout.fillWidth: !win.wide
                Layout.preferredWidth: win.wide ? 520 : -1
                Layout.maximumWidth: win.wide ? 520 : Number.POSITIVE_INFINITY
                Layout.fillHeight: win.wide
                spacing: 14

                // What's on the glass, with a scroll wheel to scrub.
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: width * 600 / 1024
                    color: "black"
                    radius: 6
                    clip: true
                    border.width: 1
                    border.color: "#2a2a30"

                    Preview {
                        anchors.fill: parent
                        anchors.margins: 1
                        backend: win.prompter
                    }
                    MouseArea {
                        anchors.fill: parent
                        onWheel: wheel => backend.nudge(-wheel.angleDelta.y / 120)
                        onClicked: focusSink.forceActiveFocus()
                    }
                }

                ProgressBar {
                    Layout.fillWidth: true
                    value: backend.progress
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Button {
                        text: "Top"
                        flat: true
                        onClicked: { backend.toTop(); focusSink.forceActiveFocus() }
                    }
                    Button {
                        Layout.fillWidth: true
                        highlighted: true
                        text: backend.playing ? "Pause" : "Roll"
                        onClicked: { backend.togglePlay(); focusSink.forceActiveFocus() }
                    }
                    Switch {
                        text: "Mirror"
                        checked: backend.mirror
                        onToggled: { backend.mirror = checked; focusSink.forceActiveFocus() }
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 3
                    columnSpacing: 12
                    rowSpacing: 0

                    Label { text: "Speed" }
                    Slider {
                        Layout.fillWidth: true
                        from: 4; to: 200; stepSize: 1
                        value: backend.speed
                        onMoved: backend.speed = value
                        onPressedChanged: if (!pressed) focusSink.forceActiveFocus()
                    }
                    Label { text: backend.speed + " lines/min"; color: "#9a9aa2"; Layout.preferredWidth: 96 }

                    Label { text: "Text size" }
                    Slider {
                        Layout.fillWidth: true
                        from: 24; to: 160; stepSize: 2
                        value: backend.fontSize
                        onMoved: backend.fontSize = value
                        onPressedChanged: if (!pressed) focusSink.forceActiveFocus()
                    }
                    Label { text: backend.fontSize + " px"; color: "#9a9aa2" }

                    Label { text: "Margins" }
                    Slider {
                        Layout.fillWidth: true
                        from: 0; to: 320; stepSize: 8
                        value: backend.margin
                        onMoved: backend.margin = value
                        onPressedChanged: if (!pressed) focusSink.forceActiveFocus()
                    }
                    Label { text: backend.margin + " px"; color: "#9a9aa2" }

                    Label { text: "Reading line" }
                    Slider {
                        Layout.fillWidth: true
                        from: 0.1; to: 0.7
                        value: backend.readingLine
                        onMoved: backend.readingLine = value
                        onPressedChanged: if (!pressed) focusSink.forceActiveFocus()
                    }
                    Label { text: Math.round(backend.readingLine * 100) + "% down"; color: "#9a9aa2" }
                }

                Item { Layout.fillHeight: true; visible: win.wide }

                Label {
                    Layout.fillWidth: true
                    visible: text !== ""
                    text: backend.message
                    color: "#9a9aa2"
                    elide: Text.ElideRight
                }
            }
        }
    }

    // Shortcut overlay.
    Rectangle {
        anchors.fill: parent
        visible: win.helpVisible
        color: "#e60e0e10"

        MouseArea { anchors.fill: parent; onClicked: win.helpVisible = false }

        GridLayout {
            anchors.centerIn: parent
            columns: 2
            columnSpacing: 32
            rowSpacing: 10

            Repeater {
                model: [
                    ["Space / Page Down", "Roll or pause (clicker forward)"],
                    ["Ctrl+Space", "Roll or pause, even while editing"],
                    ["Page Up", "Back three lines (clicker back)"],
                    ["↑ / ↓", "Nudge a line back or forward"],
                    ["← / →", "Slower / faster"],
                    ["Home", "Back to the top"],
                    ["+ / −", "Bigger / smaller text"],
                    ["[ / ]", "Narrower / wider margins"],
                    ["M", "Mirror the glass on or off"],
                    ["E / Enter", "Edit the script"],
                    ["Esc", "Leave the editor"],
                    ["Scroll on preview", "Scrub through the script"],
                    ["?", "This help"],
                    ["Q", "Quit"]
                ]
                delegate: Item {
                    required property var modelData
                    required property int index
                    Layout.columnSpan: 2
                    Layout.preferredWidth: 520
                    Layout.preferredHeight: 26
                    Label {
                        text: modelData[0]
                        color: theme.accent
                        font.bold: true
                        font.pixelSize: 16
                    }
                    Label {
                        x: 220
                        text: modelData[1]
                        font.pixelSize: 16
                    }
                }
            }
        }
    }
}
