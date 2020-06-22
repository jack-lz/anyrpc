#ifndef BT_HOOKER_H
#define BT_HOOKER_H

#include "anyrpc/anyrpc.h"
#include <string>
#include <set>

class Hooker
{
public:
    Hooker();
    ~Hooker();

    static Hooker* getInstance();

    virtual bool init();

    virtual bool start_listen();
    virtual bool stop_listen();

    // 注册供外部调用的接口，通常是各种 Callback
    virtual bool RegMethod(anyrpc::Function* function, std::string const& name);

    // 调用外部Hook server的接口
    virtual bool CallHook(const char* method, anyrpc::Value& params, anyrpc::Value& result);

    virtual bool is_hooked(const char* method);

private:
    anyrpc::Server* rpcServer;
    anyrpc::Client* rpcClient;

    static void HookIt(anyrpc::Value& params, anyrpc::Value& result);
    static void UnHook(anyrpc::Value& params, anyrpc::Value& result);
    std::set<std::string> setHookedMethod;
};

#endif // BT_HOOKER_H
