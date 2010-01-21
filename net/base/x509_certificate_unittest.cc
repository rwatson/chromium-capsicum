// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/pickle.h"
#include "net/base/cert_status_flags.h"
#include "net/base/cert_verify_result.h"
#include "net/base/net_errors.h"
#include "net/base/test_certificate_data.h"
#include "net/base/x509_certificate.h"
#include "testing/gtest/include/gtest/gtest.h"

// Unit tests aren't allowed to access external resources. Unfortunately, to
// properly verify the EV-ness of a cert, we need to check for its revocation
// through online servers. If you're manually running unit tests, feel free to
// turn this on to test EV certs. But leave it turned off for the automated
// testing.
#define ALLOW_EXTERNAL_ACCESS 0

#if ALLOW_EXTERNAL_ACCESS && defined(OS_WIN)
#define TEST_EV 1  // Test CERT_STATUS_IS_EV
#endif

using base::Time;

namespace {

// Certificates for test data. They're obtained with:
//
// $ openssl s_client -connect [host]:443 -showcerts > /tmp/host.pem < /dev/null
// $ openssl x509 -inform PEM -outform DER < /tmp/host.pem > /tmp/host.der
//
// For fingerprint
// $ openssl x509 -inform DER -fingerprint -noout < /tmp/host.der

// For valid_start, valid_expiry
// $ openssl x509 -inform DER -text -noout < /tmp/host.der |
//    grep -A 2 Validity
// $ date +%s -d '<date str>'

// Google's cert.
unsigned char google_fingerprint[] = {
  0xab, 0xbe, 0x5e, 0xb4, 0x93, 0x88, 0x4e, 0xe4, 0x60, 0xc6, 0xef, 0xf8,
  0xea, 0xd4, 0xb1, 0x55, 0x4b, 0xc9, 0x59, 0x3c
};

// webkit.org's cert.
unsigned char webkit_fingerprint[] = {
  0xa1, 0x4a, 0x94, 0x46, 0x22, 0x8e, 0x70, 0x66, 0x2b, 0x94, 0xf9, 0xf8,
  0x57, 0x83, 0x2d, 0xa2, 0xff, 0xbc, 0x84, 0xc2
};

// thawte.com's cert (it's EV-licious!).
unsigned char thawte_fingerprint[] = {
  0x85, 0x04, 0x2d, 0xfd, 0x2b, 0x0e, 0xc6, 0xc8, 0xaf, 0x2d, 0x77, 0xd6,
  0xa1, 0x3a, 0x64, 0x04, 0x27, 0x90, 0x97, 0x37
};

// A certificate for www.paypal.com with a NULL byte in the common name.
// From http://www.gossamer-threads.com/lists/fulldisc/full-disclosure/70363
unsigned char paypal_null_fingerprint[] = {
  0x4c, 0x88, 0x9e, 0x28, 0xd7, 0x7a, 0x44, 0x1e, 0x13, 0xf2, 0x6a, 0xba,
  0x1f, 0xe8, 0x1b, 0xd6, 0xab, 0x7b, 0xe8, 0xd7
};

// A certificate for https://www.unosoft.hu/, whose AIA extension contains
// an LDAP URL without a host name.
unsigned char unosoft_hu_fingerprint[] = {
  0x32, 0xff, 0xe3, 0xbe, 0x2c, 0x3b, 0xc7, 0xca, 0xbf, 0x2d, 0x64, 0xbd,
  0x25, 0x66, 0xf2, 0xec, 0x8b, 0x0f, 0xbf, 0xd8
};

}  // namespace

