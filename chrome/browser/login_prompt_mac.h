// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOGIN_PROMPT_MAC_H_
#define CHROME_BROWSER_LOGIN_PROMPT_MAC_H_

#import <Cocoa/Cocoa.h>

class LoginHandlerMac;

// Controller of the sheet used by LoginHandlerMac. Interface Builder wants
// this to be in a .h file.
@interface LoginHandlerSheet : NSWindowController {
 @private
  IBOutlet NSTextField* nameField_;
  IBOutlet NSSecureTextField* passwordField_;
  LoginHandlerMac* handler_;  // weak, owns us
}
- (id)initWithLoginHandler:(LoginHandlerMac*)handler;
- (IBAction)loginPressed:(id)sender;
- (IBAction)cancelPressed:(id)sender;
- (void)sheetDidEnd:(NSWindow*)sheet
         returnCode:(int)returnCode
        contextInfo:(void*)contextInfo;
- (void)autofillLogin:(NSString*)login password:(NSString*)password;
@end

#endif  // CHROME_BROWSER_LOGIN_PROMPT_MAC_H_
