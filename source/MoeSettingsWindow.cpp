// -*- c++ -*-
//
//  'Moe' window sitter for BeOS.
//  Settings Window
//


#include <cstdio>
#include <cstring>
#include <AppFileInfo.h>
#include <Application.h>
#include <Alert.h>
#include <Bitmap.h>
#include <Box.h>
#include <Button.h>
#include <CheckBox.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FilePanel.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <ListItem.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <Messenger.h>
#include <NodeInfo.h>
#include <Path.h>
#include <PopUpMenu.h>
#include <Roster.h>
#include <Screen.h>
#include <ScrollView.h>
#include <Size.h>
#include <StringView.h>
#include <TabView.h>
#include <TextControl.h>
#include <TextView.h>
#include <TranslationUtils.h>
#include <Catalog.h>
#include <fs_attr.h>
#include "MoeDefs.h"
#include "MoeProperty.h"
#include "MoeSettingsWindow.h"
#include "MoeClaudeClient.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "SettingsWindow"


static MoeSettingsWindow* sSettingsWindow = NULL;

// Message codes for internal use
enum {
  kMsgSaveAI         = 'SsAI',
  kMsgModelSelect    = 'SmSl',
  kMsgWinkSelect     = 'SwSl',
  kMsgPollSelect     = 'SpSl',
  kMsgDrawSelect     = 'SdSl',
  kMsgDebugToggle    = 'SdTg',
  kMsgMascotSelect   = 'SmcS',
  kMsgMascotActivate = 'SmcA',
  kMsgMascotAdd      = 'SmcD',
  kMsgMascotRemove   = 'SmcR',
  kMsgMascotAdded    = 'SmcE',
  kMsgMascotRefresh  = 'SmcF',
  kMsgTtsToggle      = 'StTg',
  kMsgVoiceSelect    = 'SvSl',
  kMsgSpeedSelect    = 'SsSl',
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


// ======== MoeMascotPreviewView ========

MoeMascotPreviewView::MoeMascotPreviewView(void)
  : BView("preview", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE)
  , fBitmap(NULL)
{
  SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
  SetExplicitMinSize(BSize(160, 160));
  SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED));
}


MoeMascotPreviewView::~MoeMascotPreviewView(void)
{
  delete fBitmap;
}


void
MoeMascotPreviewView::Draw(BRect updateRect)
{
  BRect bounds = Bounds();

  if (!fBitmap) {
    SetHighColor(ViewColor());
    FillRect(bounds);

    // Draw placeholder text
    SetHighColor(tint_color(ViewColor(), B_DARKEN_2_TINT));
    const char* text = B_TRANSLATE("No mascot selected");
    float textWidth = StringWidth(text);
    font_height fh;
    GetFontHeight(&fh);
    DrawString(text,
      BPoint((bounds.Width() - textWidth) / 2,
             bounds.Height() / 2 + fh.ascent / 2));
    return;
  }

  // Clear background
  SetHighColor(ViewColor());
  FillRect(bounds);

  // Use the content rect (visible area) for centering,
  // so the character appears centered even if it's not
  // centered in the source bitmap.
  float cw = fContentRect.Width() + 1;
  float ch = fContentRect.Height() + 1;
  float vw = bounds.Width() + 1;
  float vh = bounds.Height() + 1;

  float scale = std::min(vw / cw, vh / ch);
  // Don't scale up beyond 2x
  if (scale > 2.0f)
    scale = 2.0f;

  float dw = cw * scale;
  float dh = ch * scale;
  float dx = (vw - dw) / 2;
  float dy = (vh - dh) / 2;

  BRect destRect(dx, dy, dx + dw - 1, dy + dh - 1);

  SetDrawingMode(B_OP_ALPHA);
  SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
  DrawBitmap(fBitmap, fContentRect, destRect);
  SetDrawingMode(B_OP_COPY);
}


void
MoeMascotPreviewView::GetPreferredSize(float* width, float* height)
{
  *width = 160;
  *height = 160;
}


void
MoeMascotPreviewView::SetBitmap(BBitmap* bitmap)
{
  delete fBitmap;
  fBitmap = bitmap;
  if (fBitmap)
    fContentRect = _FindContentRect(fBitmap);
  Invalidate();
}


