/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "AIDiagnosticControllerTest.h"
#include "AIAssistantSettings.h"
#include "AIDiagnosticContextBuilder.h"
#include "AIDiagnosticController.h"
#include "AIProviderPreset.h"
#include "PX4DiagnosticEvidence.h"
#include "PX4DiagnosticProvider.h"
#include "QGCMAVLink.h"
#include "SettingsManager.h"
#include "Vehicle.h"

#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

namespace {

QJsonObject externalVision(const QJsonObject &evidence)
{
    return evidence.value(QStringLiteral("externalVision")).toObject();
}

}

void AIDiagnosticControllerTest::_assistantPagesLoad_test()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/qml"));
    const QStringList pageUrls{
        QStringLiteral("qrc:/qml/QGroundControl/AppSettings/AIAssistantSettings.qml"),
        QStringLiteral("qrc:/qml/QGroundControl/AnalyzeView/AIDiagnosticPage.qml"),
        QStringLiteral("qrc:/qml/QGroundControl/AnalyzeView/MAVLinkConsolePage.qml")
    };

    for (const QString &pageUrl : pageUrls) {
        QQmlComponent component(&engine, QUrl(pageUrl));
        QVERIFY2(component.isReady(), qPrintable(QStringLiteral("%1: %2").arg(pageUrl, component.errorString())));
        QScopedPointer<QObject> page(component.create());
        QVERIFY2(page, qPrintable(QStringLiteral("%1: %2").arg(pageUrl, component.errorString())));
    }
}

void AIDiagnosticControllerTest::_settingsCompatibilityAlias_test()
{
    QCOMPARE(QString::fromLatin1(AIAssistantSettings::settingsGroup), QStringLiteral("AIConsole"));
    SettingsManager *settingsManager = SettingsManager::instance();
    QVERIFY(settingsManager->aiAssistantSettings());
    QCOMPARE(settingsManager->aiAssistantSettings(), settingsManager->aiConsoleSettings());
}

void AIDiagnosticControllerTest::_contextSchemaWithoutVehicle_test()
{
    const QJsonObject context = AIDiagnosticContextBuilder::buildContext(
        nullptr,
        QJsonObject{{QStringLiteral("provider"), QStringLiteral("px4")}},
        QStringLiteral("listener vehicle_status 1"),
        true);

    QCOMPARE(context.value(QStringLiteral("schemaVersion")).toInt(), 1);
    QCOMPARE(context.value(QStringLiteral("source")).toString(), QStringLiteral("QGroundControl AI Flight Diagnostics"));
    QVERIFY(!context.value(QStringLiteral("timestampUtc")).toString().isEmpty());
    QCOMPARE(context.value(QStringLiteral("activeVehicle")).toBool(), false);
    QCOMPARE(context.value(QStringLiteral("firmware")).toObject().value(QStringLiteral("supported")).toBool(), false);
    QVERIFY(context.contains(QStringLiteral("core")));
    QVERIFY(context.contains(QStringLiteral("sysStatusSensorInfo")));
    QVERIFY(context.contains(QStringLiteral("healthAndArmingCheckReport")));
    QVERIFY(context.contains(QStringLiteral("linkStatus")));
    QVERIFY(context.contains(QStringLiteral("factGroups")));
    QVERIFY(context.contains(QStringLiteral("batteries")));
    QVERIFY(context.contains(QStringLiteral("px4DiagnosticEvidence")));
    const QJsonObject attachment = context.value(QStringLiteral("consoleAttachment")).toObject();
    QCOMPARE(attachment.value(QStringLiteral("available")).toBool(), true);
    QCOMPARE(attachment.value(QStringLiteral("truncated")).toBool(), true);
    QCOMPARE(attachment.value(QStringLiteral("text")).toString(), QStringLiteral("listener vehicle_status 1"));
}

