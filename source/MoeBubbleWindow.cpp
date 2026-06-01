// -*- c++ -*-
//
//  'Moe' window sitter for BeOS.
//  Speech Bubble Chat Window
//


#include <cstring>
#include <Application.h>
#include <Button.h>
#include <Font.h>
#include <GroupLayout.h>
#include <LayoutBuilder.h>
#include <Message.h>
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
    fBusy(false)
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
                                false, true);

  fInput = new BTextControl("input", NULL, "",
                            new BMessage(MOE_CHAT_SEND));

  fExpandButton = new BButton("clear",
                              "\xc3\x97",  // × UTF-8
                              new BMessage(MOE_CHAT_CLEAR));
  fExpandButton->SetToolTip(B_TRANSLATE("Clear conversation"));

  // Build layout inside the bubble view
  float inset = kPadding + kCornerRadius / 2;
  bg->SetLayout(new BGroupLayout(B_VERTICAL, 4));
  BLayoutBuilder::Group<>((BGroupLayout*)bg->GetLayout())
    .SetInsets(inset, inset, inset, kTailHeight + kPadding)
    .Add(fScrollView, 10)
    .AddGroup(B_HORIZONTAL, 2)
      .Add(fInput, 10)
      .Add(fExpandButton, 0)
    .End();

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
  float mascotCY = mascotFrame.top + mascotFrame.Height() / 2;

  // How much space in each direction?
  float spaceAbove = mascotFrame.top - screen.top;
  float spaceBelow = screen.bottom - mascotFrame.bottom;
  float spaceLeft  = mascotFrame.left - screen.left;
  float spaceRight = screen.right - mascotFrame.right;

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


void
MoeBubbleWindow::SetResponse(const char* text)
{
  // Remove the "Thinking..." line if present
  rgb_color moeColor = {0, 100, 60, 255};
  BString msg;
  msg << "Moe: " << text << "\n";
  _AppendStyled(msg.String(), moeColor, false);
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
    rgb_color gray = {150, 150, 150, 255};
    _AppendStyled(B_TRANSLATE("Thinking" B_UTF8_ELLIPSIS "\n"), gray, false);
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

    default:
      BWindow::MessageReceived(msg);
      break;
  }
}


bool
MoeBubbleWindow::QuitRequested(void)
{
  Hide();
  return false;
}


void
MoeBubbleWindow::WindowActivated(bool active)
{
  if (active)
    fInput->MakeFocus(true);
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
