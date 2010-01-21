// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_NIX)
#include <cstdlib>
#endif

#include "app/app_paths.h"
#include "app/l10n_util.h"
#if !defined(OS_MACOSX)
#include "app/test/data/resource.h"
#endif
#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#if defined(OS_WIN)
#include "base/win_util.h"
#endif
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "unicode/locid.h"

namespace {

class StringWrapper {
 public:
  explicit StringWrapper(const std::wstring& string) : string_(string) {}
  const std::wstring& string() const { return string_; }

 private:
  std::wstring string_;

  DISALLOW_COPY_AND_ASSIGN(StringWrapper);
};

l10n_util::TextDirection GetTextDirection(const char* locale_name) {
  return l10n_util::GetTextDirectionForLocale(locale_name);
}

}  // namespace

class L10nUtilTest : public PlatformTest {
};

#if defined(OS_WIN)
// TODO(beng): disabled until app strings move to app.
TEST_F(L10nUtilTest, DISABLED_GetString) {
  std::wstring s = l10n_util::GetString(IDS_SIMPLE);
  EXPECT_EQ(std::wstring(L"Hello World!"), s);

  s = l10n_util::GetStringF(IDS_PLACEHOLDERS, L"chrome", L"10");
  EXPECT_EQ(std::wstring(L"Hello, chrome. Your number is 10."), s);

  s = l10n_util::GetStringF(IDS_PLACEHOLDERS_2, 20);
  EXPECT_EQ(std::wstring(L"You owe me $20."), s);
}
#endif  // defined(OS_WIN)

TEST_F(L10nUtilTest, TruncateString) {
  std::wstring string(L"foooooey    bxxxar baz");

  // Make sure it doesn't modify the string if length > string length.
  EXPECT_EQ(string, l10n_util::TruncateString(string, 100));

  // Test no characters.
  EXPECT_EQ(L"", l10n_util::TruncateString(string, 0));

  // Test 1 character.
  EXPECT_EQ(L"\x2026", l10n_util::TruncateString(string, 1));

  // Test adds ... at right spot when there is enough room to break at a
  // word boundary.
  EXPECT_EQ(L"foooooey\x2026", l10n_util::TruncateString(string, 14));

  // Test adds ... at right spot when there is not enough space in first word.
  EXPECT_EQ(L"f\x2026", l10n_util::TruncateString(string, 2));

  // Test adds ... at right spot when there is not enough room to break at a
  // word boundary.
  EXPECT_EQ(L"foooooey\x2026", l10n_util::TruncateString(string, 11));

  // Test completely truncates string if break is on initial whitespace.
  EXPECT_EQ(L"\x2026", l10n_util::TruncateString(L"   ", 2));
}

void SetICUDefaultLocale(const std::string& locale_string) {
  icu::Locale locale(locale_string.c_str());
  UErrorCode error_code = U_ZERO_ERROR;
  icu::Locale::setDefault(locale, error_code);
  EXPECT_TRUE(U_SUCCESS(error_code));
}

