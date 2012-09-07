#include "Resolver.h"
#include <muduo/base/Logging.h>
#include <muduo/net/Channel.h>
#include <muduo/net/EventLoop.h>
#include <boost/bind.hpp>

#include "udns.h"
#include <assert.h>
#include <stdio.h>

namespace
{

int init_udns()
{
  static bool initialized = false;
  if (!initialized)
    ::dns_init(NULL, 0);
  initialized = true;
  return 1;
}

struct UdnsInitializer
{
  UdnsInitializer()
  {
    init_udns();
  }

  ~UdnsInitializer()
  {
    ::dns_reset(NULL);
  }
};

UdnsInitializer udnsInitializer;

const bool kDebug = false;
}

using namespace muduo::net;

Resolver::Resolver(EventLoop* loop)
  : loop_(loop),
    ctx_(NULL),
    fd_(-1),
    timerActive_(false)
{
  init_udns();
  ctx_ = ::dns_new(NULL);
  assert(ctx_ != NULL);
  ::dns_set_opt(ctx_, DNS_OPT_TIMEOUT, 2);
}

Resolver::~Resolver()
{
  channel_->disableAll();
  channel_->remove();
  ::dns_free(ctx_);
}

void Resolver::start()
{
  fd_ = ::dns_open(ctx_);
  channel_.reset(new Channel(loop_, fd_));
  channel_->setReadCallback(boost::bind(&Resolver::onRead, this, _1));
  channel_->enableReading();
}

bool Resolver::resolve(const StringPiece& hostname, const Callback& cb)
{
  loop_->assertInLoopThread();
  QueryData* queryData = new QueryData(this, cb);
  time_t now = time(NULL);
  struct dns_query* query =
    ::dns_submit_a4(ctx_, hostname.data(), 0, &Resolver::dns_query_a4, queryData);
  int timeout = ::dns_timeouts(ctx_, -1, now);
  LOG_DEBUG << "timeout " <<  timeout << " active " << timerActive_ << " " << queryData;
  if (!timerActive_)
  {
    loop_->runAfter(timeout, boost::bind(&Resolver::onTimer, this));
    timerActive_ = true;
  }
  return query != NULL;
}

void Resolver::onRead(Timestamp t)
{
  LOG_DEBUG << "onRead " << t.toString();
  ::dns_ioevent(ctx_, t.secondsSinceEpoch());
}

void Resolver::onTimer()
{
  assert(timerActive_ == true);
  time_t now = loop_->pollReturnTime().secondsSinceEpoch();
  int timeout = ::dns_timeouts(ctx_, -1, now);
  LOG_DEBUG << "onTimer " << loop_->pollReturnTime().toString()
            << " timeout " <<  timeout;

  if (timeout < 0)
  {
    timerActive_ = false;
  }
  else
  {
    loop_->runAfter(timeout, boost::bind(&Resolver::onTimer, this));
  }
}

void Resolver::onQueryResult(struct dns_rr_a4 *result, const Callback& callback)
{
  int status = ::dns_status(ctx_);
  LOG_DEBUG << "onQueryResult " << status;
  struct sockaddr_in addr;
  bzero(&addr, sizeof addr);
  addr.sin_family = AF_INET;
  addr.sin_port = 0;
  if (result)
  {
    if (kDebug)
    {
      printf("cname %s\n", result->dnsa4_cname);
      printf("qname %s\n", result->dnsa4_qname);
      printf("ttl %d\n", result->dnsa4_ttl);
      printf("nrr %d\n", result->dnsa4_nrr);
      for (int i = 0; i < result->dnsa4_nrr; ++i)
      {
        char buf[32];
        ::dns_ntop(AF_INET, &result->dnsa4_addr[i], buf, sizeof buf);
        printf("  %s\n", buf);
      }
    }
    addr.sin_addr = result->dnsa4_addr[0];
  }
  InetAddress inet(addr);
  callback(inet);
}

void Resolver::dns_query_a4(struct dns_ctx *ctx, struct dns_rr_a4 *result, void *data)
{
  QueryData* query = static_cast<QueryData*>(data);

  assert(ctx == query->owner->ctx_);
  query->owner->onQueryResult(result, query->callback);
  free(result);
  delete query;
}

