/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice, 
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products 
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <time.h>

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define _WINSOCKAPI_
#include <windows.h>
#define SECURITY_WIN32
#include <security.h>
#endif

#include "talk/base/base64.h"
#include "talk/base/common.h"
#include "talk/base/cryptstring.h"
#include "talk/base/httpcommon.h"
#include "talk/base/socketaddress.h"
#include "talk/base/stringdigest.h"
#include "talk/base/stringutils.h"

namespace talk_base {

#ifdef WIN32
extern const ConstantLabel SECURITY_ERRORS[];
#endif

//////////////////////////////////////////////////////////////////////
// Enum - TODO: expose globally later?
//////////////////////////////////////////////////////////////////////

bool find_string(size_t& index, const std::string& needle,
                 const char* const haystack[], size_t max_index) {
  for (index=0; index<max_index; ++index) {
	if (_stricmp(needle.c_str(), haystack[index]) == 0) {
	  return true;
	}
  }
  return false;
}

template<class E>
struct Enum {
  static const char** Names;
  static size_t Size;

  static inline const char* Name(E val) { return Names[val]; }
  static inline bool Parse(E& val, const std::string& name) { 
	size_t index;
	if (!find_string(index, name, Names, Size))
	  return false;
	val = static_cast<E>(index);
	return true;
  }

  E val;

  inline operator E&() { return val; }
  inline Enum& operator=(E rhs) { val = rhs; return *this; }

