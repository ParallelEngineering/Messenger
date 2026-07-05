import QtQuick
import QtQml

Window {
    width: 960
    height: 640
    visible: true
    title: qsTr("Messenger Client")

    color: "#f7f8fa"

    property string receivedMessages: ""

    Column {
        anchors {
            fill: parent
            margins: 32
        }
        spacing: 16

        Text {
            text: qsTr("Messenger Client")
            color: "#1f2933"
            font.pixelSize: 36
        }

        Text {
            text: networkManager.statusText
            color: networkManager.connected ? "#127a3a" : "#7a2830"
            font.pixelSize: 16
        }

        Row {
            spacing: 12

            Rectangle {
                width: 240
                height: 40
                radius: 6
                color: "#ffffff"
                border.color: "#c8d0d9"

                TextInput {
                    id: hostInput
                    anchors.fill: parent
                    anchors.margins: 10
                    text: "127.0.0.1"
                    color: "#1f2933"
                    font.pixelSize: 15
                    verticalAlignment: TextInput.AlignVCenter
                    selectByMouse: true
                }
            }

            Rectangle {
                width: 96
                height: 40
                radius: 6
                color: "#ffffff"
                border.color: "#c8d0d9"

                TextInput {
                    id: portInput
                    anchors.fill: parent
                    anchors.margins: 10
                    text: String(networkManager.defaultPort)
                    color: "#1f2933"
                    font.pixelSize: 15
                    verticalAlignment: TextInput.AlignVCenter
                    validator: IntValidator { bottom: 1; top: 65535 }
                    selectByMouse: true
                }
            }

            Rectangle {
                width: 120
                height: 40
                radius: 6
                color: networkManager.connected ? "#d7dde5" : "#205493"

                Text {
                    anchors.centerIn: parent
                    text: networkManager.connected ? qsTr("Connected") : qsTr("Connect")
                    color: networkManager.connected ? "#43515f" : "#ffffff"
                    font.pixelSize: 15
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: !networkManager.connected
                    onClicked: networkManager.connectToServer(hostInput.text, Number(portInput.text))
                }
            }

            Rectangle {
                width: 120
                height: 40
                radius: 6
                color: networkManager.connected ? "#8b2635" : "#d7dde5"

                Text {
                    anchors.centerIn: parent
                    text: qsTr("Disconnect")
                    color: networkManager.connected ? "#ffffff" : "#43515f"
                    font.pixelSize: 15
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: networkManager.connected
                    onClicked: networkManager.disconnectFromServer()
                }
            }
        }

        Row {
            spacing: 12

            Rectangle {
                width: 180
                height: 40
                radius: 6
                color: "#ffffff"
                border.color: "#c8d0d9"

                TextInput {
                    id: senderInput
                    anchors.fill: parent
                    anchors.margins: 10
                    text: "client"
                    color: "#1f2933"
                    font.pixelSize: 15
                    verticalAlignment: TextInput.AlignVCenter
                    selectByMouse: true
                }
            }

            Rectangle {
                width: 180
                height: 40
                radius: 6
                color: "#ffffff"
                border.color: "#c8d0d9"

                TextInput {
                    id: recipientInput
                    anchors.fill: parent
                    anchors.margins: 10
                    text: "server"
                    color: "#1f2933"
                    font.pixelSize: 15
                    verticalAlignment: TextInput.AlignVCenter
                    selectByMouse: true
                }
            }
        }

        Rectangle {
            width: parent.width
            height: 44
            radius: 6
            color: "#ffffff"
            border.color: "#c8d0d9"

            TextInput {
                id: messageInput
                anchors.fill: parent
                anchors.margins: 10
                color: "#1f2933"
                font.pixelSize: 15
                verticalAlignment: TextInput.AlignVCenter
                selectByMouse: true
            }
        }

        Rectangle {
            width: 140
            height: 40
            radius: 6
            color: networkManager.connected ? "#205493" : "#d7dde5"

            Text {
                anchors.centerIn: parent
                text: qsTr("Send")
                color: networkManager.connected ? "#ffffff" : "#43515f"
                font.pixelSize: 15
            }

            MouseArea {
                anchors.fill: parent
                enabled: networkManager.connected
                onClicked: {
                    networkManager.sendChatMessage(senderInput.text, recipientInput.text, messageInput.text)
                    messageInput.text = ""
                }
            }
        }

        Rectangle {
            width: parent.width
            height: Math.max(180, parent.height - y)
            radius: 6
            color: "#ffffff"
            border.color: "#c8d0d9"
            clip: true

            Text {
                anchors {
                    fill: parent
                    margins: 12
                }
                text: receivedMessages.length > 0 ? receivedMessages : qsTr("No messages received.")
                color: "#1f2933"
                font.pixelSize: 15
                wrapMode: Text.Wrap
            }
        }
    }

    Connections {
        target: networkManager

        function onMessageReceived(sender, recipient, text) {
            receivedMessages += sender + " -> " + recipient + ": " + text + "\n"
        }

        function onConnectionError(message) {
            receivedMessages += "Error: " + message + "\n"
        }
    }
}