#if defined(OS_WIN) || defined(OS_NIX)
// We are disabling this test on MacOS because GetApplicationLocale() as an
// API isn't something that we'll easily be able to unit test in this manner.
// The meaning of that API, on the Mac, is "the locale used by Cocoa's main
// nib file", which clearly can't be stubbed by a test app that doesn't use
// Cocoa.
TEST_F(L10nUtilTest, GetAppLocale) {
  // Use a temporary locale dir so we don't have to actually build the locale
  // dlls for this test.
  FilePath orig_locale_dir;
  PathService::Get(app::DIR_LOCALES, &orig_locale_dir);
  FilePath new_locale_dir;
  EXPECT_TRUE(file_util::CreateNewTempDirectory(
      FILE_PATH_LITERAL("l10n_util_test"),
      &new_locale_dir));
  PathService::Override(app::DIR_LOCALES, new_locale_dir);
  // Make fake locale files.
  std::string filenames[] = {
    "en-US",
    "en-GB",
    "fr",
    "es-419",
    "es",
    "zh-TW",
    "zh-CN",
    "he",
    "fil",
    "nb",
    "or",
  };

#if defined(OS_WIN)
  static const char kLocaleFileExtension[] = ".dll";
#elif defined(OS_POSIX)
  static const char kLocaleFileExtension[] = ".pak";
#endif
  for (size_t i = 0; i < arraysize(filenames); ++i) {
    FilePath filename = new_locale_dir.AppendASCII(
        filenames[i] + kLocaleFileExtension);
    file_util::WriteFile(filename, "", 0);
  }

  // Keep a copy of ICU's default locale before we overwrite it.
  icu::Locale locale = icu::Locale::getDefault();

#if defined(OS_NIX)
  // Test the support of LANGUAGE environment variable.
  SetICUDefaultLocale("en-US");
  ::setenv("LANGUAGE", "xx:fr_CA", 1);
  EXPECT_EQ("fr", l10n_util::GetApplicationLocale(L""));

  ::setenv("LANGUAGE", "xx:yy:en_gb.utf-8@quot", 1);
  EXPECT_EQ("en-GB", l10n_util::GetApplicationLocale(L""));

  ::setenv("LANGUAGE", "xx:zh-hk", 1);
  EXPECT_EQ("zh-TW", l10n_util::GetApplicationLocale(L""));

  // We emulate gettext's behavior here, which ignores LANG/LC_MESSAGES/LC_ALL
  // when LANGUAGE is specified. If no language specified in LANGUAGE is valid,
  // then just fallback to the default language, which is en-US for us.
  SetICUDefaultLocale("fr-FR");
  ::setenv("LANGUAGE", "xx:yy", 1);
  EXPECT_EQ("en-US", l10n_util::GetApplicationLocale(L""));

  ::setenv("LANGUAGE", "/fr:zh_CN", 1);
  EXPECT_EQ("zh-CN", l10n_util::GetApplicationLocale(L""));

  // Make sure the follow tests won't be affected by LANGUAGE environment
  // variable.
  ::unsetenv("LANGUAGE");
#endif

  SetICUDefaultLocale("en-US");
  EXPECT_EQ("en-US", l10n_util::GetApplicationLocale(L""));

  SetICUDefaultLocale("en-GB");
  EXPECT_EQ("en-GB", l10n_util::GetApplicationLocale(L""));

  SetICUDefaultLocale("fr-CA");
  EXPECT_EQ("fr", l10n_util::GetApplicationLocale(L""));

  SetICUDefaultLocale("xx");
  EXPECT_EQ("en-US", l10n_util::GetApplicationLocale(L""));

  SetICUDefaultLocale("es-MX");
  EXPECT_EQ("es-419", l10n_util::GetApplicationLocale(L""));

  SetICUDefaultLocale("es-AR");
  EXPECT_EQ("es-419", l10n_util::GetApplicationLocale(L""));

  SetICUDefaultLocale("es-ES");
  EXPECT_EQ("es", l10n_util::GetApplicationLocale(L""));

  SetICUDefaultLocale("es");
  EXPECT_EQ("es", l10n_util::GetApplicationLocale(L""));

  SetICUDefaultLocale("zh-HK");
  EXPECT_EQ("zh-TW", l10n_util::GetApplicationLocale(L""));

  SetICUDefaultLocale("zh-MK");
  EXPECT_EQ("zh-TW", l10n_util::GetApplicationLocale(L""));

  SetICUDefaultLocale("zh-SG");
  EXPECT_EQ("zh-CN", l10n_util::GetApplicationLocale(L""));

#if defined(OS_WIN)
  // We don't allow user prefs for locale on linux/mac.
  SetICUDefaultLocale("en-US");
  EXPECT_EQ("fr", l10n_util::GetApplicationLocale(L"fr"));
  EXPECT_EQ("fr", l10n_util::GetApplicationLocale(L"fr-CA"));

  SetICUDefaultLocale("en-US");
  // Aliases iw, no, tl to he, nb, fil.
  EXPECT_EQ("he", l10n_util::GetApplicationLocale(L"iw"));
  EXPECT_EQ("nb", l10n_util::GetApplicationLocale(L"no"));
  EXPECT_EQ("fil", l10n_util::GetApplicationLocale(L"tl"));
  // es-419 and es-XX (where XX is not Spain) should be
  // mapped to es-419 (Latin American Spanish).
  EXPECT_EQ("es-419", l10n_util::GetApplicationLocale(L"es-419"));
  EXPECT_EQ("es", l10n_util::GetApplicationLocale(L"es-ES"));
  EXPECT_EQ("es-419", l10n_util::GetApplicationLocale(L"es-AR"));

  SetICUDefaultLocale("es-AR");
  EXPECT_EQ("es", l10n_util::GetApplicationLocale(L"es"));

  SetICUDefaultLocale("zh-HK");
  EXPECT_EQ("zh-CN", l10n_util::GetApplicationLocale(L"zh-CN"));

  SetICUDefaultLocale("he");
  EXPECT_EQ("en-US", l10n_util::GetApplicationLocale(L"en"));

  // Oriya should be blocked unless OS is Vista or newer.
  if (win_util::GetWinVersion() < win_util::WINVERSION_VISTA) {
    SetICUDefaultLocale("or");
    EXPECT_EQ("en-US", l10n_util::GetApplicationLocale(L""));
    SetICUDefaultLocale("en-GB");
    EXPECT_EQ("en-GB", l10n_util::GetApplicationLocale(L"or"));
  } else {
    SetICUDefaultLocale("or");
    EXPECT_EQ("or", l10n_util::GetApplicationLocale(L""));
    SetICUDefaultLocale("en-GB");
    EXPECT_EQ("or", l10n_util::GetApplicationLocale(L"or"));
  }
#endif  // defined(OS_WIN)

  // Clean up.
  PathService::Override(app::DIR_LOCALES, orig_locale_dir);
  file_util::Delete(new_locale_dir, true);
  UErrorCode error_code = U_ZERO_ERROR;
  icu::Locale::setDefault(locale, error_code);
}
#endif  // defined(OS_WIN) || defined(OS_NIX)