  inline const char* name() const { return Name(val); }
  inline bool assign(const std::string& name) { return Parse(val, name); }
  inline Enum& operator=(const std::string& rhs) { assign(rhs); return *this; }
};

#define ENUM(e,n) \
  template<> const char** Enum<e>::Names = n; \
  template<> size_t Enum<e>::Size = sizeof(n)/sizeof(n[0])

//////////////////////////////////////////////////////////////////////
// HttpCommon
//////////////////////////////////////////////////////////////////////

static const char* kHttpVersions[HVER_LAST+1] = {
  "1.0", "1.1"
};
ENUM(HttpVersion, kHttpVersions);

static const char* kHttpVerbs[HV_LAST+1] = {
  "GET", "POST", "PUT", "DELETE", "CONNECT", "HEAD"
};
ENUM(HttpVerb, kHttpVerbs);

static const char* kHttpHeaders[HH_LAST+1] = {
  "Age",
  "Cache-Control",
  "Connection",
  "Content-Length",
  "Content-Range",
  "Content-Type",
  "Cookie",
  "Date",
  "ETag",
  "Expires",
  "Host",
  "If-Modified-Since",
  "If-None-Match",
  "Keep-Alive",
  "Last-Modified",
  "Location",
  "Proxy-Authenticate",
  "Proxy-Authorization",
  "Proxy-Connection",
  "Range",
  "Set-Cookie",
  "TE",
  "Trailers",
  "Transfer-Encoding",
  "Upgrade",
  "User-Agent",
  "WWW-Authenticate",
};
ENUM(HttpHeader, kHttpHeaders);

const char* ToString(HttpVersion version) {
  return Enum<HttpVersion>::Name(version);
}

bool FromString(HttpVersion& version, const std::string& str) {
  return Enum<HttpVersion>::Parse(version, str);
}

const char* ToString(HttpVerb verb) {
  return Enum<HttpVerb>::Name(verb);
}

bool FromString(HttpVerb& verb, const std::string& str) {
  return Enum<HttpVerb>::Parse(verb, str);
}

const char* ToString(HttpHeader header) {
  return Enum<HttpHeader>::Name(header);
}

bool FromString(HttpHeader& header, const std::string& str) {
  return Enum<HttpHeader>::Parse(header, str);
}

bool HttpCodeHasBody(uint32 code) {
  return !HttpCodeIsInformational(code)
         && (code != HC_NO_CONTENT) || (code != HC_NOT_MODIFIED);
}

bool HttpCodeIsCacheable(uint32 code) {
  switch (code) {
  case HC_OK:
  case HC_NON_AUTHORITATIVE:
  case HC_PARTIAL_CONTENT:
  case HC_MULTIPLE_CHOICES:
  case HC_MOVED_PERMANENTLY:
  case HC_GONE:
    return true;
  default:
    return false;
  }
}

bool HttpHeaderIsEndToEnd(HttpHeader header) {
  switch (header) {
  case HH_CONNECTION:
  case HH_KEEP_ALIVE:
  case HH_PROXY_AUTHENTICATE:
  case HH_PROXY_AUTHORIZATION:
  //case HH_PROXY_CONNECTION:??
  case HH_TE:
  case HH_TRAILERS:
  case HH_TRANSFER_ENCODING:
  case HH_UPGRADE:
    return false;
  default:
    return true;
  }
}

bool HttpHeaderIsCollapsible(HttpHeader header) {
  switch (header) {
  case HH_SET_COOKIE:
  case HH_PROXY_AUTHENTICATE:
  case HH_WWW_AUTHENTICATE:
    return false;
  default:
    return true;
  }
}

bool HttpShouldKeepAlive(const HttpData& data) {
  std::string connection;
  if ((data.hasHeader(HH_PROXY_CONNECTION, &connection)
      || data.hasHeader(HH_CONNECTION, &connection))) {
    return (_stricmp(connection.c_str(), "Keep-Alive") == 0);
  }
  return (data.version >= HVER_1_1);
}

namespace {

inline bool IsEndOfAttributeName(size_t pos, size_t len, const char * data) {
  if (pos >= len)
    return true;
  if (isspace(static_cast<unsigned char>(data[pos])))
    return true;
  // The reason for this complexity is that some attributes may contain trailing
  // equal signs (like base64 tokens in Negotiate auth headers)
  if ((pos+1 < len) && (data[pos] == '=') &&
      !isspace(static_cast<unsigned char>(data[pos+1])) &&
      (data[pos+1] != '=')) {
    return true;
  }
  return false;
}

}  // anonymous namespace

void HttpParseAttributes(const char * data, size_t len, 
                         HttpAttributeList& attributes) {
  size_t pos = 0;
  while (true) {
    // Skip leading whitespace
    while ((pos < len) && isspace(static_cast<unsigned char>(data[pos]))) {
      ++pos;
    }

    // End of attributes?
    if (pos >= len)
      return;

    // Find end of attribute name
    size_t start = pos;
    while (!IsEndOfAttributeName(pos, len, data)) {
      ++pos;
    }

    HttpAttribute attribute;
    attribute.first.assign(data + start, data + pos);

    // Attribute has value?
    if ((pos < len) && (data[pos] == '=')) {
      ++pos; // Skip '='
      // Check if quoted value
      if ((pos < len) && (data[pos] == '"')) {
        while (++pos < len) {
          if (data[pos] == '"') {
            ++pos;
            break;
          }
          if ((data[pos] == '\\') && (pos + 1 < len))
            ++pos;
          attribute.second.append(1, data[pos]);
        }
      } else {
        while ((pos < len) &&
            !isspace(static_cast<unsigned char>(data[pos])) &&
            (data[pos] != ',')) {
          attribute.second.append(1, data[pos++]);
        }
      }
    }

    attributes.push_back(attribute);
    if ((pos < len) && (data[pos] == ',')) ++pos; // Skip ','
  }
}

bool HttpHasAttribute(const HttpAttributeList& attributes,
                      const std::string& name,
                      std::string* value) {
  for (HttpAttributeList::const_iterator it = attributes.begin();
       it != attributes.end(); ++it) {
    if (it->first == name) {
      if (value) {
        *value = it->second;
      }
      return true;
    }
  }
  return false;
}

bool HttpHasNthAttribute(HttpAttributeList& attributes,
                         size_t index, 
                         std::string* name,
                         std::string* value) {
  if (index >= attributes.size())
    return false;

  if (name)
    *name = attributes[index].first;
  if (value)
    *value = attributes[index].second;
  return true;
}

bool HttpDateToSeconds(const std::string& date, unsigned long* seconds) {
  const char* const kTimeZones[] = {
    "UT", "GMT", "EST", "EDT", "CST", "CDT", "MST", "MDT", "PST", "PDT",
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "K", "L", "M",
    "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y"
  };
  const int kTimeZoneOffsets[] = {
     0,  0, -5, -4, -6, -5, -7, -6, -8, -7,
    -1, -2, -3, -4, -5, -6, -7, -8, -9, -10, -11, -12,
     1,  2,  3,  4,  5,  6,  7,  8,  9,  10,  11,  12
  };

  ASSERT(NULL != seconds);
  struct tm tval;
  memset(&tval, 0, sizeof(tval));
  char month[4], zone[6];
  memset(month, 0, sizeof(month));
  memset(zone, 0, sizeof(zone));

  if (7 != sscanf(date.c_str(), "%*3s, %d %3s %d %d:%d:%d %5c",
                  &tval.tm_mday, month, &tval.tm_year,
                  &tval.tm_hour, &tval.tm_min, &tval.tm_sec, &zone)) {
    return false;
  }
  switch (toupper(month[2])) {
  case 'N': tval.tm_mon = (month[1] == 'A') ? 0 : 5; break;
  case 'B': tval.tm_mon = 1; break;
  case 'R': tval.tm_mon = (month[0] == 'M') ? 2 : 3; break;
  case 'Y': tval.tm_mon = 4; break;
  case 'L': tval.tm_mon = 6; break;
  case 'G': tval.tm_mon = 7; break;
  case 'P': tval.tm_mon = 8; break;
  case 'T': tval.tm_mon = 9; break;
  case 'V': tval.tm_mon = 10; break;
  case 'C': tval.tm_mon = 11; break;
  }
  tval.tm_year -= 1900;
  unsigned long gmt, non_gmt = mktime(&tval);
  if ((zone[0] == '+') || (zone[0] == '-')) {
    if (!isdigit(zone[1]) || !isdigit(zone[2])
        || !isdigit(zone[3]) || !isdigit(zone[4])) {
      return false;
    }
    int hours = (zone[1] - '0') * 10 + (zone[2] - '0');
    int minutes = (zone[3] - '0') * 10 + (zone[4] - '0');
    int offset = (hours * 60 + minutes) * 60;
    gmt = non_gmt + (zone[0] == '+') ? offset : -offset;
  } else {
    size_t zindex;
    if (!find_string(zindex, zone, kTimeZones, ARRAY_SIZE(kTimeZones))) {
      return false;
    }
    gmt = non_gmt + kTimeZoneOffsets[zindex] * 60 * 60;
  }
#if defined(OSX) || defined(BSD)
  tm *tm_for_timezone = localtime((time_t *)&gmt);
  *seconds = gmt + tm_for_timezone->tm_gmtoff;
#else
  *seconds = gmt - timezone;
#endif
  return true;
}

//////////////////////////////////////////////////////////////////////
// HttpData
//////////////////////////////////////////////////////////////////////

void
HttpData::clear(bool release_document) {
  if (release_document) {
    document.reset();
  }
  m_headers.clear();
}

void
HttpData::changeHeader(const std::string& name, const std::string& value,
                       HeaderCombine combine) {
  if (combine == HC_AUTO) {
    HttpHeader header;
    // Unrecognized headers are collapsible
    combine = !FromString(header, name) || HttpHeaderIsCollapsible(header)
              ? HC_YES : HC_NO;
  } else if (combine == HC_REPLACE) {
    m_headers.erase(name);
    combine = HC_NO;
  }
  // At this point, combine is one of (YES, NO, NEW)
  if (combine != HC_NO) {
    HeaderMap::iterator it = m_headers.find(name);
    if (it != m_headers.end()) {
      if (combine == HC_YES) {
        it->second.append(",");
        it->second.append(value);
	  }
      return;
	}
  }
  m_headers.insert(HeaderMap::value_type(name, value));
}

void
HttpData::clearHeader(const std::string& name) {
  m_headers.erase(name);
}

bool
HttpData::hasHeader(const std::string& name, std::string* value) const {
  HeaderMap::const_iterator it = m_headers.find(name);
  if (it == m_headers.end()) {
    return false;
  } else if (value) {
    *value = it->second;
  }
  return true;
}

void
HttpData::setContent(const std::string& content_type,
                     StreamInterface* document) {
  ASSERT(document != NULL);
  this->document.reset(document);
  setHeader(HH_CONTENT_TYPE, content_type);
  size_t content_length = 0;
  if (this->document->GetSize(&content_length)) {
    char buffer[32];
    sprintfn(buffer, sizeof(buffer), "%d", content_length);
    setHeader(HH_CONTENT_LENGTH, buffer);
  } else {
    setHeader(HH_TRANSFER_ENCODING, "chunked");
  }
}

//
// HttpRequestData
//

void
HttpRequestData::clear(bool release_document) {
  HttpData::clear(release_document);
  verb = HV_GET;
  path.clear();
}

size_t
HttpRequestData::formatLeader(char* buffer, size_t size) {
  ASSERT(path.find(' ') == std::string::npos);
  return sprintfn(buffer, size, "%s %.*s HTTP/%s", ToString(verb), path.size(),
                  path.data(), ToString(version));
}

HttpError
HttpRequestData::parseLeader(const char* line, size_t len) {
  UNUSED(len);
  uint32 vmajor, vminor;
  int vend, dstart, dend;
  if ((sscanf(line, "%*s%n %n%*s%n HTTP/%lu.%lu", &vend, &dstart, &dend,
              &vmajor, &vminor) != 2)
      || (vmajor != 1)) {
    return HE_PROTOCOL;
  }
  if (vminor == 0) {
    version = HVER_1_0;
  } else if (vminor == 1) {
    version = HVER_1_1;
  } else {
    return HE_PROTOCOL;
  }
  std::string sverb(line, vend);
  if (!FromString(verb, sverb.c_str())) {
    return HE_PROTOCOL; // !?! HC_METHOD_NOT_SUPPORTED?
  }
  path.assign(line + dstart, line + dend);
  return HE_NONE;
}

//
// HttpResponseData
//

void
HttpResponseData::clear(bool release_document) {
  HttpData::clear(release_document);
  scode = HC_INTERNAL_SERVER_ERROR;
  message.clear();
}

void
HttpResponseData::set_success(uint32 scode) {
  this->scode = scode;
  message.clear();
  setHeader(HH_CONTENT_LENGTH, "0");
}

void
HttpResponseData::set_success(const std::string& content_type,
                              StreamInterface* document,
                              uint32 scode) {
  this->scode = scode;
  message.erase(message.begin(), message.end());
  setContent(content_type, document);
}

void
HttpResponseData::set_redirect(const std::string& location, uint32 scode) {
  this->scode = scode;
  message.clear();
  setHeader(HH_LOCATION, location);
  setHeader(HH_CONTENT_LENGTH, "0");
}

void
HttpResponseData::set_error(uint32 scode) {
  this->scode = scode;
  message.clear();
  setHeader(HH_CONTENT_LENGTH, "0");
}

size_t
HttpResponseData::formatLeader(char* buffer, size_t size) {
  size_t len = sprintfn(buffer, size, "HTTP/%s %lu", ToString(version), scode);
  if (!message.empty()) {
    len += sprintfn(buffer + len, size - len, " %.*s",
                    message.size(), message.data());
  }
  return len;
}

HttpError
HttpResponseData::parseLeader(const char* line, size_t len) {
  size_t pos = 0;
  uint32 vmajor, vminor;
  if ((sscanf(line, "HTTP/%lu.%lu %lu%n", &vmajor, &vminor, &scode, &pos) != 3)
      || (vmajor != 1)) {
    return HE_PROTOCOL;
  }
  if (vminor == 0) {
    version = HVER_1_0;
  } else if (vminor == 1) {
    version = HVER_1_1;
  } else {
    return HE_PROTOCOL;
  }
  while ((pos < len) && isspace(static_cast<unsigned char>(line[pos]))) ++pos;
  message.assign(line + pos, len - pos);
  return HE_NONE;
}

//////////////////////////////////////////////////////////////////////
// Http Authentication
//////////////////////////////////////////////////////////////////////

#define TEST_DIGEST 0
#if TEST_DIGEST
/*
const char * const DIGEST_CHALLENGE =
  "Digest realm=\"testrealm@host.com\","
  " qop=\"auth,auth-int\","
  " nonce=\"dcd98b7102dd2f0e8b11d0f600bfb0c093\","
  " opaque=\"5ccc069c403ebaf9f0171e9517f40e41\"";
const char * const DIGEST_METHOD = "GET";
const char * const DIGEST_URI =
  "/dir/index.html";;
const char * const DIGEST_CNONCE =
  "0a4f113b";
const char * const DIGEST_RESPONSE =
  "6629fae49393a05397450978507c4ef1";
//user_ = "Mufasa";
//pass_ = "Circle Of Life";
*/
const char * const DIGEST_CHALLENGE =
  "Digest realm=\"Squid proxy-caching web server\","
  " nonce=\"Nny4QuC5PwiSDixJ\","
  " qop=\"auth\","
  " stale=false";
const char * const DIGEST_URI =
  "/";
const char * const DIGEST_CNONCE =
  "6501d58e9a21cee1e7b5fec894ded024";
const char * const DIGEST_RESPONSE =
  "edffcb0829e755838b073a4a42de06bc";
#endif

static std::string quote(const std::string& str) {
  std::string result;
  result.push_back('"');
  for (size_t i=0; i<str.size(); ++i) {
    if ((str[i] == '"') || (str[i] == '\\'))
      result.push_back('\\');
    result.push_back(str[i]);
  }
  result.push_back('"');
  return result;
}

#ifdef WIN32
struct NegotiateAuthContext : public HttpAuthContext {
  CredHandle cred;
  CtxtHandle ctx;
  size_t steps;
  bool specified_credentials;

