// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_URL_REQUEST_JOB_H_
#define WEBKIT_APPCACHE_APPCACHE_URL_REQUEST_JOB_H_

#include <string>

#include "net/url_request/url_request_job.h"
#include "webkit/appcache/appcache_entry.h"
#include "webkit/appcache/appcache_response.h"
#include "webkit/appcache/appcache_storage.h"

namespace appcache {

// A URLRequestJob derivative that knows how to return a response stored
// in the appcache.
class AppCacheURLRequestJob : public URLRequestJob,
                              public AppCacheStorage::Delegate {
 public:
  explicit AppCacheURLRequestJob(URLRequest* request,
                                 AppCacheStorage* storage);
  virtual ~AppCacheURLRequestJob();

  // Informs the job of what response it should deliver. Only one of these
  // methods should be called, and only once per job. A job will sit idle and
  // wait indefinitely until one of the deliver methods is called.
  void DeliverAppCachedResponse(const GURL& manifest_url, int64 cache_id,
                                const AppCacheEntry& entry);
  void DeliverNetworkResponse();
  void DeliverErrorResponse();

  bool is_waiting() const {
    return delivery_type_ == AWAITING_DELIVERY_ORDERS;
  }

  bool is_delivering_appcache_response() const {
    return delivery_type_ == APPCACHED_DELIVERY;
  }

  bool is_delivering_network_response() const {
    return delivery_type_ == NETWORK_DELIVERY;
  }

  bool is_delivering_error_response() const {
    return delivery_type_ == ERROR_DELIVERY;
  }

  // Accessors for the info about the appcached response, if any,
  // that this job has been instructed to deliver. These are only
  // valid to call if is_delivering_appcache_response.
  const GURL& manifest_url() { return manifest_url_; }
  int64 cache_id() { return cache_id_; }
  const AppCacheEntry& entry() { return entry_; }

  // URLRequestJob's Kill method is made public so the users of this
  // class in the appcache namespace can call it.
  virtual void Kill();

  // Returns true if the job has been started by the net library.
  bool has_been_started() const {
    return has_been_started_;
  }

  // Returns true if the job has been killed.
  bool has_been_killed() const {
    return has_been_killed_;
  }

 private:
  friend class AppCacheRequestHandlerTest;
  friend class AppCacheURLRequestJobTest;

  enum DeliveryType {
    AWAITING_DELIVERY_ORDERS,
    APPCACHED_DELIVERY,
    NETWORK_DELIVERY,
    ERROR_DELIVERY
  };

  // Returns true if one of the Deliver methods has been called.
  bool has_delivery_orders() const {
    return !is_waiting();
  }

  void MaybeBeginDelivery();
  void BeginDelivery();

  // AppCacheStorage::Delegate methods
  virtual void OnResponseInfoLoaded(
      AppCacheResponseInfo* response_info, int64 response_id);

  const net::HttpResponseInfo* http_info() const {
    return info_.get() ? info_->http_response_info() : NULL;
  }

  // AppCacheResponseReader completion callback
  void OnReadComplete(int result);

  // URLRequestJob methods, see url_request_job.h for doc comments
  virtual void Start();
  virtual net::LoadState GetLoadState() const;
  virtual bool GetCharset(std::string* charset);
  virtual void GetResponseInfo(net::HttpResponseInfo* info);
  virtual bool ReadRawData(net::IOBuffer* buf, int buf_size, int *bytes_read);

  // Sets extra request headers for Job types that support request headers.
  // TODO(michaeln): support for range-requests
  virtual void SetExtraRequestHeaders(const std::string& headers) {}

  // TODO(michaeln): does this apply to our cached responses?
  // The payload we store should have been fully decoded prior to
  // us storing it. Maybe we should strip the content-encoding headers
  // when writing the response to the cache.
  // virtual bool GetContentEncodings(
  //    std::vector<Filter::FilterType>* encoding_types);

  // FilterContext methods
  virtual bool GetMimeType(std::string* mime_type) const;
  virtual int GetResponseCode() const;
  virtual bool IsCachedContent() const {
    return is_delivering_appcache_response();
  }

  AppCacheStorage* storage_;
  bool has_been_started_;
  bool has_been_killed_;
  DeliveryType delivery_type_;
  GURL manifest_url_;
  int64 cache_id_;
  AppCacheEntry entry_;
  scoped_refptr<AppCacheResponseInfo> info_;
  scoped_ptr<AppCacheResponseReader> reader_;
  net::CompletionCallbackImpl<AppCacheURLRequestJob> read_callback_;
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_APPCACHE_REQUEST_HANDLER_H_

