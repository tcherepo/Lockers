import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.VirtualKeyboard 2.4

ApplicationWindow {
    id: window
    width: 640
    height: 480
    visible: true
    title: "Lockers"

    Connections {
        target: communicator
        function onLockLocked() { action.enabled = true }
    }

    ColumnLayout {
        anchors.fill: parent

        ColumnLayout {
            Layout.alignment: Qt.AlignTop | Qt.AlignHCenter

            Label {
                text: "Board"
            }

            SpinBox {
                id: boardspin
                from: 1
                value: 1
                to: 3
                stepSize: 1

                onValueModified: communicator.setBoard(value)
            }
        }

        ColumnLayout {
            Layout.alignment: Qt.AlignTop | Qt.AlignHCenter

            Label {
                text: "Channel"
            }

            SpinBox {
                id: channelspin
                from: 1
                value: 1
                to: 25
                stepSize: 24

                onValueModified: communicator.setChannel(value)
            }
        }

        Button {
            Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
            id: action
            enabled: true
            text: "Open single lock"

            onClicked: {
                enabled = false;
                communicator.unlock();
            }
        }
    }
}
