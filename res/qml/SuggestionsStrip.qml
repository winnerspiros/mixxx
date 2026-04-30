import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Controls 2.15
import QtQuick.Layouts
import "Theme"

// Slim "next track" suggestions strip, à la Virtual DJ's Sideview.
// Tap a suggestion → load on the next free deck.
// Long-press     → append to the AutoDJ queue.
//
// Picks come from QmlSuggestionsModel, which prefers tracks already queued
// in AutoDJ, falling back to most-recently-added when the queue is short.
Rectangle {
    id: root

    color: Theme.deckBackgroundColor
    border.color: Theme.deckLineColor
    border.width: 1
    height: 56

    Mixxx.SuggestionsModel {
        id: suggestions

        count: 4
    }

    // Keep the strip fresh: poll every 2 s while visible. Two indexed SQLite
    // reads are negligible vs. the engine workload, and the timer auto-stops
    // when `visible` flips to false (e.g. user maximises a deck), so there is
    // no cost during heavy mixing when the library is hidden.
    Timer {
        interval: 2000
        repeat: true
        running: root.visible
        triggeredOnStart: true

        onTriggered: suggestions.refresh()
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 4

        Label {
            color: Theme.deckTextColor
            font.bold: true
            font.pixelSize: 11
            text: qsTr("UP NEXT")

            Layout.alignment: Qt.AlignVCenter
            Layout.leftMargin: 6
            Layout.rightMargin: 4
        }

        Repeater {
            model: suggestions

            delegate: Rectangle {
                required property int index
                required property string title
                required property string artist
                required property int durationSec
                required property real bpm
                required property string source

                Layout.fillHeight: true
                Layout.fillWidth: true

                color: pressArea.pressed
                        ? Theme.deckActiveColor
                        : (source === "autodj" ? Theme.deckBackgroundColor
                                               : Qt.darker(Theme.deckBackgroundColor, 1.15))
                border.color: source === "autodj" ? Theme.deckActiveColor : Theme.deckLineColor
                border.width: 1
                radius: 3

                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    spacing: 1

                    Label {
                        color: Theme.deckTextColor
                        elide: Text.ElideRight
                        font.bold: true
                        font.pixelSize: 11
                        text: title || qsTr("(untitled)")

                        Layout.fillWidth: true
                    }

                    Label {
                        color: Theme.deckTextColor
                        elide: Text.ElideRight
                        font.pixelSize: 9
                        opacity: 0.75
                        text: {
                            const m = Math.floor(durationSec / 60);
                            const s = String(durationSec % 60).padStart(2, "0");
                            const dur = durationSec > 0 ? ` · ${m}:${s}` : "";
                            const bpmStr = bpm > 0 ? ` · ${bpm.toFixed(0)} BPM` : "";
                            return (artist || "") + dur + bpmStr;
                        }

                        Layout.fillWidth: true
                    }
                }

                MouseArea {
                    id: pressArea

                    property bool longPressed: false

                    anchors.fill: parent
                    onPressed: longPressed = false
                    onPressAndHold: {
                        longPressed = true;
                        suggestions.appendToAutoDj(index);
                    }
                    onClicked: {
                        if (!longPressed)
                            suggestions.loadOntoNextDeck(index);

                    }
                }
            }
        }

        // When the model is empty (clean DB on first launch) show a hint.
        Label {
            color: Theme.deckTextColor
            opacity: 0.5
            text: qsTr("Add tracks to AutoDJ or your library to see suggestions")
            visible: suggestions.count === 0

            Layout.alignment: Qt.AlignVCenter
            Layout.fillWidth: true
        }
    }
}
