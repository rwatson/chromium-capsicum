// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_NIX)
#include <locale.h>
#endif

#if defined(OS_WIN)
#define JPEG_EXT L".jpg"
#define HTML_EXT L".htm"
#define TXT_EXT L".txt"
#define TAR_EXT L".tar"
#elif defined(OS_MACOSX)
#define JPEG_EXT L".jpeg"
#define HTML_EXT L".html"
#define TXT_EXT L".txt"
#define TAR_EXT L".tar"
#else
#define JPEG_EXT L""
#define HTML_EXT L""
#define TXT_EXT L""
#define TAR_EXT L""
#endif

class DownloadManagerTest : public testing::Test {
 public:
  DownloadManagerTest() {
    download_manager_ = new DownloadManager();
    download_util::InitializeExeTypes(&download_manager_->exe_types_);
  }

  void GetGeneratedFilename(const std::string& content_disposition,
                            const std::string& url,
                            const std::string& mime_type,
                            const std::string& referrer_charset,
                            std::wstring* generated_name_string) {
    DownloadCreateInfo info;
    info.content_disposition = content_disposition;
    info.url = GURL(url);
    info.mime_type = mime_type;
    info.referrer_charset = referrer_charset;
    FilePath generated_name;
    download_manager_->GenerateFilename(&info, &generated_name);
    *generated_name_string = generated_name.ToWStringHack();
  }

 protected:
  scoped_refptr<DownloadManager> download_manager_;
  MessageLoopForUI message_loop_;

  DISALLOW_EVIL_CONSTRUCTORS(DownloadManagerTest);
};

