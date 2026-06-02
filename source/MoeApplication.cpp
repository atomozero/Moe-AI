// -*- c++ -*-
//
//  'Moe' window sitter for BeOS.
//  Copyright (C) 2001
//  Okada Jun (yun@be-in.org)
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//  Moe is window sitter running on BeOS Intel and PPC,
//  originally designed and developed by Okada Jun in 2001.
//  http://www.be-in.org/~yun/
//


#include <cstdio>
#include <cstdlib>
#include <getopt.h>
#include <String.h>
#include <Application.h>
#include <AboutWindow.h>
#include <Roster.h>
#include <AppFileInfo.h>
#include <Directory.h>
#include <File.h>
#include <Entry.h>
#include <Path.h>
#include <fs_attr.h>
#include <Alert.h>
#include <Catalog.h>
#include <NodeInfo.h>
#include "MoeDefs.h"
#include "MoeUtils.h"
#include "MoeConsole.h"
#include "MoeMascot.h"
#include "MoeMascotManager.h"
#include "MoeProperty.h"
#include "MoeAppUtils.h"
#include "MoeActiveWindowWatcher.h"
#include "MoeBubbleWindow.h"
#include "MoeClaudeClient.h"
#include "MoeSettingsWindow.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Command line strings"

static const char *
sHelp =
B_TRANSLATE("Usage: Moe [options] [files]\n"
"Options: [default in brackets after descriptions]\n"
"Configuration:\n"
"  --help                print this message\n"
"Animation:\n"
"  --add-anime=ANIME     add anime named ANIME to primary image file\n"
"  --delete-anime=ANIME  delete anime named ANIME from mascot files\n"
"  --wait=WAIT           set anime wait to WAIT\n"
"                        [10]\n"
"Files:\n"
"  --info                show mascot file information\n"
"\n")
;


class MoeApplication : public BApplication
{
public:
  MoeApplication(void);
  virtual ~MoeApplication(void);

protected:  
  virtual void RefsReceived(BMessage *msg);
  virtual void ArgvReceived(int32 argc, char **argv);
  virtual void ReadyToRun(void);
  virtual void AboutRequested(void);
  virtual bool QuitRequested(void);
  virtual void MessageReceived(BMessage *msg);

private:
  MoeMascotManager mMascotManager;
};



MoeApplication::MoeApplication(void)
  : BApplication(MOE_APP_SIGNATURE)
{
}


MoeApplication::~MoeApplication(void)
{
}


void
MoeApplication::RefsReceived(BMessage *msg)
{
  entry_ref ref;
  int i;

  for (i = 0; msg->FindRef("refs", i, &ref) == B_OK; i++)
    mMascotManager.Open(ref);
}


void
MoeApplication::ArgvReceived(int32 argc, char **argv)
{
  const char *cwd;
  BPath path;
  BList entries;
  entry_ref entry;
  int i;

  int wait, c;
  enum {OPEN, ADD, DELETE, INFO} mode;
  BString animeName;
  static struct option
    long_options[] = {
      {"add-anime", 1, 0, 'a'},
      {"delete-anime", 1, 0, 'd'},
      {"info", 0, 0, 'i'},
      {"help", 0, 0, 'h'},
      {"wait", 1, 0, 'w'},
      {0, 0, 0, 0},
    };

  ::optind = 0;
  wait = 10;
  mode = OPEN;

  while (1)
    {
      c = ::getopt_long(argc, argv, "a:d:w:ih", long_options, NULL);

      if (c == -1)
	break;

      switch (c) 
	{
	case 'a':
	  if (mode != OPEN)
	    {
	      MoeConsole::Printf(B_TRANSLATE("overlapping mode specific options.\n\n"));
	      return;
	    }
	  mode = ADD;
	  animeName = ::optarg;
  	  break;

	case 'd':
	  if (mode != OPEN)
	    {
	      MoeConsole::Printf(B_TRANSLATE("overlapping mode specific options.\n\n"));
	      return;
	    }
	  mode = DELETE;
	  animeName = ::optarg;
  	  break;
	  
	case 'i':
	  if (mode != OPEN)
	    {
	      MoeConsole::Printf(B_TRANSLATE("overlapping mode specific options.\n\n"));
	      return;
	    }
	  mode = INFO;
  	  break;
	  
	case 'w':
	  wait = ::atoi(::optarg);
	  break;

	case 'h':
	  MoeConsole::Printf(sHelp);
	  break;

	default:
	  MoeConsole::Printf(B_TRANSLATE("unknown option.\n\n"));
	  return;
	}
    }
  
  if (this->CurrentMessage()->FindString("cwd", &cwd) != B_OK)
    return;

  for (i = optind; i < argc; i++)
    {
      if (argv[i][0] == '/')
	{
	  path.SetTo(argv[i]);
	}
      else
	{
	  path.SetTo(cwd);
	  path.Append(argv[i]);
	}

      if (path.InitCheck() != B_NO_ERROR)
	continue;

      ::get_ref_for_path(path.Path(), &entry);
      entries.AddItem(new entry_ref(entry));
    }

  switch (mode)
    {
    default:
      for (i = 0; i < entries.CountItems(); i++)
	mMascotManager.Open(*reinterpret_cast<entry_ref*>(entries.ItemAt(i)));
      break;

    case ADD:
      MoeAppUtils::AddAnime(&entries, animeName.String(), wait);
      break;

    case DELETE:
      MoeAppUtils::DeleteAnime(&entries, animeName.String());
      break;

    case INFO:
      MoeAppUtils::Info(&entries);
      break;
    }

  for (i = 0; i < entries.CountItems(); i++)
    delete reinterpret_cast<entry_ref*>(entries.ItemAt(i));
}



