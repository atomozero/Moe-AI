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


#ifndef MOE_JSON_HELPER_H
#define MOE_JSON_HELPER_H


#include <String.h>


namespace MoeJsonHelper {

  // JSON string escaping
  BString Escape(const char* raw);

  // Targeted extraction from JSON strings
  BString FindString(const BString& json, const char* key);
  int32   FindInt(const BString& json, const char* key);
  BString FindObject(const BString& json, const char* key);
  BString FindArray(const BString& json, const char* key);

  // Find the stop_reason field
  BString FindStopReason(const BString& json);

  // Extract text content from Claude Messages API response
  BString ExtractTextContent(const BString& json);

  // Extract tool_use blocks from response
  // Returns true if tool_use blocks were found
  bool    HasToolUse(const BString& json);

  // Extract individual tool_use entries
  // Each entry has "id", "name", "input" fields
  struct ToolUseEntry {
    BString id;
    BString name;
    BString input;  // raw JSON object string
  };

  int32   ExtractToolUseEntries(const BString& json,
                                ToolUseEntry* entries, int32 maxEntries);
}


#endif
