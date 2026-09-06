// TranslationPanel.qml —— 翻译结果面板（原文 / 译文，可选中复制）
import QtQuick
import QtQuick.Controls
import Frap

Rectangle {
    id: root

    property string src: ""
    property string dst: ""
    visible: src.length > 0 || dst.length > 0

    width: Math.max(column.width + 32, 220)
    height: column.height + 28
    radius: Theme.radiusLg
    color: Theme.surface
    border.color: Theme.border
    border.width: 1

    Column {
        id: column
        anchors.centerIn: parent
        spacing: 6

        Row {
            id: srcRow
            spacing: 6
            Text { text: "原文"; color: Theme.textSecondary; font: Theme.fontHint;
                anchors.verticalCenter: parent.verticalCenter }
            Text {
                text: root.src
                color: "#bbb"
                font: Theme.fontHint
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                width: Math.max(180, Math.min(460, srcRow.implicitWidth))
            }
        }
        Rectangle { width: column.width; height: 1; color: Theme.border }
        Row {
            id: dstRow
            spacing: 6
            Text { text: "译文"; color: Theme.accent; font: Theme.fontLabel;
                anchors.verticalCenter: parent.verticalCenter }
            Text {
                text: root.dst
                color: Theme.textPrimary
                font: Theme.fontBase
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                width: Math.max(180, Math.min(460, dstRow.implicitWidth))
            }
        }
    }
}
