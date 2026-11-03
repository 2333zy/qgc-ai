/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "AIProviderPreset.h"

#include <QtCore/QUrl>

QString AIProviderPreset::displayName(Preset preset)
{
    switch (preset) {
    case DeepSeek:      return QStringLiteral("DeepSeek");
    case OpenAI:        return QStringLiteral("OpenAI");
    case Ollama:        return QStringLiteral("Ollama");
    case QwenDashScope: return QStringLiteral("Qwen (DashScope)");
    case Custom:        return QStringLiteral("Custom");
    default:            return QString();
    }
}

QString AIProviderPreset::baseUrl(Preset preset)
{
    switch (preset) {
    case DeepSeek:      return QStringLiteral("https://api.deepseek.com");
    case OpenAI:        return QStringLiteral("https://api.openai.com");
    case Ollama:        return QStringLiteral("http://127.0.0.1:11434");
    case QwenDashScope: return QStringLiteral("https://dashscope.aliyuncs.com/compatible-mode");
    case Custom:        return QString();
    default:            return QString();
    }
}

QString AIProviderPreset::defaultModel(Preset preset)
{
    switch (preset) {
    case DeepSeek:      return QStringLiteral("deepseek-chat");
    case OpenAI:        return QStringLiteral("gpt-4o-mini");
    case Ollama:        return QStringLiteral("qwen2.5:7b");
    case QwenDashScope: return QStringLiteral("qwen-plus");
    case Custom:        return QString();
    default:            return QString();
    }
}

QString AIProviderPreset::endpointUrl(Preset preset)
{
    return normalizedEndpointUrl(baseUrl(preset));
}

QString AIProviderPreset::normalizedEndpointUrl(const QString &rawUrl)
{
    const QString trimmed = rawUrl.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }

    QUrl url = QUrl::fromUserInput(trimmed);
    if (!url.isValid() || url.host().isEmpty()) {
        return trimmed;
    }

    QString path = url.path();
    while (path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }

    if (path.endsWith(QStringLiteral("/chat/completions"))) {
        // Already a complete chat completions route.
    } else if (path.endsWith(QStringLiteral("/v1"))) {
        path += QStringLiteral("/chat/completions");
    } else {
        path += QStringLiteral("/v1/chat/completions");
    }

    url.setPath(path);
    return url.toString();
}