TEST_F(L10nUtilTest, SortStringsUsingFunction) {
  std::vector<StringWrapper*> strings;
  strings.push_back(new StringWrapper(L"C"));
  strings.push_back(new StringWrapper(L"d"));
  strings.push_back(new StringWrapper(L"b"));
  strings.push_back(new StringWrapper(L"a"));
  l10n_util::SortStringsUsingMethod(L"en-US", &strings, &StringWrapper::string);
  ASSERT_TRUE(L"a" == strings[0]->string());
  ASSERT_TRUE(L"b" == strings[1]->string());
  ASSERT_TRUE(L"C" == strings[2]->string());
  ASSERT_TRUE(L"d" == strings[3]->string());
  STLDeleteElements(&strings);
}

TEST_F(L10nUtilTest, GetFirstStrongCharacterDirection) {
  // Test pure LTR string.
  std::wstring string(L"foo bar");
  EXPECT_EQ(l10n_util::LEFT_TO_RIGHT,
            l10n_util::GetFirstStrongCharacterDirection(string));

  // Test bidi string in which the first character with strong directionality
  // is a character with type L.
  string.assign(L"foo \x05d0 bar");
  EXPECT_EQ(l10n_util::LEFT_TO_RIGHT,
            l10n_util::GetFirstStrongCharacterDirection(string));

  // Test bidi string in which the first character with strong directionality
  // is a character with type R.
  string.assign(L"\x05d0 foo bar");
  EXPECT_EQ(l10n_util::RIGHT_TO_LEFT,
            l10n_util::GetFirstStrongCharacterDirection(string));

  // Test bidi string which starts with a character with weak directionality
  // and in which the first character with strong directionality is a character
  // with type L.
  string.assign(L"!foo \x05d0 bar");
  EXPECT_EQ(l10n_util::LEFT_TO_RIGHT,
            l10n_util::GetFirstStrongCharacterDirection(string));

  // Test bidi string which starts with a character with weak directionality
  // and in which the first character with strong directionality is a character
  // with type R.
  string.assign(L",\x05d0 foo bar");
  EXPECT_EQ(l10n_util::RIGHT_TO_LEFT,
            l10n_util::GetFirstStrongCharacterDirection(string));

  // Test bidi string in which the first character with strong directionality
  // is a character with type LRE.
  string.assign(L"\x202a \x05d0 foo  bar");
  EXPECT_EQ(l10n_util::LEFT_TO_RIGHT,
            l10n_util::GetFirstStrongCharacterDirection(string));

  // Test bidi string in which the first character with strong directionality
  // is a character with type LRO.
  string.assign(L"\x202d \x05d0 foo  bar");
  EXPECT_EQ(l10n_util::LEFT_TO_RIGHT,
            l10n_util::GetFirstStrongCharacterDirection(string));

  // Test bidi string in which the first character with strong directionality
  // is a character with type RLE.
  string.assign(L"\x202b foo \x05d0 bar");
  EXPECT_EQ(l10n_util::RIGHT_TO_LEFT,
            l10n_util::GetFirstStrongCharacterDirection(string));

  // Test bidi string in which the first character with strong directionality
  // is a character with type RLO.
  string.assign(L"\x202e foo \x05d0 bar");
  EXPECT_EQ(l10n_util::RIGHT_TO_LEFT,
            l10n_util::GetFirstStrongCharacterDirection(string));

  // Test bidi string in which the first character with strong directionality
  // is a character with type AL.
  string.assign(L"\x0622 foo \x05d0 bar");
  EXPECT_EQ(l10n_util::RIGHT_TO_LEFT,
            l10n_util::GetFirstStrongCharacterDirection(string));

  // Test a string without strong directionality characters.
  string.assign(L",!.{}");
  EXPECT_EQ(l10n_util::LEFT_TO_RIGHT,
            l10n_util::GetFirstStrongCharacterDirection(string));

  // Test empty string.
  string.assign(L"");
  EXPECT_EQ(l10n_util::LEFT_TO_RIGHT,
            l10n_util::GetFirstStrongCharacterDirection(string));

  // Test characters in non-BMP (e.g. Phoenician letters. Please refer to
  // http://demo.icu-project.org/icu-bin/ubrowse?scr=151&b=10910 for more
  // information).
#if defined(WCHAR_T_IS_UTF32)
  string.assign(L" ! \x10910" L"abc 123");
#elif defined(WCHAR_T_IS_UTF16)
  string.assign(L" ! \xd802\xdd10" L"abc 123");
#else
#error wchar_t should be either UTF-16 or UTF-32
#endif
  EXPECT_EQ(l10n_util::RIGHT_TO_LEFT,
            l10n_util::GetFirstStrongCharacterDirection(string));

#if defined(WCHAR_T_IS_UTF32)
  string.assign(L" ! \x10401" L"abc 123");
#elif defined(WCHAR_T_IS_UTF16)
  string.assign(L" ! \xd801\xdc01" L"abc 123");
#else
#error wchar_t should be either UTF-16 or UTF-32
#endif
  EXPECT_EQ(l10n_util::LEFT_TO_RIGHT,
            l10n_util::GetFirstStrongCharacterDirection(string));
}

