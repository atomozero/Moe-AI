// -*- c++ -*-
//
//  'Moe' window sitter for BeOS.
//  AI Chat Window
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//


#ifndef MOE_CHAT_WINDOW_H
#define MOE_CHAT_WINDOW_H


#include <Window.h>
#include <String.h>


class BTextView;
class BScrollView;
class BTextControl;
class BButton;


class MoeChatWindow : public BWindow
{
public:
  static MoeChatWindow* Window(void);

  void AppendUser(const char* text);
  void AppendAssistant(const char* text);
  void AppendSystem(const char* text);
  void AppendToolProgress(const char* toolName);
  void SetBusy(bool busy);

protected:
  virtual void MessageReceived(BMessage* msg);
  virtual void DispatchMessage(BMessage* msg, BHandler* handler);

private:
  MoeChatWindow(void);
  virtual ~MoeChatWindow(void);

  void _AppendText(const char* prefix, const char* text,
                   rgb_color color, bool bold);
  void _ScrollToBottom(void);

  BTextView*    fChatView;
  BScrollView*  fScrollView;
  BTextControl* fInput;
  BButton*      fSendButton;
  BButton*      fClearButton;
  bool          fBusy;
};


#endif
