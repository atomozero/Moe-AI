// -*- c++ -*-
//
//  'Moe' window sitter for BeOS.
//  Settings Window
//


#include <cstdio>
#include <AppFileInfo.h>
#include <Application.h>
#include <Entry.h>
#include <FilePanel.h>
#include <Path.h>
#include <Roster.h>
#include <Size.h>
#include <Box.h>
#include <Button.h>
#include <CheckBox.h>
#include <Directory.h>
#include <File.h>
#include <LayoutBuilder.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <Screen.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TabView.h>
#include <TextControl.h>
#include <TextView.h>
#include <Catalog.h>
#include "MoeDefs.h"
#include "MoeProperty.h"
#include "MoeSettingsWindow.h"
#include "MoeClaudeClient.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "SettingsWindow"


static MoeSettingsWindow* sSettingsWindow = NULL;

// Message codes for internal use
enum {
  kMsgSaveAI      = 'SsAI',
  kMsgModelSelect = 'SmSl',
  kMsgWinkSelect  = 'SwSl',
  kMsgPollSelect  = 'SpSl',
  kMsgDrawSelect  = 'SdSl',
  kMsgDebugToggle = 'SdTg',
  kMsgBrowseMascot = 'SbMc',
  kMsgMascotSelect = 'SmcS',
  kMsgTtsToggle    = 'StTg',
  kMsgVoiceSelect  = 'SvSl',
  kMsgSpeedSelect  = 'SsSl',
};

struct ModelInfo {
  const char* label;
  const char* id;
};

static const ModelInfo kModels[] = {
  { "Claude Sonnet 4", "claude-sonnet-4-20250514" },
  { "Claude Opus 4",   "claude-opus-4-20250514" },
  { "Claude Haiku 3.5","claude-haiku-4-5-20251001" },
};
static const int32 kModelCount = 3;

static const char* kSpeedLabels[] = {
  "Fastest", "Fast", "Slow", "Very Slow", "Slowest"
};


MoeSettingsWindow*
MoeSettingsWindow::Window(void)
{
  // Called from app thread only (via MOE_SETTINGS_OPEN handler)
  if (!sSettingsWindow)
    sSettingsWindow = new MoeSettingsWindow();
  return sSettingsWindow;
}


