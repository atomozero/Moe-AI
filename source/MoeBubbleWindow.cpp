// -*- c++ -*-
//
//  'Moe' window sitter for BeOS.
//  Speech Bubble Chat Window
//


#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <Application.h>
#include <Directory.h>
#include <File.h>
#include <Button.h>
#include <Font.h>
#include <GroupLayout.h>
#include <LayoutBuilder.h>
#include <Message.h>
#include <MessageRunner.h>
#include <Region.h>
#include <Screen.h>
#include <ScrollView.h>
#include <Shape.h>
#include <StringView.h>
#include <TextControl.h>
#include <TextView.h>
#include <View.h>
#include <Catalog.h>
#include "MoeDefs.h"
#include "MoeBubbleWindow.h"
#include "MoeClaudeClient.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "BubbleWindow"


static MoeBubbleWindow* sBubbleWindow = NULL;

static const uint32 kMsgAutoHide = 'MaHd';
static const bigtime_t kAutoHideDelay = 10000000; // 10 seconds

static const float kBubbleWidth = 300;
static const float kBubbleHeight = 200;
static const float kTailHeight = 12;
static const float kCornerRadius = 10;
static const float kPadding = 10;


// Custom view that draws the bubble shape
class BubbleView : public BView
{
public:
  bool fTailOnBottom;  // true = tail at bottom, false = tail at top
  float fTailX;        // horizontal position of tail tip (relative to view)

  BubbleView(void)
    : BView("bubble_bg", B_WILL_DRAW | B_DRAW_ON_CHILDREN),
      fTailOnBottom(true),
      fTailX(kBubbleWidth / 2)
  {
    SetViewColor(B_TRANSPARENT_COLOR);
  }

  virtual void Draw(BRect updateRect)
  {
    BRect bounds = Bounds();
    float r = kCornerRadius;

    BRect bubble;
    if (fTailOnBottom)
      bubble.Set(bounds.left, bounds.top,
                 bounds.right, bounds.bottom - kTailHeight);
    else
      bubble.Set(bounds.left, bounds.top + kTailHeight,
                 bounds.right, bounds.bottom);

    // Draw shadow
    SetHighColor(0, 0, 0, 40);
    FillRoundRect(bubble.OffsetByCopy(2, 2), r, r);

    // Draw bubble body
    SetHighColor(255, 255, 255, 255);
    FillRoundRect(bubble, r, r);

    // Draw the tail
    float tailX = fTailX;
    if (tailX < 20) tailX = 20;
    if (tailX > bounds.Width() - 20) tailX = bounds.Width() - 20;

    BPoint tailPts[3];
    if (fTailOnBottom) {
      tailPts[0] = BPoint(tailX - 10, bubble.bottom - 1);
      tailPts[1] = BPoint(tailX, bounds.bottom);
      tailPts[2] = BPoint(tailX + 10, bubble.bottom - 1);
    } else {
      tailPts[0] = BPoint(tailX - 10, bubble.top + 1);
      tailPts[1] = BPoint(tailX, bounds.top);
      tailPts[2] = BPoint(tailX + 10, bubble.top + 1);
    }
    FillTriangle(tailPts[0], tailPts[1], tailPts[2]);

    // Draw border
    SetHighColor(180, 180, 180, 255);
    SetPenSize(1.0);
    StrokeRoundRect(bubble, r, r);
    StrokeLine(tailPts[0], tailPts[1]);
    StrokeLine(tailPts[1], tailPts[2]);

    // Cover border between bubble and tail
    SetHighColor(255, 255, 255, 255);
    if (fTailOnBottom)
      FillRect(BRect(tailX - 9, bubble.bottom - 1,
                     tailX + 9, bubble.bottom + 1));
    else
      FillRect(BRect(tailX - 9, bubble.top - 1,
                     tailX + 9, bubble.top + 1));
  }
};


MoeBubbleWindow*
MoeBubbleWindow::Window(void)
{
  // Called from app thread only (via MOE_CHAT_BUBBLE_OPEN handler)
  if (!sBubbleWindow)
    sBubbleWindow = new MoeBubbleWindow();
  return sBubbleWindow;
}


