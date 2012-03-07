#include "Resolver.h"
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

}

using namespace muduo::net;

Resolver::Resolver(EventLoop* loop)
  : loop_(loop),
    ctx_(NULL)
{
  init_udns();
  ctx_ = ::dns_new(NULL);
  assert(ctx_ != NULL);
}

Resolver::~Resolver()
{
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
  struct dns_query* query =
    ::dns_submit_a4(ctx_, hostname.data(), 0, &Resolver::dns_query_a4, queryData);
  time_t now = time(NULL);
  int timeout = ::dns_timeouts(ctx_, -1, now);
  // FIXME timer
  printf("timeout %d\n", timeout);
  // ::dns_ioevent(ctx_, now);
  return query != NULL;
}

void Resolver::onRead(Timestamp t)
{
  printf("onRead\n");
  ::dns_ioevent(ctx_, t.microSecondsSinceEpoch() / Timestamp::kMicroSecondsPerSecond);
}

void Resolver::onQueryResult(struct dns_rr_a4 *result, const Callback& callback)
{
  printf("onQueryResult %p %d\n", result, ::dns_status(ctx_));
  struct sockaddr_in addr;
  bzero(&addr, sizeof addr);
  addr.sin_family = AF_INET;
  addr.sin_port = 0;
  if (result)
  {
    printf("cname %s\n", result->dnsa4_cname);
    printf("qname %s\n", result->dnsa4_qname);
    printf("ttl %d\n", result->dnsa4_ttl);
    printf("nrr %d\n", result->dnsa4_nrr);
    for (int i = 0; i < result->dnsa4_nrr; ++i)
    {
      char buf[32];
      dns_ntop(AF_INET, &result->dnsa4_addr[i], buf, sizeof buf);
      printf("  %s\n", buf);
    }
    addr.sin_addr = result->dnsa4_addr[0];
  }
  InetAddress inet(addr);
  callback(inet);
}

void Resolver::dns_query_a4(struct dns_ctx *ctx, struct dns_rr_a4 *result, void *data)
{
  QueryData* query = static_cast<QueryData*>(data);

  printf("dns_query_a4 %p\n", query);
  assert(ctx == query->owner->ctx_);
  query->owner->onQueryResult(result, query->callback);
  free(result);
  delete query;
}