MoeSettingsWindow::MoeSettingsWindow(void)
  : BWindow(BRect(200, 200, 700, 600),
            B_TRANSLATE("Moe-AI Settings"),
            B_TITLED_WINDOW_LOOK,
            B_NORMAL_WINDOW_FEEL,
            B_AUTO_UPDATE_SIZE_LIMITS
            | B_CLOSE_ON_ESCAPE)
{
  BTabView* tabView = new BTabView("tabs", B_WIDTH_FROM_WIDEST);

  // === AI Tab ===
  BView* aiTab = new BView(B_TRANSLATE("AI"), B_WILL_DRAW);
  aiTab->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

  fApiKeyControl = new BTextControl(B_TRANSLATE("API Key:"), "",
                                    NULL);
  fApiKeyControl->TextView()->HideTyping(true);
  fApiKeyControl->SetExplicitMinSize(BSize(350, B_SIZE_UNSET));

  // Model popup
  BPopUpMenu* modelMenu = new BPopUpMenu("");
  for (int32 i = 0; i < kModelCount; i++) {
    BMessage* msg = new BMessage(kMsgModelSelect);
    msg->AddString("model", kModels[i].id);
    modelMenu->AddItem(new BMenuItem(kModels[i].label, msg));
  }
  fModelField = new BMenuField(B_TRANSLATE("Model:"), modelMenu);

  fPippoUrlControl = new BTextControl(B_TRANSLATE("MCP URL:"),
                                      "http://127.0.0.1:2607/mcp",
                                      NULL);

  fSystemPromptView = new BTextView("system_prompt");
  fSystemPromptView->SetWordWrap(true);
  BScrollView* promptScroll = new BScrollView("prompt_scroll",
                                              fSystemPromptView,
                                              B_WILL_DRAW | B_FRAME_EVENTS,
                                              false, true);

  BButton* saveAIButton = new BButton("save_ai",
                                      B_TRANSLATE("Save"),
                                      new BMessage(kMsgSaveAI));

  BLayoutBuilder::Group<>(aiTab, B_VERTICAL, 4)
    .SetInsets(B_USE_DEFAULT_SPACING)
    .Add(fApiKeyControl)
    .Add(fModelField)
    .Add(fPippoUrlControl)
    .Add(new BStringView("prompt_label",
                          B_TRANSLATE("System prompt:")))
    .Add(promptScroll, 5)
    .Add(saveAIButton)
  .End();

  tabView->AddTab(aiTab);

  // === Mascot Tab ===
  BView* mascotTab = new BView(B_TRANSLATE("Mascot"), B_WILL_DRAW);
  mascotTab->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

  // Wink popup
  BPopUpMenu* winkMenu = new BPopUpMenu("");
  for (int32 i = 0; i < 5; i++) {
    BMessage* msg = new BMessage(kMsgWinkSelect);
    bigtime_t interval = (i + 1) * 1000000;
    msg->AddInt64("interval", interval);
    BString label;
    label << (i + 1) << (i == 0 ? " second" : " seconds");
    winkMenu->AddItem(new BMenuItem(label.String(), msg));
  }
  fWinkField = new BMenuField(B_TRANSLATE("Wink:"), winkMenu);

  // Polling popup
  BPopUpMenu* pollMenu = new BPopUpMenu("");
  bigtime_t interval = MOE_FASTEST_POLLING_INTERVAL;
  for (int32 i = 0; i < 5; i++, interval *= 2) {
    BMessage* msg = new BMessage(kMsgPollSelect);
    msg->AddInt64("interval", interval);
    pollMenu->AddItem(new BMenuItem(kSpeedLabels[i], msg));
  }
  fPollingField = new BMenuField(B_TRANSLATE("Polling:"), pollMenu);

  // Redraw popup
  BPopUpMenu* drawMenu = new BPopUpMenu("");
  interval = MOE_FASTEST_POLLING_INTERVAL * 2;
  for (int32 i = 0; i < 5; i++, interval *= 2) {
    BMessage* msg = new BMessage(kMsgDrawSelect);
    msg->AddInt64("interval", interval);
    drawMenu->AddItem(new BMenuItem(kSpeedLabels[i], msg));
  }
  fRedrawField = new BMenuField(B_TRANSLATE("Redraw:"), drawMenu);

  fDebugFrameCheck = new BCheckBox("debug_frame",
                                   B_TRANSLATE("Debug frame visible"),
                                   new BMessage(kMsgDebugToggle));

  // Build mascot selection menu from mascots/ directory
  BPopUpMenu* mascotMenu = new BPopUpMenu("");
  {
    // Find app directory and look for mascots/ next to it
    app_info appInfo;
    be_app->GetAppInfo(&appInfo);
    BEntry appEntry(&appInfo.ref);
    BPath appPath;
    appEntry.GetPath(&appPath);
    appPath.GetParent(&appPath);  // up from objects.xxx/
    appPath.GetParent(&appPath);  // up from source/
    // If still inside objects dir, go up once more
    BPath testPath(appPath);
    testPath.Append("mascots");
    BDirectory testDir(testPath.Path());
    if (testDir.InitCheck() != B_OK)
      appPath.GetParent(&appPath);
    BPath mascotsPath(appPath);
    mascotsPath.Append("mascots");

    BDirectory dir(mascotsPath.Path());
    if (dir.InitCheck() == B_OK) {
      BEntry entry;
      while (dir.GetNextEntry(&entry) == B_OK) {
        BPath filePath;
        entry.GetPath(&filePath);
        BString name(filePath.Leaf());
        // Only show image files
        if (name.FindLast(".png") > 0 || name.FindLast(".jpg") > 0
            || name.FindLast(".bmp") > 0) {
          BMessage* msg = new BMessage(kMsgMascotSelect);
          entry_ref ref;
          entry.GetRef(&ref);
          msg->AddRef("refs", &ref);
          mascotMenu->AddItem(new BMenuItem(name.String(), msg));
        }
      }
    }

    // Add separator and browse option
    if (mascotMenu->CountItems() > 0)
      mascotMenu->AddSeparatorItem();
    mascotMenu->AddItem(new BMenuItem(
      B_TRANSLATE("Browse" B_UTF8_ELLIPSIS),
      new BMessage(kMsgBrowseMascot)));
  }
  fMascotField = new BMenuField(B_TRANSLATE("Mascot:"), mascotMenu);

  BLayoutBuilder::Group<>(mascotTab, B_VERTICAL, 4)
    .SetInsets(B_USE_DEFAULT_SPACING)
    .Add(fMascotField)
    .Add(fWinkField)
    .Add(fPollingField)
    .Add(fRedrawField)
    .AddGlue()
    .Add(fDebugFrameCheck)
  .End();

  tabView->AddTab(mascotTab);

  // === Voice Tab ===
  BView* voiceTab = new BView(B_TRANSLATE("Voice"), B_WILL_DRAW);
  voiceTab->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

  fTtsEnabledCheck = new BCheckBox("tts_enabled",
    B_TRANSLATE("Read responses aloud"),
    new BMessage(kMsgTtsToggle));

  // Voice selection
  BPopUpMenu* voiceMenu = new BPopUpMenu("");
  {
    BMessage* m1 = new BMessage(kMsgVoiceSelect);
    m1->AddString("voice", "espeak");
    voiceMenu->AddItem(new BMenuItem(
      B_TRANSLATE("espeak (fast, robotic)"), m1));

    BMessage* m2 = new BMessage(kMsgVoiceSelect);
    m2->AddString("voice", "it_IT-dii");
    voiceMenu->AddItem(new BMenuItem(
      B_TRANSLATE("Piper Female (natural, slow)"), m2));

    BMessage* m3 = new BMessage(kMsgVoiceSelect);
    m3->AddString("voice", "it_IT-miro");
    voiceMenu->AddItem(new BMenuItem(
      B_TRANSLATE("Piper Male (natural, slow)"), m3));
  }
  fVoiceField = new BMenuField(B_TRANSLATE("Voice:"), voiceMenu);

  // Speed
  BPopUpMenu* speedMenu = new BPopUpMenu("");
  {
    const struct { const char* label; const char* val; } speeds[] = {
      { "Slow",    "130" },
      { "Normal",  "160" },
      { "Fast",    "190" },
      { "Fastest", "220" },
    };
    for (int i = 0; i < 4; i++) {
      BMessage* sm = new BMessage(kMsgSpeedSelect);
      sm->AddString("speed", speeds[i].val);
      speedMenu->AddItem(new BMenuItem(speeds[i].label, sm));
    }
  }
  fSpeedField = new BMenuField(B_TRANSLATE("Speed:"), speedMenu);

  BLayoutBuilder::Group<>(voiceTab, B_VERTICAL, 4)
    .SetInsets(B_USE_DEFAULT_SPACING)
    .Add(fTtsEnabledCheck)
    .Add(fVoiceField)
    .Add(fSpeedField)
    .AddGlue()
    .Add(new BStringView("voice_info",
      B_TRANSLATE("Language is detected automatically.")))
  .End();

  tabView->AddTab(voiceTab);

  // === Window layout ===
  BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
    .Add(tabView)
  .End();

  // Set minimum window size
  SetSizeLimits(400, 900, 350, 700);
  ResizeTo(500, 400);
  CenterOnScreen();

  _LoadSettings();
}