void
MoeApplication::ReadyToRun(void)
{
  this->PostMessage(MOE_EXAMINE_QUIT_REQUESTED);

  MoeActiveWindowWatcher::Watcher()->Run();
}

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Application About dialog box"

void
MoeApplication::AboutRequested(void)
{
  BAboutWindow* about = new BAboutWindow(B_TRANSLATE_SYSTEM_NAME("Moe-AI"),
  	MOE_APP_SIGNATURE);
  const char* authors [] = {
  	B_TRANSLATE("Okada Jun (original programming)"),
  	B_TRANSLATE("Yu-Ki (illustration)"),
  	"Cafeina",
  	B_TRANSLATE("atomozero (AI chat integration)"),
  	NULL
  };
  const char* extraCopyrights [] = {
  	"2021 Cafeina",
  	"2026 atomozero",
  	NULL
  };
  const char* thanks [] = {
  	"Toyoshima",
  	"Yu-Ki",
  	NULL
  };

  about->AddCopyright(2001, "Okada Jun", extraCopyrights);
  about->AddAuthors(authors);
  about->AddSpecialThanks(thanks);
  about->AddExtraInfo(B_TRANSLATE("Moe - AI Desktop Mascot for Haiku"));
  about->AddDescription(B_TRANSLATE(
  	"Moe is a desktop mascot that sits on your active window.\n"
  	"Double-click to chat with Moe, powered by Claude AI.\n"));
  about->Show();
}


bool
MoeApplication::QuitRequested(void)
{
  MoeActiveWindowWatcher::Watcher()->Stop();
  
  while (mMascotManager.CountMascots() > 0)
    mMascotManager.Close(mMascotManager.MascotAt(0));

  return BApplication::QuitRequested();
}


