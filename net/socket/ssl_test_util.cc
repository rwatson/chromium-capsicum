// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <vector>

#include "net/socket/ssl_test_util.h"

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#include <wincrypt.h>
#elif defined(USE_NSS)
#include <nspr.h>
#include <nss.h>
#include <secerr.h>
#include <ssl.h>
#include <sslerr.h>
#include <pk11pub.h>
#include "base/nss_init.h"
#elif defined(OS_MACOSX)
#include <Security/Security.h>
#include "base/scoped_cftyperef.h"
#include "net/base/x509_certificate.h"
#endif

#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "net/base/host_resolver.h"
#include "net/base/net_test_constants.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/tcp_client_socket.h"
#include "net/socket/tcp_pinger.h"
#include "testing/platform_test.h"

#if defined(OS_WIN)
#pragma comment(lib, "crypt32.lib")
#endif

namespace {

#if defined(USE_NSS)
static CERTCertificate* LoadTemporaryCert(const FilePath& filename) {
  base::EnsureNSSInit();

  std::string rawcert;
  if (!file_util::ReadFileToString(filename, &rawcert)) {
    LOG(ERROR) << "Can't load certificate " << filename.value();
    return NULL;
  }

  CERTCertificate *cert;
  cert = CERT_DecodeCertFromPackage(const_cast<char *>(rawcert.c_str()),
                                    rawcert.length());
  if (!cert) {
    LOG(ERROR) << "Can't convert certificate " << filename.value();
    return NULL;
  }

  // TODO(port): remove this const_cast after NSS 3.12.3 is released
  CERTCertTrust trust;
  int rv = CERT_DecodeTrustString(&trust, const_cast<char *>("TCu,Cu,Tu"));
  if (rv != SECSuccess) {
    LOG(ERROR) << "Can't decode trust string";
    CERT_DestroyCertificate(cert);
    return NULL;
  }

  rv = CERT_ChangeCertTrust(CERT_GetDefaultCertDB(), cert, &trust);
  if (rv != SECSuccess) {
    LOG(ERROR) << "Can't change trust for certificate " << filename.value();
    CERT_DestroyCertificate(cert);
    return NULL;
  }

  return cert;
}
#endif

#if defined(OS_MACOSX)
static net::X509Certificate* LoadTemporaryCert(const FilePath& filename) {
  std::string rawcert;
  if (!file_util::ReadFileToString(filename, &rawcert)) {
    LOG(ERROR) << "Can't load certificate " << filename.value();
    return NULL;
  }

  CFDataRef pem = CFDataCreate(kCFAllocatorDefault,
                               reinterpret_cast<const UInt8*>(rawcert.data()),
                               static_cast<CFIndex>(rawcert.size()));
  if (!pem)
    return NULL;
  scoped_cftyperef<CFDataRef> scoped_pem(pem);

  SecExternalFormat input_format = kSecFormatUnknown;
  SecExternalItemType item_type = kSecItemTypeUnknown;
  CFArrayRef cert_array = NULL;
  if (SecKeychainItemImport(pem, NULL, &input_format, &item_type, 0, NULL, NULL,
                            &cert_array))
    return NULL;
  scoped_cftyperef<CFArrayRef> scoped_cert_array(cert_array);

  if (!CFArrayGetCount(cert_array))
    return NULL;

  SecCertificateRef cert_ref = static_cast<SecCertificateRef>(
      const_cast<void*>(CFArrayGetValueAtIndex(cert_array, 0)));
  CFRetain(cert_ref);
  return net::X509Certificate::CreateFromHandle(cert_ref,
      net::X509Certificate::SOURCE_FROM_NETWORK);
}
#endif

}  // namespace

