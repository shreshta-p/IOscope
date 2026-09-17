#pragma once
// 7E (docs/07-EXPERIMENT-SPEC.md): storage-to-GPU pipeline. Capability-gated
// only -- no execution engine is implemented (this VM has no GPU to build or
// test one against; writing untestable CUDA code would violate the project's
// "never mark untested gates complete" rule). This probe exists so the
// experiment is honestly reported as unsupported here rather than silently
// omitted, and so a future implementation has a real capability check to
// build on.
//
// Per spec: "CUDA runtime/device presence must be probed; NVML alone does not
// establish CUDA availability." This probes the actual CUDA driver API
// (libcuda, not NVML) by attempting cuInit() and a device count query --
// dlopen'd, not linked, so the agent still builds and runs with no CUDA
// driver installed at all.
#include <dlfcn.h>
namespace ioscope {
inline bool cuda_available(){
 void* handle=dlopen("libcuda.so.1",RTLD_LAZY|RTLD_LOCAL);
 if(!handle)handle=dlopen("libcuda.so",RTLD_LAZY|RTLD_LOCAL);
 if(!handle)return false;
 struct Closer{void* h;~Closer(){dlclose(h);}} closer{handle};
 using InitFn=int(*)(unsigned int);
 using DeviceCountFn=int(*)(int*);
 auto cuInit=reinterpret_cast<InitFn>(dlsym(handle,"cuInit"));
 auto cuDeviceGetCount=reinterpret_cast<DeviceCountFn>(dlsym(handle,"cuDeviceGetCount"));
 if(!cuInit||!cuDeviceGetCount)return false;
 if(cuInit(0)!=0)return false;
 int count=0;
 if(cuDeviceGetCount(&count)!=0)return false;
 return count>0;
}
}
