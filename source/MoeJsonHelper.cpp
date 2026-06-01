// -*- c++ -*-
//
//  'Moe' window sitter for BeOS.
//  JSON Helper utilities
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//


#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "MoeJsonHelper.h"


BString
MoeJsonHelper::Escape(const char* raw)
{
  BString result;
  if (!raw)
    return result;

  for (const char* p = raw; *p; p++) {
    switch (*p) {
      case '"':  result << "\\\""; break;
      case '\\': result << "\\\\"; break;
      case '\n': result << "\\n"; break;
      case '\r': result << "\\r"; break;
      case '\t': result << "\\t"; break;
      case '\b': result << "\\b"; break;
      case '\f': result << "\\f"; break;
      default:
        if ((unsigned char)*p < 0x20) {
          char buf[8];
          snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)*p);
          result << buf;
        } else {
          result << *p;
        }
        break;
    }
  }
  return result;
}


// Find the end of a JSON string starting after the opening quote
static int32
_SkipJsonString(const BString& json, int32 start)
{
  for (int32 i = start; i < json.Length(); i++) {
    char c = json[i];
    if (c == '\\') {
      i++;  // skip escaped char
      continue;
    }
    if (c == '"')
      return i + 1;
  }
  return json.Length();
}


// Find matching close bracket/brace, respecting nesting and strings
static int32
_FindMatchingClose(const BString& json, int32 start, char open, char close)
{
  int depth = 1;
  for (int32 i = start; i < json.Length() && depth > 0; i++) {
    char c = json[i];
    if (c == '"') {
      i = _SkipJsonString(json, i + 1) - 1;
      continue;
    }
    if (c == open) depth++;
    else if (c == close) depth--;
    if (depth == 0)
      return i + 1;
  }
  return json.Length();
}


// Find the value associated with a key in JSON (searches for "key":)
static int32
_FindKeyValue(const BString& json, const char* key, int32 startFrom = 0)
{
  BString search;
  search << "\"" << key << "\"";

  int32 pos = json.FindFirst(search, startFrom);
  if (pos < 0)
    return -1;

  // skip past the key and find the colon
  pos += search.Length();
  while (pos < json.Length() && json[pos] != ':')
    pos++;
  if (pos >= json.Length())
    return -1;

  pos++; // skip colon

  // skip whitespace
  while (pos < json.Length() && (json[pos] == ' ' || json[pos] == '\t'
         || json[pos] == '\n' || json[pos] == '\r'))
    pos++;

  return pos;
}


BString
MoeJsonHelper::FindString(const BString& json, const char* key)
{
  int32 pos = _FindKeyValue(json, key);
  if (pos < 0 || json[pos] != '"')
    return BString();

  pos++; // skip opening quote
  BString result;
  for (int32 i = pos; i < json.Length(); i++) {
    char c = json[i];
    if (c == '\\' && i + 1 < json.Length()) {
      char next = json[i + 1];
      switch (next) {
        case '"':  result << '"'; break;
        case '\\': result << '\\'; break;
        case 'n':  result << '\n'; break;
        case 'r':  result << '\r'; break;
        case 't':  result << '\t'; break;
        default:   result << next; break;
      }
      i++;
      continue;
    }
    if (c == '"')
      break;
    result << c;
  }
  return result;
}


int32
MoeJsonHelper::FindInt(const BString& json, const char* key)
{
  int32 pos = _FindKeyValue(json, key);
  if (pos < 0)
    return 0;

  BString numStr;
  for (int32 i = pos; i < json.Length(); i++) {
    char c = json[i];
    if ((c >= '0' && c <= '9') || c == '-')
      numStr << c;
    else
      break;
  }
  return atoi(numStr.String());
}


BString
MoeJsonHelper::FindObject(const BString& json, const char* key)
{
  int32 pos = _FindKeyValue(json, key);
  if (pos < 0 || json[pos] != '{')
    return BString();

  int32 end = _FindMatchingClose(json, pos + 1, '{', '}');
  BString result;
  json.CopyInto(result, pos, end - pos);
  return result;
}


BString
MoeJsonHelper::FindArray(const BString& json, const char* key)
{
  int32 pos = _FindKeyValue(json, key);
  if (pos < 0 || json[pos] != '[')
    return BString();

  int32 end = _FindMatchingClose(json, pos + 1, '[', ']');
  BString result;
  json.CopyInto(result, pos, end - pos);
  return result;
}


BString
MoeJsonHelper::FindStopReason(const BString& json)
{
  return FindString(json, "stop_reason");
}


BString
MoeJsonHelper::ExtractTextContent(const BString& json)
{
  // Look for content array, find "type":"text" blocks
  BString content = FindArray(json, "content");
  if (content.Length() == 0)
    return BString();

  BString result;
  int32 pos = 0;

  while (pos < content.Length()) {
    // Find next "type":"text"
    int32 typePos = content.FindFirst("\"type\"", pos);
    if (typePos < 0)
      break;

    // Check if this is a text block
    BString typeStr;
    int32 valPos = _FindKeyValue(content, "type", typePos - pos > 0 ? typePos : 0);
    if (valPos < 0) {
      pos = typePos + 6;
      continue;
    }

    // Read the type value
    if (content[valPos] == '"') {
      int32 end = content.FindFirst('"', valPos + 1);
      if (end > 0) {
        BString typeVal;
        content.CopyInto(typeVal, valPos + 1, end - valPos - 1);
        if (typeVal == "text") {
          // Find the "text" field near this type field
          int32 textPos = content.FindFirst("\"text\"", valPos);
          if (textPos >= 0) {
            // Extract using FindString on a substring
            BString sub;
            content.CopyInto(sub, textPos, content.Length() - textPos);
            BString textVal = FindString(sub, "text");
            if (textVal.Length() > 0) {
              if (result.Length() > 0)
                result << "\n";
              result << textVal;
            }
          }
        }
      }
    }
    pos = typePos + 6;
  }

  return result;
}


bool
MoeJsonHelper::HasToolUse(const BString& json)
{
  return FindStopReason(json) == "tool_use";
}


int32
MoeJsonHelper::ExtractToolUseEntries(const BString& json,
                                     ToolUseEntry* entries, int32 maxEntries)
{
  BString content = FindArray(json, "content");
  if (content.Length() == 0)
    return 0;

  int32 count = 0;
  int32 pos = 0;

  while (pos < content.Length() && count < maxEntries) {
    // Find "type":"tool_use"
    int32 typePos = content.FindFirst("\"tool_use\"", pos);
    if (typePos < 0)
      break;

    // Find the enclosing object - search backwards for '{'
    int32 objStart = typePos;
    while (objStart > 0) {
      if (content[objStart] == '{') {
        // Found a potential object start
        break;
      }
      objStart--;
    }

    if (objStart < 0 || content[objStart] != '{') {
      pos = typePos + 10;
      continue;
    }

    // Find the end of this object
    int32 objEnd = _FindMatchingClose(content, objStart + 1, '{', '}');
    BString toolObj;
    content.CopyInto(toolObj, objStart, objEnd - objStart);

    // Extract fields
    entries[count].id = FindString(toolObj, "id");
    entries[count].name = FindString(toolObj, "name");
    entries[count].input = FindObject(toolObj, "input");

    if (entries[count].id.Length() > 0 && entries[count].name.Length() > 0) {
      count++;
    }

    pos = objEnd;
  }

  return count;
}