BRect
MoeMascotPreviewView::_FindContentRect(BBitmap* bitmap)
{
  BRect bounds = bitmap->Bounds();
  int32 width = bounds.IntegerWidth() + 1;
  int32 height = bounds.IntegerHeight() + 1;
  int32 bpr = bitmap->BytesPerRow() / sizeof(rgb_color);
  const rgb_color* bits =
    reinterpret_cast<const rgb_color*>(bitmap->Bits());

  int32 minX = width, minY = height, maxX = -1, maxY = -1;

  for (int32 y = 0; y < height; y++)
    {
      for (int32 x = 0; x < width; x++)
	{
	  const rgb_color& pixel = bits[y * bpr + x];
	  if (pixel.alpha > 0)
	    {
	      if (x < minX) minX = x;
	      if (x > maxX) maxX = x;
	      if (y < minY) minY = y;
	      if (y > maxY) maxY = y;
	    }
	}
    }

  // If no visible content found, return full bounds
  if (maxX < 0)
    return bounds;

  return BRect(minX, minY, maxX, maxY);
}


// ======== MoeSettingsWindow ========

MoeSettingsWindow*
MoeSettingsWindow::Window(void)
{
  if (!sSettingsWindow)
    sSettingsWindow = new MoeSettingsWindow();
  return sSettingsWindow;
}


MoeSettingsWindow::MoeSettingsWindow(void)
  : BWindow(BRect(200, 200, 750, 650),
            B_TRANSLATE("Moe-AI Settings"),
            B_TITLED_WINDOW_LOOK,
            B_NORMAL_WINDOW_FEEL,
            B_AUTO_UPDATE_SIZE_LIMITS
            | B_CLOSE_ON_ESCAPE)
  , fAddPanel(NULL)
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

  // Mascot list view
  fMascotList = new BListView("mascot_list", B_SINGLE_SELECTION_LIST);
  fMascotList->SetSelectionMessage(new BMessage(kMsgMascotSelect));
  fMascotList->SetInvocationMessage(new BMessage(kMsgMascotActivate));
  fMascotListScroll = new BScrollView("mascot_list_scroll",
                                      fMascotList,
                                      B_WILL_DRAW | B_FRAME_EVENTS,
                                      false, true);

  // Preview
  fPreviewView = new MoeMascotPreviewView();

  // Buttons
  fActivateButton = new BButton("activate",
    B_TRANSLATE("Set active"),
    new BMessage(kMsgMascotActivate));
  fAddButton = new BButton("add",
    B_TRANSLATE("Add" B_UTF8_ELLIPSIS),
    new BMessage(kMsgMascotAdd));
  fRemoveButton = new BButton("remove",
    B_TRANSLATE("Remove"),
    new BMessage(kMsgMascotRemove));
  fRemoveButton->SetEnabled(false);

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

  // Layout: top half is list + preview side by side, bottom is settings
  BLayoutBuilder::Group<>(mascotTab, B_VERTICAL, 4)
    .SetInsets(B_USE_DEFAULT_SPACING)
    .AddGroup(B_HORIZONTAL, 4, 3)
      .Add(fMascotListScroll, 2)
      .Add(fPreviewView, 3)
    .End()
    .AddGroup(B_HORIZONTAL, 4)
      .Add(fActivateButton)
      .Add(fAddButton)
      .Add(fRemoveButton)
      .AddGlue()
    .End()
    .AddGroup(B_HORIZONTAL, 8)
      .Add(fWinkField)
      .Add(fPollingField)
    .End()
    .AddGroup(B_HORIZONTAL, 8)
      .Add(fRedrawField)
      .AddGlue()
    .End()
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
  SetSizeLimits(450, 1000, 400, 800);
  ResizeTo(550, 500);
  CenterOnScreen();

  _LoadSettings();
  _PopulateMascotList();
}


MoeSettingsWindow::~MoeSettingsWindow(void)
{
  sSettingsWindow = NULL;
  delete fAddPanel;

  // Clean up entry_ref list
  for (int32 i = 0; i < fMascotRefs.CountItems(); i++)
    delete reinterpret_cast<entry_ref*>(fMascotRefs.ItemAt(i));
}


BPath
MoeSettingsWindow::_MascotsPath(void)
{
  app_info appInfo;
  be_app->GetAppInfo(&appInfo);
  BEntry appEntry(&appInfo.ref);
  BPath appPath;
  appEntry.GetPath(&appPath);
  appPath.GetParent(&appPath);
  appPath.GetParent(&appPath);

  // Check if mascots/ exists here
  BPath testPath(appPath);
  testPath.Append("mascots");
  BDirectory testDir(testPath.Path());
  if (testDir.InitCheck() != B_OK)
    appPath.GetParent(&appPath);

  BPath mascotsPath(appPath);
  mascotsPath.Append("mascots");
  return mascotsPath;
}


