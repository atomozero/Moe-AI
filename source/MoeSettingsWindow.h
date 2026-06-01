// -*- c++ -*-
//
//  'Moe' window sitter for BeOS.
//  Settings Window
//


#ifndef MOE_SETTINGS_WINDOW_H
#define MOE_SETTINGS_WINDOW_H


#include <Window.h>
#include <String.h>


class BTabView;
class BTextControl;
class BTextView;
class BMenuField;
class BCheckBox;
class BButton;


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

  // AI tab
  BTextControl* fApiKeyControl;
  BMenuField*   fModelField;
  BTextControl* fPippoUrlControl;
  BTextView*    fSystemPromptView;

  // Mascot tab
  BMenuField*   fWinkField;
  BMenuField*   fPollingField;
  BMenuField*   fRedrawField;
  BCheckBox*    fDebugFrameCheck;
};


#endif