void AIDiagnosticControllerTest::_responseLanguagePolicy_test()
{
    AIDiagnosticController controller;
    const QString policy = controller._responseLanguagePolicy();

    QVERIFY(policy.contains(QStringLiteral("latest user question")));
    QVERIFY(policy.contains(QStringLiteral("Reply in Chinese")));
    QVERIFY(policy.contains(QStringLiteral("reply in English")));
    QVERIFY(policy.contains(QStringLiteral("explicit request")));
    QVERIFY(policy.contains(QStringLiteral("dominant natural language")));
    QVERIFY(policy.contains(QStringLiteral("ignoring PX4 identifiers")));
    QVERIFY(policy.contains(QStringLiteral("QGroundControl UI locale")));
    QVERIFY(policy.contains(QStringLiteral("chat history")));
    QVERIFY(policy.contains(QStringLiteral("account-level preferences")));

    const QString standardPrompt = controller._standardSystemPrompt();
    QCOMPARE(standardPrompt.count(policy), 1);
    QVERIFY(!standardPrompt.contains(QStringLiteral("Answer in the user's language")));
}

void AIDiagnosticControllerTest::_providerPresetAndEndpointNormalization_test()
{
    // A bare base URL must be completed to the chat completions route: pasting only
    // the host used to produce an opaque HTTP failure instead of a usable request.
    QCOMPARE(AIProviderPreset::normalizedEndpointUrl(QStringLiteral("https://api.deepseek.com")),
             QStringLiteral("https://api.deepseek.com/v1/chat/completions"));
    QCOMPARE(AIProviderPreset::normalizedEndpointUrl(QStringLiteral("https://api.deepseek.com/")),
             QStringLiteral("https://api.deepseek.com/v1/chat/completions"));
    QCOMPARE(AIProviderPreset::normalizedEndpointUrl(QStringLiteral("  https://api.deepseek.com  ")),
             QStringLiteral("https://api.deepseek.com/v1/chat/completions"));
    QCOMPARE(AIProviderPreset::normalizedEndpointUrl(QStringLiteral("https://host/v1")),
             QStringLiteral("https://host/v1/chat/completions"));
    QCOMPARE(AIProviderPreset::normalizedEndpointUrl(QStringLiteral("https://dashscope.aliyuncs.com/compatible-mode")),
             QStringLiteral("https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions"));

    // A complete route is left alone.
    QCOMPARE(AIProviderPreset::normalizedEndpointUrl(QStringLiteral("https://host/v1/chat/completions")),
             QStringLiteral("https://host/v1/chat/completions"));
    QCOMPARE(AIProviderPreset::normalizedEndpointUrl(QStringLiteral("https://host/v1/chat/completions/")),
             QStringLiteral("https://host/v1/chat/completions"));

    QVERIFY(AIProviderPreset::normalizedEndpointUrl(QString()).isEmpty());
    QVERIFY(AIProviderPreset::normalizedEndpointUrl(QStringLiteral("   ")).isEmpty());

    // Presets carry a base URL, a route and a suggested model.
    QCOMPARE(AIProviderPreset::endpointUrl(AIProviderPreset::DeepSeek),
             QStringLiteral("https://api.deepseek.com/v1/chat/completions"));
    QCOMPARE(AIProviderPreset::defaultModel(AIProviderPreset::DeepSeek), QStringLiteral("deepseek-chat"));
    QCOMPARE(AIProviderPreset::endpointUrl(AIProviderPreset::Ollama),
             QStringLiteral("http://127.0.0.1:11434/v1/chat/completions"));
    QCOMPARE(AIProviderPreset::endpointUrl(AIProviderPreset::QwenDashScope),
             QStringLiteral("https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions"));

    // Custom must never supply a value: it means the user owns the fields.
    QVERIFY(AIProviderPreset::endpointUrl(AIProviderPreset::Custom).isEmpty());
    QVERIFY(AIProviderPreset::defaultModel(AIProviderPreset::Custom).isEmpty());
    for (int i = 0; i < AIProviderPreset::PresetCount; i++) {
        QVERIFY(!AIProviderPreset::displayName(static_cast<AIProviderPreset::Preset>(i)).isEmpty());
    }
}