MoeSettingsWindow::~MoeSettingsWindow(void)
{
  sSettingsWindow = NULL;
}


void
MoeSettingsWindow::ShowNear(BRect mascotFrame)
{
  BRect screen = BScreen().Frame();
  BRect frame = Frame();
  float w = frame.Width();
  float h = frame.Height();

  float mascotCX = mascotFrame.left + mascotFrame.Width() / 2;

  // Try right of mascot, then left, then centered
  float x, y;

  float spaceRight = screen.right - mascotFrame.right;
  float spaceLeft = mascotFrame.left - screen.left;

  if (spaceRight >= w + 10)
    x = mascotFrame.right + 10;
  else if (spaceLeft >= w + 10)
    x = mascotFrame.left - w - 10;
  else
    x = mascotCX - w / 2;

  // Vertical: align top with mascot, clamp to screen
  y = mascotFrame.top;

  // Clamp to screen
  if (x < screen.left + 5) x = screen.left + 5;
  if (x + w > screen.right - 5) x = screen.right - w - 5;
  if (y < screen.top + 5) y = screen.top + 5;
  if (y + h > screen.bottom - 5) y = screen.bottom - h - 5;

  MoveTo(x, y);
  if (IsHidden())
    Show();
  Activate();
}


static BString
_ReadConfigString(const char* filename, const char* defaultVal)
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

  if (result.Length() == 0)
    return BString(defaultVal);
  return result;
}


