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
import QGroundControl.Controllers
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

    // Owns the preset wiring: selecting a provider preset fills the endpoint URL and
    // model, so those fields update live while this page is open.
    AIDiagnosticController {
        id: aiSettingsController
    }

    SettingsGroupLayout {
        Layout.fillWidth:   true
        heading:            qsTr("AI Assistant")
        headingDescription: qsTr("The PX4 AI flight diagnostic assistant sends active vehicle status and any console evidence you explicitly attach to the selected OpenAI-compatible endpoint.")

        LabelledFactComboBox {
            Layout.fillWidth:           true
            comboBoxPreferredWidth:     _fieldWidth
            label:                      qsTr("Provider")
            fact:                       _aiAssistantSettings.providerPreset
            indexModel:                 false
        }

        LabelledFactTextField {
            Layout.fillWidth:           true
            textFieldPreferredWidth:    _fieldWidth
            label:                      qsTr("Endpoint URL")
            fact:                       _aiAssistantSettings.endpointUrl
            textField.placeholderText:  qsTr("Base URL or full /v1/chat/completions URL")
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
            text:             qsTr("A base URL is completed to /v1/chat/completions automatically. The model must support structured tool calls for full diagnostics; endpoints without tool support still answer from the vehicle snapshot.")
        }
    }
}