void AIDiagnosticControllerTest::_streamingDeltaAssembly_test()
{
    AIDiagnosticController controller;
    QSignalSpy deltaSpy(&controller, &AIDiagnosticController::answerDelta);

    // Content deltas accumulate and are surfaced to the page as they arrive.
    controller._resetStreamState();
    controller._handleSseLine(QByteArrayLiteral("data: {\"choices\":[{\"delta\":{\"content\":\"Hel\"}}]}"));
    controller._handleSseLine(QByteArrayLiteral("data: {\"choices\":[{\"delta\":{\"content\":\"lo\"}}]}"));
    // Keep-alive comments, unknown fields and the terminator must not add content.
    controller._handleSseLine(QByteArrayLiteral(": ping"));
    controller._handleSseLine(QByteArrayLiteral("event: message"));
    controller._handleSseLine(QByteArrayLiteral(""));
    controller._handleSseLine(QByteArrayLiteral("data: [DONE]"));
    QCOMPARE(controller._streamContent, QStringLiteral("Hello"));
    QCOMPARE(deltaSpy.count(), 2);
    QVERIFY(controller._streamDetected);
    QVERIFY(controller._streamToolCalls.isEmpty());

    // Tool calls arrive as fragments and must be concatenated per index.
    controller._resetStreamState();
    controller._handleSseLine(QByteArrayLiteral(
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call_1\",\"type\":\"function\",\"function\":{\"name\":\"get_battery\"}}]}}]}"));
    controller._handleSseLine(QByteArrayLiteral(
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":\"{\\\"param\\\":\"}}]}}]}"));
    controller._handleSseLine(QByteArrayLiteral(
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":\"\\\"BATT_CAPACITY\\\"}\"}}]}}]}"));

    QCOMPARE(controller._streamToolCalls.size(), 1);
    const QJsonObject toolCall = controller._streamToolCalls.at(0).toObject();
    QCOMPARE(toolCall.value(QStringLiteral("id")).toString(), QStringLiteral("call_1"));
    QCOMPARE(toolCall.value(QStringLiteral("type")).toString(), QStringLiteral("function"));
    const QJsonObject function = toolCall.value(QStringLiteral("function")).toObject();
    QCOMPARE(function.value(QStringLiteral("name")).toString(), QStringLiteral("get_battery"));
    QCOMPARE(function.value(QStringLiteral("arguments")).toString(), QStringLiteral("{\"param\":\"BATT_CAPACITY\"}"));

    // Two concurrent tool calls keep separate accumulation slots.
    controller._resetStreamState();
    controller._handleSseLine(QByteArrayLiteral(
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"a\",\"function\":{\"name\":\"first\",\"arguments\":\"{}\"}}]}}]}"));
    controller._handleSseLine(QByteArrayLiteral(
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":1,\"id\":\"b\",\"function\":{\"name\":\"second\",\"arguments\":\"{}\"}}]}}]}"));
    QCOMPARE(controller._streamToolCalls.size(), 2);
    QCOMPARE(controller._streamToolCalls.at(1).toObject().value(QStringLiteral("function")).toObject()
                 .value(QStringLiteral("name")).toString(), QStringLiteral("second"));

    // An error object inside the stream is captured rather than silently dropped.
    controller._resetStreamState();
    controller._handleSseLine(QByteArrayLiteral("data: {\"error\":{\"message\":\"quota exceeded\"}}"));
    QCOMPARE(controller._streamErrorText, QStringLiteral("quota exceeded"));

    // Malformed JSON is ignored instead of aborting the stream.
    controller._resetStreamState();
    controller._handleSseLine(QByteArrayLiteral("data: {not json"));
    QVERIFY(controller._streamContent.isEmpty());
    QVERIFY(controller._streamToolCalls.isEmpty());
    QVERIFY(controller._streamErrorText.isEmpty());
}

