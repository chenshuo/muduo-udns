#include "Resolver.h"
#include <muduo/net/EventLoop.h>
#include <stdio.h>

using namespace muduo::net;

EventLoop* g_loop;
int count = 0;

void resolveCallback(const InetAddress& addr)
{
  printf("resolveCallback %s\n", addr.toHostPort().c_str());
  if (++count == 2)
    g_loop->quit();
}

int main(int argc, char* argv[])
{
  EventLoop loop;
  g_loop = &loop;
  Resolver resolver(&loop);
  resolver.start();
  resolver.resolve("www.chenshuo.com", resolveCallback);
  resolver.resolve("www.google.com", resolveCallback);
  loop.loop();
}