void
MoeSettingsWindow::_LoadSettings(void)
{
  // Load API key
  BString path(MOE_CONFIG_DIRECTORY);
  path << "claude_api_key";
  BFile file(path.String(), B_READ_ONLY);
  if (file.InitCheck() == B_OK) {
    off_t size;
    file.GetSize(&size);
    if (size > 0 && size < 256) {
      char buf[257];
      file.Read(buf, size);
      buf[size] = '\0';
      BString key(buf);
      key.Trim();
      fApiKeyControl->SetText(key.String());
    }
  }

  // Load model
  path.SetTo(MOE_CONFIG_DIRECTORY);
  path << "claude_model";
  BFile modelFile(path.String(), B_READ_ONLY);
  BString currentModel("claude-sonnet-4-20250514");
  if (modelFile.InitCheck() == B_OK) {
    off_t size;
    modelFile.GetSize(&size);
    if (size > 0 && size < 128) {
      char buf[129];
      modelFile.Read(buf, size);
      buf[size] = '\0';
      currentModel.SetTo(buf);
      currentModel.Trim();
    }
  }
  // Mark current model in menu
  BMenu* modelMenu = fModelField->Menu();
  for (int32 i = 0; i < kModelCount; i++) {
    BMenuItem* item = modelMenu->ItemAt(i);
    if (item)
      item->SetMarked(currentModel == kModels[i].id);
  }

  // Load Pippo URL
  path.SetTo(MOE_CONFIG_DIRECTORY);
  path << "pippo_url";
  BFile urlFile(path.String(), B_READ_ONLY);
  if (urlFile.InitCheck() == B_OK) {
    off_t size;
    urlFile.GetSize(&size);
    if (size > 0 && size < 512) {
      char buf[513];
      urlFile.Read(buf, size);
      buf[size] = '\0';
      BString url(buf);
      url.Trim();
      fPippoUrlControl->SetText(url.String());
    }
  }

  // Load system prompt
  path.SetTo(MOE_CONFIG_DIRECTORY);
  path << "system_prompt";
  BFile promptFile(path.String(), B_READ_ONLY);
  if (promptFile.InitCheck() == B_OK) {
    off_t size;
    promptFile.GetSize(&size);
    if (size > 0 && size < 4096) {
      char* buf = new char[size + 1];
      promptFile.Read(buf, size);
      buf[size] = '\0';
      fSystemPromptView->SetText(buf);
      delete[] buf;
    }
  }

  // Load mascot settings from MoeProperty
  MoeProperty* property = MoeProperty::Property();

  // Mark wink interval
  bigtime_t winkInterval = property->GetWinkInterval();
  BMenu* winkMenu = fWinkField->Menu();
  for (int32 i = 0; i < 5; i++) {
    bigtime_t val = (i + 1) * 1000000;
    BMenuItem* item = winkMenu->ItemAt(i);
    if (item)
      item->SetMarked(winkInterval == val);
  }

  // Mark polling interval
  bigtime_t pollInterval = property->GetPollingInterval();
  BMenu* pollMenu = fPollingField->Menu();
  bigtime_t pval = MOE_FASTEST_POLLING_INTERVAL;
  for (int32 i = 0; i < 5; i++, pval *= 2) {
    BMenuItem* item = pollMenu->ItemAt(i);
    if (item)
      item->SetMarked(pollInterval == pval);
  }

  // Mark redraw interval
  bigtime_t drawInterval = property->GetRedrawInterval();
  BMenu* drawMenu = fRedrawField->Menu();
  bigtime_t dval = MOE_FASTEST_POLLING_INTERVAL * 2;
  for (int32 i = 0; i < 5; i++, dval *= 2) {
    BMenuItem* item = drawMenu->ItemAt(i);
    if (item)
      item->SetMarked(drawInterval == dval);
  }

  // Debug frame
  fDebugFrameCheck->SetValue(property->IsDebugFrameVisible() ? 1 : 0);

  // Load TTS settings
  BString ttsEnabled = _ReadConfigString("tts_enabled", "0");
  fTtsEnabledCheck->SetValue(ttsEnabled == "1" ? 1 : 0);

  BString voice = _ReadConfigString("tts_voice", "espeak");
  BMenu* vMenu = fVoiceField->Menu();
  for (int32 i = 0; i < vMenu->CountItems(); i++) {
    BMenuItem* item = vMenu->ItemAt(i);
    if (item) {
      const char* v;
      if (item->Message()->FindString("voice", &v) == B_OK)
        item->SetMarked(voice == v);
    }
  }

  BString speed = _ReadConfigString("tts_speed", "160");
  BMenu* sMenu = fSpeedField->Menu();
  for (int32 i = 0; i < sMenu->CountItems(); i++) {
    BMenuItem* item = sMenu->ItemAt(i);
    if (item) {
      const char* s;
      if (item->Message()->FindString("speed", &s) == B_OK)
        item->SetMarked(speed == s);
    }
  }
}


