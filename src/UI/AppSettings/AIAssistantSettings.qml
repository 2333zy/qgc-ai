/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FactControls
import QGroundControl.FactSystem
import QGroundControl.Palette
import QGroundControl.ScreenTools

SettingsPage {
    property var  _aiAssistantSettings: QGroundControl.settingsManager.aiAssistantSettings
    property real _fieldWidth:        ScreenTools.defaultFontPixelWidth * 42

    QGCPalette {
        id: qgcPal
    }

    SettingsGroupLayout {
        Layout.fillWidth:   true
        heading:            qsTr("AI Assistant")
        headingDescription: qsTr("The PX4 AI flight diagnostic assistant sends active vehicle status and any console evidence you explicitly attach to the selected OpenAI-compatible endpoint.")

        LabelledFactTextField {
            Layout.fillWidth:           true
            textFieldPreferredWidth:    _fieldWidth
            label:                      qsTr("Endpoint URL")
            fact:                       _aiAssistantSettings.endpointUrl
            textField.placeholderText:  qsTr("e.g. https://api.deepseek.com/v1/chat/completions")
        }

        LabelledFactTextField {
            Layout.fillWidth:           true
            textFieldPreferredWidth:    _fieldWidth
            label:                      qsTr("Model")
            fact:                       _aiAssistantSettings.modelName
            textField.placeholderText:  qsTr("e.g. deepseek-chat")
        }

        LabelledFactTextField {
            Layout.fillWidth:           true
            textFieldPreferredWidth:    _fieldWidth
            label:                      qsTr("API key (optional)")
            fact:                       _aiAssistantSettings.apiKey
            textField.echoMode:         TextInput.Password
        }

        QGCLabel {
            Layout.fillWidth: true
            wrapMode:         Text.WordWrap
            text:             qsTr("Any OpenAI-compatible chat completions endpoint works. The model must support structured tool calls for full diagnostics; endpoints without tool support still answer from the vehicle snapshot.")
        }
    }
}
