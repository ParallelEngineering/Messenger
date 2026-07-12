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

    property string pendingDeleteKeyName: ""
    property bool connectionInputValid: userNameInput.text.trim().length > 0
                                        && hostInput.text.trim().length > 0
                                        && portInput.acceptableInput
                                        && connectionStore.selectedKeyName.trim().length > 0

    ListModel {
        id: chatMessages
    }

    function connectWithInput() {
        if (userNameInput.text.trim().length === 0
                || hostInput.text.trim().length === 0
                || !portInput.acceptableInput
                || connectionStore.selectedKeyName.trim().length === 0) {
            return
        }

        networkManager.userName = userNameInput.text
        if (!connectionStore.saveLastConnection(hostInput.text,
                                                Number(portInput.text),
                                                userNameInput.text,
                                                connectionStore.selectedKeyName)) {
            return
        }

        networkManager.connectToServer(hostInput.text, Number(portInput.text))
    }

    function syncKeyComboBox() {
        if (!keyComboBox) {
            return
        }

        keyComboBox.currentIndex = connectionStore.selectedKeyName.length > 0
                ? keyComboBox.find(connectionStore.selectedKeyName)
                : -1
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

    component QuietButton: Rectangle {
        id: quietButton

        property alias text: label.text
        property bool enabledState: true
        property color normalColor: "#ffffff"
        property color hoverColor: "#f7f8fa"
        property color disabledColor: "#d7dde5"
        property color borderColor: "#c8d0d9"
        property color textColor: "#1f2933"
        signal clicked()

        height: 44
        radius: 6
        color: enabledState ? (quietButtonMouseArea.containsMouse ? hoverColor : normalColor) : disabledColor
        border.color: enabledState ? borderColor : "#c8d0d9"

        Text {
            id: label
            anchors.centerIn: parent
            color: enabledState ? quietButton.textColor : "#43515f"
            font.pixelSize: 15
        }

        MouseArea {
            id: quietButtonMouseArea
            anchors.fill: parent
            enabled: quietButton.enabledState
            hoverEnabled: true
            cursorShape: quietButton.enabledState ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: quietButton.clicked()
        }
    }

    component IconButton: Rectangle {
        id: iconButton

        property bool enabledState: true
        property string iconName: "edit"
        property color activeColor: "#205493"
        property color iconColor: enabledState ? "#ffffff" : "#43515f"
        signal clicked()

        width: 44
        height: 44
        radius: 6
        color: enabledState ? activeColor : "#d7dde5"

        Canvas {
            id: iconCanvas
            anchors.centerIn: parent
            width: 24
            height: 24

            onPaint: {
                const context = getContext("2d")
                context.clearRect(0, 0, width, height)
                context.strokeStyle = iconButton.iconColor
                context.fillStyle = iconButton.iconColor
                context.lineWidth = 2
                context.lineCap = "round"
                context.lineJoin = "round"

                if (iconButton.iconName === "close") {
                    context.beginPath()
                    context.moveTo(7, 7)
                    context.lineTo(17, 17)
                    context.moveTo(17, 7)
                    context.lineTo(7, 17)
                    context.stroke()
                } else {
                    context.beginPath()
                    context.moveTo(5, 19)
                    context.lineTo(9, 18)
                    context.lineTo(18, 9)
                    context.lineTo(15, 6)
                    context.lineTo(6, 15)
                    context.closePath()
                    context.stroke()

                    context.beginPath()
                    context.moveTo(14, 7)
                    context.lineTo(17, 10)
                    context.stroke()
                }
            }
        }

        MouseArea {
            id: iconMouseArea
            anchors.fill: parent
            enabled: iconButton.enabledState
            hoverEnabled: true
            cursorShape: iconButton.enabledState ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: iconButton.clicked()
        }

        onIconNameChanged: iconCanvas.requestPaint()
        onIconColorChanged: iconCanvas.requestPaint()
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
                    text: connectionStore.userName
                    onAccepted: connectWithInput()
                    onTextChanged: {
                        networkManager.userName = text
                        connectionStore.userName = text
                    }
                }
            }

            Column {
                width: parent.width
                spacing: 8

                FieldLabel {
                    text: qsTr("RSA Key")
                }

                Row {
                    id: keySelectionRow
                    width: parent.width
                    spacing: 12

                    ComboBox {
                        id: keyComboBox
                        width: Math.max(120, keySelectionRow.width - manageKeysButton.width - keySelectionRow.spacing)
                        height: 44
                        model: connectionStore.availableKeyNames
                        currentIndex: -1

                        background: Rectangle {
                            radius: 8
                            color: "#ffffff"
                            border.color: keyComboBox.activeFocus ? "#205493" : "#c8d0d9"
                        }

                        contentItem: Text {
                            leftPadding: 11
                            rightPadding: 36
                            text: keyComboBox.displayText
                            color: "#1f2933"
                            font.pixelSize: 15
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }

                        indicator: Canvas {
                            x: keyComboBox.width - width - 12
                            y: (keyComboBox.height - height) / 2
                            width: 16
                            height: 16

                            onPaint: {
                                const context = getContext("2d")
                                context.clearRect(0, 0, width, height)
                                context.strokeStyle = "#43515f"
                                context.lineWidth = 2
                                context.lineCap = "round"
                                context.lineJoin = "round"
                                context.beginPath()
                                context.moveTo(4, 6)
                                context.lineTo(8, 10)
                                context.lineTo(12, 6)
                                context.stroke()
                            }
                        }

                        delegate: ItemDelegate {
                            required property string modelData
                            required property int index

                            width: keyComboBox.width - 8
                            height: 40
                            highlighted: keyComboBox.highlightedIndex === index

                            contentItem: Text {
                                text: modelData
                                color: "#1f2933"
                                font.pixelSize: 15
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }

                            background: Rectangle {
                                radius: 6
                                color: parent.highlighted ? "#eef4fb" : "#ffffff"
                            }
                        }

                        popup: Popup {
                            y: keyComboBox.height + 4
                            width: keyComboBox.width
                            implicitHeight: Math.min(contentItem.implicitHeight + 8, 220)
                            padding: 4

                            background: Rectangle {
                                radius: 8
                                color: "#ffffff"
                                border.color: "#c8d0d9"
                            }

                            contentItem: ListView {
                                clip: true
                                implicitHeight: contentHeight
                                model: keyComboBox.popup.visible ? keyComboBox.delegateModel : null
                                currentIndex: keyComboBox.highlightedIndex

                                ScrollBar.vertical: ScrollBar {
                                    policy: ScrollBar.AsNeeded
                                }
                            }
                        }

                        onActivated: function(index) {
                            connectionStore.selectedKeyName = textAt(index)
                        }

                        Component.onCompleted: syncKeyComboBox()
                    }

                    IconButton {
                        id: manageKeysButton
                        iconName: "edit"
                        onClicked: keyManagementPopup.open()
                    }
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
                    text: connectionStore.host
                    onAccepted: connectWithInput()
                    onTextChanged: connectionStore.host = text
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
                    text: String(connectionStore.port)
                    onAccepted: connectWithInput()
                    onTextChanged: {
                        if (acceptableInput) {
                            connectionStore.port = Number(text)
                        }
                    }
                    validator: IntValidator {
                        bottom: 1
                        top: 65535
                    }
                }
            }

            ActionButton {
                width: parent.width
                text: networkManager.busy ? qsTr("Authenticating ...") : qsTr("Connect")
                enabledState: connectionInputValid && !networkManager.busy
                onClicked: connectWithInput()
            }
        }
    }

    Popup {
        id: keyManagementPopup
        width: Math.min(476, parent.width - 64)
        height: Math.min(446, parent.height - 64)
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2
        modal: true
        focus: true
        padding: 8
        closePolicy: connectionStore.keyGenerationInProgress
                     ? Popup.NoAutoClose
                     : Popup.CloseOnEscape | Popup.CloseOnPressOutside
        transformOrigin: Item.Center

        enter: Transition {
            NumberAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: 140
                easing.type: Easing.OutCubic
            }

            NumberAnimation {
                property: "scale"
                from: 0.98
                to: 1
                duration: 140
                easing.type: Easing.OutCubic
            }
        }

        exit: Transition {
            NumberAnimation {
                property: "opacity"
                from: 1
                to: 0
                duration: 100
                easing.type: Easing.InCubic
            }

            NumberAnimation {
                property: "scale"
                from: 1
                to: 0.98
                duration: 100
                easing.type: Easing.InCubic
            }
        }

        function submit() {
            if (newKeyNameInput.text.trim().length === 0) {
                return
            }

            if (connectionStore.startKeyPairCreation(newKeyNameInput.text)) {
                newKeyNameInput.text = ""
            }
        }

        onOpened: {
            connectionStore.clearErrorText()
            pendingDeleteKeyName = ""
            newKeyNameInput.text = ""
            newKeyNameInput.forceActiveFocus()
        }

        background: Rectangle {
            anchors {
                fill: parent
                margins: 8
            }
            radius: 8
            color: "#ffffff"
            border.color: "#c8d0d9"
        }

        Overlay.modal: Rectangle {
            color: "#1f2933"
            opacity: keyManagementPopup.visible ? 0.28 : 0

            Behavior on opacity {
                NumberAnimation {
                    duration: 140
                    easing.type: Easing.OutCubic
                }
            }
        }

        contentItem: Item {
            anchors.fill: parent

            Column {
                anchors {
                    fill: parent
                    margins: 20
                }
                spacing: 16

                Row {
                    width: parent.width
                    height: 32
                    spacing: 12

                    Text {
                        width: Math.max(120, parent.width - closeKeyManagementButton.width - parent.spacing)
                        text: qsTr("RSA Keys")
                        color: "#1f2933"
                        font.pixelSize: 22
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }

                    IconButton {
                        id: closeKeyManagementButton
                        width: 32
                        height: 32
                        iconName: "close"
                        enabledState: !connectionStore.keyGenerationInProgress
                        activeColor: "#d7dde5"
                        iconColor: "#43515f"
                        onClicked: keyManagementPopup.close()
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: "#e3e8ef"
                }

                Item {
                    width: parent.width
                    height: Math.max(92, parent.height - 32 - 1
                                     - (connectionStore.keyGenerationInProgress
                                        ? keyGenerationSection.height : createKeySection.height)
                                     - errorMessage.height - parent.spacing * 4)

                    Text {
                        anchors.centerIn: parent
                        width: parent.width
                        text: qsTr("No RSA keys available yet.")
                        color: "#607080"
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                        visible: connectionStore.availableKeyNames.length === 0
                    }

                    ListView {
                        id: keysListView
                        anchors.fill: parent
                        visible: connectionStore.availableKeyNames.length > 0
                        clip: true
                        spacing: 8
                        model: connectionStore.availableKeyNames

                        delegate: Rectangle {
                            required property string modelData

                            width: keysListView.width
                            height: 48
                            radius: 6
                            color: "#f7f8fa"
                            border.color: "#d7dde5"

                            Text {
                                anchors {
                                    left: parent.left
                                    right: deleteKeyButton.left
                                    verticalCenter: parent.verticalCenter
                                    leftMargin: 12
                                    rightMargin: 12
                                }
                                text: modelData
                                color: "#1f2933"
                                font.pixelSize: 15
                                elide: Text.ElideRight
                            }

                            Rectangle {
                                id: deleteKeyButton
                                anchors {
                                    right: parent.right
                                    verticalCenter: parent.verticalCenter
                                    rightMargin: 8
                                }
                                width: 76
                                height: 32
                                radius: 6
                                color: deleteKeyMouseArea.containsMouse ? "#ead8da" : "#f3e7e8"
                                border.color: "#ddb9bd"

                                Text {
                                    anchors.centerIn: parent
                                    text: qsTr("Delete")
                                    color: "#7a2830"
                                    font.pixelSize: 13
                                }

                                MouseArea {
                                    id: deleteKeyMouseArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    enabled: !connectionStore.keyGenerationInProgress
                                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                    onClicked: {
                                        connectionStore.clearErrorText()
                                        pendingDeleteKeyName = modelData
                                        deleteKeyConfirmationPopup.open()
                                    }
                                }
                            }
                        }

                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                        }
                    }
                }

                Column {
                    id: createKeySection
                    width: parent.width
                    spacing: 8
                    visible: !connectionStore.keyGenerationInProgress

                    FieldLabel {
                        text: qsTr("New Key")
                    }

                    Row {
                        width: parent.width
                        spacing: 12

                        TextInputBox {
                            id: newKeyNameInput
                            width: Math.max(120, parent.width - createManagedKeyButton.width - parent.spacing)
                            onAccepted: keyManagementPopup.submit()
                        }

                        ActionButton {
                            id: createManagedKeyButton
                            width: 112
                            text: qsTr("Create")
                            enabledState: newKeyNameInput.text.trim().length > 0
                            onClicked: keyManagementPopup.submit()
                        }
                    }
                }

                Column {
                    id: keyGenerationSection
                    width: parent.width
                    spacing: 10
                    visible: connectionStore.keyGenerationInProgress

                    FieldLabel {
                        width: parent.width
                        text: qsTr("Generating key ...")
                        elide: Text.ElideRight
                    }

                    Canvas {
                        id: progressTrack
                        width: parent.width
                        height: 12
                        antialiasing: true
                        property real barX: -width * 0.32

                        onBarXChanged: requestPaint()
                        onWidthChanged: requestPaint()
                        onHeightChanged: requestPaint()

                        onPaint: {
                            const context = getContext("2d")
                            const radius = height / 2
                            const barWidth = width * 0.32
                            context.reset()

                            context.beginPath()
                            context.roundedRect(0, 0, width, height, radius, radius)
                            context.fillStyle = "#e3e8ef"
                            context.fill()
                            context.clip()

                            context.beginPath()
                            context.roundedRect(barX, 0, barWidth, height, radius, radius)
                            context.fillStyle = "#205493"
                            context.fill()
                        }

                        NumberAnimation on barX {
                            running: connectionStore.keyGenerationInProgress
                            loops: Animation.Infinite
                            from: -progressTrack.width * 0.32
                            to: progressTrack.width
                            duration: 1100
                            easing.type: Easing.InOutCubic
                        }
                    }

                    Item {
                        width: parent.width
                        height: 44

                        QuietButton {
                            id: cancelKeyGenerationButton
                            anchors.right: parent.right
                            width: 112
                            text: qsTr("Cancel")
                            normalColor: "#ffffff"
                            hoverColor: "#f7f8fa"
                            borderColor: "#c8d0d9"
                            textColor: "#43515f"
                            onClicked: connectionStore.cancelKeyPairCreation()
                        }
                    }
                }

                Text {
                    id: errorMessage
                    width: parent.width
                    height: visible ? paintedHeight : 0
                    text: connectionStore.errorText
                    color: "#7a2830"
                    font.pixelSize: 13
                    wrapMode: Text.Wrap
                    visible: text.length > 0
                }
            }
        }
    }

    Popup {
        id: deleteKeyConfirmationPopup
        width: Math.min(396, parent.width - 64)
        height: Math.min(276, parent.height - 64)
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2
        modal: true
        focus: true
        padding: 8
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        transformOrigin: Item.Center

        enter: Transition {
            NumberAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: 120
                easing.type: Easing.OutCubic
            }

            NumberAnimation {
                property: "scale"
                from: 0.98
                to: 1
                duration: 120
                easing.type: Easing.OutCubic
            }
        }

        exit: Transition {
            NumberAnimation {
                property: "opacity"
                from: 1
                to: 0
                duration: 90
                easing.type: Easing.InCubic
            }

            NumberAnimation {
                property: "scale"
                from: 1
                to: 0.98
                duration: 90
                easing.type: Easing.InCubic
            }
        }

        background: Rectangle {
            anchors {
                fill: parent
                margins: 8
            }
            radius: 8
            color: "#ffffff"
            border.color: "#c8d0d9"
        }

        Overlay.modal: Rectangle {
            color: "#1f2933"
            opacity: deleteKeyConfirmationPopup.visible ? 0.34 : 0

            Behavior on opacity {
                NumberAnimation {
                    duration: 120
                    easing.type: Easing.OutCubic
                }
            }
        }

        onClosed: {
            if (!visible) {
                pendingDeleteKeyName = ""
            }
        }

        contentItem: Item {
            anchors.fill: parent

            Column {
                anchors {
                    fill: parent
                    margins: 20
                }
                spacing: 14

                Text {
                    width: parent.width
                    text: qsTr("Delete Key")
                    color: "#1f2933"
                    font.pixelSize: 22
                    elide: Text.ElideRight
                }

                Text {
                    width: parent.width
                    text: qsTr("The RSA key \"%1\" cannot be recovered. After deletion, connecting to the server as this user will no longer be possible.").arg(pendingDeleteKeyName)
                    color: "#43515f"
                    font.pixelSize: 14
                    wrapMode: Text.Wrap
                }

                Text {
                    width: parent.width
                    height: visible ? paintedHeight : 0
                    text: connectionStore.errorText
                    color: "#7a2830"
                    font.pixelSize: 13
                    wrapMode: Text.Wrap
                    visible: text.length > 0
                }

                Item {
                    width: parent.width
                    height: Math.max(0, parent.height - 22 - 56 - 44 - parent.spacing * 3)
                }

                Row {
                    width: parent.width
                    spacing: 12

                    QuietButton {
                        width: Math.max(120, (parent.width - parent.spacing) / 2)
                        text: qsTr("Cancel")
                        normalColor: "#ffffff"
                        hoverColor: "#f7f8fa"
                        borderColor: "#c8d0d9"
                        textColor: "#43515f"
                        onClicked: deleteKeyConfirmationPopup.close()
                    }

                    QuietButton {
                        width: Math.max(120, (parent.width - parent.spacing) / 2)
                        text: qsTr("Delete")
                        normalColor: "#f3e7e8"
                        hoverColor: "#ead8da"
                        borderColor: "#ddb9bd"
                        textColor: "#7a2830"
                        enabledState: pendingDeleteKeyName.length > 0
                        onClicked: {
                            if (connectionStore.deleteKeyPair(pendingDeleteKeyName)) {
                                deleteKeyConfirmationPopup.close()
                            }
                        }
                    }
                }
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
            radius: 12
            color: "#f0f3f7"
            border.color: "#d7dde5"
            clip: true

            Text {
                anchors.centerIn: parent
                text: qsTr("No messages yet")
                color: "#7b8794"
                font.pixelSize: 15
                visible: chatMessages.count === 0
            }

            ListView {
                id: messageList
                anchors {
                    left: parent.left
                    right: parent.right
                    bottom: parent.bottom
                    margins: 16
                }
                height: Math.min(parent.height - 32, contentHeight)
                clip: true
                spacing: 4
                model: chatMessages
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                delegate: Item {
                    required property string senderName
                    required property string messageText
                    required property string sentAt
                    required property bool ownMessage
                    required property bool systemMessage

                    width: messageList.width
                    height: messageBubble.height + 8

                    Rectangle {
                        id: messageBubble
                        anchors {
                            right: ownMessage ? parent.right : undefined
                            left: ownMessage ? undefined : parent.left
                        }
                        width: Math.min(parent.width * 0.72,
                                        Math.max(messageBody.implicitWidth,
                                                 ownMessage ? 0 : senderLabel.implicitWidth) + 28)
                        height: messageContent.implicitHeight + 20
                        radius: 14
                        color: systemMessage ? "#fff4d6"
                                             : ownMessage ? "#205493" : "#ffffff"
                        border.color: systemMessage ? "#ead59a"
                                                   : ownMessage ? "#205493" : "#d7dde5"

                        Column {
                            id: messageContent
                            anchors {
                                fill: parent
                                margins: 10
                                leftMargin: 14
                                rightMargin: 14
                            }
                            spacing: 4

                            Text {
                                id: senderLabel
                                width: parent.width
                                text: senderName
                                color: systemMessage ? "#806000"
                                                     : ownMessage ? "#dcecff" : "#607080"
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                visible: !ownMessage
                                wrapMode: Text.Wrap
                            }

                            Text {
                                id: messageBody
                                width: parent.width
                                text: messageText
                                color: ownMessage ? "#ffffff" : "#1f2933"
                                font.pixelSize: 15
                                wrapMode: Text.Wrap
                            }
                        }
                    }
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
                text: qsTr("Send")
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
                chatMessages.clear()
            }
        }

        function onMessageReceived(senderName, text, sentAt) {
            chatMessages.append({
                "senderName": senderName,
                "messageText": text,
                "sentAt": sentAt,
                "ownMessage": senderName === networkManager.userName,
                "systemMessage": false
            })
            Qt.callLater(function() {
                messageList.positionViewAtEnd()
            })
        }

        function onConnectionError(message) {
            if (!networkManager.connected) {
                return
            }
            chatMessages.append({
                "senderName": qsTr("System"),
                "messageText": message,
                "sentAt": "",
                "ownMessage": false,
                "systemMessage": true
            })
            Qt.callLater(function() {
                messageList.positionViewAtEnd()
            })
        }
    }

    Connections {
        target: connectionStore

        function onAvailableKeyNamesChanged() {
            syncKeyComboBox()
        }

        function onSelectedKeyNameChanged() {
            syncKeyComboBox()
        }
    }
}
