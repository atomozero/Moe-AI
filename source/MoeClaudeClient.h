// -*- c++ -*-
//
//  'Moe' window sitter for BeOS.
//  Claude API Client
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//


#ifndef MOE_CLAUDE_CLIENT_H
#define MOE_CLAUDE_CLIENT_H


#include <Looper.h>
#include <Messenger.h>
#include <String.h>
#include <List.h>

namespace BPrivate {
namespace Network {
class BHttpSession;
}
}


class MoeClaudeClient : public BLooper
{
public:
  static MoeClaudeClient* Client(void);

  void    SendChat(const char* userText, BMessenger replyTo);
  void    ClearHistory(void);
  void    ReloadSettings(void);
  bool    HasApiKey(void) const;

protected:
  virtual void MessageReceived(BMessage* msg);

private:
  MoeClaudeClient(void);
  virtual ~MoeClaudeClient(void);

  void    _LoadApiKey(void);
  void    _LoadSettings(void);
  bool    _PromptForApiKey(void);
  BString _BuildRequestBody(const char* userText);
  BString _CallClaudeApi(const BString& requestBody);
  BString _CallPippoTool(const BString& toolName, const BString& argsJson);
  BString _FetchToolSchemas(void);
  void    _ProcessResponse(const BString& response, BMessenger replyTo);
  void    _AddToHistory(const char* role, const char* contentJson);
  void    _TrimHistory(void);

  BPrivate::Network::BHttpSession* fHttpSession;
  BList   fHistory;       // BString* entries (JSON message objects)
  BString fApiKey;
  BString fModel;
  BString fPippoUrl;
  BString fSystemPrompt;
  BString fToolSchemas;   // cached tools array JSON
  bigtime_t fToolSchemasFetchedAt;

  static const int32 kMaxHistoryPairs = 20;
  static const int32 kMaxToolLoopIterations = 10;
};


#endif