static void
_WriteConfigFile(const char* filename, const char* content)
{
  BString path(MOE_CONFIG_DIRECTORY);

  // Ensure directory exists
  create_directory(MOE_CONFIG_DIRECTORY, 0755);

  path << filename;
  BFile file(path.String(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
  if (file.InitCheck() == B_OK)
    file.Write(content, strlen(content));
}


void
MoeSettingsWindow::_SaveAISettings(void)
{
  _WriteConfigFile("claude_api_key", fApiKeyControl->Text());

  // Get selected model
  BMenuItem* modelItem = fModelField->Menu()->FindMarked();
  if (modelItem) {
    BMessage* msg = modelItem->Message();
    const char* model;
    if (msg && msg->FindString("model", &model) == B_OK)
      _WriteConfigFile("claude_model", model);
  }

  _WriteConfigFile("pippo_url", fPippoUrlControl->Text());

  const char* prompt = fSystemPromptView->Text();
  if (prompt && prompt[0] != '\0')
    _WriteConfigFile("system_prompt", prompt);

  // Tell MoeClaudeClient to reload
  MoeClaudeClient::Client()->ReloadSettings();
}


void
MoeSettingsWindow::_SaveMascotSettings(void)
{
  MoeProperty* property = MoeProperty::Property();

  // Wink
  BMenuItem* winkItem = fWinkField->Menu()->FindMarked();
  if (winkItem) {
    bigtime_t interval;
    if (winkItem->Message()->FindInt64("interval", &interval) == B_OK) {
      BMessage msg(MOE_SET_WINK_INTERVAL);
      msg.AddInt64("data", interval);
      property->PostMessage(&msg);
    }
  }

  // Polling
  BMenuItem* pollItem = fPollingField->Menu()->FindMarked();
  if (pollItem) {
    bigtime_t interval;
    if (pollItem->Message()->FindInt64("interval", &interval) == B_OK) {
      BMessage msg(MOE_SET_POLLING_INTERVAL);
      msg.AddInt64("data", interval);
      property->PostMessage(&msg);
    }
  }

  // Redraw
  BMenuItem* drawItem = fRedrawField->Menu()->FindMarked();
  if (drawItem) {
    bigtime_t interval;
    if (drawItem->Message()->FindInt64("interval", &interval) == B_OK) {
      BMessage msg(MOE_SET_REDRAW_INTERVAL);
      msg.AddInt64("data", interval);
      property->PostMessage(&msg);
    }
  }

  // Debug frame
  bool debugVisible = fDebugFrameCheck->Value() != 0;
  BMessage dbgMsg(MOE_SET_DEBUG_FRAME_VISIBLE);
  dbgMsg.AddBool("data", debugVisible);
  property->PostMessage(&dbgMsg);
}


void
MoeSettingsWindow::MessageReceived(BMessage* msg)
{
  switch (msg->what) {
    case kMsgSaveAI:
      _SaveAISettings();
      break;

    case kMsgModelSelect:
      break;

    case kMsgWinkSelect:
    {
      bigtime_t interval;
      if (msg->FindInt64("interval", &interval) == B_OK) {
        BMessage wmsg(MOE_SET_WINK_INTERVAL);
        wmsg.AddInt64("data", interval);
        MoeProperty::Property()->PostMessage(&wmsg);
      }
      break;
    }

    case kMsgPollSelect:
    {
      bigtime_t interval;
      if (msg->FindInt64("interval", &interval) == B_OK) {
        BMessage pmsg(MOE_SET_POLLING_INTERVAL);
        pmsg.AddInt64("data", interval);
        MoeProperty::Property()->PostMessage(&pmsg);
      }
      break;
    }

    case kMsgDrawSelect:
    {
      bigtime_t interval;
      if (msg->FindInt64("interval", &interval) == B_OK) {
        BMessage dmsg(MOE_SET_REDRAW_INTERVAL);
        dmsg.AddInt64("data", interval);
        MoeProperty::Property()->PostMessage(&dmsg);
      }
      break;
    }

    case kMsgDebugToggle:
    {
      bool visible = fDebugFrameCheck->Value() != 0;
      BMessage dbgMsg(MOE_SET_DEBUG_FRAME_VISIBLE);
      dbgMsg.AddBool("data", visible);
      MoeProperty::Property()->PostMessage(&dbgMsg);
      break;
    }

    case kMsgMascotSelect:
    {
      entry_ref ref;
      if (msg->FindRef("refs", &ref) == B_OK) {
        BMessage toggleMsg(MOE_MASCOT_TOGGLE);
        toggleMsg.AddRef("refs", &ref);
        be_app->PostMessage(&toggleMsg);
      }
      break;
    }

    case kMsgBrowseMascot:
    {
      static BFilePanel* sPanel = NULL;
      if (!sPanel) {
        sPanel = new BFilePanel(B_OPEN_PANEL,
          new BMessenger(be_app), NULL,
          B_FILE_NODE, false, NULL, NULL, true, true);
        sPanel->Window()->SetTitle(B_TRANSLATE("Choose mascot image"));
      }
      sPanel->SetPanelDirectory("/boot/home");
      sPanel->Show();
      break;
    }

    case kMsgTtsToggle:
      _WriteConfigFile("tts_enabled",
        fTtsEnabledCheck->Value() ? "1" : "0");
      break;

    case kMsgVoiceSelect:
    {
      const char* voice;
      if (msg->FindString("voice", &voice) == B_OK)
        _WriteConfigFile("tts_voice", voice);
      break;
    }

    case kMsgSpeedSelect:
    {
      const char* speed;
      if (msg->FindString("speed", &speed) == B_OK)
        _WriteConfigFile("tts_speed", speed);
      break;
    }

    default:
      BWindow::MessageReceived(msg);
      break;
  }
}


bool
MoeSettingsWindow::QuitRequested(void)
{
  // Save mascot settings on close
  _SaveMascotSettings();
  Hide();
  return false;
}