namespace net {

TEST(X509CertificateTest, GoogleCertParsing) {
  scoped_refptr<X509Certificate> google_cert = X509Certificate::CreateFromBytes(
      reinterpret_cast<const char*>(google_der), sizeof(google_der));

  ASSERT_NE(static_cast<X509Certificate*>(NULL), google_cert);

  const X509Certificate::Principal& subject = google_cert->subject();
  EXPECT_EQ("www.google.com", subject.common_name);
  EXPECT_EQ("Mountain View", subject.locality_name);
  EXPECT_EQ("California", subject.state_or_province_name);
  EXPECT_EQ("US", subject.country_name);
  EXPECT_EQ(0U, subject.street_addresses.size());
  EXPECT_EQ(1U, subject.organization_names.size());
  EXPECT_EQ("Google Inc", subject.organization_names[0]);
  EXPECT_EQ(0U, subject.organization_unit_names.size());
  EXPECT_EQ(0U, subject.domain_components.size());

  const X509Certificate::Principal& issuer = google_cert->issuer();
  EXPECT_EQ("Thawte SGC CA", issuer.common_name);
  EXPECT_EQ("", issuer.locality_name);
  EXPECT_EQ("", issuer.state_or_province_name);
  EXPECT_EQ("ZA", issuer.country_name);
  EXPECT_EQ(0U, issuer.street_addresses.size());
  EXPECT_EQ(1U, issuer.organization_names.size());
  EXPECT_EQ("Thawte Consulting (Pty) Ltd.", issuer.organization_names[0]);
  EXPECT_EQ(0U, issuer.organization_unit_names.size());
  EXPECT_EQ(0U, issuer.domain_components.size());

  // Use DoubleT because its epoch is the same on all platforms
  const Time& valid_start = google_cert->valid_start();
  EXPECT_EQ(1238192407, valid_start.ToDoubleT());  // Mar 27 22:20:07 2009 GMT

  const Time& valid_expiry = google_cert->valid_expiry();
  EXPECT_EQ(1269728407, valid_expiry.ToDoubleT());  // Mar 27 22:20:07 2010 GMT

  const X509Certificate::Fingerprint& fingerprint = google_cert->fingerprint();
  for (size_t i = 0; i < 20; ++i)
    EXPECT_EQ(google_fingerprint[i], fingerprint.data[i]);

  std::vector<std::string> dns_names;
  google_cert->GetDNSNames(&dns_names);
  EXPECT_EQ(1U, dns_names.size());
  EXPECT_EQ("www.google.com", dns_names[0]);

#if TEST_EV
  // TODO(avi): turn this on for the Mac once EV checking is implemented.
  CertVerifyResult verify_result;
  int flags = X509Certificate::VERIFY_REV_CHECKING_ENABLED |
                X509Certificate::VERIFY_EV_CERT;
  EXPECT_EQ(OK, google_cert->Verify("www.google.com", flags, &verify_result));
  EXPECT_EQ(0, verify_result.cert_status & CERT_STATUS_IS_EV);
#endif
}

TEST(X509CertificateTest, WebkitCertParsing) {
  scoped_refptr<X509Certificate> webkit_cert = X509Certificate::CreateFromBytes(
      reinterpret_cast<const char*>(webkit_der), sizeof(webkit_der));

  ASSERT_NE(static_cast<X509Certificate*>(NULL), webkit_cert);

  const X509Certificate::Principal& subject = webkit_cert->subject();
  EXPECT_EQ("Cupertino", subject.locality_name);
  EXPECT_EQ("California", subject.state_or_province_name);
  EXPECT_EQ("US", subject.country_name);
  EXPECT_EQ(0U, subject.street_addresses.size());
  EXPECT_EQ(1U, subject.organization_names.size());
  EXPECT_EQ("Apple Inc.", subject.organization_names[0]);
  EXPECT_EQ(1U, subject.organization_unit_names.size());
  EXPECT_EQ("Mac OS Forge", subject.organization_unit_names[0]);
  EXPECT_EQ(0U, subject.domain_components.size());

  const X509Certificate::Principal& issuer = webkit_cert->issuer();
  EXPECT_EQ("Go Daddy Secure Certification Authority", issuer.common_name);
  EXPECT_EQ("Scottsdale", issuer.locality_name);
  EXPECT_EQ("Arizona", issuer.state_or_province_name);
  EXPECT_EQ("US", issuer.country_name);
  EXPECT_EQ(0U, issuer.street_addresses.size());
  EXPECT_EQ(1U, issuer.organization_names.size());
  EXPECT_EQ("GoDaddy.com, Inc.", issuer.organization_names[0]);
  EXPECT_EQ(1U, issuer.organization_unit_names.size());
  EXPECT_EQ("http://certificates.godaddy.com/repository",
      issuer.organization_unit_names[0]);
  EXPECT_EQ(0U, issuer.domain_components.size());

  // Use DoubleT because its epoch is the same on all platforms
  const Time& valid_start = webkit_cert->valid_start();
  EXPECT_EQ(1205883319, valid_start.ToDoubleT());  // Mar 18 23:35:19 2008 GMT

  const Time& valid_expiry = webkit_cert->valid_expiry();
  EXPECT_EQ(1300491319, valid_expiry.ToDoubleT());  // Mar 18 23:35:19 2011 GMT

  const X509Certificate::Fingerprint& fingerprint = webkit_cert->fingerprint();
  for (size_t i = 0; i < 20; ++i)
    EXPECT_EQ(webkit_fingerprint[i], fingerprint.data[i]);

  std::vector<std::string> dns_names;
  webkit_cert->GetDNSNames(&dns_names);
  EXPECT_EQ(2U, dns_names.size());
  EXPECT_EQ("*.webkit.org", dns_names[0]);
  EXPECT_EQ("webkit.org", dns_names[1]);

#if TEST_EV
  int flags = X509Certificate::VERIFY_REV_CHECKING_ENABLED |
                X509Certificate::VERIFY_EV_CERT;
  CertVerifyResult verify_result;
  EXPECT_EQ(OK, webkit_cert->Verify("webkit.org", flags, &verify_result));
  EXPECT_EQ(0, verify_result.cert_status & CERT_STATUS_IS_EV);
#endif
}

TEST(X509CertificateTest, ThawteCertParsing) {
  scoped_refptr<X509Certificate> thawte_cert = X509Certificate::CreateFromBytes(
      reinterpret_cast<const char*>(thawte_der), sizeof(thawte_der));

  ASSERT_NE(static_cast<X509Certificate*>(NULL), thawte_cert);

  const X509Certificate::Principal& subject = thawte_cert->subject();
  EXPECT_EQ("www.thawte.com", subject.common_name);
  EXPECT_EQ("Mountain View", subject.locality_name);
  EXPECT_EQ("California", subject.state_or_province_name);
  EXPECT_EQ("US", subject.country_name);
  EXPECT_EQ(0U, subject.street_addresses.size());
  EXPECT_EQ(1U, subject.organization_names.size());
  EXPECT_EQ("Thawte Inc", subject.organization_names[0]);
  EXPECT_EQ(0U, subject.organization_unit_names.size());
  EXPECT_EQ(0U, subject.domain_components.size());

  const X509Certificate::Principal& issuer = thawte_cert->issuer();
  EXPECT_EQ("thawte Extended Validation SSL CA", issuer.common_name);
  EXPECT_EQ("", issuer.locality_name);
  EXPECT_EQ("", issuer.state_or_province_name);
  EXPECT_EQ("US", issuer.country_name);
  EXPECT_EQ(0U, issuer.street_addresses.size());
  EXPECT_EQ(1U, issuer.organization_names.size());
  EXPECT_EQ("thawte, Inc.", issuer.organization_names[0]);
  EXPECT_EQ(1U, issuer.organization_unit_names.size());
  EXPECT_EQ("Terms of use at https://www.thawte.com/cps (c)06",
            issuer.organization_unit_names[0]);
  EXPECT_EQ(0U, issuer.domain_components.size());

  // Use DoubleT because its epoch is the same on all platforms
  const Time& valid_start = thawte_cert->valid_start();
  EXPECT_EQ(1227052800, valid_start.ToDoubleT());  // Nov 19 00:00:00 2008 GMT

  const Time& valid_expiry = thawte_cert->valid_expiry();
  EXPECT_EQ(1263772799, valid_expiry.ToDoubleT());  // Jan 17 23:59:59 2010 GMT

  const X509Certificate::Fingerprint& fingerprint = thawte_cert->fingerprint();
  for (size_t i = 0; i < 20; ++i)
    EXPECT_EQ(thawte_fingerprint[i], fingerprint.data[i]);

  std::vector<std::string> dns_names;
  thawte_cert->GetDNSNames(&dns_names);
  EXPECT_EQ(1U, dns_names.size());
  EXPECT_EQ("www.thawte.com", dns_names[0]);

#if TEST_EV
  int flags = X509Certificate::VERIFY_REV_CHECKING_ENABLED |
                X509Certificate::VERIFY_EV_CERT;
  CertVerifyResult verify_result;
  // EV cert verification requires revocation checking.
  EXPECT_EQ(OK, thawte_cert->Verify("www.thawte.com", flags, &verify_result));
  EXPECT_NE(0, verify_result.cert_status & CERT_STATUS_IS_EV);
  // Consequently, if we don't have revocation checking enabled, we can't claim
  // any cert is EV.
  flags = X509Certificate::VERIFY_EV_CERT;
  EXPECT_EQ(OK, thawte_cert->Verify("www.thawte.com", flags, &verify_result));
  EXPECT_EQ(0, verify_result.cert_status & CERT_STATUS_IS_EV);
#endif
}

TEST(X509CertificateTest, PaypalNullCertParsing) {
  scoped_refptr<X509Certificate> paypal_null_cert =
      X509Certificate::CreateFromBytes(
          reinterpret_cast<const char*>(paypal_null_der),
          sizeof(paypal_null_der));

  ASSERT_NE(static_cast<X509Certificate*>(NULL), paypal_null_cert);

  const X509Certificate::Fingerprint& fingerprint =
      paypal_null_cert->fingerprint();
  for (size_t i = 0; i < 20; ++i)
    EXPECT_EQ(paypal_null_fingerprint[i], fingerprint.data[i]);

  int flags = 0;
  CertVerifyResult verify_result;
  int error = paypal_null_cert->Verify("www.paypal.com", flags,
                                       &verify_result);
  EXPECT_NE(OK, error);
  // Either the system crypto library should correctly report a certificate
  // name mismatch, or our certificate blacklist should cause us to report an
  // invalid certificate.
#if !defined(OS_MACOSX)
  EXPECT_NE(0, verify_result.cert_status &
            (CERT_STATUS_COMMON_NAME_INVALID | CERT_STATUS_INVALID));
#endif
}

// This certificate will expire on 2011-09-08.
TEST(X509CertificateTest, UnoSoftCertParsing) {
  scoped_refptr<X509Certificate> unosoft_hu_cert =
      X509Certificate::CreateFromBytes(
          reinterpret_cast<const char*>(unosoft_hu_der),
          sizeof(unosoft_hu_der));

  ASSERT_NE(static_cast<X509Certificate*>(NULL), unosoft_hu_cert);

  const X509Certificate::Fingerprint& fingerprint =
      unosoft_hu_cert->fingerprint();
  for (size_t i = 0; i < 20; ++i)
    EXPECT_EQ(unosoft_hu_fingerprint[i], fingerprint.data[i]);

  int flags = 0;
  CertVerifyResult verify_result;
  int error = unosoft_hu_cert->Verify("www.unosoft.hu", flags,
                                      &verify_result);
  EXPECT_NE(OK, error);
  EXPECT_NE(0, verify_result.cert_status & CERT_STATUS_AUTHORITY_INVALID);
}

// Tests X509Certificate::Cache via X509Certificate::CreateFromHandle.  We
// call X509Certificate::CreateFromHandle several times and observe whether
// it returns a cached or new X509Certificate object.
//
// All the OS certificate handles in this test are actually from the same
// source (the bytes of a lone certificate), but we pretend that some of them
// come from the network.
TEST(X509CertificateTest, Cache) {
  X509Certificate::OSCertHandle google_cert_handle;

  // Add a certificate from the source SOURCE_LONE_CERT_IMPORT to our
  // certificate cache.
  google_cert_handle = X509Certificate::CreateOSCertHandleFromBytes(
      reinterpret_cast<const char*>(google_der), sizeof(google_der));
  scoped_refptr<X509Certificate> cert1 = X509Certificate::CreateFromHandle(
      google_cert_handle, X509Certificate::SOURCE_LONE_CERT_IMPORT);

  // Add a certificate from the same source (SOURCE_LONE_CERT_IMPORT).  This
  // should return the cached certificate (cert1).
  google_cert_handle = X509Certificate::CreateOSCertHandleFromBytes(
      reinterpret_cast<const char*>(google_der), sizeof(google_der));
  scoped_refptr<X509Certificate> cert2 = X509Certificate::CreateFromHandle(
      google_cert_handle, X509Certificate::SOURCE_LONE_CERT_IMPORT);

  EXPECT_EQ(cert1, cert2);

  // Add a certificate from the network.  This should kick out the original
  // cached certificate (cert1) and return a new certificate.
  google_cert_handle = X509Certificate::CreateOSCertHandleFromBytes(
      reinterpret_cast<const char*>(google_der), sizeof(google_der));
  scoped_refptr<X509Certificate> cert3 = X509Certificate::CreateFromHandle(
      google_cert_handle, X509Certificate::SOURCE_FROM_NETWORK);

  EXPECT_NE(cert1, cert3);

  // Add one certificate from each source.  Both should return the new cached
  // certificate (cert3).
  google_cert_handle = X509Certificate::CreateOSCertHandleFromBytes(
      reinterpret_cast<const char*>(google_der), sizeof(google_der));
  scoped_refptr<X509Certificate> cert4 = X509Certificate::CreateFromHandle(
      google_cert_handle, X509Certificate::SOURCE_FROM_NETWORK);

  EXPECT_EQ(cert3, cert4);

  google_cert_handle = X509Certificate::CreateOSCertHandleFromBytes(
      reinterpret_cast<const char*>(google_der), sizeof(google_der));
  scoped_refptr<X509Certificate> cert5 = X509Certificate::CreateFromHandle(
      google_cert_handle, X509Certificate::SOURCE_FROM_NETWORK);

  EXPECT_EQ(cert3, cert5);
}

TEST(X509CertificateTest, Pickle) {
  scoped_refptr<X509Certificate> cert1 = X509Certificate::CreateFromBytes(
      reinterpret_cast<const char*>(google_der), sizeof(google_der));

  Pickle pickle;
  cert1->Persist(&pickle);

  void* iter = NULL;
  scoped_refptr<X509Certificate> cert2 =
      X509Certificate::CreateFromPickle(pickle, &iter);

  EXPECT_EQ(cert1, cert2);
}

TEST(X509CertificateTest, Policy) {
  scoped_refptr<X509Certificate> google_cert = X509Certificate::CreateFromBytes(
      reinterpret_cast<const char*>(google_der), sizeof(google_der));

  scoped_refptr<X509Certificate> webkit_cert = X509Certificate::CreateFromBytes(
      reinterpret_cast<const char*>(webkit_der), sizeof(webkit_der));

  X509Certificate::Policy policy;

  EXPECT_EQ(policy.Check(google_cert.get()), X509Certificate::Policy::UNKNOWN);
  EXPECT_EQ(policy.Check(webkit_cert.get()), X509Certificate::Policy::UNKNOWN);
  EXPECT_FALSE(policy.HasAllowedCert());
  EXPECT_FALSE(policy.HasDeniedCert());

  policy.Allow(google_cert.get());

  EXPECT_EQ(policy.Check(google_cert.get()), X509Certificate::Policy::ALLOWED);
  EXPECT_EQ(policy.Check(webkit_cert.get()), X509Certificate::Policy::UNKNOWN);
  EXPECT_TRUE(policy.HasAllowedCert());
  EXPECT_FALSE(policy.HasDeniedCert());

  policy.Deny(google_cert.get());

  EXPECT_EQ(policy.Check(google_cert.get()), X509Certificate::Policy::DENIED);
  EXPECT_EQ(policy.Check(webkit_cert.get()), X509Certificate::Policy::UNKNOWN);
  EXPECT_FALSE(policy.HasAllowedCert());
  EXPECT_TRUE(policy.HasDeniedCert());

  policy.Allow(webkit_cert.get());

  EXPECT_EQ(policy.Check(google_cert.get()), X509Certificate::Policy::DENIED);
  EXPECT_EQ(policy.Check(webkit_cert.get()), X509Certificate::Policy::ALLOWED);
  EXPECT_TRUE(policy.HasAllowedCert());
  EXPECT_TRUE(policy.HasDeniedCert());
}

}  // namespace net
