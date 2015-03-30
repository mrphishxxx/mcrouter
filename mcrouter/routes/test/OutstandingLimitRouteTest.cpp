/*
 *  Copyright (c) 2015, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#include <memory>
#include <random>
#include <vector>

#include <gtest/gtest.h>

#include "mcrouter/lib/test/RouteHandleTestUtil.h"
#include "mcrouter/routes/OutstandingLimitRoute.h"

using namespace facebook::memcache;
using namespace facebook::memcache::mcrouter;

std::string makeKey(size_t id) {
  return folly::format("test-key:{}", id).str();
}

void sendRequest(FiberManager& fm,
                 TestRouteHandleIf& rh,
                 size_t id,
                 size_t senderId,
                 std::vector<std::string>& replyOrder) {
  fm.addTask([&rh, id, senderId, &replyOrder]() {
      McRequest request(makeKey(id));
      auto context = std::make_shared<TestContext>();
      context->setSenderId(senderId);

      rh.route(request, McOperation<mc_op_get>(), context);
      replyOrder.push_back(makeKey(id));
    });
}

TEST(oustandingLimitRouteTest, basic) {
  auto normalHandle = std::make_shared<TestHandle>(
    GetRouteTestData(mc_res_found, "a"));

  TestRouteHandle<OutstandingLimitRoute<TestRouteHandleIf>> rh(
    normalHandle->rh,
    3);

  normalHandle->pause();

  std::vector<std::string> replyOrder;

  TestFiberManager testfm;
  auto& fm = testfm.getFiberManager();

  sendRequest(fm, rh, 1, 1, replyOrder);
  sendRequest(fm, rh, 2, 1, replyOrder);
  sendRequest(fm, rh, 3, 1, replyOrder);
  sendRequest(fm, rh, 4, 1, replyOrder);
  sendRequest(fm, rh, 5, 2, replyOrder);
  sendRequest(fm, rh, 6, 2, replyOrder);
  sendRequest(fm, rh, 7, 2, replyOrder);
  sendRequest(fm, rh, 8, 1, replyOrder);
  sendRequest(fm, rh, 9, 0, replyOrder);
  sendRequest(fm, rh, 10, 3, replyOrder);
  sendRequest(fm, rh, 11, 0, replyOrder);
  sendRequest(fm, rh, 12, 4, replyOrder);
  sendRequest(fm, rh, 13, 3, replyOrder);
  sendRequest(fm, rh, 14, 0, replyOrder);

  auto& loopController =
    dynamic_cast<SimpleLoopController&>(fm.loopController());
  loopController.loop([&]() {
      fm.addTask([&]() {
          normalHandle->unpause();
        });
      loopController.stop();
    });

  EXPECT_EQ(14, replyOrder.size());
  EXPECT_EQ(makeKey(1), replyOrder[0]);
  EXPECT_EQ(makeKey(2), replyOrder[1]);
  EXPECT_EQ(makeKey(3), replyOrder[2]);
  EXPECT_EQ(makeKey(4), replyOrder[3]);
  EXPECT_EQ(makeKey(5), replyOrder[4]);
  EXPECT_EQ(makeKey(9), replyOrder[5]);
  EXPECT_EQ(makeKey(10), replyOrder[6]);
  EXPECT_EQ(makeKey(11), replyOrder[7]);
  EXPECT_EQ(makeKey(12), replyOrder[8]);
  EXPECT_EQ(makeKey(14), replyOrder[9]);
  EXPECT_EQ(makeKey(8), replyOrder[10]);
  EXPECT_EQ(makeKey(6), replyOrder[11]);
  EXPECT_EQ(makeKey(13), replyOrder[12]);
  EXPECT_EQ(makeKey(7), replyOrder[13]);
}