namespace net {

#if defined(OS_MACOSX)
void SetMacTestCertificate(X509Certificate* cert);
#endif

// static
const char TestServerLauncher::kHostName[] = "127.0.0.1";
const char TestServerLauncher::kMismatchedHostName[] = "localhost";
const int TestServerLauncher::kOKHTTPSPort = 9443;
const int TestServerLauncher::kBadHTTPSPort = 9666;

// The issuer name of the cert that should be trusted for the test to work.
const wchar_t TestServerLauncher::kCertIssuerName[] = L"Test CA";

TestServerLauncher::TestServerLauncher() : process_handle_(
    base::kNullProcessHandle),
    forking_(false),
    connection_attempts_(kDefaultTestConnectionAttempts),
    connection_timeout_(kDefaultTestConnectionTimeout)
#if defined(USE_NSS)
, cert_(NULL)
#endif
{
  InitCertPath();
}

TestServerLauncher::TestServerLauncher(int connection_attempts,
                                       int connection_timeout)
                        : process_handle_(base::kNullProcessHandle),
                          forking_(false),
                          connection_attempts_(connection_attempts),
                          connection_timeout_(connection_timeout)
#if defined(USE_NSS)
, cert_(NULL)
#endif
{
  InitCertPath();
}

void TestServerLauncher::InitCertPath() {
  PathService::Get(base::DIR_SOURCE_ROOT, &cert_dir_);
  cert_dir_ = cert_dir_.Append(FILE_PATH_LITERAL("net"))
                       .Append(FILE_PATH_LITERAL("data"))
                       .Append(FILE_PATH_LITERAL("ssl"))
                       .Append(FILE_PATH_LITERAL("certificates"));
}

namespace {

void AppendToPythonPath(const FilePath& dir) {
  // Do nothing if dir already on path.

#if defined(OS_WIN)
  const wchar_t kPythonPath[] = L"PYTHONPATH";
  // FIXME(dkegel): handle longer PYTHONPATH variables
  wchar_t oldpath[4096];
  if (GetEnvironmentVariable(kPythonPath, oldpath, arraysize(oldpath)) == 0) {
    SetEnvironmentVariableW(kPythonPath, dir.value().c_str());
  } else if (!wcsstr(oldpath, dir.value().c_str())) {
    std::wstring newpath(oldpath);
    newpath.append(L":");
    newpath.append(dir.value());
    SetEnvironmentVariableW(kPythonPath, newpath.c_str());
  }
#elif defined(OS_POSIX)
  const char kPythonPath[] = "PYTHONPATH";
  const char* oldpath = getenv(kPythonPath);
  // setenv() leaks memory intentionally on Mac
  if (!oldpath) {
    setenv(kPythonPath, dir.value().c_str(), 1);
  } else if (!strstr(oldpath, dir.value().c_str())) {
    std::string newpath(oldpath);
    newpath.append(":");
    newpath.append(dir.value());
    setenv(kPythonPath, newpath.c_str(), 1);
  }
#endif
}

}  // end namespace

void TestServerLauncher::SetPythonPath() {
  FilePath third_party_dir;
  CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &third_party_dir));
  third_party_dir = third_party_dir.Append(FILE_PATH_LITERAL("third_party"));

  AppendToPythonPath(third_party_dir.Append(FILE_PATH_LITERAL("tlslite")));
  AppendToPythonPath(third_party_dir.Append(FILE_PATH_LITERAL("pyftpdlib")));
}

bool TestServerLauncher::Start(Protocol protocol,
                               const std::string& host_name, int port,
                               const FilePath& document_root,
                               const FilePath& cert_path,
                               const std::wstring& file_root_url) {
  if (!cert_path.value().empty()) {
    if (!LoadTestRootCert())
      return false;
    if (!CheckCATrusted())
      return false;
  }

  std::string port_str = IntToString(port);

  // Get path to python server script
  FilePath testserver_path;
  if (!PathService::Get(base::DIR_SOURCE_ROOT, &testserver_path))
    return false;
  testserver_path = testserver_path
      .Append(FILE_PATH_LITERAL("net"))
      .Append(FILE_PATH_LITERAL("tools"))
      .Append(FILE_PATH_LITERAL("testserver"))
      .Append(FILE_PATH_LITERAL("testserver.py"));

  PathService::Get(base::DIR_SOURCE_ROOT, &document_root_dir_);
  document_root_dir_ = document_root_dir_.Append(document_root);

  SetPythonPath();

#if defined(OS_WIN)
  // Get path to python interpreter
  if (!PathService::Get(base::DIR_SOURCE_ROOT, &python_runtime_))
    return false;
  python_runtime_ = python_runtime_
      .Append(FILE_PATH_LITERAL("third_party"))
      .Append(FILE_PATH_LITERAL("python_24"))
      .Append(FILE_PATH_LITERAL("python.exe"));

  std::wstring command_line =
      L"\"" + python_runtime_.ToWStringHack() + L"\" " +
      L"\"" + testserver_path.ToWStringHack() +
      L"\" --port=" + UTF8ToWide(port_str) +
      L" --data-dir=\"" + document_root_dir_.ToWStringHack() + L"\"";
  if (protocol == ProtoFTP)
    command_line.append(L" -f");
  if (!cert_path.value().empty()) {
    command_line.append(L" --https=\"");
    command_line.append(cert_path.ToWStringHack());
    command_line.append(L"\"");
  }
  if (!file_root_url.empty()) {
    command_line.append(L" --file-root-url=\"");
    command_line.append(file_root_url);
    command_line.append(L"\"");
  }
  // Deliberately do not pass the --forking flag. It breaks the tests
  // on Windows.

  if (!base::LaunchApp(command_line, false, true, &process_handle_)) {
    LOG(ERROR) << "Failed to launch " << command_line;
    return false;
  }
#elif defined(OS_POSIX)
  std::vector<std::string> command_line;
  command_line.push_back("python");
  command_line.push_back(testserver_path.value());
  command_line.push_back("--port=" + port_str);
  command_line.push_back("--data-dir=" + document_root_dir_.value());
  if (protocol == ProtoFTP)
    command_line.push_back("-f");
  if (!cert_path.value().empty())
    command_line.push_back("--https=" + cert_path.value());
  if (forking_)
    command_line.push_back("--forking");

  base::file_handle_mapping_vector no_mappings;
  LOG(INFO) << "Trying to launch " << command_line[0] << " ...";
  if (!base::LaunchApp(command_line, no_mappings, false, &process_handle_)) {
    LOG(ERROR) << "Failed to launch " << command_line[0] << " ...";
    return false;
  }
#endif

  // Let the server start, then verify that it's up.
  // Our server is Python, and takes about 500ms to start
  // up the first time, and about 200ms after that.
  if (!WaitToStart(host_name, port)) {
    LOG(ERROR) << "Failed to connect to server";
    Stop();
    return false;
  }

  LOG(INFO) << "Started on port " << port_str;
  return true;
}