void
MoeSettingsWindow::_PopulateMascotList(void)
{
  // This function modifies BViews (BListView, preview, buttons)
  // and MUST be called from the window's own thread only.
  // Use PostMessage(kMsgMascotRefresh) from other threads.

  // Clear existing
  while (fMascotList->CountItems() > 0) {
    BListItem* item = fMascotList->RemoveItem((int32)0);
    delete item;
  }
  for (int32 i = 0; i < fMascotRefs.CountItems(); i++)
    delete reinterpret_cast<entry_ref*>(fMascotRefs.ItemAt(i));
  fMascotRefs.MakeEmpty();

  // Query active mascots from MoeApplication
  BMessage query(MOE_QUERY_MASCOTS);
  BMessage reply;
  BList activeRefs;

  BMessenger appMessenger(be_app);
  if (appMessenger.SendMessage(&query, &reply, 1000000, 1000000) == B_OK) {
    entry_ref ref;
    for (int32 i = 0; reply.FindRef("refs", i, &ref) == B_OK; i++)
      activeRefs.AddItem(new entry_ref(ref));
  }

  // Scan mascots/ directory
  BPath mascotsPath = _MascotsPath();
  BDirectory dir(mascotsPath.Path());

  if (dir.InitCheck() == B_OK) {
    BEntry entry;
    while (dir.GetNextEntry(&entry) == B_OK) {
      BPath filePath;
      entry.GetPath(&filePath);
      BString name(filePath.Leaf());

      // Only show image files
      if (name.FindLast(".png") < 0 && name.FindLast(".jpg") < 0
          && name.FindLast(".bmp") < 0 && name.FindLast(".gif") < 0)
        continue;

      entry_ref ref;
      entry.GetRef(&ref);

      // Check if this mascot is active
      bool isActive = false;
      for (int32 j = 0; j < activeRefs.CountItems(); j++) {
        entry_ref* activeRef = reinterpret_cast<entry_ref*>(activeRefs.ItemAt(j));
        if (*activeRef == ref) {
          isActive = true;
          break;
        }
      }

      // Build display name with active indicator
      BString displayName;
      if (isActive)
        displayName << B_UTF8_BULLET " ";
      // Strip extension for display
      int32 dotPos = name.FindLast('.');
      if (dotPos > 0)
        name.Truncate(dotPos);
      // Capitalize first letter
      if (name.Length() > 0) {
        char first = name.ByteAt(0);
        if (first >= 'a' && first <= 'z')
          name.SetByteAt(0, first - 'a' + 'A');
      }
      displayName << name;

      fMascotList->AddItem(new BStringItem(displayName.String()));
      fMascotRefs.AddItem(new entry_ref(ref));
    }
  }

  // Clean up active refs
  for (int32 i = 0; i < activeRefs.CountItems(); i++)
    delete reinterpret_cast<entry_ref*>(activeRefs.ItemAt(i));

  // Select first item if any
  if (fMascotList->CountItems() > 0) {
    fMascotList->Select(0);
    _UpdatePreview(0);
  }
}


void
MoeSettingsWindow::_UpdatePreview(int32 index)
{
  if (index < 0 || index >= fMascotRefs.CountItems()) {
    fPreviewView->SetBitmap(NULL);
    fRemoveButton->SetEnabled(false);
    return;
  }

  entry_ref* ref = reinterpret_cast<entry_ref*>(fMascotRefs.ItemAt(index));
  BBitmap* bitmap = BTranslationUtils::GetBitmap(ref);
  fPreviewView->SetBitmap(bitmap);
  fRemoveButton->SetEnabled(true);
}


void
MoeSettingsWindow::ShowNear(BRect mascotFrame)
{
  BRect screen = BScreen().Frame();
  BRect frame = Frame();
  float w = frame.Width();
  float h = frame.Height();

  float mascotCX = mascotFrame.left + mascotFrame.Width() / 2;

  float x, y;

  float spaceRight = screen.right - mascotFrame.right;
  float spaceLeft = mascotFrame.left - screen.left;

  if (spaceRight >= w + 10)
    x = mascotFrame.right + 10;
  else if (spaceLeft >= w + 10)
    x = mascotFrame.left - w - 10;
  else
    x = mascotCX - w / 2;

  y = mascotFrame.top;

  if (x < screen.left + 5) x = screen.left + 5;
  if (x + w > screen.right - 5) x = screen.right - w - 5;
  if (y < screen.top + 5) y = screen.top + 5;
  if (y + h > screen.bottom - 5) y = screen.bottom - h - 5;

  MoveTo(x, y);

  if (IsHidden())
    Show();
  Activate();

  // Post a message to refresh the mascot list on the window's own
  // thread. This is critical: _PopulateMascotList() modifies BViews
  // and must run on the window thread, not the application thread.
  PostMessage(kMsgMascotRefresh);
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

  MoeClaudeClient::Client()->ReloadSettings();
}


