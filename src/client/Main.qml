import QtQuick
import QtQuick.Controls
import QtQml

Window {
    width: 960
    height: 640
    minimumWidth: 720
    minimumHeight: 520
    visible: true
    title: qsTr("Messenger Client")

    color: "#f7f8fa"

    property string receivedMessages: ""

    function connectWithInput() {
        if (userNameInput.text.trim().length === 0
                || hostInput.text.trim().length === 0
                || !portInput.acceptableInput) {
            return
        }

        networkManager.userName = userNameInput.text
        networkManager.connectToServer(hostInput.text, Number(portInput.text))
    }

    component FieldLabel: Text {
        color: "#43515f"
        font.pixelSize: 14
    }

    component TextInputBox: Rectangle {
        id: inputBox

        property alias text: input.text
        property alias validator: input.validator
        property alias acceptableInput: input.acceptableInput
        property bool passwordMode: false
        signal accepted()

        height: 44
        radius: 6
        color: "#ffffff"
        border.color: input.activeFocus ? "#205493" : "#c8d0d9"

        TextInput {
            id: input
            anchors.fill: parent
            anchors.margins: 11
            color: "#1f2933"
            font.pixelSize: 15
            verticalAlignment: TextInput.AlignVCenter
            selectByMouse: true
            echoMode: passwordMode ? TextInput.Password : TextInput.Normal
            onAccepted: inputBox.accepted()
        }
    }

    component ActionButton: Rectangle {
        property alias text: label.text
        property bool enabledState: true
        property color activeColor: "#205493"
        signal clicked()

        height: 44
        radius: 6
        color: enabledState ? activeColor : "#d7dde5"

        Text {
            id: label
            anchors.centerIn: parent
            color: enabledState ? "#ffffff" : "#43515f"
            font.pixelSize: 15
        }

        MouseArea {
            anchors.fill: parent
            enabled: enabledState
            cursorShape: enabledState ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: parent.clicked()
        }
    }

    Item {
        id: connectionPage
        anchors.fill: parent
        visible: !networkManager.connected

        Column {
            anchors.centerIn: parent
            width: Math.min(420, parent.width - 64)
            spacing: 18

            Text {
                width: parent.width
                text: qsTr("Messenger Client")
                color: "#1f2933"
                font.pixelSize: 36
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                width: parent.width
                text: networkManager.statusText
                color: "#7a2830"
                font.pixelSize: 16
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }

            Column {
                width: parent.width
                spacing: 8

                FieldLabel {
                    text: qsTr("Name")
                }

                TextInputBox {
                    id: userNameInput
                    width: parent.width
                    text: networkManager.userName
                    onAccepted: connectWithInput()
                    onTextChanged: networkManager.userName = text
                }
            }

            Column {
                width: parent.width
                spacing: 8

                FieldLabel {
                    text: qsTr("Server IP")
                }

                TextInputBox {
                    id: hostInput
                    width: parent.width
                    text: "127.0.0.1"
                    onAccepted: connectWithInput()
                }
            }

            Column {
                width: parent.width
                spacing: 8

                FieldLabel {
                    text: qsTr("Port")
                }

                TextInputBox {
                    id: portInput
                    width: parent.width
                    text: String(networkManager.defaultPort)
                    onAccepted: connectWithInput()
                    validator: IntValidator {
                        bottom: 1
                        top: 65535
                    }
                }
            }

            ActionButton {
                width: parent.width
                text: qsTr("Verbinden")
                enabledState: userNameInput.text.trim().length > 0
                              && hostInput.text.trim().length > 0
                              && portInput.acceptableInput
                onClicked: connectWithInput()
            }
        }
    }

    Column {
        id: chatPage
        anchors {
            fill: parent
            margins: 32
        }
        spacing: 16
        visible: networkManager.connected

        Row {
            width: parent.width
            spacing: 12

            Column {
                width: Math.max(180, parent.width - disconnectButton.width - parent.spacing)
                spacing: 4

                Text {
                    text: qsTr("Messenger Client")
                    color: "#1f2933"
                    font.pixelSize: 32
                }

                Text {
                    text: networkManager.statusText
                    color: "#127a3a"
                    font.pixelSize: 16
                }
            }

            Rectangle {
                id: disconnectButton
                width: 44
                height: 44
                radius: 6
                color: disconnectMouseArea.containsMouse ? "#c8d0d9" : "#d7dde5"
                border.color: "#b7c0ca"

                Canvas {
                    anchors.centerIn: parent
                    width: 24
                    height: 24

                    onPaint: {
                        const context = getContext("2d")
                        context.clearRect(0, 0, width, height)
                        context.strokeStyle = "#43515f"
                        context.lineWidth = 2
                        context.lineCap = "round"
                        context.lineJoin = "round"

                        context.beginPath()
                        context.moveTo(10, 5)
                        context.lineTo(5, 5)
                        context.lineTo(5, 19)
                        context.lineTo(10, 19)
                        context.stroke()

                        context.beginPath()
                        context.moveTo(12, 12)
                        context.lineTo(21, 12)
                        context.stroke()

                        context.beginPath()
                        context.moveTo(17, 8)
                        context.lineTo(21, 12)
                        context.lineTo(17, 16)
                        context.stroke()
                    }
                }

                MouseArea {
                    id: disconnectMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: networkManager.disconnectFromServer()
                }
            }
        }

        Rectangle {
            width: parent.width
            height: Math.max(120, parent.height - y - composeRow.height - parent.spacing)
            radius: 6
            color: "#ffffff"
            border.color: "#c8d0d9"
            clip: true

            Flickable {
                id: messageFlickable
                anchors {
                    fill: parent
                    margins: 12
                }
                contentWidth: width
                contentHeight: Math.max(height, messageText.paintedHeight)
                boundsBehavior: Flickable.StopAtBounds
                clip: true

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                Text {
                    id: messageText
                    width: messageFlickable.width
                    y: Math.max(0, messageFlickable.height - paintedHeight)
                    text: receivedMessages.length > 0 ? receivedMessages : qsTr("Keine Nachrichten empfangen.")
                    color: "#1f2933"
                    font.pixelSize: 15
                    wrapMode: Text.Wrap
                }
            }
        }

        Row {
            id: composeRow
            width: parent.width
            spacing: 12

            TextInputBox {
                id: messageInput
                width: Math.max(120, composeRow.width - sendButton.width - composeRow.spacing)
                onAccepted: {
                    if (networkManager.connected && messageInput.text.length > 0) {
                        networkManager.sendChatMessage(messageInput.text)
                        messageInput.text = ""
                    }
                }
            }

            ActionButton {
                id: sendButton
                width: 140
                text: qsTr("Senden")
                enabledState: networkManager.connected && messageInput.text.length > 0
                onClicked: {
                    networkManager.sendChatMessage(messageInput.text)
                    messageInput.text = ""
                }
            }
        }
    }

    Connections {
        target: networkManager

        function onConnectedChanged() {
            if (!networkManager.connected) {
                receivedMessages = ""
            }
        }

        function onMessageReceived(senderName, text, sentAt) {
            receivedMessages += "[" + sentAt + "] " + senderName + ": " + text + "\n"
            Qt.callLater(function() {
                messageFlickable.contentY = Math.max(0, messageFlickable.contentHeight - messageFlickable.height)
            })
        }

        function onConnectionError(message) {
            receivedMessages += "Error: " + message + "\n"
            Qt.callLater(function() {
                messageFlickable.contentY = Math.max(0, messageFlickable.contentHeight - messageFlickable.height)
            })
        }
    }
}