namespace {

const struct {
  const char* disposition;
  const char* url;
  const char* mime_type;
  const wchar_t* expected_name;
} kGeneratedFiles[] = {
  // No 'filename' keyword in the disposition, use the URL
  {"a_file_name.txt",
   "http://www.evil.com/my_download.txt",
   "text/plain",
   L"my_download.txt"},

  // Disposition has relative paths, remove them
  {"filename=../../../../././../a_file_name.txt",
   "http://www.evil.com/my_download.txt",
   "text/plain",
   L"a_file_name.txt"},

  // Disposition has parent directories, remove them
  {"filename=dir1/dir2/a_file_name.txt",
   "http://www.evil.com/my_download.txt",
   "text/plain",
   L"a_file_name.txt"},

  // No useful information in disposition or URL, use default
  {"", "http://www.truncated.com/path/", "text/plain",
   L"download" TXT_EXT
  },

  // A normal avi should get .avi and not .avi.avi
  {"", "https://blah.google.com/misc/2.avi", "video/x-msvideo", L"2.avi"},

  // Spaces in the disposition file name
  {"filename=My Downloaded File.exe",
   "http://www.frontpagehacker.com/a_download.exe",
   "application/octet-stream",
   L"My Downloaded File.exe"},

  // This block tests whether we append extensions based on MIME types;
  // we don't do this on Linux, so we skip the tests rather than #ifdef
  // them up.
#if !defined(OS_NIX)
  {"filename=my-cat",
   "http://www.example.com/my-cat",
   "image/jpeg",
   L"my-cat" JPEG_EXT
  },

  {"filename=my-cat",
   "http://www.example.com/my-cat",
   "text/plain",
   L"my-cat.txt"},

  {"filename=my-cat",
   "http://www.example.com/my-cat",
   "text/html",
   L"my-cat" HTML_EXT
  },

  {"filename=my-cat",
   "http://www.example.com/my-cat",
   "dance/party",
   L"my-cat"},
#endif  // defined(OS_NIX)

  {"filename=my-cat.jpg",
   "http://www.example.com/my-cat.jpg",
   "text/plain",
   L"my-cat.jpg"},

  // .exe tests.
#if defined(OS_WIN)
  {"filename=evil.exe",
   "http://www.goodguy.com/evil.exe",
   "image/jpeg",
   L"evil.jpg"},

  {"filename=ok.exe",
   "http://www.goodguy.com/ok.exe",
   "binary/octet-stream",
   L"ok.exe"},

  {"filename=evil.exe.exe",
   "http://www.goodguy.com/evil.exe.exe",
   "dance/party",
   L"evil.exe.download"},

  {"filename=evil.exe",
   "http://www.goodguy.com/evil.exe",
   "application/xml",
   L"evil.xml"},

  {"filename=evil.exe",
   "http://www.goodguy.com/evil.exe",
   "application/html+xml",
   L"evil.download"},

  {"filename=evil.exe",
   "http://www.goodguy.com/evil.exe",
   "application/rss+xml",
   L"evil.download"},
#endif  // OS_WIN

  {"filename=utils.js",
   "http://www.goodguy.com/utils.js",
   "application/x-javascript",
   L"utils.js"},

  {"filename=contacts.js",
   "http://www.goodguy.com/contacts.js",
   "application/json",
   L"contacts.js"},

  {"filename=utils.js",
   "http://www.goodguy.com/utils.js",
   "text/javascript",
   L"utils.js"},

  {"filename=utils.js",
   "http://www.goodguy.com/utils.js",
   "text/javascript;version=2",
   L"utils.js"},

  {"filename=utils.js",
   "http://www.goodguy.com/utils.js",
   "application/ecmascript",
   L"utils.js"},

  {"filename=utils.js",
   "http://www.goodguy.com/utils.js",
   "application/ecmascript;version=4",
   L"utils.js"},

  {"filename=program.exe",
   "http://www.goodguy.com/program.exe",
   "application/foo-bar",
   L"program.exe"},

  {"filename=../foo.txt",
   "http://www.evil.com/../foo.txt",
   "text/plain",
   L"foo.txt"},

  {"filename=..\\foo.txt",
   "http://www.evil.com/..\\foo.txt",
   "text/plain",
#if defined(OS_WIN)
   L"foo.txt"
#else
   L"\\foo.txt"
#endif
  },

  {"filename=.hidden",
   "http://www.evil.com/.hidden",
   "text/plain",
   L"hidden" TXT_EXT
  },

  {"filename=trailing.",
   "http://www.evil.com/trailing.",
   "dance/party",
   L"trailing"
  },

  {"filename=trailing.",
   "http://www.evil.com/trailing.",
   "text/plain",
   L"trailing" TXT_EXT
  },

  {"filename=.",
   "http://www.evil.com/.",
   "dance/party",
   L"download"},

  {"filename=..",
   "http://www.evil.com/..",
   "dance/party",
   L"download"},

  {"filename=...",
   "http://www.evil.com/...",
   "dance/party",
   L"download"},

  // Note that this one doesn't have "filename=" on it.
  {"a_file_name.txt",
   "http://www.evil.com/",
   "image/jpeg",
   L"download" JPEG_EXT
  },

  {"filename=",
   "http://www.evil.com/",
   "image/jpeg",
   L"download" JPEG_EXT
  },

  {"filename=simple",
   "http://www.example.com/simple",
   "application/octet-stream",
   L"simple"},

  {"filename=COM1",
   "http://www.goodguy.com/COM1",
   "application/foo-bar",
#if defined(OS_WIN)
   L"_COM1"
#else
   L"COM1"
#endif
  },

  {"filename=COM4.txt",
   "http://www.goodguy.com/COM4.txt",
   "text/plain",
#if defined(OS_WIN)
   L"_COM4.txt"
#else
   L"COM4.txt"
#endif
  },

  {"filename=lpt1.TXT",
   "http://www.goodguy.com/lpt1.TXT",
   "text/plain",
#if defined(OS_WIN)
   L"_lpt1.TXT"
#else
   L"lpt1.TXT"
#endif
  },

  {"filename=clock$.txt",
   "http://www.goodguy.com/clock$.txt",
   "text/plain",
#if defined(OS_WIN)
   L"_clock$.txt"
#else
   L"clock$.txt"
#endif
  },

  {"filename=mycom1.foo",
   "http://www.goodguy.com/mycom1.foo",
   "text/plain",
   L"mycom1.foo"},

  {"filename=Setup.exe.local",
   "http://www.badguy.com/Setup.exe.local",
   "application/foo-bar",
#if defined(OS_WIN)
   L"Setup.exe.download"
#else
   L"Setup.exe.local"
#endif
  },

  {"filename=Setup.exe.local.local",
   "http://www.badguy.com/Setup.exe.local",
   "application/foo-bar",
#if defined(OS_WIN)
   L"Setup.exe.local.download"
#else
   L"Setup.exe.local.local"
#endif
  },

  {"filename=Setup.exe.lnk",
   "http://www.badguy.com/Setup.exe.lnk",
   "application/foo-bar",
#if defined(OS_WIN)
   L"Setup.exe.download"
#else
   L"Setup.exe.lnk"
#endif
  },

  {"filename=Desktop.ini",
   "http://www.badguy.com/Desktop.ini",
   "application/foo-bar",
#if defined(OS_WIN)
   L"_Desktop.ini"
#else
   L"Desktop.ini"
#endif
  },

  {"filename=Thumbs.db",
   "http://www.badguy.com/Thumbs.db",
   "application/foo-bar",
#if defined(OS_WIN)
   L"_Thumbs.db"
#else
   L"Thumbs.db"
#endif
  },

  {"filename=source.srf",
   "http://www.hotmail.com",
   "image/jpeg",
   L"source.srf" JPEG_EXT
  },

  {"filename=source.jpg",
   "http://www.hotmail.com",
   "application/x-javascript",
#if defined(OS_WIN)
   L"source.jpg"
#elif defined(OS_MACOSX)
   L"source.jpg.js"
#else
   L"source.jpg"
#endif
  },

  // NetUtilTest.{GetSuggestedFilename, GetFileNameFromCD} test these
  // more thoroughly. Tested below are a small set of samples.
  {"attachment; filename=\"%EC%98%88%EC%88%A0%20%EC%98%88%EC%88%A0.jpg\"",
   "http://www.examples.com/",
   "image/jpeg",
   L"\uc608\uc220 \uc608\uc220.jpg"},

  {"attachment; name=abc de.pdf",
   "http://www.examples.com/q.cgi?id=abc",
   "application/octet-stream",
   L"abc de.pdf"},

  {"filename=\"=?EUC-JP?Q?=B7=DD=BD=D13=2Epng?=\"",
   "http://www.example.com/path",
   "image/png",
   L"\x82b8\x8853" L"3.png"},

  // The following two have invalid CD headers and filenames come
  // from the URL.
  {"attachment; filename==?iiso88591?Q?caf=EG?=",
   "http://www.example.com/test%20123",
   "image/jpeg",
   L"test 123" JPEG_EXT
  },

  {"malformed_disposition",
   "http://www.google.com/%EC%98%88%EC%88%A0%20%EC%98%88%EC%88%A0.jpg",
   "image/jpeg",
   L"\uc608\uc220 \uc608\uc220.jpg"},

  // Invalid C-D. No filename from URL. Falls back to 'download'.
  {"attachment; filename==?iso88591?Q?caf=E3?",
   "http://www.google.com/path1/path2/",
   "image/jpeg",
   L"download" JPEG_EXT
  },

  // Issue=5772.
  {"",
   "http://www.example.com/foo.tar.gz",
   "application/x-tar",
   L"foo.tar.gz"},

  // Issue=7337.
  {"",
   "http://maged.lordaeron.org/blank.reg",
   "text/x-registry",
   L"blank.reg"},

  {"",
   "http://www.example.com/bar.tar",
   "application/x-tar",
   L"bar.tar"},

  {"",
   "http://www.example.com/bar.bogus",
   "application/x-tar",
   L"bar.bogus" TAR_EXT
  },

  // http://code.google.com/p/chromium/issues/detail?id=20337
  {"filename=.download.txt",
   "http://www.example.com/.download.txt",
   "text/plain",
   L"download.txt"},
};

}  // namespace