void
MoeSettingsWindow::_SaveMascotSettings(void)
{
  MoeProperty* property = MoeProperty::Property();

  BMenuItem* winkItem = fWinkField->Menu()->FindMarked();
  if (winkItem) {
    bigtime_t interval;
    if (winkItem->Message()->FindInt64("interval", &interval) == B_OK) {
      BMessage msg(MOE_SET_WINK_INTERVAL);
      msg.AddInt64("data", interval);
      property->PostMessage(&msg);
    }
  }

  BMenuItem* pollItem = fPollingField->Menu()->FindMarked();
  if (pollItem) {
    bigtime_t interval;
    if (pollItem->Message()->FindInt64("interval", &interval) == B_OK) {
      BMessage msg(MOE_SET_POLLING_INTERVAL);
      msg.AddInt64("data", interval);
      property->PostMessage(&msg);
    }
  }

  BMenuItem* drawItem = fRedrawField->Menu()->FindMarked();
  if (drawItem) {
    bigtime_t interval;
    if (drawItem->Message()->FindInt64("interval", &interval) == B_OK) {
      BMessage msg(MOE_SET_REDRAW_INTERVAL);
      msg.AddInt64("data", interval);
      property->PostMessage(&msg);
    }
  }

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

    case kMsgMascotRefresh:
      _PopulateMascotList();
      break;

    case kMsgMascotSelect:
    {
      int32 index = fMascotList->CurrentSelection();
      _UpdatePreview(index);
      break;
    }

    case kMsgMascotActivate:
    {
      int32 index = fMascotList->CurrentSelection();
      if (index < 0 || index >= fMascotRefs.CountItems())
        break;

      entry_ref* ref = reinterpret_cast<entry_ref*>(fMascotRefs.ItemAt(index));
      BMessage toggleMsg(MOE_MASCOT_REPLACE);
      toggleMsg.AddRef("refs", ref);
      be_app->PostMessage(&toggleMsg);

      // Refresh the list after a short delay to show updated active state
      BMessage refreshMsg(kMsgMascotSelect);
      BMessenger(this).SendMessage(&refreshMsg);
      // Re-populate to update active indicators
      _PopulateMascotList();
      break;
    }

    case kMsgMascotAdd:
    {
      if (!fAddPanel) {
        BMessage* panelMsg = new BMessage(kMsgMascotAdded);
        fAddPanel = new BFilePanel(B_OPEN_PANEL,
          new BMessenger(this), NULL,
          B_FILE_NODE, false, panelMsg, NULL, true, true);
        fAddPanel->Window()->SetTitle(
          B_TRANSLATE("Choose mascot image to add"));
      }
      fAddPanel->SetPanelDirectory("/boot/home");
      fAddPanel->Show();
      break;
    }

    case kMsgMascotAdded:
    {
      // File selected from panel - copy to mascots/ directory
      entry_ref srcRef;
      if (msg->FindRef("refs", &srcRef) != B_OK)
        break;

      BMessage addMsg(MOE_MASCOT_ADD);
      addMsg.AddRef("refs", &srcRef);
      BMessage reply;
      BMessenger(be_app).SendMessage(&addMsg, &reply, 2000000, 2000000);

      // Refresh list
      _PopulateMascotList();
      break;
    }

    case kMsgMascotRemove:
    {
      int32 index = fMascotList->CurrentSelection();
      if (index < 0 || index >= fMascotRefs.CountItems())
        break;

      entry_ref* ref = reinterpret_cast<entry_ref*>(fMascotRefs.ItemAt(index));
      BString text(B_TRANSLATE("Remove mascot \"%name%\" from the collection?"));
      text.ReplaceAll("%name%", ref->name);

      BAlert* alert = new BAlert(
        B_TRANSLATE("Remove mascot"),
        text.String(),
        B_TRANSLATE("Remove"),
        B_TRANSLATE("Cancel"),
        NULL,
        B_WIDTH_AS_USUAL,
        B_WARNING_ALERT);
      alert->SetShortcut(1, B_ESCAPE);

      if (alert->Go() == 0) {
        BMessage removeMsg(MOE_MASCOT_REMOVE);
        removeMsg.AddRef("refs", ref);
        be_app->PostMessage(&removeMsg);

        // Refresh list
        _PopulateMascotList();
      }
      break;
    }

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
  _SaveMascotSettings();
  Hide();
  return false;
}