MoeBubbleWindow::MoeBubbleWindow(void)
  : BWindow(BRect(0, 0, kBubbleWidth, kBubbleHeight + kTailHeight),
            "Moe",
            B_NO_BORDER_WINDOW_LOOK,
            B_FLOATING_ALL_WINDOW_FEEL,
            B_NOT_ZOOMABLE | B_NOT_MINIMIZABLE
            | B_NOT_RESIZABLE
            | B_CLOSE_ON_ESCAPE
            | B_WILL_ACCEPT_FIRST_CLICK),
    fBusy(false),
    fAutoHideTimer(NULL)
{
  BubbleView* bg = new BubbleView();
  fBubbleView = bg;

  fResponseView = new BTextView("response");
  fResponseView->MakeEditable(false);
  fResponseView->MakeSelectable(true);
  fResponseView->SetStylable(true);
  fResponseView->SetWordWrap(true);
  fResponseView->SetViewColor(255, 255, 255);

  BFont font(be_plain_font);
  font.SetSize(font.Size() - 1);
  fResponseView->SetFontAndColor(&font);

  fScrollView = new BScrollView("scroll", fResponseView,
                                B_WILL_DRAW | B_FRAME_EVENTS,
                                false, false);

  fInput = new BTextControl("input", NULL, "",
                            new BMessage(MOE_CHAT_SEND));

  // Build layout inside the bubble view
  float inset = kPadding + kCornerRadius / 2;
  bg->SetLayout(new BGroupLayout(B_VERTICAL, 4));
  BLayoutBuilder::Group<>((BGroupLayout*)bg->GetLayout())
    .SetInsets(inset, inset, inset, kTailHeight + kPadding)
    .Add(fScrollView, 10)
    .Add(fInput);

  // Add the bubble view to the window
  AddChild(bg);
  bg->ResizeTo(kBubbleWidth, kBubbleHeight + kTailHeight);
}


MoeBubbleWindow::~MoeBubbleWindow(void)
{
  sBubbleWindow = NULL;
}


void
MoeBubbleWindow::ShowNear(BRect mascotFrame)
{
  float totalH = kBubbleHeight + kTailHeight;
  BRect screen = BScreen().Frame();

  float mascotCX = mascotFrame.left + mascotFrame.Width() / 2;

  float spaceAbove = mascotFrame.top - screen.top;
  float spaceBelow = screen.bottom - mascotFrame.bottom;

  float x, y;
  bool tailOnBottom;

  // Prefer above, then below
  if (spaceAbove >= totalH + 5) {
    // Place above mascot
    y = mascotFrame.top - totalH - 2;
    tailOnBottom = true;
  } else if (spaceBelow >= totalH + 5) {
    // Place below mascot
    y = mascotFrame.bottom + 2;
    tailOnBottom = false;
  } else if (spaceAbove >= spaceBelow) {
    y = screen.top + 5;
    tailOnBottom = true;
  } else {
    y = screen.bottom - totalH - 5;
    tailOnBottom = false;
  }

  // Horizontal: center on mascot, clamp to screen
  x = mascotCX - kBubbleWidth / 2;
  if (x < screen.left + 5) x = screen.left + 5;
  if (x + kBubbleWidth > screen.right - 5)
    x = screen.right - kBubbleWidth - 5;

  // Tail X points to mascot center, relative to bubble
  float tailX = mascotCX - x;

  Lock();
  BubbleView* bv = (BubbleView*)fBubbleView;
  bv->fTailOnBottom = tailOnBottom;
  bv->fTailX = tailX;

  // Update layout insets based on tail position
  float inset = kPadding + kCornerRadius / 2;
  float topInset = tailOnBottom ? inset : (kTailHeight + kPadding);
  float bottomInset = tailOnBottom ? (kTailHeight + kPadding) : inset;
  BGroupLayout* layout = (BGroupLayout*)bv->GetLayout();
  layout->SetInsets(inset, topInset, inset, bottomInset);

  MoveTo(x, y);
  bv->Invalidate();
  if (IsHidden())
    Show();
  Activate();
  fInput->MakeFocus(true);
  Unlock();
}


void
MoeBubbleWindow::_AppendStyled(const char* text, rgb_color color, bool bold)
{
  BFont font(be_plain_font);
  font.SetSize(font.Size() - 1);
  if (bold)
    font.SetFace(B_BOLD_FACE);

  int32 start = fResponseView->TextLength();
  fResponseView->Insert(start, text, strlen(text));
  int32 end = fResponseView->TextLength();
  fResponseView->SetFontAndColor(start, end, &font, B_FONT_ALL, &color);
  _ScrollToBottom();
}