// Tests to ensure that the file names we generate from hints from the server
// (content-disposition, URL name, etc) don't cause security holes.
TEST_F(DownloadManagerTest, TestDownloadFilename) {
#if defined(OS_NIX)
  // This test doesn't run when the locale is not UTF-8 becuase some of the
  // string conversions fail. This is OK (we have the default value) but they
  // don't match our expectations.
  std::string locale = setlocale(LC_CTYPE, NULL);
  StringToLowerASCII(&locale);
  ASSERT_NE(std::string::npos, locale.find("utf-8"))
      << "Your locale must be set to UTF-8 for this test to pass!";
#endif

  std::wstring file_name;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kGeneratedFiles); ++i) {
    GetGeneratedFilename(kGeneratedFiles[i].disposition,
                         kGeneratedFiles[i].url,
                         kGeneratedFiles[i].mime_type,
                         "",
                         &file_name);
    EXPECT_EQ(kGeneratedFiles[i].expected_name, file_name);
    GetGeneratedFilename(kGeneratedFiles[i].disposition,
                         kGeneratedFiles[i].url,
                         kGeneratedFiles[i].mime_type,
                         "GBK",
                         &file_name);
    EXPECT_EQ(kGeneratedFiles[i].expected_name, file_name);
  }

  // A couple of cases with raw 8bit characters in C-D.
  GetGeneratedFilename("attachment; filename=caf\xc3\xa9.png",
                       "http://www.example.com/images?id=3",
                       "image/png",
                       "iso-8859-1",
                       &file_name);
  EXPECT_EQ(L"caf\u00e9.png", file_name);
  GetGeneratedFilename("attachment; filename=caf\xe5.png",
                       "http://www.example.com/images?id=3",
                       "image/png",
                       "windows-1253",
                       &file_name);
  EXPECT_EQ(L"caf\u03b5.png", file_name);
}

