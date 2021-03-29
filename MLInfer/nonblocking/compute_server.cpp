#include "gen-cpp/Compute.h"
#include "gen-cpp/compute_types.h"
#include <thrift/TProcessor.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/protocol/TCompactProtocol.h>
#include <thrift/transport/TNonblockingServerSocket.h>
#include <thrift/server/TNonblockingServer.h>
#include <thrift/concurrency/ThreadManager.h>
#include <thrift/concurrency/ThreadFactory.h>

#include <torch/script.h>
#include <memory>
#include <thread>
#include <iostream>
#include <string>
#include <atomic>
#include <unistd.h>
#include <cstdlib>
#include <mutex>
#include <sstream>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;
using namespace ::apache::thrift;
using std::chrono::milliseconds;
using std::chrono::duration_cast;
using std::chrono::system_clock;

using std::make_shared;
using std::shared_ptr;

void ml_inference(){
    torch::jit::script::Module module;
    try{
        module = torch::jit::load("./traced_resnet_model.pt");
    }catch(const c10::Error& e){
        std::cerr << "Error loading the model" << std::endl;
    }

    std::vector<torch::jit::IValue> inputs;
    inputs.push_back(torch::ones({1,3,224,224}));

    at::Tensor output = module.forward(inputs).toTensor();
    std::cout << output.slice(1, 0, 5) << std::endl;
    std::cout <<"Inference completed" << std::endl;
}

class ComputeHandler: public ComputeIf {

    public: 

        virtual void computed_value(const int32_t row, const int32_t column) override{
            std::cout<<"Matrix values are "<< row << " "<< column <<std::endl;
            ml_inference();
        }
        virtual void compute_list(std::string& _return, const std::vector<int64_t> & values){
            _return.assign("Hello World");
        }
};

class ComputeHandlerFactory: public ComputeIfFactory {
    public:

    ComputeHandlerFactory() {;}

    virtual ComputeIf* getHandler(const TConnectionInfo& conn){
        return new ComputeHandler();
    }

    virtual void releaseHandler(ComputeIf* handler){
        delete handler;
    }
};

int main(){
    auto trans = make_shared<TNonblockingServerSocket>(3057);
    auto handler_fac = make_shared<ComputeHandlerFactory>();
    auto processor_fac = make_shared<ComputeProcessorFactory>(handler_fac);
    auto protocol_fac = make_shared<TCompactProtocolFactoryT<TMemoryBuffer>>();

    auto thread_man = ThreadManager::newSimpleThreadManager(2);
    thread_man->threadFactory(make_shared<ThreadFactory>());
    thread_man->start();

    TNonblockingServer server(processor_fac, protocol_fac, trans, thread_man);
    server.setNumIOThreads(2);
    server.serve();

    return 0;
}