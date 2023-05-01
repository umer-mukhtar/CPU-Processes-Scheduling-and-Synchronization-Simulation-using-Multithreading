#include <iostream>

using namespace std;


struct PCB{
    int pid;
    string pName;
    char procType;
    int pointer;
    int programCounter;
    int priority;
    string pState;

    double arrivTime;
    double cpuTime;
    double IOTime;
    double remCpuTime;
    double remIOTime;

    int lastCpu;
};