// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "content/browser/browser_thread_impl.h"
#include "content/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace content {

class BrowserThreadTest : public testing::Test {
 public:
  void Release() const {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    loop_.PostTask(FROM_HERE, MessageLoop::QuitClosure());
  }

 protected:
  virtual void SetUp() {
    ui_thread_.reset(new BrowserThreadImpl(BrowserThread::UI));
    file_thread_.reset(new BrowserThreadImpl(BrowserThread::FILE));
    ui_thread_->Start();
    file_thread_->Start();
  }

  virtual void TearDown() {
    ui_thread_->Stop();
    file_thread_->Stop();
  }

  static void BasicFunction(MessageLoop* message_loop) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    message_loop->PostTask(FROM_HERE, MessageLoop::QuitClosure());
  }

  static void DoNothing() {
  }

  class DeletedOnFile
      : public base::RefCountedThreadSafe<
            DeletedOnFile, BrowserThread::DeleteOnFileThread> {
   public:
    explicit DeletedOnFile(MessageLoop* message_loop)
        : message_loop_(message_loop) { }

    ~DeletedOnFile() {
      CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
      message_loop_->PostTask(FROM_HERE, MessageLoop::QuitClosure());
    }

   private:
    MessageLoop* message_loop_;
  };

  class NeverDeleted
      : public base::RefCountedThreadSafe<
            NeverDeleted, BrowserThread::DeleteOnWebKitThread> {
   public:
    ~NeverDeleted() {
      CHECK(false);
    }
  };

 private:
  scoped_ptr<BrowserThreadImpl> ui_thread_;
  scoped_ptr<BrowserThreadImpl> file_thread_;
  // It's kind of ugly to make this mutable - solely so we can post the Quit
  // Task from Release(). This should be fixed.
  mutable MessageLoop loop_;
};

TEST_F(BrowserThreadTest, PostTask) {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&BasicFunction, MessageLoop::current()));
  MessageLoop::current()->Run();
}

TEST_F(BrowserThreadTest, Release) {
  BrowserThread::ReleaseSoon(BrowserThread::UI, FROM_HERE, this);
  MessageLoop::current()->Run();
}

TEST_F(BrowserThreadTest, ReleasedOnCorrectThread) {
  {
    scoped_refptr<DeletedOnFile> test(
        new DeletedOnFile(MessageLoop::current()));
  }
  MessageLoop::current()->Run();
}

TEST_F(BrowserThreadTest, NotReleasedIfTargetThreadNonExistent) {
  scoped_refptr<NeverDeleted> test(new NeverDeleted());
}

TEST_F(BrowserThreadTest, PostTaskViaMessageLoopProxy) {
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE);
  message_loop_proxy->PostTask(
      FROM_HERE, base::Bind(&BasicFunction, MessageLoop::current()));
  MessageLoop::current()->Run();
}

TEST_F(BrowserThreadTest, ReleaseViaMessageLoopProxy) {
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI);
  message_loop_proxy->ReleaseSoon(FROM_HERE, this);
  MessageLoop::current()->Run();
}

TEST_F(BrowserThreadTest, PostTaskAndReply) {
  // Most of the heavy testing for PostTaskAndReply() is done inside the
  // MessageLoopProxy test.  This just makes sure we get piped through at all.
  ASSERT_TRUE(BrowserThread::PostTaskAndReply(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&BrowserThreadTest::DoNothing),
      base::Bind(&MessageLoop::Quit,
                 base::Unretained(MessageLoop::current()->current()))));
  MessageLoop::current()->Run();
}

}
