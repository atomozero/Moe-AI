// -*- c++ -*-
//
//  'Moe' window sitter for BeOS.
//  Settings Window
//


#ifndef MOE_SETTINGS_WINDOW_H
#define MOE_SETTINGS_WINDOW_H


#include <Window.h>
#include <String.h>
#include <Entry.h>
#include <List.h>
#include <View.h>
#include <Bitmap.h>


class BTabView;
class BTextControl;
class BTextView;
class BMenuField;
class BCheckBox;
class BButton;
class BListView;
class BScrollView;
class BFilePanel;


// Custom view to display mascot preview bitmap
class MoeMascotPreviewView : public BView
{
public:
  MoeMascotPreviewView(void);
  virtual ~MoeMascotPreviewView(void);

  virtual void Draw(BRect updateRect);
  virtual void GetPreferredSize(float* width, float* height);

  void SetBitmap(BBitmap* bitmap);  // takes ownership

private:
  BRect _FindContentRect(BBitmap* bitmap);

  BBitmap* fBitmap;
  BRect    fContentRect;  // bounding box of non-transparent pixels
};


class MoeSettingsWindow : public BWindow
{
public:
  static MoeSettingsWindow* Window(void);

  void ShowNear(BRect mascotFrame);

protected:
  virtual void MessageReceived(BMessage* msg);
  virtual bool QuitRequested(void);

private:
  MoeSettingsWindow(void);
  virtual ~MoeSettingsWindow(void);

  void _LoadSettings(void);
  void _SaveAISettings(void);
  void _SaveMascotSettings(void);
  void _PopulateMascotList(void);
  void _UpdatePreview(int32 index);
  BPath _MascotsPath(void);

  // AI tab
  BTextControl* fApiKeyControl;
  BMenuField*   fModelField;
  BTextControl* fPippoUrlControl;
  BTextView*    fSystemPromptView;

  // Mascot tab
  BListView*            fMascotList;
  BScrollView*          fMascotListScroll;
  MoeMascotPreviewView* fPreviewView;
  BButton*              fAddButton;
  BButton*              fRemoveButton;
  BButton*              fActivateButton;
  BFilePanel*           fAddPanel;
  BMenuField*           fWinkField;
  BMenuField*           fPollingField;
  BMenuField*           fRedrawField;
  BCheckBox*            fDebugFrameCheck;

  // Mascot entry_refs (parallel to list items)
  BList                 fMascotRefs;

  // Voice tab
  BCheckBox*    fTtsEnabledCheck;
  BMenuField*   fVoiceField;
  BMenuField*   fSpeedField;
};


#endif
