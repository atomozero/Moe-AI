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


#include <Application.h>
#include <Button.h>
#include <Font.h>
#include <LayoutBuilder.h>
#include <Message.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TextControl.h>
#include <TextView.h>
#include <Catalog.h>
#include "MoeDefs.h"
#include "MoeChatWindow.h"
#include "MoeClaudeClient.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "ChatWindow"


static MoeChatWindow* sChatWindow = NULL;


MoeChatWindow*
MoeChatWindow::Window(void)
{
  if (!sChatWindow)
    sChatWindow = new MoeChatWindow();
  return sChatWindow;
}


MoeChatWindow::MoeChatWindow(void)
  : BWindow(BRect(200, 200, 700, 600),
            B_TRANSLATE("Moe AI Chat"),
            B_TITLED_WINDOW,
            B_AUTO_UPDATE_SIZE_LIMITS),
    fBusy(false)
{
  fChatView = new BTextView("chat_view");
  fChatView->MakeEditable(false);
  fChatView->MakeSelectable(true);
  fChatView->SetStylable(true);
  fChatView->SetWordWrap(true);

  fScrollView = new BScrollView("scroll_view", fChatView,
                                B_WILL_DRAW | B_FRAME_EVENTS,
                                false, true);

  fInput = new BTextControl("input", NULL, "",
                            new BMessage(MOE_CHAT_SEND));
  fInput->SetModificationMessage(NULL);

  fSendButton = new BButton("send", B_TRANSLATE("Send"),
                            new BMessage(MOE_CHAT_SEND));
  fSendButton->MakeDefault(true);

  fClearButton = new BButton("clear", B_TRANSLATE("Clear"),
                             new BMessage(MOE_CHAT_CLEAR));

  BLayoutBuilder::Group<>(this, B_VERTICAL, 4)
    .SetInsets(B_USE_WINDOW_INSETS)
    .Add(fScrollView, 10)
    .AddGroup(B_HORIZONTAL, 4)
      .Add(fInput, 10)
      .Add(fSendButton, 1)
      .Add(fClearButton, 1)
    .End()
  .End();

  AppendSystem(B_TRANSLATE("Welcome! Ask Moe anything, or ask it to "
               "control your Haiku system.\n"));
}


MoeChatWindow::~MoeChatWindow(void)
{
  sChatWindow = NULL;
}


void
MoeChatWindow::AppendUser(const char* text)
{
  rgb_color blue = {0, 0, 180, 255};
  _AppendText(B_TRANSLATE("You: "), text, blue, true);
}


void
MoeChatWindow::AppendAssistant(const char* text)
{
  rgb_color black = {0, 0, 0, 255};
  _AppendText(B_TRANSLATE("Moe: "), text, black, true);
}


void
MoeChatWindow::AppendSystem(const char* text)
{
  rgb_color gray = {100, 100, 100, 255};
  _AppendText("", text, gray, false);
}


void
MoeChatWindow::AppendToolProgress(const char* toolName)
{
  BString msg;
  msg << B_TRANSLATE("  [Using tool: ") << toolName << "]\n";
  rgb_color toolColor = {120, 80, 0, 255};
  _AppendText("", msg.String(), toolColor, false);
}


void
MoeChatWindow::SetBusy(bool busy)
{
  fBusy = busy;
  fInput->SetEnabled(!busy);
  fSendButton->SetEnabled(!busy);
  if (busy) {
    rgb_color thinking = {100, 100, 100, 255};
    _AppendText("", B_TRANSLATE("Moe is thinking...\n"), thinking, false);
  }
}


void
MoeChatWindow::MessageReceived(BMessage* msg)
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

      AppendUser(userText.String());
      SetBusy(true);

      MoeClaudeClient::Client()->SendChat(userText.String(),
                                          BMessenger(this));
      break;
    }

    case MOE_CHAT_RESPONSE:
    {
      const char* text;
      if (msg->FindString("text", &text) == B_OK)
        AppendAssistant(text);
      SetBusy(false);
      break;
    }

    case MOE_CHAT_ERROR:
    {
      const char* error;
      if (msg->FindString("error", &error) == B_OK) {
        BString errMsg;
        errMsg << B_TRANSLATE("Error: ") << error << "\n";
        rgb_color red = {200, 0, 0, 255};
        _AppendText("", errMsg.String(), red, true);
      }
      SetBusy(false);
      break;
    }

    case MOE_CHAT_TOOL_PROGRESS:
    {
      const char* toolName;
      if (msg->FindString("tool", &toolName) == B_OK)
        AppendToolProgress(toolName);
      break;
    }

    case MOE_CHAT_CLEAR:
    {
      fChatView->SetText("");
      MoeClaudeClient::Client()->ClearHistory();
      AppendSystem(B_TRANSLATE("Conversation cleared.\n"));
      break;
    }

    default:
      BWindow::MessageReceived(msg);
      break;
  }
}


void
MoeChatWindow::DispatchMessage(BMessage* msg, BHandler* handler)
{
  if (msg->what == B_QUIT_REQUESTED) {
    if (!IsHidden())
      Hide();
    return;
  }
  BWindow::DispatchMessage(msg, handler);
}


void
MoeChatWindow::_AppendText(const char* prefix, const char* text,
                           rgb_color color, bool bold)
{
  BFont font(be_plain_font);
  if (bold)
    font.SetFace(B_BOLD_FACE);

  int32 start = fChatView->TextLength();

  BString full;
  full << prefix << text;
  if (full.Length() == 0 || full[full.Length() - 1] != '\n')
    full << "\n";

  fChatView->Insert(fChatView->TextLength(), full.String(), full.Length());

  int32 end = fChatView->TextLength();
  fChatView->SetFontAndColor(start, end, &font, B_FONT_ALL, &color);

  _ScrollToBottom();
}


void
MoeChatWindow::_ScrollToBottom(void)
{
  float height = fChatView->TextHeight(0, fChatView->CountLines() - 1);
  BRect bounds = fChatView->Bounds();
  float scrollTo = height - bounds.Height();
  if (scrollTo > 0)
    fChatView->ScrollTo(0, scrollTo);
}
