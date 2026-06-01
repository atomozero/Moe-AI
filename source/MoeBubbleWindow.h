// -*- c++ -*-
//
//  'Moe' window sitter for BeOS.
//  Speech Bubble Chat Window
//


#ifndef MOE_BUBBLE_WINDOW_H
#define MOE_BUBBLE_WINDOW_H


#include <Window.h>
#include <String.h>
#include <Point.h>
#include <Rect.h>


class BTextView;
class BScrollView;
class BTextControl;
class BButton;


class MoeBubbleWindow : public BWindow
{
public:
  static MoeBubbleWindow* Window(void);

  void ShowNear(BRect mascotFrame);
  void SetResponse(const char* text);
  void SetError(const char* text);
  void SetToolProgress(const char* toolName);
  void SetBusy(bool busy);

protected:
  virtual void MessageReceived(BMessage* msg);
  virtual bool QuitRequested(void);
  virtual void WindowActivated(bool active);
  virtual void WorkspaceActivated(int32 workspace, bool active);

private:
  MoeBubbleWindow(void);
  virtual ~MoeBubbleWindow(void);

  void _ClearResponse(void);
  void _AppendStyled(const char* text, rgb_color color, bool bold);
  void _ScrollToBottom(void);

  BView*        fBubbleView;
  BTextView*    fResponseView;
  BScrollView*  fScrollView;
  BTextControl* fInput;
  BButton*      fExpandButton;
  bool          fBusy;
};


#endif
