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


#include <cstdio>
#include <memory>
#include <Alert.h>
#include <Application.h>
#include <Directory.h>
#include <File.h>
#include <HttpFields.h>
#include <HttpRequest.h>
#include <HttpResult.h>
#include <HttpSession.h>
#include <DataIO.h>
#include <Message.h>
#include <Path.h>
#include <TextControl.h>
#include <Url.h>
#include <String.h>
#include <OS.h>
#include <Catalog.h>
#include "MoeDefs.h"
#include "MoeActiveWindowWatcher.h"
#include "MoeJsonHelper.h"
#include "MoeClaudeClient.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "ClaudeClient"

using namespace BPrivate::Network;


static const char* kClaudeApiUrl =
  "https://api.anthropic.com/v1/messages";
static const char* kAnthropicVersion =
  "2023-06-01";
static const char* kDefaultModel =
  "claude-sonnet-4-20250514";
static const char* kDefaultPippoUrl =
  "http://127.0.0.1:2607/mcp";
static const char* kDefaultSystemPrompt =
  "You are Moe, a cute and helpful desktop mascot living on Haiku OS. "
  "You are an AI assistant powered by Claude. You can help the user with "
  "questions and also control their Haiku system using the tools available to you. "
  "Be concise, friendly, and helpful. When the user asks you to do something "
  "on their system, use the appropriate tool. When performing destructive actions "
  "(quitting apps, deleting files, etc.), confirm with the user first. "
  "Respond in the same language the user writes to you. "
  "IMPORTANT: Always start your response with a language tag like [lang:it] or [lang:en] "
  "indicating the language you are responding in (ISO 639-1 code). "
  "This tag will be used for text-to-speech and will not be shown to the user.";


static MoeClaudeClient* sClient = NULL;


MoeClaudeClient*
MoeClaudeClient::Client(void)
{
  if (!sClient) {
    sClient = new MoeClaudeClient();
    sClient->Run();
  }
  return sClient;
}


MoeClaudeClient::MoeClaudeClient(void)
  : BLooper("MoeClaudeClient"),
    fHttpSession(new BHttpSession()),
    fModel(kDefaultModel),
    fPippoUrl(kDefaultPippoUrl),
    fSystemPrompt(kDefaultSystemPrompt),
    fToolSchemasFetchedAt(0),
    fToolLoopCount(0)
{
  _LoadApiKey();
  _LoadSettings();
  _LoadHistory();
}


MoeClaudeClient::~MoeClaudeClient(void)
{
  delete fHttpSession;
  // Free history entries
  for (int32 i = 0; i < fHistory.CountItems(); i++)
    delete (BString*)fHistory.ItemAt(i);
  fHistory.MakeEmpty();
  sClient = NULL;
}


