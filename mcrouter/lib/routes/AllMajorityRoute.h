/*
 *  Copyright (c) 2016, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#pragma once

#include <memory>
#include <string>
#include <vector>

#include <folly/fibers/AddTasks.h>

#include "mcrouter/lib/mc/msg.h"
#include "mcrouter/lib/Operation.h"
#include "mcrouter/lib/Reply.h"
#include "mcrouter/lib/RouteHandleTraverser.h"

namespace facebook { namespace memcache {

/**
 * Sends the same request to all child route handles.
 * Collects replies until some result appears (half + 1) times
 * (or all results if that never happens).
 * Responds with one of the replies with the most common result.
 * Ties are broken using Reply::reduce().
 */
template <class RouteHandleIf>
class AllMajorityRoute {
 public:
  static std::string routeName() { return "all-majority"; }

  template <class Request>
  void traverse(const Request& req,
                const RouteHandleTraverser<RouteHandleIf>& t) const {
    t(children_, req);
  }

  explicit AllMajorityRoute(std::vector<std::shared_ptr<RouteHandleIf>> rh)
      : children_(std::move(rh)) {
    assert(!children_.empty());
  }

  template <class Request>
  ReplyT<Request> route(const Request& req) const {
    using Reply = ReplyT<Request>;

    std::vector<std::function<Reply()>> funcs;
    funcs.reserve(children_.size());
    auto reqCopy = std::make_shared<Request>(req.clone());
    for (auto& rh : children_) {
      funcs.push_back(
        [reqCopy, rh]() {
          return rh->route(*reqCopy);
        }
      );
    }

    size_t counts[mc_nres];
    std::fill(counts, counts + mc_nres, 0);
    size_t majorityCount = 0;
    Reply majorityReply = Reply(DefaultReply, req);

    auto taskIt = folly::fibers::addTasks(funcs.begin(), funcs.end());
    taskIt.reserve(children_.size() / 2 + 1);
    while (taskIt.hasNext() &&
           majorityCount < children_.size() / 2 + 1) {

      auto reply = taskIt.awaitNext();
      auto result = reply.result();

      ++counts[result];
      if ((counts[result] == majorityCount && reply.worseThan(majorityReply)) ||
          (counts[result] > majorityCount)) {
        majorityReply = std::move(reply);
        majorityCount = counts[result];
      }
    }

    return majorityReply;
  }

 private:
  const std::vector<std::shared_ptr<RouteHandleIf>> children_;
};

}} // facebook::memcache
