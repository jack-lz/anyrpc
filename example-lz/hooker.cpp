#include "VRHooker.h"
#include "hooker.h"
#include <unistd.h>
#include "vrlog.h"

using namespace anyrpc;
using namespace std;

// Define the method functions that can be called
void Add(Value& params, Value& result)
{
    if ((!params.IsArray()) ||
        (params.Size() != 2) ||
        (!params[0].IsNumber()) ||
        (!params[1].IsNumber()))
    {
        result = "Invalid parameters";
        return;
    }
    result = params[0].GetDouble() + params[1].GetDouble();
}

void Subtract(Value& params, Value& result)
{
    if ((!params.IsArray()) ||
        (params.Size() != 2) ||
        (!params[0].IsNumber()) ||
        (!params[1].IsNumber()))
    {
        result = "Invalid parameters";
        return;
    }
    result = params[0].GetDouble() - params[1].GetDouble();
}

// Define a class method that could keep its own state variables
class Multiply : public Method
{
public:
    Multiply() :
        Method("multiply", "Multiply two numbers",false) {}
    virtual void Execute(Value& params, Value& result)
    {
        if ((!params.IsArray()) ||
            (params.Size() != 2) ||
            (!params[0].IsNumber()) ||
            (!params[1].IsNumber()))
        {
            result = "Invalid parameters";
            return;
        }
        result = params[0].GetDouble() * params[1].GetDouble();
    }
};

Multiply multiply;

// The Wait function will delay a given number of milliseconds before returning.
void Wait(Value& params, Value& result)
{
    if ((!params.IsArray()) ||
        (params.Size() != 1) ||
        (!params[0].IsNumber()))
    {
        result = "Invalid parameters";
        return;
    }
    int delay = params[0].GetInt();
    usleep(delay * 1000);
    result.SetNull();
}

void Echo(Value& params, Value& result)
{
    result = params;
}

//================================================================

Hooker::Hooker()
{
    rpcServer = NULL;
    rpcClient = NULL;
}

Hooker::~Hooker()
{

}

Hooker* Hooker::getInstance()
{
    Hooker * pInstance = NULL;
    static Hooker hooker;
    pInstance = &hooker;
    if(pInstance->rpcServer == NULL)
    {
        pInstance->init();
    }
    return pInstance;
}

bool Hooker::init()
{
    rpcServer = new JsonTcpServerMT();

    MethodManager *methodManager = rpcServer->GetMethodManager();
    methodManager->AddFunction( &Add, "add", "Add two numbers");
    methodManager->AddFunction( &Subtract, "subtract", "Subtract two numbers");
    methodManager->AddMethod( &multiply );
    methodManager->AddFunction( &Wait, "wait", "Delay execution for a given number of milliseconds");
    methodManager->AddFunction( &Echo, "echo", "Return the same data that was sent");
    methodManager->AddFunction( &HookIt, "hook", "hook a method");

    android::VRHooker::getInstance();    // 构造实例，注册接口
    return true;
}

bool Hooker::start_listen()
{
    if (NULL == rpcServer)
    {
        return false;
    }

    // 如果 9000 端口被占用会怎样, TODO
    rpcServer->BindAndListen(9000);
    rpcServer->SetMaxConnections(10);
    rpcServer->StartThread();
    return true;
}

bool Hooker::RegMethod(anyrpc::Function* function, std::string const& name)
{
    if (NULL == rpcServer)
    {
        return false;
    }
    MethodManager *methodManager = rpcServer->GetMethodManager();
    methodManager->AddFunction(function, name, "no info");
    return true;
}

bool Hooker::stop_listen()
{
    rpcServer->StopThread();
    rpcServer->Shutdown();
    delete rpcServer;
    rpcServer = NULL;
    return true;
}

// 调用外部Hook的接口
bool Hooker::CallHook(const char* method, Value& params, Value& result)
{
    if (NULL == rpcClient)
    {
        return false;
    }

    // Hook 接口对应同一个 method，Hook Method 名字作为第一个参数传递给外部
    Value finalParams;
    finalParams[0] = method;
    if (params.IsArray())
    {
        for (int i = 0; i < params.Size(); ++i)
        {
            finalParams[i+1] = params[i];
        }
    }
    return rpcClient->Call("CallHook", finalParams, result);
}

void Hooker::HookIt(anyrpc::Value& params, anyrpc::Value& result)
{
    if ((!params.IsArray()) ||
        (params.Size() != 1) ||
        (!params[0].IsString()))
    {
        result = "Invalid parameters";
        return;
    }

    Hooker* pHooker = Hooker::getInstance();
    if (NULL == pHooker->rpcClient)
    {
        std::list<std::string> ips;
        std::list<unsigned> ports;
        pHooker->rpcServer->GetConnectionsPeerInfo(ips, ports);
        if (ips.empty())
        {
            result = "internal error";
            return;
        }
        // 只使用 ips 中的第一个元素，port 固定为 9100
        string remoteIP = *(ips.begin());
        // 创建用来调用 VRVM 的连接
        pHooker->rpcClient = new JsonTcpClient();
        pHooker->rpcClient->SetServer(remoteIP.data(), 9100);
        pHooker->rpcClient->SetTimeout(10 * 1000);

        bool bStart = pHooker->rpcClient->Start();
        if (!bStart)
        {
            VR_LOG("rpcClient Start Failed!");
            delete pHooker->rpcClient;
            pHooker->rpcClient = NULL;

            result = "connect error";
            return;
        }
    }
    const char* methodName = params[0].GetString();
    pHooker->setHookedMethod.insert(methodName);
    result = "ok";
}

void Hooker::UnHook(anyrpc::Value& params, anyrpc::Value& result)
{
    if ((!params.IsArray()) ||
        (params.Size() != 1) ||
        (!params[0].IsString()))
    {
        result = "Invalid parameters";
        return;
    }

    const char* methodName = params[0].GetString();
    Hooker* pHooker = Hooker::getInstance();
    if (pHooker->setHookedMethod.find(methodName) == pHooker->setHookedMethod.end())
    {
        result = "method can't find";
        return;
    }
    pHooker->setHookedMethod.erase(methodName);
    result = "ok";
}

bool Hooker::is_hooked(const char* method)
{
    if (NULL == rpcClient)
    {
        return false;
    }
    return setHookedMethod.find(method) != setHookedMethod.end();
}
