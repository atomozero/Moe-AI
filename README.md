# Moe-AI

> *Your Haiku desktop mascot just got smarter.*

AI-powered desktop mascot for Haiku OS. Chat with Claude, control your system, hear responses spoken aloud.

Fork of [Moe](https://github.com/HaikuArchives/Moe), a classic BeOS window sitter originally created by Okada Jun in 2001. Moe-AI extends it with AI chat, text-to-speech, and system control capabilities.

![Moe-AI Screenshot](documentation/img/sc1.png)

---

## What is Moe-AI?

Moe-AI is a cute mascot that lives on your active window. But unlike the original Moe, this one can **talk back**. Double-click the mascot and a speech bubble appears where you can chat with an AI assistant powered by Claude. Moe can answer questions, help with tasks, and even control your Haiku system.

### Key Features

| Feature | Description |
|---------|-------------|
| Speech Bubble Chat | Double-click the mascot to chat. Minimal, clean UI |
| Claude AI | Sonnet 4, Opus 4, or Haiku 3.5 models |
| System Control | Launch apps, manage files, query system info via [Pippo MCP](https://github.com/atomozero/pippo) |
| Text-to-Speech | Piper neural voices (natural) or espeak (fast) |
| Smart Positioning | Bubble positions itself around the mascot automatically |
| Conversation Memory | History persists across sessions |
| Context Awareness | Moe knows which app you're currently using |
| Multiple Mascots | 5 included mascots, or use your own images |

---

## Quick Start

### 1. Build

```bash
cd source
make -j4
```

> **Requirements:** Haiku OS, development headers for `netservices2`, `libnetwork`, `libbnetapi`, `libtracker`

### 2. Run

```bash
source/objects.x86_64-cc13-release/Moe-AI mascots/moe.png
```

### 3. Set up AI

1. Get an API key from [console.anthropic.com](https://console.anthropic.com)
2. Right-click mascot > **Settings** > **AI** tab
3. Paste key, click **Save**
4. **Double-click** the mascot to chat!

---

## Text-to-Speech (optional)

Moe can read responses aloud. Two engines are available:

| Engine | Quality | Speed | Install |
|--------|---------|-------|---------|
| **espeak** | Robotic | Instant | `pkgman install espeak` |
| **Piper** | Natural | ~6s/sentence | `pkgman install pipertts espeak` |

For Piper, download a voice model:

```bash
mkdir -p ~/config/non-packaged/data/pipertts/models/
cd ~/config/non-packaged/data/pipertts/models/
wget https://huggingface.co/gyroing/PiperTTS-NCNN-Models/resolve/main/it_IT-dii.zip
unzip it_IT-dii.zip
```

Enable in **Settings** > **Voice** tab. Language is detected automatically from the AI response.

---

## Usage

| Action | How |
|--------|-----|
| Chat with Moe | Double-click mascot |
| Close bubble | Escape, or wait 30s |
| Open settings | Right-click > Settings |
| Change mascot | Settings > Mascot > Change mascot |
| Close mascot | Right-click > Close |
| Quit all | Right-click > Quit |

---

## Included Mascots

Five ready-to-use mascot images in `mascots/`:

| Image | Name | Author |
|-------|------|--------|
| moe.png | Moe | Yu-Ki |
| kano.png | Kano | BeBits community |
| SleepyQuatre.png | Sleepy Quatre | BeBits community |
| toheartserika.png | ToHeart Serika | BeBits community |
| yuno.png | Yuno | BeBits community |

You can use any PNG image as a mascot. Transparent backgrounds work best.

---

## Settings Reference

### AI
| Setting | Description | Default |
|---------|-------------|---------|
| API Key | Claude API key | *(none)* |
| Model | Claude model to use | Sonnet 4 |
| MCP URL | Pippo MCP server | `http://127.0.0.1:2607/mcp` |
| System prompt | Moe's personality | Built-in prompt |

### Mascot
| Setting | Description |
|---------|-------------|
| Change mascot | Pick a new mascot image |
| Wink | Eye blink interval |
| Polling | Window tracking speed |
| Redraw | Rendering speed |
| Debug frame | Show window boundaries |

### Voice
| Setting | Description | Default |
|---------|-------------|---------|
| Read responses aloud | Enable TTS | Off |
| Voice | espeak or Piper model | espeak |
| Speed | Speech rate | Normal |

All settings are stored in `~/config/settings/Moe-AI/`.

---

## Architecture

```
                    +------------------+
                    |  MoeBubbleWindow |  Speech bubble UI
                    +--------+---------+
                             |
                    +--------v---------+
  Double-click ---->|  MoeClaudeClient |  BLooper (async thread)
                    +--------+---------+
                             |
              +--------------+--------------+
              |              |              |
     +--------v---+  +------v------+  +----v-----+
     | Claude API |  | Pippo MCP   |  | PiperTTS |
     | (HTTPS)    |  | (JSON-RPC)  |  | (local)  |
     +------------+  +-------------+  +----------+
```

| Component | Role |
|-----------|------|
| **MoeBubbleWindow** | Speech bubble UI, TTS playback, auto-positioning |
| **MoeClaudeClient** | API calls, tool loop, conversation history |
| **MoeJsonHelper** | Lightweight JSON parser for API responses |
| **MoeSettingsWindow** | Tabbed settings UI (AI, Mascot, Voice) |
| **MoeActiveWindowWatcher** | Tracks active window for context awareness |
| **MoeMascot** | Mascot rendering, double-click detection |

---

## Credits

| Who | What | When |
|-----|------|------|
| **Okada Jun** (Yun) | Original Moe programming | 2001 |
| **Yu-Ki** | Original mascot illustration | 2001 |
| **Cafeina** | Haiku OS port | 2021 |
| **atomozero** | AI chat, TTS, settings, bubble UI | 2026 |

---

## License

GNU General Public License v2 or later.

Based on [Moe](https://github.com/HaikuArchives/Moe) by Kamnagi Software. See [full license text](documentation/en/gpl.html).