void
MoeClaudeClient::_LoadApiKey(void)
{
  BString path(MOE_CONFIG_DIRECTORY);
  path << "claude_api_key";

  BFile file(path.String(), B_READ_ONLY);
  if (file.InitCheck() != B_OK)
    return;

  off_t size;
  file.GetSize(&size);
  if (size <= 0 || size > 256)
    return;

  char* buf = fApiKey.LockBuffer(size + 1);
  file.Read(buf, size);
  buf[size] = '\0';
  fApiKey.UnlockBuffer(size);
  fApiKey.Trim();
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
MoeClaudeClient::_LoadSettings(void)
{
  fModel = _ReadConfigString("claude_model", kDefaultModel);
  fPippoUrl = _ReadConfigString("pippo_url", kDefaultPippoUrl);
  fSystemPrompt = _ReadConfigString("system_prompt", kDefaultSystemPrompt);
}


void
MoeClaudeClient::ReloadSettings(void)
{
  _LoadApiKey();
  _LoadSettings();
  // Force re-fetch tool schemas on next request
  fToolSchemasFetchedAt = 0;
}


bool
MoeClaudeClient::_PromptForApiKey(void)
{
  BString msg;
  msg << B_TRANSLATE("Claude API key not found.\n\n"
         "Please create the file:\n")
      << MOE_CONFIG_DIRECTORY << "claude_api_key\n\n"
      << B_TRANSLATE("and paste your API key in it.\n"
         "Or use Settings to configure it.");

  // BAlert::Go() with no invoker is synchronous and deletes the alert
  (new BAlert(
    B_TRANSLATE("API Key Required"),
    msg.String(),
    B_TRANSLATE("OK"),
    NULL, NULL, B_WIDTH_AS_USUAL, B_WARNING_ALERT))->Go();

  // Try loading again in case user created it
  _LoadApiKey();
  return fApiKey.Length() > 0;
}


bool
MoeClaudeClient::HasApiKey(void) const
{
  return fApiKey.Length() > 0;
}


void
MoeClaudeClient::SendChat(const char* userText, BMessenger replyTo)
{
  BMessage* msg = new BMessage(MOE_CLAUDE_REQUEST);
  msg->AddString("text", userText);
  msg->AddMessenger("reply_to", replyTo);
  PostMessage(msg);
}


void
MoeClaudeClient::ClearHistory(void)
{
  BMessage msg(MOE_CHAT_CLEAR);
  PostMessage(&msg);
}


void
MoeClaudeClient::MessageReceived(BMessage* msg)
{
  switch (msg->what) {
    case MOE_CLAUDE_REQUEST:
    {
      const char* userText;
      BMessenger replyTo;
      if (msg->FindString("text", &userText) != B_OK
          || msg->FindMessenger("reply_to", &replyTo) != B_OK)
        break;

      // Check API key
      if (!HasApiKey()) {
        if (!_PromptForApiKey()) {
          BMessage errMsg(MOE_CHAT_ERROR);
          errMsg.AddString("error",
            B_TRANSLATE("No API key configured."));
          replyTo.SendMessage(&errMsg);
          break;
        }
      }

      // Fetch tool schemas if needed (cache 5 min)
      bigtime_t now = system_time();
      if (fToolSchemas.Length() == 0
          || (now - fToolSchemasFetchedAt) > 300000000LL) {
        fToolSchemas = _FetchToolSchemas();
        fToolSchemasFetchedAt = now;
      }

      // Add user message to history
      BString userContentJson;
      userContentJson << "[{\"type\":\"text\",\"text\":\""
                      << MoeJsonHelper::Escape(userText) << "\"}]";
      _AddToHistory("user", userContentJson.String());

      // Build request and call API
      BString requestBody = _BuildRequestBody(userText);
      BString response = _CallClaudeApi(requestBody);

      if (response.Length() == 0) {
        BMessage errMsg(MOE_CHAT_ERROR);
        errMsg.AddString("error",
          B_TRANSLATE("Failed to connect to Claude API."));
        replyTo.SendMessage(&errMsg);
        break;
      }

      _ProcessResponse(response, replyTo);
      break;
    }

    case MOE_CHAT_CLEAR:
    {
      for (int32 i = 0; i < fHistory.CountItems(); i++)
        delete (BString*)fHistory.ItemAt(i);
      fHistory.MakeEmpty();
      _SaveHistory();
      break;
    }

    default:
      BLooper::MessageReceived(msg);
      break;
  }
}


static bool
_IsTtsEnabled(void)
{
  BString path(MOE_CONFIG_DIRECTORY);
  path << "tts_enabled";
  BFile file(path.String(), B_READ_ONLY);
  if (file.InitCheck() != B_OK)
    return false;
  char c = '0';
  file.Read(&c, 1);
  return c == '1';
}


BString
MoeClaudeClient::_BuildRequestBody(const char* /*userText*/)
{
  BString systemPrompt(fSystemPrompt);

  // Add current window context
  MoeActiveWindowWatcher* watcher = MoeActiveWindowWatcher::Watcher();
  watcher->Lock();
  if (watcher->IsActive()) {
    systemPrompt << " The user is currently using: "
                 << watcher->CurrentAppName()
                 << " (window: " << watcher->CurrentWin() << ").";
  }
  watcher->Unlock();

  if (_IsTtsEnabled()) {
    systemPrompt << " IMPORTANT: Voice mode is active. Your responses will be "
                    "read aloud. Keep answers SHORT (1-3 sentences max). "
                    "Be conversational and direct. No lists, no markdown, "
                    "no code blocks.";
  }

  BString body;
  body << "{\"model\":\"" << fModel << "\","
       << "\"max_tokens\":4096,"
       << "\"system\":\"" << MoeJsonHelper::Escape(systemPrompt.String()) << "\",";

  // Add tools if available
  if (fToolSchemas.Length() > 0)
    body << "\"tools\":" << fToolSchemas << ",";

  // Build messages array from history
  body << "\"messages\":[";
  for (int32 i = 0; i < fHistory.CountItems(); i++) {
    if (i > 0) body << ",";
    body << *((BString*)fHistory.ItemAt(i));
  }
  body << "]}";

  return body;
}


BString
MoeClaudeClient::_CallClaudeApi(const BString& requestBody)
{
  try {
    BUrl url(kClaudeApiUrl, false);
    BHttpRequest request(url);
    request.SetMethod(BHttpMethod(BHttpMethod::Post));
    request.SetTimeout(60000000); // 60 seconds

    BHttpFields fields;
    fields.AddField("x-api-key", fApiKey.String());
    fields.AddField("anthropic-version", kAnthropicVersion);
    fields.AddField("content-type", "application/json");
    request.SetFields(fields);

    auto body = std::make_unique<BMallocIO>();
    body->Write(requestBody.String(), requestBody.Length());
    body->Seek(0, SEEK_SET);
    request.SetRequestBody(std::move(body), "application/json",
                           requestBody.Length());

    // Execute without target - response stored in Body().text
    BHttpResult result = fHttpSession->Execute(std::move(request));

    // Wait for body (blocking call)
    BHttpBody& httpBody = result.Body();

    BString response;
    if (httpBody.text.has_value())
      response = httpBody.text.value();

    if (response.Length() == 0)
      return BString();

    // Check HTTP status
    const BHttpStatus& status = result.Status();
    if (status.code != 200) {
      BString error;
      error << "HTTP " << status.code;
      BString apiError = MoeJsonHelper::FindString(response, "message");
      if (apiError.Length() > 0)
        error << ": " << apiError;
      fprintf(stderr, "Claude API error: %s\n", error.String());
    }

    return response;
  } catch (...) {
    fprintf(stderr, "Exception in Claude API call\n");
    return BString();
  }
}


BString
MoeClaudeClient::_CallPippoTool(const BString& toolName, const BString& argsJson)
{
  try {
    // Build MCP JSON-RPC request
    BString mcpRequest;
    mcpRequest << "{\"jsonrpc\":\"2.0\",\"id\":1,"
               << "\"method\":\"tools/call\","
               << "\"params\":{\"name\":\"" << MoeJsonHelper::Escape(toolName.String())
               << "\",\"arguments\":" << argsJson << "}}";

    BUrl url(fPippoUrl.String(), false);
    BHttpRequest request(url);
    request.SetMethod(BHttpMethod(BHttpMethod::Post));
    request.SetTimeout(10000000); // 10 seconds

    BHttpFields fields;
    fields.AddField("content-type", "application/json");
    request.SetFields(fields);

    auto body = std::make_unique<BMallocIO>();
    body->Write(mcpRequest.String(), mcpRequest.Length());
    body->Seek(0, SEEK_SET);
    request.SetRequestBody(std::move(body), "application/json",
                           mcpRequest.Length());

    BHttpResult result = fHttpSession->Execute(std::move(request));
    BHttpBody& httpBody = result.Body();

    BString response;
    if (httpBody.text.has_value())
      response = httpBody.text.value();

    if (response.Length() == 0)
      return BString("{\"error\":\"empty response from Pippo\"}");

    // Extract the result from JSON-RPC response
    BString mcpResult = MoeJsonHelper::FindObject(response, "result");
    if (mcpResult.Length() > 0)
      return mcpResult;

    // Check for error
    BString mcpError = MoeJsonHelper::FindObject(response, "error");
    if (mcpError.Length() > 0)
      return mcpError;

    return response;
  } catch (...) {
    return BString("{\"error\":\"failed to connect to Pippo MCP server\"}");
  }
}


BString
MoeClaudeClient::_FetchToolSchemas(void)
{
  try {
    BString mcpRequest(
      "{\"jsonrpc\":\"2.0\",\"id\":1,"
      "\"method\":\"tools/list\",\"params\":{}}");

    BUrl url(fPippoUrl.String(), false);
    BHttpRequest request(url);
    request.SetMethod(BHttpMethod(BHttpMethod::Post));
    request.SetTimeout(5000000); // 5 seconds

    BHttpFields fields;
    fields.AddField("content-type", "application/json");
    request.SetFields(fields);

    auto body = std::make_unique<BMallocIO>();
    body->Write(mcpRequest.String(), mcpRequest.Length());
    body->Seek(0, SEEK_SET);
    request.SetRequestBody(std::move(body), "application/json",
                           mcpRequest.Length());

    BHttpResult result = fHttpSession->Execute(std::move(request));
    BHttpBody& httpBody = result.Body();

    BString response;
    if (httpBody.text.has_value())
      response = httpBody.text.value();

    // The MCP response has result.tools array
    // We need to convert from MCP tool format to Claude API tool format
    BString resultObj = MoeJsonHelper::FindObject(response, "result");
    BString tools = MoeJsonHelper::FindArray(resultObj, "tools");

    if (tools.Length() == 0)
      return BString();

    // MCP tools have: name, description, inputSchema
    // Claude API wants: name, description, input_schema
    // Need to rename inputSchema -> input_schema
    tools.ReplaceAll("inputSchema", "input_schema");

    return tools;
  } catch (...) {
    fprintf(stderr, "Failed to fetch tool schemas from Pippo\n");
    return BString();
  }
}


void
MoeClaudeClient::_ProcessResponse(const BString& response, BMessenger replyTo)
{
  // Check for API-level error
  BString errorType = MoeJsonHelper::FindString(response, "type");
  if (errorType == "error") {
    BString errorMsg = MoeJsonHelper::FindString(response, "message");
    if (errorMsg.Length() == 0)
      errorMsg = "Unknown API error";
    BMessage errMsg(MOE_CHAT_ERROR);
    errMsg.AddString("error", errorMsg.String());
    replyTo.SendMessage(&errMsg);
    return;
  }

  BString stopReason = MoeJsonHelper::FindStopReason(response);

  if (stopReason == "tool_use") {
    // Add the full assistant response to history (with tool_use blocks)
    BString contentArray = MoeJsonHelper::FindArray(response, "content");
    _AddToHistory("assistant", contentArray.String());

    // Extract tool_use entries
    MoeJsonHelper::ToolUseEntry entries[10];
    int32 count = MoeJsonHelper::ExtractToolUseEntries(response,
                                                       entries, 10);

    if (count == 0) {
      BMessage errMsg(MOE_CHAT_ERROR);
      errMsg.AddString("error", "Failed to parse tool_use response");
      replyTo.SendMessage(&errMsg);
      return;
    }

    // Execute each tool and build tool_result array
    BString toolResults;
    toolResults << "[";

    for (int32 i = 0; i < count; i++) {
      // Notify UI about tool progress
      BMessage progress(MOE_CHAT_TOOL_PROGRESS);
      progress.AddString("tool", entries[i].name.String());
      replyTo.SendMessage(&progress);

      // Call Pippo
      BString result = _CallPippoTool(entries[i].name, entries[i].input);

      if (i > 0) toolResults << ",";
      toolResults << "{\"type\":\"tool_result\","
                  << "\"tool_use_id\":\"" << entries[i].id << "\","
                  << "\"content\":\"" << MoeJsonHelper::Escape(result.String())
                  << "\"}";
    }
    toolResults << "]";

    // Add tool results as user message
    _AddToHistory("user", toolResults.String());

    // Re-call Claude with tool results (iterative, limited)
    fToolLoopCount++;
    int32 loopCount = fToolLoopCount;
    if (loopCount > kMaxToolLoopIterations) {
      fToolLoopCount = 0;
      // Extract any text content from the last response
      BString text = MoeJsonHelper::ExtractTextContent(response);
      if (text.Length() > 0) {
        BMessage reply(MOE_CHAT_RESPONSE);
        reply.AddString("text", text.String());
        replyTo.SendMessage(&reply);
      } else {
        BMessage errMsg(MOE_CHAT_ERROR);
        errMsg.AddString("error",
          B_TRANSLATE("Tool loop limit reached."));
        replyTo.SendMessage(&errMsg);
      }
      return;
    }

    BString newRequest = _BuildRequestBody(NULL);
    BString newResponse = _CallClaudeApi(newRequest);

    if (newResponse.Length() == 0) {
      fToolLoopCount = 0;
      BMessage errMsg(MOE_CHAT_ERROR);
      errMsg.AddString("error",
        B_TRANSLATE("Failed to get response after tool execution."));
      replyTo.SendMessage(&errMsg);
      return;
    }

    _ProcessResponse(newResponse, replyTo);
    loopCount = 0;  // reset on successful completion
  } else {
    // end_turn or other - extract text and send to UI
    BString contentArray = MoeJsonHelper::FindArray(response, "content");
    _AddToHistory("assistant", contentArray.String());

    BString text = MoeJsonHelper::ExtractTextContent(response);
    if (text.Length() > 0) {
      BMessage reply(MOE_CHAT_RESPONSE);
      reply.AddString("text", text.String());
      replyTo.SendMessage(&reply);
    } else {
      BMessage reply(MOE_CHAT_RESPONSE);
      reply.AddString("text", "(no response)");
      replyTo.SendMessage(&reply);
    }
  }
}


void
MoeClaudeClient::_AddToHistory(const char* role, const char* contentJson)
{
  BString* entry = new BString();
  *entry << "{\"role\":\"" << role << "\","
         << "\"content\":" << contentJson << "}";
  fHistory.AddItem(entry);
  _TrimHistory();
  _SaveHistory();
}


void
MoeClaudeClient::_TrimHistory(void)
{
  int32 maxItems = kMaxHistoryPairs * 2;
  while (fHistory.CountItems() > maxItems) {
    BString* old = (BString*)fHistory.RemoveItem((int32)0);
    delete old;
  }
}


void
MoeClaudeClient::_SaveHistory(void)
{
  BString path(MOE_CONFIG_DIRECTORY);
  path << "chat_history";

  create_directory(MOE_CONFIG_DIRECTORY, 0755);

  BFile file(path.String(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
  if (file.InitCheck() != B_OK)
    return;

  // Write as JSON array of message objects
  BString json("[");
  for (int32 i = 0; i < fHistory.CountItems(); i++) {
    if (i > 0) json << ",";
    json << *((BString*)fHistory.ItemAt(i));
  }
  json << "]";

  file.Write(json.String(), json.Length());
}


void
MoeClaudeClient::_LoadHistory(void)
{
  BString path(MOE_CONFIG_DIRECTORY);
  path << "chat_history";

  BFile file(path.String(), B_READ_ONLY);
  if (file.InitCheck() != B_OK)
    return;

  off_t size;
  file.GetSize(&size);
  if (size <= 2 || size > 256000)  // [] minimum, 256KB max
    return;

  char* buf = new char[size + 1];
  file.Read(buf, size);
  buf[size] = '\0';
  BString json(buf);
  delete[] buf;

  // Parse the JSON array - split by top-level objects
  // Look for {"role": patterns
  int32 pos = 0;
  while (pos < json.Length()) {
    int32 start = json.FindFirst("{\"role\"", pos);
    if (start < 0) break;

    // Find matching closing brace
    int depth = 0;
    int32 end = start;
    for (; end < json.Length(); end++) {
      if (json[end] == '{') depth++;
      else if (json[end] == '}') {
        depth--;
        if (depth == 0) { end++; break; }
      } else if (json[end] == '"') {
        // Skip string content
        end++;
        while (end < json.Length() && json[end] != '"') {
          if (json[end] == '\\') end++;
          end++;
        }
      }
    }

    BString entry;
    json.CopyInto(entry, start, end - start);
    fHistory.AddItem(new BString(entry));

    pos = end;
  }

  _TrimHistory();
}