typedef struct {
  std::wstring path;
  std::wstring wrapped_path;
} PathAndWrappedPath;

TEST_F(L10nUtilTest, WrapPathWithLTRFormatting) {
  std::wstring kSeparator;
  kSeparator.push_back(static_cast<wchar_t>(FilePath::kSeparators[0]));
  const PathAndWrappedPath test_data[] = {
    // Test common path, such as "c:\foo\bar".
    { L"c:" + kSeparator + L"foo" + kSeparator + L"bar",
      L"\x202a"L"c:" + kSeparator + L"foo" + kSeparator +
      L"bar\x202c"
    },
    // Test path with file name, such as "c:\foo\bar\test.jpg".
    { L"c:" + kSeparator + L"foo" + kSeparator + L"bar" + kSeparator +
      L"test.jpg",
      L"\x202a"L"c:" + kSeparator + L"foo" + kSeparator +
      L"bar" + kSeparator + L"test.jpg\x202c"
    },
    // Test path ending with punctuation, such as "c:\(foo)\bar.".
    { L"c:" + kSeparator + L"(foo)" + kSeparator + L"bar.",
      L"\x202a"L"c:" + kSeparator + L"(foo)" + kSeparator +
      L"bar.\x202c"
    },
    // Test path ending with separator, such as "c:\foo\bar\".
    { L"c:" + kSeparator + L"foo" + kSeparator + L"bar" + kSeparator,
      L"\x202a"L"c:" + kSeparator + L"foo" + kSeparator +
      L"bar" + kSeparator + L"\x202c",
    },
    // Test path with RTL character.
    { L"c:" + kSeparator + L"\x05d0",
      L"\x202a"L"c:" + kSeparator + L"\x05d0\x202c",
    },
    // Test path with 2 level RTL directory names.
    { L"c:" + kSeparator + L"\x05d0" + kSeparator + L"\x0622",
      L"\x202a"L"c:" + kSeparator + L"\x05d0" + kSeparator +
      L"\x0622\x202c",
    },
    // Test path with mixed RTL/LTR directory names and ending with punctuation.
    { L"c:" + kSeparator + L"\x05d0" + kSeparator + L"\x0622" + kSeparator +
      L"(foo)" + kSeparator + L"b.a.r.",
      L"\x202a"L"c:" + kSeparator + L"\x05d0" + kSeparator +
      L"\x0622" + kSeparator + L"(foo)" + kSeparator +
      L"b.a.r.\x202c",
    },
    // Test path without driver name, such as "/foo/bar/test/jpg".
    { kSeparator + L"foo" + kSeparator + L"bar" + kSeparator + L"test.jpg",
      L"\x202a" + kSeparator + L"foo" + kSeparator + L"bar" +
      kSeparator + L"test.jpg" + L"\x202c"
    },
    // Test path start with current directory, such as "./foo".
    { L"." + kSeparator + L"foo",
      L"\x202a"L"." + kSeparator + L"foo" + L"\x202c"
    },
    // Test path start with parent directory, such as "../foo/bar.jpg".
    { L".." + kSeparator + L"foo" + kSeparator + L"bar.jpg",
      L"\x202a"L".." + kSeparator + L"foo" + kSeparator +
      L"bar.jpg" + L"\x202c"
    },
    // Test absolute path, such as "//foo/bar.jpg".
    { kSeparator + kSeparator + L"foo" + kSeparator + L"bar.jpg",
      L"\x202a" + kSeparator + kSeparator + L"foo" + kSeparator +
      L"bar.jpg" + L"\x202c"
    },
    // Test path with mixed RTL/LTR directory names.
    { L"c:" + kSeparator + L"foo" + kSeparator + L"\x05d0" + kSeparator +
      L"\x0622" + kSeparator + L"\x05d1.jpg",
      L"\x202a"L"c:" + kSeparator + L"foo" + kSeparator + L"\x05d0" +
      kSeparator + L"\x0622" + kSeparator + L"\x05d1.jpg" + L"\x202c",
    },
    // Test empty path.
    { L"",
      L"\x202a\x202c"
    }
  };
  for (unsigned int i = 0; i < arraysize(test_data); ++i) {
    string16 localized_file_path_string;
    FilePath path = FilePath::FromWStringHack(test_data[i].path);
    l10n_util::WrapPathWithLTRFormatting(path, &localized_file_path_string);
    std::wstring wrapped_path = UTF16ToWide(localized_file_path_string);
    EXPECT_EQ(wrapped_path, test_data[i].wrapped_path);
  }
}