void AIDiagnosticControllerTest::_px4ProviderBoundaryAndPolicy_test()
{
    PX4DiagnosticProvider provider;
    Vehicle px4Vehicle(MAV_AUTOPILOT_PX4, MAV_TYPE_QUADROTOR);
    Vehicle ardupilotVehicle(MAV_AUTOPILOT_ARDUPILOTMEGA, MAV_TYPE_QUADROTOR);

    QVERIFY(provider.supportsVehicle(&px4Vehicle));
    QVERIFY(!provider.supportsVehicle(&ardupilotVehicle));
    QVERIFY(provider.unsupportedReason(&ardupilotVehicle).contains(QStringLiteral("PX4")));
    QVERIFY(!provider.supportsVehicle(nullptr));

    QVERIFY(provider.isSafeParameterName(QStringLiteral("EKF2_EV_CTRL")));
    QVERIFY(!provider.isSafeParameterName(QStringLiteral("EKF2_EV_CTRL; reboot")));
    QVERIFY(provider.isWhitelistedConsoleCommand(QStringLiteral("param show EKF2_EV_CTRL")));
    QVERIFY(!provider.isWhitelistedConsoleCommand(QStringLiteral("param set EKF2_EV_CTRL 15")));
    QVERIFY(!provider.isWhitelistedConsoleCommand(QStringLiteral("reboot")));
    QVERIFY(provider.isSafeMavlinkMessageId(MAVLINK_MSG_ID_SYS_STATUS));
    QVERIFY(!provider.isSafeMavlinkMessageId(MAVLINK_MSG_ID_COMMAND_LONG));

    const QList<PX4DiagnosticProvider::AutomaticToolRequest> visionRequests =
        provider.automaticToolsForQuestion(QStringLiteral("检查外部视觉 VIO 是否输入并融合"), 3);
    QCOMPARE(visionRequests.size(), 3);
    QCOMPARE(visionRequests.at(0).kind, PX4DiagnosticProvider::AutomaticToolRequest::Kind::SensorStatus);
    QCOMPARE(visionRequests.at(1).kind, PX4DiagnosticProvider::AutomaticToolRequest::Kind::SensorStatus);
    QCOMPARE(visionRequests.at(2).kind, PX4DiagnosticProvider::AutomaticToolRequest::Kind::Parameter);
    QCOMPARE(visionRequests.at(2).value, QStringLiteral("EKF2_EV_CTRL"));
}

void AIDiagnosticControllerTest::_vehicleSwitchAndStaleEventIsolation_test()
{
    AIDiagnosticController controller;
    Vehicle firstVehicle(MAV_AUTOPILOT_PX4, MAV_TYPE_QUADROTOR);
    Vehicle secondVehicle(MAV_AUTOPILOT_PX4, MAV_TYPE_QUADROTOR);
    QSignalSpy clearedSpy(&controller, &AIDiagnosticController::conversationCleared);

    controller._bindRequestVehicle(&firstVehicle);
    controller._busy = true;
    const quint64 requestGeneration = controller._requestGeneration;

    controller._activeVehicleChanged(&secondVehicle);

    QVERIFY(!controller.busy());
    QVERIFY(controller._requestVehicle.isNull());
    QVERIFY(controller._requestGeneration > requestGeneration);
    QVERIFY(!controller.pendingActionAvailable());
    QCOMPARE(clearedSpy.count(), 1);

    controller.clearConversation();
}

void AIDiagnosticControllerTest::_externalVisionInputPresentButNotConfigured_test()
{
    PX4ExternalVisionSnapshot snapshot;
    snapshot.sysStatusAvailable = true;
    snapshot.sysStatusPresent = true;
    snapshot.parameterManagerReady = true;
    snapshot.evCtrlAvailable = true;
    snapshot.evCtrl = 0;

    const QJsonObject snapshotEvidence = PX4DiagnosticEvidence::externalVisionSnapshot(snapshot);
    const QJsonObject configuration = externalVision(snapshotEvidence).value(QStringLiteral("configuration")).toObject();
    QCOMPARE(configuration.value(QStringLiteral("state")).toString(), QStringLiteral("confirmed"));
    const QJsonObject configurationObservations = configuration.value(QStringLiteral("observations")).toObject();
    QCOMPARE(configurationObservations.value(QStringLiteral("ekf2EvCtrl")).toInt(), 0);
    QCOMPARE(configurationObservations.value(QStringLiteral("horizontalPositionFusionEnabled")).toBool(), false);

    const QString output = QStringLiteral(
        "TOPIC: vehicle_visual_odometry\n"
        " vehicle_visual_odometry\n"
        "\ttimestamp: 209174792 (0.082695 seconds ago)\n"
        "\tposition: [-0.00160, -0.00052, 0.00079]\n"
        "\tq: [0.99999, -0.00236, -0.00163, -0.00040]\n"
        "\tvelocity: [nan, nan, nan]\n");
    const QJsonObject consoleEvidence = PX4DiagnosticEvidence::externalVisionConsoleEvidence(
        PX4DiagnosticEvidence::externalVisionConsoleCommands().at(0), output, false);
    const QJsonObject input = externalVision(consoleEvidence).value(QStringLiteral("input")).toObject();
    QCOMPARE(input.value(QStringLiteral("state")).toString(), QStringLiteral("confirmed"));
    const QJsonObject inputObservations = input.value(QStringLiteral("observations")).toObject();
    QCOMPARE(inputObservations.value(QStringLiteral("positionState")).toString(), QStringLiteral("confirmed"));
    QCOMPARE(inputObservations.value(QStringLiteral("quaternionState")).toString(), QStringLiteral("confirmed"));
    QCOMPARE(inputObservations.value(QStringLiteral("velocityProvided")).toBool(), true);
    QCOMPARE(inputObservations.value(QStringLiteral("velocityState")).toString(), QStringLiteral("invalid"));
    QVERIFY(!inputObservations.contains(QStringLiteral("rateHz")));
}