  NegotiateAuthContext(const std::string& auth, CredHandle c1, CtxtHandle c2)
  : HttpAuthContext(auth), cred(c1), ctx(c2), steps(0),
    specified_credentials(false)
  { }

  virtual ~NegotiateAuthContext() {
    DeleteSecurityContext(&ctx);
    FreeCredentialsHandle(&cred);
  }
};
#endif // WIN32

HttpAuthResult HttpAuthenticate(
  const char * challenge, size_t len,
  const SocketAddress& server,
  const std::string& method, const std::string& uri,
  const std::string& username, const CryptString& password,
  HttpAuthContext *& context, std::string& response, std::string& auth_method)
{
#if TEST_DIGEST
  challenge = DIGEST_CHALLENGE;
  len = strlen(challenge);
#endif

  HttpAttributeList args;
  HttpParseAttributes(challenge, len, args);
  HttpHasNthAttribute(args, 0, &auth_method, NULL);

  if (context && (context->auth_method != auth_method))
    return HAR_IGNORE;

  // BASIC
  if (_stricmp(auth_method.c_str(), "basic") == 0) {
    if (context)
      return HAR_CREDENTIALS; // Bad credentials
    if (username.empty())
      return HAR_CREDENTIALS; // Missing credentials

    context = new HttpAuthContext(auth_method);

    // TODO: convert sensitive to a secure buffer that gets securely deleted
    //std::string decoded = username + ":" + password;
    size_t len = username.size() + password.GetLength() + 2;
    char * sensitive = new char[len];
    size_t pos = strcpyn(sensitive, len, username.data(), username.size());
    pos += strcpyn(sensitive + pos, len - pos, ":");
    password.CopyTo(sensitive + pos, true);

    response = auth_method;
    response.append(" ");
    // TODO: create a sensitive-source version of Base64::encode
    response.append(Base64::encode(sensitive));
    memset(sensitive, 0, len);
    delete [] sensitive;
    return HAR_RESPONSE;
  }

  // DIGEST
  if (_stricmp(auth_method.c_str(), "digest") == 0) {
    if (context)
      return HAR_CREDENTIALS; // Bad credentials
    if (username.empty())
      return HAR_CREDENTIALS; // Missing credentials

    context = new HttpAuthContext(auth_method);

    std::string cnonce, ncount;
#if TEST_DIGEST
    method = DIGEST_METHOD;
    uri    = DIGEST_URI;
    cnonce = DIGEST_CNONCE;
#else
    char buffer[256];
    sprintf(buffer, "%d", time(0));
    cnonce = MD5(buffer);
#endif
    ncount = "00000001";

    std::string realm, nonce, qop, opaque;
    HttpHasAttribute(args, "realm", &realm);
    HttpHasAttribute(args, "nonce", &nonce);
    bool has_qop = HttpHasAttribute(args, "qop", &qop);
    bool has_opaque = HttpHasAttribute(args, "opaque", &opaque);

    // TODO: convert sensitive to be secure buffer
    //std::string A1 = username + ":" + realm + ":" + password;
    size_t len = username.size() + realm.size() + password.GetLength() + 3;
    char * sensitive = new char[len];  // A1
    size_t pos = strcpyn(sensitive, len, username.data(), username.size());
    pos += strcpyn(sensitive + pos, len - pos, ":");
    pos += strcpyn(sensitive + pos, len - pos, realm.c_str());
    pos += strcpyn(sensitive + pos, len - pos, ":");
    password.CopyTo(sensitive + pos, true);

    std::string A2 = method + ":" + uri;
    std::string middle;
    if (has_qop) {
      qop = "auth";
      middle = nonce + ":" + ncount + ":" + cnonce + ":" + qop;
    } else {
      middle = nonce;
    }
    std::string HA1 = MD5(sensitive);
    memset(sensitive, 0, len);
    delete [] sensitive;
    std::string HA2 = MD5(A2);
    std::string dig_response = MD5(HA1 + ":" + middle + ":" + HA2);

#if TEST_DIGEST
    assert(strcmp(dig_response.c_str(), DIGEST_RESPONSE) == 0);
#endif

    std::stringstream ss;
    ss << auth_method;
    ss << " username=" << quote(username);
    ss << ", realm=" << quote(realm);
    ss << ", nonce=" << quote(nonce);
    ss << ", uri=" << quote(uri);
    if (has_qop) {
      ss << ", qop=" << qop;
      ss << ", nc="  << ncount;
      ss << ", cnonce=" << quote(cnonce);
    }
    ss << ", response=\"" << dig_response << "\"";
    if (has_opaque) {
      ss << ", opaque=" << quote(opaque);
    }
    response = ss.str();
    return HAR_RESPONSE;
  }

#ifdef WIN32
#if 1
  bool want_negotiate = (_stricmp(auth_method.c_str(), "negotiate") == 0);
  bool want_ntlm = (_stricmp(auth_method.c_str(), "ntlm") == 0);
  // SPNEGO & NTLM
  if (want_negotiate || want_ntlm) {
    const size_t MAX_MESSAGE = 12000, MAX_SPN = 256;
    char out_buf[MAX_MESSAGE], spn[MAX_SPN];

#if 0 // Requires funky windows versions
    DWORD len = MAX_SPN;
    if (DsMakeSpn("HTTP", server.IPAsString().c_str(), NULL, server.port(),
                  0, &len, spn) != ERROR_SUCCESS) {
      LOG_F(WARNING) << "(Negotiate) - DsMakeSpn failed";
      return HAR_IGNORE;
    }
#else
    sprintfn(spn, MAX_SPN, "HTTP/%s", server.ToString().c_str());
#endif

    SecBuffer out_sec;
    out_sec.pvBuffer   = out_buf;
    out_sec.cbBuffer   = sizeof(out_buf);
    out_sec.BufferType = SECBUFFER_TOKEN;

    SecBufferDesc out_buf_desc;
    out_buf_desc.ulVersion = 0;
    out_buf_desc.cBuffers  = 1;
    out_buf_desc.pBuffers  = &out_sec;

    const ULONG NEG_FLAGS_DEFAULT =
      //ISC_REQ_ALLOCATE_MEMORY
      ISC_REQ_CONFIDENTIALITY
      //| ISC_REQ_EXTENDED_ERROR
      //| ISC_REQ_INTEGRITY
      | ISC_REQ_REPLAY_DETECT
      | ISC_REQ_SEQUENCE_DETECT
      //| ISC_REQ_STREAM
      //| ISC_REQ_USE_SUPPLIED_CREDS
      ;

    TimeStamp lifetime;
    SECURITY_STATUS ret = S_OK;
    ULONG ret_flags = 0, flags = NEG_FLAGS_DEFAULT;

    bool specify_credentials = !username.empty();
    size_t steps = 0;

    //uint32 now = Time();

    NegotiateAuthContext * neg = static_cast<NegotiateAuthContext *>(context);
    if (neg) {
      const size_t max_steps = 10;
      if (++neg->steps >= max_steps) {
        LOG(WARNING) << "AsyncHttpsProxySocket::Authenticate(Negotiate) too many retries";
        return HAR_ERROR;
      }
      steps = neg->steps;

      std::string decoded_challenge;
      HttpHasNthAttribute(args, 1, &decoded_challenge, NULL);
      decoded_challenge = Base64::decode(decoded_challenge);
      if (!decoded_challenge.empty()) {
        SecBuffer in_sec;
        in_sec.pvBuffer   = const_cast<char *>(decoded_challenge.data());
        in_sec.cbBuffer   = static_cast<unsigned long>(decoded_challenge.size());
        in_sec.BufferType = SECBUFFER_TOKEN;

        SecBufferDesc in_buf_desc;
        in_buf_desc.ulVersion = 0;
        in_buf_desc.cBuffers  = 1;
        in_buf_desc.pBuffers  = &in_sec;

        ret = InitializeSecurityContextA(&neg->cred, &neg->ctx, spn, flags, 0, SECURITY_NATIVE_DREP, &in_buf_desc, 0, &neg->ctx, &out_buf_desc, &ret_flags, &lifetime);
        //LOG(INFO) << "$$$ InitializeSecurityContext @ " << TimeDiff(talk_base::Time(), now);
        if (FAILED(ret)) {
          LOG(LS_ERROR) << "InitializeSecurityContext returned: "
                      << ErrorName(ret, SECURITY_ERRORS);
          return HAR_ERROR;
        }
      } else if (neg->specified_credentials) {
        // Try again with default credentials
        specify_credentials = false;
        delete context;
        context = neg = 0;
      } else {
        return HAR_CREDENTIALS;
      }
    }

    if (!neg) {
      unsigned char userbuf[256], passbuf[256], domainbuf[16];
      SEC_WINNT_AUTH_IDENTITY_A auth_id, * pauth_id = 0;
      if (specify_credentials) {
        memset(&auth_id, 0, sizeof(auth_id));
        size_t len = password.GetLength()+1;
        char * sensitive = new char[len];
        password.CopyTo(sensitive, true);
        std::string::size_type pos = username.find('\\');
        if (pos == std::string::npos) {
          auth_id.UserLength = static_cast<unsigned long>(
            _min(sizeof(userbuf) - 1, username.size()));
          memcpy(userbuf, username.c_str(), auth_id.UserLength);
          userbuf[auth_id.UserLength] = 0;
          auth_id.DomainLength = 0;
          domainbuf[auth_id.DomainLength] = 0;
          auth_id.PasswordLength = static_cast<unsigned long>(
            _min(sizeof(passbuf) - 1, password.GetLength()));
          memcpy(passbuf, sensitive, auth_id.PasswordLength);
          passbuf[auth_id.PasswordLength] = 0;
        } else {
          auth_id.UserLength = static_cast<unsigned long>(
            _min(sizeof(userbuf) - 1, username.size() - pos - 1));
          memcpy(userbuf, username.c_str() + pos + 1, auth_id.UserLength);
          userbuf[auth_id.UserLength] = 0;
          auth_id.DomainLength = static_cast<unsigned long>(
            _min(sizeof(domainbuf) - 1, pos));
          memcpy(domainbuf, username.c_str(), auth_id.DomainLength);
          domainbuf[auth_id.DomainLength] = 0;
          auth_id.PasswordLength = static_cast<unsigned long>(
            _min(sizeof(passbuf) - 1, password.GetLength()));
          memcpy(passbuf, sensitive, auth_id.PasswordLength);
          passbuf[auth_id.PasswordLength] = 0;
        }
        memset(sensitive, 0, len);
        delete [] sensitive;
        auth_id.User = userbuf;
        auth_id.Domain = domainbuf;
        auth_id.Password = passbuf;
        auth_id.Flags = SEC_WINNT_AUTH_IDENTITY_ANSI;
        pauth_id = &auth_id;
        LOG(LS_VERBOSE) << "Negotiate protocol: Using specified credentials";
      } else {
        LOG(LS_VERBOSE) << "Negotiate protocol: Using default credentials";
      }

      CredHandle cred;
      ret = AcquireCredentialsHandleA(0, want_negotiate ? NEGOSSP_NAME_A : NTLMSP_NAME_A, SECPKG_CRED_OUTBOUND, 0, pauth_id, 0, 0, &cred, &lifetime);
      //LOG(INFO) << "$$$ AcquireCredentialsHandle @ " << TimeDiff(talk_base::Time(), now);
      if (ret != SEC_E_OK) {
        LOG(LS_ERROR) << "AcquireCredentialsHandle error: "
                    << ErrorName(ret, SECURITY_ERRORS);
        return HAR_IGNORE;
      }

      //CSecBufferBundle<5, CSecBufferBase::FreeSSPI> sb_out;

      CtxtHandle ctx;
      ret = InitializeSecurityContextA(&cred, 0, spn, flags, 0, SECURITY_NATIVE_DREP, 0, 0, &ctx, &out_buf_desc, &ret_flags, &lifetime);
      //LOG(INFO) << "$$$ InitializeSecurityContext @ " << TimeDiff(talk_base::Time(), now);
      if (FAILED(ret)) {
        LOG(LS_ERROR) << "InitializeSecurityContext returned: "
                    << ErrorName(ret, SECURITY_ERRORS);
        FreeCredentialsHandle(&cred);
        return HAR_IGNORE;
      }

      assert(!context);
      context = neg = new NegotiateAuthContext(auth_method, cred, ctx);
      neg->specified_credentials = specify_credentials;
      neg->steps = steps;
    }

    if ((ret == SEC_I_COMPLETE_NEEDED) || (ret == SEC_I_COMPLETE_AND_CONTINUE)) {
      ret = CompleteAuthToken(&neg->ctx, &out_buf_desc);
      //LOG(INFO) << "$$$ CompleteAuthToken @ " << TimeDiff(talk_base::Time(), now);
      LOG(LS_VERBOSE) << "CompleteAuthToken returned: "
                      << ErrorName(ret, SECURITY_ERRORS);
      if (FAILED(ret)) {
        return HAR_ERROR;
      }
    }

    //LOG(INFO) << "$$$ NEGOTIATE took " << TimeDiff(talk_base::Time(), now) << "ms";

    std::string decoded(out_buf, out_buf + out_sec.cbBuffer);
    response = auth_method;
    response.append(" ");
    response.append(Base64::encode(decoded));
    return HAR_RESPONSE;
  }
#endif
#endif // WIN32

  return HAR_IGNORE;
}

//////////////////////////////////////////////////////////////////////

} // namespace talk_base