static BString
_ReadConfig(const char* filename, const char* defaultVal)
{
  BString path(MOE_CONFIG_DIRECTORY);
  path << filename;
  BFile file(path.String(), B_READ_ONLY);
  if (file.InitCheck() != B_OK)
    return BString(defaultVal);
  off_t size;
  file.GetSize(&size);
  if (size <= 0 || size > 4096)
    return BString(defaultVal);
  BString result;
  char* buf = result.LockBuffer(size + 1);
  file.Read(buf, size);
  buf[size] = '\0';
  result.UnlockBuffer(size);
  result.Trim();
  return result.Length() > 0 ? result : BString(defaultVal);
}


static void
_UpdateTtsConfig(const char* voice, const char* speed)
{
  BString configPath;
  configPath << "/boot/home/config/non-packaged/data/pipertts/models/"
             << voice << "/it_config.txt";

  BFile configFile(configPath.String(), B_READ_ONLY);
  if (configFile.InitCheck() != B_OK)
    return;

  off_t size;
  configFile.GetSize(&size);
  if (size <= 0 || size > 2048)
    return;

  char* buf = new char[size + 1];
  configFile.Read(buf, size);
  buf[size] = '\0';
  BString config(buf);
  delete[] buf;
  configFile.Unset();

  // Replace length_scale line
  BString newConfig;
  int32 pos = 0;
  while (pos < config.Length()) {
    int32 eol = config.FindFirst("\n", pos);
    if (eol < 0) eol = config.Length();

    BString line;
    config.CopyInto(line, pos, eol - pos);

    if (line.FindFirst("\"length_scale\"") >= 0) {
      newConfig << "\"length_scale\"=" << speed << "f\n";
    } else {
      newConfig << line << "\n";
    }
    pos = eol + 1;
  }

  BFile writeFile(configPath.String(), B_WRITE_ONLY | B_ERASE_FILE);
  if (writeFile.InitCheck() == B_OK)
    writeFile.Write(newConfig.String(), newConfig.Length());
}


static BString sLastVoice;
static BString sLastSpeed;