void AIDiagnosticControllerTest::_externalVisionFusionControl_test()
{
    const QString output = QStringLiteral(
        "TOPIC: estimator_status_flags\n"
        " estimator_status_flags\n"
        "\tcs_ev_pos: true\n"
        "\tcs_ev_vel: false\n"
        "\tcs_ev_yaw: true\n"
        "\tcs_ev_hgt: false\n");
    const QJsonObject evidence = PX4DiagnosticEvidence::externalVisionConsoleEvidence(
        PX4DiagnosticEvidence::externalVisionConsoleCommands().at(1), output, false);
    const QJsonObject fusion = externalVision(evidence).value(QStringLiteral("fusionControl")).toObject();
    QCOMPARE(fusion.value(QStringLiteral("state")).toString(), QStringLiteral("confirmed"));
    const QJsonObject observations = fusion.value(QStringLiteral("observations")).toObject();
    QCOMPARE(observations.value(QStringLiteral("cs_ev_pos")).toBool(), true);
    QCOMPARE(observations.value(QStringLiteral("cs_ev_vel")).toBool(), false);
    QCOMPARE(observations.value(QStringLiteral("cs_ev_yaw")).toBool(), true);
    QCOMPARE(observations.value(QStringLiteral("cs_ev_hgt")).toBool(), false);
    QVERIFY(observations.value(QStringLiteral("semanticBoundary")).toString().contains(QStringLiteral("do not")));
}

void AIDiagnosticControllerTest::_externalVisionInvalidPayload_test()
{
    const QString output = QStringLiteral(
        "TOPIC: vehicle_visual_odometry\n"
        "\ttimestamp: 209174792 (0.082695 seconds ago)\n"
        "\tposition: [nan, nan, nan]\n"
        "\tq: [1.0, 0.0, 0.0, 0.0]\n");
    const QJsonObject evidence = PX4DiagnosticEvidence::externalVisionConsoleEvidence(
        PX4DiagnosticEvidence::externalVisionConsoleCommands().at(0), output, false);
    const QJsonObject input = externalVision(evidence).value(QStringLiteral("input")).toObject();
    QCOMPARE(input.value(QStringLiteral("state")).toString(), QStringLiteral("invalid"));
    QCOMPARE(input.value(QStringLiteral("observations")).toObject().value(QStringLiteral("positionState")).toString(), QStringLiteral("invalid"));
}