void
MoeApplication::MessageReceived(BMessage *msg)
{
  switch (msg->what)
    {
    default:
      BApplication::MessageReceived(msg);
      break;

    case MOE_MASCOT_REPLACE:
      {
	entry_ref ref;
	if (msg->FindRef("refs", &ref) == B_OK) {
	  while (mMascotManager.CountMascots() > 0)
	    mMascotManager.Close(mMascotManager.MascotAt(0));
	  mMascotManager.Open(ref);
	}
	break;
      }

    case MOE_MASCOT_TOGGLE:
      {
	entry_ref ref;
	if (msg->FindRef("refs", &ref) != B_OK)
	  break;

	MoeMascot *existing = mMascotManager.FindByRef(ref);
	if (existing) {
	  if (mMascotManager.CountMascots() > 1)
	    mMascotManager.Close(existing);
	} else {
	  mMascotManager.Open(ref);
	}

	this->PostMessage(MOE_EXAMINE_QUIT_REQUESTED);
	break;
      }

    case MOE_CHAT_BUBBLE_OPEN:
      {
	BRect mascotFrame;
	if (msg->FindRect("mascot_frame", &mascotFrame) == B_OK)
	  MoeBubbleWindow::Window()->ShowNear(mascotFrame);
	break;
      }

    case MOE_SETTINGS_OPEN:
      {
	BRect mascotFrame;
	MoeSettingsWindow* settingsWin = MoeSettingsWindow::Window();
	if (msg->FindRect("mascot_frame", &mascotFrame) == B_OK) {
	  settingsWin->ShowNear(mascotFrame);
	} else {
	  if (settingsWin->IsHidden())
	    settingsWin->Show();
	  settingsWin->Activate();
	}
	break;
      }

    case MOE_QUERY_MASCOTS:
      {
        BMessage reply(B_REPLY);
        for (int32 i = 0; i < mMascotManager.CountMascots(); i++) {
          entry_ref ref = mMascotManager.MascotAt(i)->Entry();
          reply.AddRef("refs", &ref);
        }
        msg->SendReply(&reply);
        break;
      }

    case MOE_MASCOT_ADD:
      {
        entry_ref srcRef;
        if (msg->FindRef("refs", &srcRef) != B_OK)
          break;

        // Find mascots/ directory relative to app
        app_info appInfo;
        be_app->GetAppInfo(&appInfo);
        BEntry appEntry(&appInfo.ref);
        BPath appPath;
        appEntry.GetPath(&appPath);
        appPath.GetParent(&appPath);
        appPath.GetParent(&appPath);
        BPath testPath(appPath);
        testPath.Append("mascots");
        BDirectory testDir(testPath.Path());
        if (testDir.InitCheck() != B_OK)
          appPath.GetParent(&appPath);
        BPath mascotsPath(appPath);
        mascotsPath.Append("mascots");

        create_directory(mascotsPath.Path(), 0755);

        // Copy the file
        BPath srcPath;
        BEntry srcEntry(&srcRef);
        srcEntry.GetPath(&srcPath);

        BPath dstPath(mascotsPath);
        dstPath.Append(srcRef.name);

        BFile srcFile(srcPath.Path(), B_READ_ONLY);
        BFile dstFile(dstPath.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
        if (srcFile.InitCheck() == B_OK && dstFile.InitCheck() == B_OK) {
          off_t size;
          srcFile.GetSize(&size);
          char* buf = new char[size];
          srcFile.Read(buf, size);
          dstFile.Write(buf, size);
          delete[] buf;

          // Copy attributes (especially MOE:ANIME:*)
          char attrName[B_ATTR_NAME_LENGTH];
          srcFile.RewindAttrs();
          while (srcFile.GetNextAttrName(attrName) == B_OK) {
            attr_info info;
            if (srcFile.GetAttrInfo(attrName, &info) == B_OK) {
              char* attrBuf = new char[info.size];
              srcFile.ReadAttr(attrName, info.type, 0, attrBuf, info.size);
              dstFile.WriteAttr(attrName, info.type, 0, attrBuf, info.size);
              delete[] attrBuf;
            }
          }

          // Set MIME type
          BNodeInfo nodeInfo(&dstFile);
          nodeInfo.SetType("image/png");
        }

        // Reply with success and the new entry_ref
        BMessage reply(B_REPLY);
        entry_ref newRef;
        get_ref_for_path(dstPath.Path(), &newRef);
        reply.AddRef("refs", &newRef);
        msg->SendReply(&reply);
        break;
      }

    case MOE_MASCOT_REMOVE:
      {
        entry_ref ref;
        if (msg->FindRef("refs", &ref) != B_OK)
          break;

        // Close mascot if it's open
        MoeMascot* existing = mMascotManager.FindByRef(ref);
        if (existing)
          mMascotManager.Close(existing);

        // Delete the file
        BEntry entry(&ref);
        if (entry.Exists())
          entry.Remove();

        this->PostMessage(MOE_EXAMINE_QUIT_REQUESTED);
        break;
      }

    case MOE_MASCOT_REOPEN_REQUESTED:
      {
	MoeMascot *mascot;

	msg->FindPointer("data", reinterpret_cast<void**>(&mascot));
	mMascotManager.Reopen(mascot);
	
	break;
      }

    case MOE_MASCOT_QUIT_REQUESTED:
      {
	MoeMascot *mascot;

	msg->FindPointer("data", reinterpret_cast<void**>(&mascot));
	mMascotManager.Close(mascot);
      }
      // PASS THROUGH
      
    case MOE_EXAMINE_QUIT_REQUESTED:
      {
	if (mMascotManager.CountMascots() == 0)
	  this->PostMessage(B_QUIT_REQUESTED);
	break;
      }
    }
}


int
main(int argc, char **argv)
{
  if (argc == 1)
    printf(sHelp);

  MoeApplication().Run();
}