typedef struct  {
    std::wstring raw_filename;
    std::wstring display_string;
} StringAndLTRString;

TEST_F(L10nUtilTest, GetDisplayStringInLTRDirectionality) {
  const StringAndLTRString test_data[] = {
    { L"test", L"\x202atest\x202c" },
    { L"test.html", L"\x202atest.html\x202c" },
    { L"\x05d0\x05d1\x05d2", L"\x202a\x05d0\x05d1\x05d2\x202c" },
    { L"\x05d0\x05d1\x05d2.txt", L"\x202a\x05d0\x05d1\x05d2.txt\x202c" },
    { L"\x05d0"L"abc", L"\x202a\x05d0"L"abc\x202c" },
    { L"\x05d0"L"abc.txt", L"\x202a\x05d0"L"abc.txt\x202c" },
    { L"abc\x05d0\x05d1", L"\x202a"L"abc\x05d0\x05d1\x202c" },
    { L"abc\x05d0\x05d1.jpg", L"\x202a"L"abc\x05d0\x05d1.jpg\x202c" },
  };
  for (unsigned int i = 0; i < arraysize(test_data); ++i) {
    std::wstring input = test_data[i].raw_filename;
    std::wstring expected =
        l10n_util::GetDisplayStringInLTRDirectionality(&input);
    if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT)
      EXPECT_EQ(test_data[i].display_string, expected);
    else
      EXPECT_EQ(input, expected);
  }
}

