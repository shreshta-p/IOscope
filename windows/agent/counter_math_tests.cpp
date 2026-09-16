#include "counter_math.hpp"
#include <iostream>
#include <cmath>
int main(){
using ioscope::cpu_percent;const ioscope::CpuTimes baseline{100,150,50};
if(std::abs(*cpu_percent({125,200,100},baseline)-75)>.00001)return 1;
if(cpu_percent(baseline,baseline)||cpu_percent({99,200,100},baseline)||cpu_percent({200,190,100},baseline)||cpu_percent({120,149,100},baseline))return 2;
if(*cpu_percent({200,250,50},baseline)!=0)return 3;
std::cout<<"PASS: CPU delta, idle, reset, zero interval and inconsistent counters\n";return 0;
}
