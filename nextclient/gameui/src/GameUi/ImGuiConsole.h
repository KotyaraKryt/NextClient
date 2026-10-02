#pragma once

#include "ImGuiPanel.h"

#include <console_buffer/command_history.h>
#include <console_buffer/selection.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class CImGuiConsole : public CImGuiPanel
{
    DECLARE_CLASS_SIMPLE(CImGuiConsole, CImGuiPanel);

public:
    explicit CImGuiConsole(console_buffer::ConsoleBuffer& scrollback);

    void Activate();

protected:
    void DrawImGui() override;

    void OnKeyCodeTyped(vgui2::KeyCode code) override;
    void OnKeyTyped(wchar_t unichar) override;

private:
    static int OnInputCallback(struct ImGuiInputTextCallbackData* data);

    void DrawToolbar();
    void DrawScrollback();
    void RebuildView();
    std::string Localized(const char* token);
    void DrawSuggestions();
    void RebuildCompletionNames();
    void UpdateSuggestions();
    void AcceptSuggestion(const std::string& name);
    const std::string& Describe(const std::string& name);
    void LoadHistory();
    void SaveHistory();
    void Execute(const char* command);
    void CloseToGame();

    console_buffer::ConsoleBuffer& m_Scrollback;
    console_buffer::CommandHistory m_History;
    char m_szInput[256] = {};
    bool m_bFocusWindow = false;
    bool m_bFocusInput = false;
    bool m_bIgnoreNextChar = false;
    // a focus given by code selects the whole input, and the next key would replace it
    bool m_bCursorToEnd = false;

    std::vector<std::string> m_CompletionNames;
    std::unordered_map<std::string, std::string> m_Descriptions;
    std::vector<std::string> m_Suggestions;
    std::string m_SuggestionsFor;
    // a command brought back with Up/Down isn't being typed, so it gets no suggestions
    std::string m_RecalledText;
    int m_iSuggestion = 0;
    float m_flInputX = 0.0f;
    float m_flInputTop = 0.0f;
    float m_flConsoleWidth = 0.0f;

    // the lines that pass the kind filters and the search, rebuilt when any of them change
    bool m_bKindVisible[console_buffer::kKindCount] = { true, true, true, true, true, true, true };
    int m_iKindCounts[console_buffer::kKindCount] = {};
    char m_szSearch[128] = {};
    console_buffer::LineView m_View;
    uint64_t m_iViewGeneration = UINT64_MAX;
    uint32_t m_iViewMask = 0;
    std::string m_ViewSearch;

    console_buffer::TextPos m_SelectionStart;
    console_buffer::TextPos m_SelectionEnd;
    bool m_bSelecting = false;
};