TEST_F(L10nUtilTest, GetTextDirection) {
  EXPECT_EQ(l10n_util::RIGHT_TO_LEFT, GetTextDirection("ar"));
  EXPECT_EQ(l10n_util::RIGHT_TO_LEFT, GetTextDirection("ar_EG"));
  EXPECT_EQ(l10n_util::RIGHT_TO_LEFT, GetTextDirection("he"));
  EXPECT_EQ(l10n_util::RIGHT_TO_LEFT, GetTextDirection("he_IL"));
  // iw is an obsolete code for Hebrew.
  EXPECT_EQ(l10n_util::RIGHT_TO_LEFT, GetTextDirection("iw"));
  // Although we're not yet localized to Farsi and Urdu, we
  // do have the text layout direction information for them.
  EXPECT_EQ(l10n_util::RIGHT_TO_LEFT, GetTextDirection("fa"));
  EXPECT_EQ(l10n_util::RIGHT_TO_LEFT, GetTextDirection("ur"));
#if 0
  // Enable these when we include the minimal locale data for Azerbaijani
  // written in Arabic and Dhivehi. At the moment, our copy of
  // ICU data does not have entries for them.
  EXPECT_EQ(l10n_util::RIGHT_TO_LEFT, GetTextDirection("az_Arab"));
  // Dhivehi that uses Thaana script.
  EXPECT_EQ(l10n_util::RIGHT_TO_LEFT, GetTextDirection("dv"));
#endif
  EXPECT_EQ(l10n_util::LEFT_TO_RIGHT, GetTextDirection("en"));
  // Chinese in China with '-'.
  EXPECT_EQ(l10n_util::LEFT_TO_RIGHT, GetTextDirection("zh-CN"));
  // Filipino : 3-letter code
  EXPECT_EQ(l10n_util::LEFT_TO_RIGHT, GetTextDirection("fil"));
  // Russian
  EXPECT_EQ(l10n_util::LEFT_TO_RIGHT, GetTextDirection("ru"));
  // Japanese that uses multiple scripts
  EXPECT_EQ(l10n_util::LEFT_TO_RIGHT, GetTextDirection("ja"));
}

// Test upper and lower case string conversion.
TEST_F(L10nUtilTest, UpperLower) {
  string16 mixed(ASCIIToUTF16("Text with UPPer & lowER casE."));
  const string16 expected_lower(ASCIIToUTF16("text with upper & lower case."));
  const string16 expected_upper(ASCIIToUTF16("TEXT WITH UPPER & LOWER CASE."));

  string16 result = l10n_util::ToLower(mixed);
  EXPECT_EQ(result, expected_lower);

  result = l10n_util::ToUpper(mixed);
  EXPECT_EQ(result, expected_upper);
}