namespace {

const struct {
  const FilePath::CharType* path;
  const char* mime_type;
  const FilePath::CharType* expected_path;
} kSafeFilenameCases[] = {
#if defined(OS_WIN)
  { FILE_PATH_LITERAL("C:\\foo\\bar.htm"),
    "text/html",
    FILE_PATH_LITERAL("C:\\foo\\bar.htm") },
  { FILE_PATH_LITERAL("C:\\foo\\bar.html"),
    "text/html",
    FILE_PATH_LITERAL("C:\\foo\\bar.html") },
  { FILE_PATH_LITERAL("C:\\foo\\bar"),
    "text/html",
    FILE_PATH_LITERAL("C:\\foo\\bar.htm") },

  { FILE_PATH_LITERAL("C:\\bar.html"),
    "image/png",
    FILE_PATH_LITERAL("C:\\bar.png") },
  { FILE_PATH_LITERAL("C:\\bar"),
    "image/png",
    FILE_PATH_LITERAL("C:\\bar.png") },

  { FILE_PATH_LITERAL("C:\\foo\\bar.exe"),
    "text/html",
    FILE_PATH_LITERAL("C:\\foo\\bar.htm") },
  { FILE_PATH_LITERAL("C:\\foo\\bar.exe"),
    "image/gif",
    FILE_PATH_LITERAL("C:\\foo\\bar.gif") },

  { FILE_PATH_LITERAL("C:\\foo\\google.com"),
    "text/html",
    FILE_PATH_LITERAL("C:\\foo\\google.htm") },

  { FILE_PATH_LITERAL("C:\\foo\\con.htm"),
    "text/html",
    FILE_PATH_LITERAL("C:\\foo\\_con.htm") },
  { FILE_PATH_LITERAL("C:\\foo\\con"),
    "text/html",
    FILE_PATH_LITERAL("C:\\foo\\_con.htm") },
#else
  { FILE_PATH_LITERAL("/foo/bar.htm"),
    "text/html",
    FILE_PATH_LITERAL("/foo/bar.htm") },
  { FILE_PATH_LITERAL("/foo/bar.html"),
    "text/html",
    FILE_PATH_LITERAL("/foo/bar.html") },
  { FILE_PATH_LITERAL("/foo/bar"),
    "text/html",
    FILE_PATH_LITERAL("/foo/bar.html") },

  { FILE_PATH_LITERAL("/bar.html"),
    "image/png",
    FILE_PATH_LITERAL("/bar.html.png") },
  { FILE_PATH_LITERAL("/bar"),
    "image/png",
    FILE_PATH_LITERAL("/bar.png") },

  { FILE_PATH_LITERAL("/foo/bar.exe"),
    "text/html",
    FILE_PATH_LITERAL("/foo/bar.exe.html") },
  { FILE_PATH_LITERAL("/foo/bar.exe"),
    "image/gif",
    FILE_PATH_LITERAL("/foo/bar.exe.gif") },

  { FILE_PATH_LITERAL("/foo/google.com"),
    "text/html",
    FILE_PATH_LITERAL("/foo/google.com.html") },

  { FILE_PATH_LITERAL("/foo/con.htm"),
    "text/html",
    FILE_PATH_LITERAL("/foo/con.htm") },
  { FILE_PATH_LITERAL("/foo/con"),
    "text/html",
    FILE_PATH_LITERAL("/foo/con.html") },
#endif  // OS_WIN
};

}  // namespace

#if !defined(OS_NIX)
// TODO(port): port to Linux.
TEST_F(DownloadManagerTest, GetSafeFilename) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kSafeFilenameCases); ++i) {
    FilePath path(kSafeFilenameCases[i].path);
    download_manager_->GenerateSafeFilename(kSafeFilenameCases[i].mime_type,
        &path);
    EXPECT_EQ(kSafeFilenameCases[i].expected_path, path.value());
  }
}
#endif  // OS_NIX
