# Moe-AI

AI-powered desktop mascot for Haiku OS. Chat with Claude, control your system, hear responses spoken aloud.

![Moe-AI Screenshot](documentation/img/sc1.png)

## Features

- **Speech Bubble Chat** - Double-click the mascot to open a chat bubble. Ask questions, get help, or just have a conversation
- **Claude AI Integration** - Powered by Claude API with support for Sonnet, Opus, and Haiku models
- **System Control** - Moe can control your Haiku system through Pippo MCP tools (launch apps, manage files, query system info)
- **Text-to-Speech** - Hear responses read aloud with Piper neural voices or espeak
- **Smart Positioning** - Bubble and settings windows position themselves around the mascot based on available screen space
- **Conversation Memory** - Chat history persists across sessions
- **Context Awareness** - Moe knows which app and window you're using
- **Multiple Mascots** - Included mascot collection with classic BeOS characters
- **Customizable** - Settings for AI model, voice, speed, system prompt, and mascot behavior

## Getting Started

### Build

```bash
cd source
make -j4
```

**Requirements:** Haiku OS with `netservices2`, `libnetwork`, `libbnetapi`, `libtracker`

### Install

1. Build the project
2. Run with a mascot image:
   ```bash
   objects.x86_64-cc13-release/Moe-AI ../mascots/moe.png
   ```

### Configure AI

1. Get a Claude API key from [console.anthropic.com](https://console.anthropic.com)
2. Right-click the mascot > **Settings** > **AI** tab
3. Paste your API key and click **Save**
4. Double-click the mascot to start chatting

### Text-to-Speech (optional)

For neural voice (recommended):
```bash
pkgman install pipertts espeak
```
Download an Italian voice model:
```bash
mkdir -p ~/config/non-packaged/data/pipertts/models/
cd ~/config/non-packaged/data/pipertts/models/
wget https://huggingface.co/gyroing/PiperTTS-NCNN-Models/resolve/main/it_IT-dii.zip
unzip it_IT-dii.zip
```
Then enable TTS in **Settings** > **Voice** tab.

## Usage

| Action | How |
|--------|-----|
| Open chat | Double-click mascot |
| Close chat | Escape or wait 30 seconds |
| Settings | Right-click > Settings |
| Change mascot | Settings > Mascot > Change mascot |
| Close mascot | Right-click > Close |
| Quit | Right-click > Quit |

## Included Mascots

The `mascots/` directory contains ready-to-use mascot images:

- **moe.png** - Original Moe by Yu-Ki
- **kano.png** - Kano
- **SleepyQuatre.png** - Sleepy Quatre
- **toheartserika.png** - ToHeart Serika
- **yuno.png** - Yuno

## Settings

### AI Tab
- **API Key** - Claude API key (stored locally)
- **Model** - Claude Sonnet 4, Opus 4, or Haiku 3.5
- **MCP URL** - Pippo MCP server address (default: `http://127.0.0.1:2607/mcp`)
- **System prompt** - Customize Moe's personality

### Mascot Tab
- **Change mascot** - Select a different mascot image
- **Wink / Polling / Redraw** - Animation timing
- **Debug frame** - Show window boundaries

### Voice Tab
- **Read responses aloud** - Enable/disable TTS
- **Voice** - espeak (fast) or Piper neural voices (natural)
- **Speed** - Speech rate

## Architecture

```
User -> MoeBubbleWindow (UI) -> MoeClaudeClient (BLooper)
                                    |-- Claude API (HTTPS)
                                    |-- Pippo MCP (HTTP JSON-RPC)
                                    |-- PiperTTS / espeak (TTS)
```

- **MoeBubbleWindow** - Speech bubble UI with smart positioning
- **MoeClaudeClient** - API client with agentic tool loop
- **MoeJsonHelper** - Lightweight JSON parser
- **MoeSettingsWindow** - Tabbed settings (AI, Mascot, Voice)
- **MoeActiveWindowWatcher** - Tracks active window for context

## Credits

- **Okada Jun** - Original Moe programming (2001)
- **Yu-Ki** - Original illustration
- **Cafeina** - Haiku port (2021)
- **atomozero** - AI chat integration (2026)

## License

GNU General Public License v2 or later. See [LICENSE](documentation/en/gpl.html).

Based on [Moe](https://github.com/HaikuArchives/Moe) by Kamnagi Software.
