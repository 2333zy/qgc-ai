/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QtCore/QString>

/// Provider presets for the OpenAI-compatible chat endpoint plus the URL
/// normalisation that lets a user configure just a base URL.
///
/// Every supported provider exposes the same OpenAI-compatible route, so a preset
/// is only a base URL, a suggested model and a display name. Anything beyond that
/// (auth header shape, vendor specific parameters) would be a different protocol
/// and is deliberately not modelled here.
class AIProviderPreset
{
public:
    enum Preset {
        DeepSeek = 0,
        OpenAI,
        Ollama,
        QwenDashScope,
        Custom,
        PresetCount
    };

    static QString displayName(Preset preset);

    /// Base URL of the provider, without the chat completions route. Empty for Custom.
    static QString baseUrl(Preset preset);

    /// Suggested model for the preset. Empty for Custom.
    static QString defaultModel(Preset preset);

    /// Full chat completions URL for a preset. Empty for Custom.
    static QString endpointUrl(Preset preset);

    /// Turns whatever the user typed into a full chat completions URL:
    ///   "https://api.deepseek.com"          -> ".../v1/chat/completions"
    ///   "https://host/v1"                   -> ".../v1/chat/completions"
    ///   "https://host/compatible-mode"      -> ".../compatible-mode/v1/chat/completions"
    ///   "https://host/v1/chat/completions"  -> unchanged
    /// Trailing slashes are ignored. An input that is not a usable URL is returned
    /// trimmed so the network layer can report it.
    static QString normalizedEndpointUrl(const QString &rawUrl);
};