static void
_SpeakText(const char* text, const char* lang)
{
  BString enabled = _ReadConfig("tts_enabled", "0");
  if (enabled != "1")
    return;

  BString voice = _ReadConfig("tts_voice", "espeak");
  BString speed = _ReadConfig("tts_speed", "160");

  // Sanitize voice and speed: whitelist safe characters only
  for (int32 i = voice.Length() - 1; i >= 0; i--) {
    char c = voice[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
        || (c >= '0' && c <= '9') || c == '-' || c == '_'))
      voice.Remove(i, 1);
  }
  for (int32 i = speed.Length() - 1; i >= 0; i--) {
    char c = speed[i];
    if (!(c >= '0' && c <= '9') && c != '.')
      speed.Remove(i, 1);
  }
  if (voice.Length() == 0) voice = "espeak";
  if (speed.Length() == 0) speed = "160";

  // Kill any ongoing TTS playback
  system("pkill -f moe_tts_speak 2>/dev/null; pkill pipertts 2>/dev/null; pkill espeak 2>/dev/null");

  // Write text to temp file
  BFile tmpFile("/tmp/moe_tts_text.txt",
                B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
  if (tmpFile.InitCheck() == B_OK)
    tmpFile.Write(text, strlen(text));
  tmpFile.Unset();

  BString cmd;

  if (voice == "espeak") {
    // espeak: instant playback
    BString espeakLang(lang && lang[0] ? lang : "it");
    cmd << "espeak -v " << espeakLang
        << " -s " << speed
        << " -p 50"
        << " -f /tmp/moe_tts_text.txt &";
  } else {
    // Piper TTS with instant filler hack:
    // 1. Play a random pre-cached filler WAV instantly
    // 2. Generate real audio in background
    // 3. Play real audio when ready

    // Update piper config if settings changed
    if (voice != sLastVoice || speed != sLastSpeed) {
      float lengthScale = 0.8;
      int speedInt = atoi(speed.String());
      if (speedInt <= 130) lengthScale = 1.0;
      else if (speedInt <= 160) lengthScale = 0.8;
      else if (speedInt <= 190) lengthScale = 0.7;
      else lengthScale = 0.6;
      BString scaleStr;
      scaleStr.SetToFormat("%.1f", lengthScale);
      _UpdateTtsConfig(voice.String(), scaleStr.String());
      sLastVoice = voice;
      sLastSpeed = speed;
    }

    // Split text into sentences
    BString fullText(text);
    int32 tpos = 0;
    int32 sentNum = 0;

    while (tpos < fullText.Length()) {
      int32 end = -1;
      const char* delims[] = { ". ", "! ", "? ", ".\n", "!\n", "?\n" };
      for (int d = 0; d < 6; d++) {
        int32 found = fullText.FindFirst(delims[d], tpos);
        if (found >= 0 && (end < 0 || found < end))
          end = found;
      }
      BString sentence;
      if (end >= 0) {
        fullText.CopyInto(sentence, tpos, end - tpos + 1);
        tpos = end + 2;
      } else {
        fullText.CopyInto(sentence, tpos, fullText.Length() - tpos);
        tpos = fullText.Length();
      }
      sentence.Trim();
      if (sentence.Length() == 0) continue;

      BString tmpPath;
      tmpPath << "/tmp/moe_tts_" << sentNum << ".txt";
      BFile stFile(tmpPath.String(),
                   B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
      if (stFile.InitCheck() == B_OK)
        stFile.Write(sentence.String(), sentence.Length());
      stFile.Unset();
      sentNum++;
    }

    BString piperCmd;
    piperCmd << "pipertts -m " << voice;
    if (lang && lang[0])
      piperCmd << " -l " << lang;

    // Build fully pipelined script:
    // - Start generating sentence 0 WAV immediately in background
    // - Play filler while waiting
    // - Then for each sentence: play it while generating the next
    BString script("#!/bin/sh\n");
    int fillerNum = rand() % 6;

    if (sentNum > 0) {
      // Start generating first sentence immediately (parallel with filler)
      script << piperCmd << " -f /tmp/moe_tts_0.txt"
             << " -o /tmp/moe_tts_0.wav &\n";
      script << "GEN_PID=$!\n";

      // Play filler while sentence 0 generates
      script << "ffplay -nodisp -autoexit "
             << MOE_CONFIG_DIRECTORY << "tts_cache/filler_"
             << fillerNum << ".wav 2>/dev/null\n";

      // Wait for sentence 0 to finish generating
      script << "wait $GEN_PID\n";

      // For each sentence: play current, generate next in parallel
      for (int32 i = 0; i < sentNum; i++) {
        if (i < sentNum - 1) {
          // Start generating next sentence in background
          script << piperCmd << " -f /tmp/moe_tts_" << (i + 1)
                 << ".txt -o /tmp/moe_tts_" << (i + 1) << ".wav &\n";
          script << "GEN_PID=$!\n";
        }
        // Play current sentence
        script << "ffplay -nodisp -autoexit /tmp/moe_tts_"
               << i << ".wav 2>/dev/null\n";
        if (i < sentNum - 1)
          script << "wait $GEN_PID\n";
      }
    }

    // Cleanup temp files after playback
    script << "rm -f /tmp/moe_tts_*.txt /tmp/moe_tts_*.wav "
              "/tmp/moe_tts_speak.sh 2>/dev/null\n";

    BFile scriptFile("/tmp/moe_tts_speak.sh",
                     B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
    if (scriptFile.InitCheck() == B_OK)
      scriptFile.Write(script.String(), script.Length());
    scriptFile.Unset();

    cmd << "chmod +x /tmp/moe_tts_speak.sh && /tmp/moe_tts_speak.sh &";
  }

  system(cmd.String());
}


void
MoeBubbleWindow::SetResponse(const char* text)
{
  // Check for language tag [lang:XX] at start
  BString displayText(text);
  BString lang("it");

  if (displayText.FindFirst("[lang:") == 0) {
    int32 end = displayText.FindFirst("]");
    if (end > 6 && end <= 16) {
      BString rawLang;
      displayText.CopyInto(rawLang, 6, end - 6);
      // Sanitize: only allow a-z, A-Z, 0-9, dash, underscore
      BString safeLang;
      for (int32 i = 0; i < rawLang.Length(); i++) {
        char c = rawLang[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
            || (c >= '0' && c <= '9') || c == '-' || c == '_')
          safeLang << c;
      }
      if (safeLang.Length() >= 2)
        lang = safeLang;
      displayText.Remove(0, end + 1);
      while (displayText.Length() > 0
             && (displayText[0] == ' ' || displayText[0] == '\n'))
        displayText.Remove(0, 1);
    }
  }

  rgb_color moeColor = {0, 100, 60, 255};
  BString msg;
  msg << "Moe: " << displayText << "\n";
  _AppendStyled(msg.String(), moeColor, false);

  // Speak the response
  _SpeakText(displayText.String(), lang.String());
}


void
MoeBubbleWindow::SetError(const char* text)
{
  rgb_color red = {200, 0, 0, 255};
  BString msg;
  msg << text << "\n";
  _AppendStyled(msg.String(), red, true);
}


void
MoeBubbleWindow::SetToolProgress(const char* toolName)
{
  rgb_color toolColor = {120, 80, 0, 255};
  BString msg;
  msg << "  \xe2\x9a\x99 " << toolName << "\n";
  _AppendStyled(msg.String(), toolColor, false);
}


void
MoeBubbleWindow::SetBusy(bool busy)
{
  fBusy = busy;
  fInput->SetEnabled(!busy);
  if (busy) {
    _StopAutoHideTimer();
    rgb_color gray = {150, 150, 150, 255};
    _AppendStyled(B_TRANSLATE("Thinking" B_UTF8_ELLIPSIS "\n"), gray, false);
  } else {
    _ResetAutoHideTimer();
  }
}


void
MoeBubbleWindow::MessageReceived(BMessage* msg)
{
  switch (msg->what) {
    case MOE_CHAT_SEND:
    {
      if (fBusy)
        break;

      const char* text = fInput->Text();
      if (text == NULL || text[0] == '\0')
        break;

      BString userText(text);
      fInput->SetText("");

      // Show user message in bubble
      rgb_color userColor = {0, 0, 160, 255};
      BString userMsg;
      userMsg << B_TRANSLATE("You: ") << userText << "\n";
      _AppendStyled(userMsg.String(), userColor, true);

      SetBusy(true);

      MoeClaudeClient::Client()->SendChat(userText.String(),
                                          BMessenger(this));
      break;
    }

    case MOE_CHAT_RESPONSE:
    {
      const char* text;
      if (msg->FindString("text", &text) == B_OK)
        SetResponse(text);
      SetBusy(false);
      break;
    }

    case MOE_CHAT_ERROR:
    {
      const char* error;
      if (msg->FindString("error", &error) == B_OK)
        SetError(error);
      SetBusy(false);
      break;
    }

    case MOE_CHAT_TOOL_PROGRESS:
    {
      const char* toolName;
      if (msg->FindString("tool", &toolName) == B_OK)
        SetToolProgress(toolName);
      break;
    }

    case MOE_CHAT_CLEAR:
    {
      _ClearResponse();
      MoeClaudeClient::Client()->ClearHistory();
      break;
    }

    case kMsgAutoHide:
    {
      if (!fBusy && !IsHidden())
        Hide();
      break;
    }

    default:
      BWindow::MessageReceived(msg);
      break;
  }
}


bool
MoeBubbleWindow::QuitRequested(void)
{
  _StopAutoHideTimer();
  Hide();
  return false;
}


void
MoeBubbleWindow::WindowActivated(bool active)
{
  if (active) {
    fInput->MakeFocus(true);
    _StopAutoHideTimer();
  } else if (!fBusy) {
    _ResetAutoHideTimer();
  }
  BWindow::WindowActivated(active);
}


void
MoeBubbleWindow::WorkspaceActivated(int32 workspace, bool active)
{
  if (!active && !IsHidden())
    Hide();
  BWindow::WorkspaceActivated(workspace, active);
}


void
MoeBubbleWindow::_ResetAutoHideTimer(void)
{
  _StopAutoHideTimer();
  BMessage msg(kMsgAutoHide);
  fAutoHideTimer = new BMessageRunner(BMessenger(this),
                                      &msg, kAutoHideDelay, 1);
}


void
MoeBubbleWindow::_StopAutoHideTimer(void)
{
  delete fAutoHideTimer;
  fAutoHideTimer = NULL;
}


void
MoeBubbleWindow::_ClearResponse(void)
{
  fResponseView->SetText("");
}


void
MoeBubbleWindow::_ScrollToBottom(void)
{
  float height = fResponseView->TextHeight(0, fResponseView->CountLines() - 1);
  BRect bounds = fResponseView->Bounds();
  float scrollTo = height - bounds.Height();
  if (scrollTo > 0)
    fResponseView->ScrollTo(0, scrollTo);
}