void AIDiagnosticControllerTest::_externalVisionUnavailableAndUnknown_test()
{
    const QJsonObject unavailableEvidence = PX4DiagnosticEvidence::externalVisionConsoleEvidence(
        PX4DiagnosticEvidence::externalVisionConsoleCommands().at(0),
        QStringLiteral("nsh: listener: topic vehicle_visual_odometry not found\n"), false);
    QCOMPARE(externalVision(unavailableEvidence).value(QStringLiteral("input")).toObject().value(QStringLiteral("state")).toString(), QStringLiteral("unavailable"));

    const QJsonObject unknownEvidence = PX4DiagnosticEvidence::externalVisionConsoleEvidence(
        PX4DiagnosticEvidence::externalVisionConsoleCommands().at(1),
        QStringLiteral("TOPIC: estimator_status_flags\n"), false);
    QCOMPARE(externalVision(unknownEvidence).value(QStringLiteral("fusionControl")).toObject().value(QStringLiteral("state")).toString(), QStringLiteral("unknown"));

    const QJsonObject timedOutEvidence = PX4DiagnosticEvidence::externalVisionConsoleEvidence(
        PX4DiagnosticEvidence::externalVisionConsoleCommands().at(0), QString(), false);
    QCOMPARE(externalVision(timedOutEvidence).value(QStringLiteral("input")).toObject().value(QStringLiteral("state")).toString(), QStringLiteral("unknown"));

    const QJsonObject rejectedEvidence = PX4DiagnosticEvidence::externalVisionUnavailableEvidence(
        PX4DiagnosticEvidence::externalVisionConsoleCommands().at(1), QStringLiteral("Vehicle communication is currently lost."));
    QCOMPARE(externalVision(rejectedEvidence).value(QStringLiteral("fusionControl")).toObject().value(QStringLiteral("state")).toString(), QStringLiteral("unavailable"));

    PX4ExternalVisionSnapshot snapshot;
    const QJsonObject snapshotEvidence = PX4DiagnosticEvidence::externalVisionSnapshot(snapshot);
    QCOMPARE(externalVision(snapshotEvidence).value(QStringLiteral("configuration")).toObject().value(QStringLiteral("state")).toString(), QStringLiteral("unavailable"));
}

void AIDiagnosticControllerTest::_externalVisionRoutingAndSafety_test()
{
    QVERIFY(PX4DiagnosticEvidence::isExternalVisionQuestion(QStringLiteral("有外部视觉 VIO 数据吗？")));
    QVERIFY(PX4DiagnosticEvidence::isExternalVisionQuestion(QStringLiteral("Check vehicle_visual_odometry")));
    QVERIFY(!PX4DiagnosticEvidence::isExternalVisionQuestion(QStringLiteral("GPS 是否锁定？")));

    const QStringList commands = PX4DiagnosticEvidence::externalVisionConsoleCommands();
    QCOMPARE(commands.size(), 3);
    QCOMPARE(commands.at(0), QStringLiteral("listener vehicle_visual_odometry 1"));
    QCOMPARE(commands.at(1), QStringLiteral("listener estimator_status_flags 1"));
    QCOMPARE(commands.at(2), QStringLiteral("param show EKF2_EV_CTRL"));

    QCOMPARE(static_cast<int>(PX4DiagnosticEvidence::consoleQueryAvailability(true, false, false)), static_cast<int>(PX4DiagnosticEvidence::ConsoleQueryAvailability::ArmedOrFlying));
    QCOMPARE(static_cast<int>(PX4DiagnosticEvidence::consoleQueryAvailability(false, true, false)), static_cast<int>(PX4DiagnosticEvidence::ConsoleQueryAvailability::ArmedOrFlying));
    QCOMPARE(static_cast<int>(PX4DiagnosticEvidence::consoleQueryAvailability(false, false, true)), static_cast<int>(PX4DiagnosticEvidence::ConsoleQueryAvailability::CommunicationLost));
    QCOMPARE(static_cast<int>(PX4DiagnosticEvidence::consoleQueryAvailability(false, false, false)), static_cast<int>(PX4DiagnosticEvidence::ConsoleQueryAvailability::Allowed));

    PX4ExternalVisionSnapshot snapshot;
    snapshot.sysStatusAvailable = true;
    snapshot.sysStatusEnabled = false;
    const QJsonObject snapshotEvidence = PX4DiagnosticEvidence::externalVisionSnapshot(snapshot);
    const QJsonObject sysStatus = externalVision(snapshotEvidence).value(QStringLiteral("sysStatus")).toObject();
    QVERIFY(sysStatus.value(QStringLiteral("observations")).toObject().value(QStringLiteral("semanticBoundary")).toString().contains(QStringLiteral("do not prove")));
}