bool TestServerLauncher::WaitToStart(const std::string& host_name, int port) {
  // Verify that the webserver is actually started.
  // Otherwise tests can fail if they run faster than Python can start.
  net::AddressList addr;
  scoped_refptr<net::HostResolver> resolver(net::CreateSystemHostResolver());
  net::HostResolver::RequestInfo info(host_name, port);
  int rv = resolver->Resolve(info, &addr, NULL, NULL, NULL);
  if (rv != net::OK)
    return false;

  net::TCPPinger pinger(addr);
  rv = pinger.Ping(base::TimeDelta::FromMilliseconds(connection_timeout_),
                   connection_attempts_);
  return rv == net::OK;
}

bool TestServerLauncher::WaitToFinish(int timeout_ms) {
  if (!process_handle_)
    return true;

  bool ret = base::WaitForSingleProcess(process_handle_, timeout_ms);
  if (ret) {
    base::CloseProcessHandle(process_handle_);
    process_handle_ = base::kNullProcessHandle;
    LOG(INFO) << "Finished.";
  } else {
    LOG(INFO) << "Timed out.";
  }
  return ret;
}

bool TestServerLauncher::Stop() {
  if (!process_handle_)
    return true;

  bool ret = base::KillProcess(process_handle_, 1, true);
  if (ret) {
    base::CloseProcessHandle(process_handle_);
    process_handle_ = base::kNullProcessHandle;
    LOG(INFO) << "Stopped.";
  } else {
    LOG(INFO) << "Kill failed?";
  }

  return ret;
}

TestServerLauncher::~TestServerLauncher() {
#if defined(USE_NSS)
  if (cert_)
    CERT_DestroyCertificate(reinterpret_cast<CERTCertificate*>(cert_));
#elif defined(OS_MACOSX)
  SetMacTestCertificate(NULL);
#endif
  Stop();
}

FilePath TestServerLauncher::GetRootCertPath() {
  FilePath path(cert_dir_);
  path = path.AppendASCII("root_ca_cert.crt");
  return path;
}

FilePath TestServerLauncher::GetOKCertPath() {
  FilePath path(cert_dir_);
  path = path.AppendASCII("ok_cert.pem");
  return path;
}

FilePath TestServerLauncher::GetExpiredCertPath() {
  FilePath path(cert_dir_);
  path = path.AppendASCII("expired_cert.pem");
  return path;
}

bool TestServerLauncher::LoadTestRootCert() {
#if defined(USE_NSS)
  if (cert_)
    return true;

  // TODO(dkegel): figure out how to get this to only happen once?

  // This currently leaks a little memory.
  // TODO(dkegel): fix the leak and remove the entry in
  // tools/valgrind/suppressions.txt
  cert_ = reinterpret_cast<PrivateCERTCertificate*>(
          LoadTemporaryCert(GetRootCertPath()));
  DCHECK(cert_);
  return (cert_ != NULL);
#elif defined(OS_MACOSX)
  X509Certificate* cert = LoadTemporaryCert(GetRootCertPath());
  if (!cert)
    return false;
  SetMacTestCertificate(cert);
  return true;
#else
  return true;
#endif
}

bool TestServerLauncher::CheckCATrusted() {
// TODO(port): Port either this or LoadTemporaryCert to MacOSX.
#if defined(OS_WIN)
  HCERTSTORE cert_store = CertOpenSystemStore(NULL, L"ROOT");
  if (!cert_store) {
    LOG(ERROR) << " could not open trusted root CA store";
    return false;
  }
  PCCERT_CONTEXT cert =
      CertFindCertificateInStore(cert_store,
                                 X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                                 0,
                                 CERT_FIND_ISSUER_STR,
                                 kCertIssuerName,
                                 NULL);
  if (cert)
    CertFreeCertificateContext(cert);
  CertCloseStore(cert_store, 0);

  if (!cert) {
    LOG(ERROR) << " TEST CONFIGURATION ERROR: you need to import the test ca "
                  "certificate to your trusted roots for this test to work. "
                  "For more info visit:\n"
                  "http://dev.chromium.org/developers/testing\n";
    return false;
  }
#endif
  return true;
}

}  // namespace net
