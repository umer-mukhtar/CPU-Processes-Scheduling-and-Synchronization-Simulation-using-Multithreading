#include "maxQ.h"
//20i-0555 Saad Bin Farooq
//20i-0696 Umer Mukhtar
//OS FINAL PROJECT
#include <unistd.h>
#include <sys/types.h>
#include <fstream>
#include <errno.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cmath>
#include <iostream>
#include <pthread.h>
#include <time.h>
#include <semaphore.h>
#include <fstream>
#include <iomanip>
#include <vector>
#include <chrono>
// #include <queue>

using namespace std;
using namespace std::chrono;

/*
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
};
*/

void wake_up(PCB*);

sem_t pcbSem;
sem_t stdoutSem;
sem_t executionSem;
sem_t outputSem;
sem_t schedSem;

pthread_mutex_t schedMut;
int noOfCpusGlobal = -1;
char schedTypeGlobal = '0';
int processCounter = 1; //for assigning pids
vector<int> isCpuBusy; //0 for idle, 1 for true
vector<int> executingProcess;
auto programStart = high_resolution_clock::now();

double RRTimeout = 0.5; //Timeout for the Round robin


PCB* thisState = NULL;

vector<PCB*> pcb;
Queue readyQueue;

void* jobQueue(void* args){
    auto start = high_resolution_clock::now(); //get start time
    programStart = high_resolution_clock::now();
    for(int i = 0;i<pcb.size();i++){
        auto curr = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(curr - start);
        if(duration.count() >= pcb[i]->arrivTime * 1000000){
            if(schedTypeGlobal == 'f' || schedTypeGlobal == 'r'){
                pcb[i]->priority=-1;
            }
            wake_up(pcb[i]);

            // Uncomment for display
            //sem_wait(&stdoutSem);
            //cout << "Process arrived " << pcb[i]->pid << " at time " << (duration.count() / 1000000) <<"s"<< endl;
            //sem_post(&stdoutSem);
            
        }
        else{
            i--;
        }
    }
    pthread_exit(NULL);
}

void terminate(PCB* pcbPtr, long long terminationTime){
    sem_wait(&pcbSem);

    // uncomment for display
    //sem_wait(&stdoutSem);
    //cout << "Process terminated " << pcbPtr->pid<< " at time " << terminationTime / 1000000 << endl;
    //sem_post(&stdoutSem);
    

    // for(int i = 0;i<pcb.size();i++){
    //     if(pcbPtr->pid == pcb[i].pid){
    //         // pcb.erase(pcb.begin() + i);
    //         pcb[i].pid = -1;
    //         pcb[i].pState = "TERMINATED";
    //     }
    // }
    pcbPtr->pState = "TERMINATED";
    pcbPtr->pid = -1;
    sem_post(&pcbSem);
}

void wake_up(PCB* pcbPtr){ //places process into ready queue

        /*
        sem_wait(&stdoutSem);
        cout << "I was called " << pcbPtr << endl  ;
        sem_post(&stdoutSem);
        */

    readyQueue.enqueue(pcbPtr);
    sem_wait(&pcbSem);
    pcbPtr->pState="READY";
    sem_post(&pcbSem);
}

void* yieldThread(void* args){ //args is pcbPtr
    PCB* pcbPtr = (PCB*) args;
    auto start = high_resolution_clock::now();
    while(1){
        auto curr = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(curr - start);
        if(pcbPtr->IOTime > 0 && pcbPtr->cpuTime == 0 && pcbPtr->remCpuTime == 0) //Check if CPU bursts are finished and I/O is left
        {
            if(duration.count() >= pcbPtr->IOTime * 2 * 1000000) //wait for the duration
            {
                auto dur2 = duration_cast<microseconds>(curr - programStart);
                sem_wait(&pcbSem);
                pcbPtr->IOTime = 0; //finishing all I/O tasks
                sem_post(&pcbSem);

                //wake_up(pcbPtr, dur2.count()); //This will execute the process just once after its I/O is complete, the alternative is direct termination

                //Uncomment for display
                //sem_wait(&stdoutSem);
                //cout << "Process finished waiting " << pcbPtr->pid << pcbPtr->pState <<" at time " << (dur2.count() / 1000000)<<"s"<< endl;
                //sem_post(&stdoutSem);
                

                terminate(pcbPtr, dur2.count());

                break;
            }
            
        }
        else if(duration.count() >= 2 * 1000000 && (pcbPtr->cpuTime > 0 || pcbPtr->remCpuTime > 0)){ //2 seconds for the waiting
            auto dur2 = duration_cast<microseconds>(curr - programStart);
            pcbPtr->IOTime -= 1; //Decrementing the number of I/O cycles

            /*
            sem_wait(&stdoutSem);
            cout << "Waking up in yield" << pcbPtr->pid << endl;
            sem_post(&stdoutSem);
            */

           // Uncomment for display
           // sem_wait(&stdoutSem);
           // cout << "Process finished waiting " << pcbPtr->pid <<pcbPtr->pState <<" at time " << (dur2.count() / 1000000) <<"s"<< endl;
           // sem_post(&stdoutSem);

            wake_up(pcbPtr);
            
            
            break;
        }
    }
    pthread_exit(NULL);
}

void yield(PCB* pcbPtr, long long yieldTime){
    sem_wait(&pcbSem);
    // Uncomment for display
    //sem_wait(&stdoutSem);
    //cout << "Process started waiting " << pcbPtr->pid<< " at time " << yieldTime / double(1000000) << endl;
    //sem_post(&stdoutSem);
    

    // for(int i = 0;i<pcb.size();i++){
    //     if(pcbPtr->pid == pcb[i].pid){
    //         pcb[i].pState = "YIELDED";
    //     }
    // }
    pcbPtr->pState = "WAITING";
    sem_post(&pcbSem);
    pthread_t yieldT;
    pthread_create(&yieldT,NULL,yieldThread,(void*)pcbPtr);
    pthread_detach(yieldT);
}

void idle(int cpuNo){
    isCpuBusy[cpuNo] = 0;
}

void contextSwitch(PCB* pcbPtr,int cpuNo){ //called by scheduler, also the dispatcher
    if(pcbPtr == NULL){
        idle(cpuNo);
        // isCpuBusy[cpuNo] = 0;
    }
    else{
        executingProcess[cpuNo]=pcbPtr->pid;
        sem_wait(&pcbSem);
        pcbPtr->pState="RUNNING";
        sem_post(&pcbSem);

            /*
            sem_wait(&stdoutSem);
            cout << "Process state is " << pcbPtr << " for ID: " << pcbPtr->pid << endl;
            sem_post(&stdoutSem);
            */

        thisState = pcbPtr;
        isCpuBusy[cpuNo]=1;

        sem_post(&executionSem);
        
    }
}

void force_preempt(PCB* pcbPtr, int cpuNo)
{
    if(pcbPtr == NULL)
    {
        idle(cpuNo);
        // isCpuBusy[cpuNo] = 0;
    }
    else
    {
        // Uncomment for display
        //sem_wait(&stdoutSem);
        //cout << "Forced Preemption, process " << pcbPtr->pid  << " into cpu " << cpuNo <<endl;
        //sem_post(&stdoutSem);
        sem_wait(&pcbSem);
        for(int i = 0;i<noOfCpusGlobal;i++){
            if(executingProcess[cpuNo] == pcb[i]->pid){
                pcb[i]->lastCpu=cpuNo;
            }
        }

        executingProcess[cpuNo]=pcbPtr->pid;
        // sem_wait(&pcbSem);
        pcbPtr->pState="RUNNING";
        sem_post(&pcbSem);
        isCpuBusy[cpuNo]=1;
        sem_post(&executionSem);
    }
}

void preempt(PCB* pcbPtr)
{ //places process into ready queue

     //Uncomment for display
    //sem_wait(&stdoutSem);
    //cout << "Preemption, process " << pcbPtr->pid  << " moved into ready queue\n ";
    //sem_post(&stdoutSem);

    readyQueue.enqueue(pcbPtr);
    sem_wait(&pcbSem);
    pcbPtr->pState="READY";
    sem_post(&pcbSem);
}

void* scheduler(void* args){ //selects a process to execute

    int* selfId = (int*) args;
    while(1)
    {
        if(isCpuBusy[*selfId] == 0 && schedTypeGlobal != 'r'){
            if(readyQueue.isEmpty()){
                contextSwitch(NULL, *selfId);
            }
            else{
                if(schedTypeGlobal=='p'){
                    sem_wait(&executionSem);
                    // sem_wait(&stdoutSem);
                    // readyQueue.display();
                    // cout<<endl;
                    // sem_post(&stdoutSem);
                    PCB* tempPcb = readyQueue.dequeue();
                    contextSwitch(tempPcb, *selfId);
                    sem_post(&schedSem);
                }
                else{
                    sem_wait(&schedSem);
                    PCB* tempPcb;
                    QueueNode* currNode = readyQueue.front;
                    QueueNode* prev = NULL;
                    bool affinityFound=false;
                    while(currNode!=NULL){
                        if(currNode->pcb->lastCpu==*selfId && currNode==readyQueue.front){
                            // affinityFound=true;
                            //as affinity is at the head of the queue
                            break;
                        }
                        else if(currNode->pcb->lastCpu==*selfId ){
                            tempPcb = currNode->pcb;
                            prev->next=currNode->next;
                            affinityFound=true;
                            break;
                        }
                        prev=currNode;
                        currNode=currNode->next;
                    }

                    if(affinityFound){
                        sem_wait(&executionSem);
                    }
                    else{
                        sem_wait(&executionSem);
                        // sem_wait(&stdoutSem);
                        // readyQueue.display();
                        // cout<<endl;
                        // sem_post(&stdoutSem);
                        tempPcb = readyQueue.dequeue();
                    }
                    contextSwitch(tempPcb, *selfId);
                    sem_post(&schedSem);
                }
            }
        }
        else if(isCpuBusy[*selfId] != 0 && schedTypeGlobal == 'p')
        {
            
            bool allBusy = true;
            if(isCpuBusy[*selfId] == 0)
            {
                    allBusy = false;
            }

            if(allBusy)
            {
                if(readyQueue.peek()->pid != -99)
                {
                    int currentCPU = *selfId;
                    PCB* currentProc = NULL;
                    for(int i = 0; i < pcb.size(); i++)
                    {
                        for(int j = 0; j < executingProcess.size(); j++)
                        {
                            if(pcb[i]->pid == executingProcess[j] && j == currentCPU)
                            {
                                currentProc = pcb[i];
                                break;
                            }
                            
                        }
                    }

                    if(currentProc)
                    {
                        if(currentProc->priority < readyQueue.peek()->priority) //force preempt
                        {
                            sem_wait(&executionSem);

                            //Uncomment for display
                            //sem_wait(&stdoutSem);
                            //cout << "Waking up in scheduler" << currentProc->pid << endl;
                            //sem_post(&stdoutSem);
                            

                            wake_up(currentProc);
                            PCB* tempPcb = readyQueue.dequeue();
                            
                            force_preempt(tempPcb, *selfId);
                        }
                    }
                }
            }
            
                
            
        }
        else if(isCpuBusy[*selfId] == 0 && schedTypeGlobal == 'r')
        {
                if(readyQueue.isEmpty()){
                contextSwitch(NULL, *selfId);
            }
            else{
                sem_wait(&schedSem);
                PCB* tempPcb;
                QueueNode* currNode = readyQueue.front;
                QueueNode* prev = NULL;
                bool affinityFound=false;
                while(currNode!=NULL){
                    if(currNode->pcb->lastCpu==*selfId && currNode==readyQueue.front){
                        // affinityFound=true;
                        //as affinity is at the head of the queue
                        break;
                    }
                    else if(currNode->pcb->lastCpu==*selfId ){
                        tempPcb = currNode->pcb;
                        prev->next=currNode->next;
                        affinityFound=true;
                        break;
                    }
                    prev=currNode;
                    currNode=currNode->next;
                }

                if(affinityFound){
                    sem_wait(&executionSem);
                }
                else{
                    sem_wait(&executionSem);
                    // sem_wait(&stdoutSem);
                    // readyQueue.display();
                    // cout<<endl;
                    // sem_post(&stdoutSem);
                    tempPcb = readyQueue.dequeue();
                }
                contextSwitch(tempPcb, *selfId);
                sem_post(&schedSem);
            }
        }
    }
    pthread_exit(NULL);
}

void* controller(void* args){ //new-implementation: does nothing (for now)  //old-implementation:dispatcher, dispatches the process selected by scheduler to the cpu
    
    ofstream outFile;
    outFile.open("output.txt");

    bool isFirst = true;
    if(isFirst)
    {
        sem_wait(&stdoutSem);
        cout << "Time(seconds)   " << "Ru  " << "Re  " << "Wa  ";
        outFile << "Time(seconds)   " << "Ru  " << "Re  " << "Wa  ";
        for(int i = 0; i < noOfCpusGlobal; i++){
            cout << "\tCPU  " << i << "\t";
            outFile << "\tCPU  " << i << "\t";
        }
        cout << "  I/O\n\n";
        outFile << "  I/O\n\n";
        cout << "-------------------------------------------------------\n\n";
        outFile << "-------------------------------------------------------\n\n";
        sem_post(&stdoutSem);
        isFirst = false;
    }
    
    while(1)
    {
        auto start = high_resolution_clock::now();
        while(1)
        {
            auto curr = high_resolution_clock::now();
            auto duration = duration_cast<microseconds>(curr - start);

            if(duration.count() >= 0.1 * 1000000)
            {
                auto duration2 = duration_cast<microseconds>(curr - programStart);
                int running = 0;
                int ready = 0;
                int waiting = 0;
                for(int i = 0; i < pcb.size(); i++)
                {
                    if(pcb[i]->pState == "RUNNING")
                        running++;
                    if(pcb[i]->pState == "READY")
                        ready++;
                    if(pcb[i]->pState == "WAITING")
                        waiting++;
                }

                sem_wait(&outputSem);
                cout << round((duration2.count() / double(1000000)) * 10) / 10.0;
                outFile << round((duration2.count() / double(1000000)) * 10) / 10.0;
                cout << "               " << running <<"   "<<ready << "   " <<waiting<<"\t";
                outFile << "               " << running <<"   "<<ready << "   " <<waiting<<"\t";
                sem_post(&outputSem);




                for(int i = 0; i < isCpuBusy.size(); i++)
                {
                    if(isCpuBusy[i] == 0){
                        cout << "  (IDLE)  ";
                        outFile << "  (IDLE)  ";
                    }
                    else
                    {
                        for(int j = 0; j < pcb.size(); j++)
                        {
                            if(executingProcess[i] == pcb[j]->pid)
                            {
                                cout << " " << pcb[j]->pName << "\t";
                                outFile << " " << pcb[j]->pName << "\t";
                                break;
                            }
                                
                        }
                    }
                    
                }

                for(int i = 0;i<pcb.size();i++){
                    if(pcb[i]->pState=="WAITING"){
                        cout<<pcb[i]->pName<<"  ";
                        outFile<<pcb[i]->pName<<"  ";
                    }
                }
                cout << endl;
                outFile << endl;

                break;
            }


        }

    }
   

    pthread_exit(NULL);
}

void* cpuFunc(void* args){
    int* selfId = (int*) args;
    while(1){
        if(isCpuBusy[*selfId] == 1){
            if(schedTypeGlobal == 'f'){ //FCFS
                auto start2 = high_resolution_clock::now(); //get start time
                auto duration2 = duration_cast<microseconds>(start2 - programStart);
                //Uncomment for display
                //sem_wait(&stdoutSem);
                //cout << "Executing process " << executingProcess[*selfId] <<" at time " << duration2.count() / 1000000 <<endl;
                //sem_post(&stdoutSem);
                



                int procIndex=-1;
                
                sem_wait(&executionSem);
                for(int i = 0;i < pcb.size(); i++){
                    if(pcb[i]->pid == executingProcess[*selfId]){
                        procIndex = i;
                        break;
                    }
                }
                sem_post(&executionSem);
                
                /*
                sem_wait(&stdoutSem);
                cout << "Executing process " << pcb[procIndex]->pState << endl;
                sem_post(&stdoutSem);
                */

                auto start = high_resolution_clock::now(); //get start time
                auto curr = high_resolution_clock::now();
                auto duration = duration_cast<microseconds>(curr - start);
                while(1){
                    auto curr = high_resolution_clock::now();
                    auto duration = duration_cast<microseconds>(curr - start);
                    if(pcb[procIndex]->cpuTime != 0 && pcb[procIndex]->IOTime != -1 && (pcb[procIndex]->cpuTime + pcb[procIndex]->remCpuTime) > 4){
                        if(duration.count() >= pcb[procIndex]->cpuTime * 1000000){

                            auto dur2 = duration_cast<microseconds>(curr - programStart);
                            pcb[procIndex]->cpuTime = 0; //zeroing cpu time so that it gets yielded and then go to the else condition after waiting to terminate
                            pcb[procIndex]->lastCpu=*selfId;
                            yield(pcb[procIndex],dur2.count());
                            isCpuBusy[*selfId] = 0;
                            break;
                        }
                    }
                    else{ //remaining cpu time

                        
                        if(duration.count() >= pcb[procIndex]->remCpuTime * 1000000){
                            auto dur2 = duration_cast<microseconds>(curr - programStart);
                            if(pcb[procIndex]->IOTime > 0) //To complete the remaining I/O cycles
                            {
                                pcb[procIndex]->remCpuTime = 0;
                                pcb[procIndex]->lastCpu=*selfId;
                                yield(pcb[procIndex], dur2.count());
                                isCpuBusy[*selfId] = 0;
                                break;
                            }
                            terminate(pcb[procIndex],dur2.count());
                            isCpuBusy[*selfId] = 0;
                            break;
                        }
                    }
                }
            }
            if(schedTypeGlobal == 'p'){ //PRIORITY

                auto start2 = high_resolution_clock::now(); //get start time
                auto duration2 = duration_cast<microseconds>(start2 - programStart);

                // Uncomment for display
                //sem_wait(&stdoutSem);
                //cout << "Executing process " << executingProcess[*selfId] << " at time " << duration2.count() / 1000000 <<" on cpu " << *selfId <<endl;
                //sem_post(&stdoutSem);
                



                int procIndex=-1;
                
                for(int i = 0;i < pcb.size(); i++){
                    if(pcb[i]->pid == executingProcess[*selfId]){
                        procIndex = i;
                    }
                }
                auto start = high_resolution_clock::now(); //get start time
                auto curr = high_resolution_clock::now();
                auto duration = duration_cast<microseconds>(curr - start);
                
                while(1){

                    auto curr = high_resolution_clock::now();
                    auto duration = duration_cast<microseconds>(curr - start);
                    if(pcb[procIndex]->cpuTime != 0)
                    {
                       
                        if(pcb[procIndex]->pid != executingProcess[*selfId]) //if another process with higher priority needs to run
                        {
                            auto dur2 = duration_cast<microseconds>(curr - programStart);
                            //Uncomment for display
                            //sem_wait(&stdoutSem);
                            //cout << "A forced preempt occurred, process  "  << pcb[procIndex]->pid << " has been preempted "<< " at time " << dur2.count() / 1000000 <<endl;
                            //cout << "Rem time was " << pcb[procIndex].cpuTime - ((dur2.count() / 1000000) - duration2.count() / 1000000) << endl;
                            //sem_post(&stdoutSem);
                            

                            sem_wait(&pcbSem);
                            pcb[procIndex]->cpuTime -= (dur2.count() / 1000000) - duration2.count() / double(1000000); //remaining time
                            pcb[procIndex]->remCpuTime = pcb[procIndex]->cpuTime;
                            sem_post(&pcbSem);

                           
                            break;
                        } 

                        else if(duration.count() >= pcb[procIndex]->cpuTime * 1000000){

                            auto dur2 = duration_cast<microseconds>(curr - programStart);

                            //Uncomment for display
                            //sem_wait(&stdoutSem);
                            //cout << "Here for process id " << pcb[procIndex]->pid <<endl;
                            //sem_post(&stdoutSem);
                            

                            
                            pcb[procIndex]->cpuTime = 0; //zeroing cpu time so that it gets yielded and then go to the else condition after waiting to terminate
                            pcb[procIndex]->remCpuTime = 0;
                            
                            
                            if(pcb[procIndex]->IOTime > 0)
                            {
                                pcb[procIndex]->lastCpu=*selfId;
                                yield(pcb[procIndex],dur2.count());
                                isCpuBusy[*selfId] = 0;
                                break;
                            }
                            else
                            {
                                terminate(pcb[procIndex], dur2.count());
                                isCpuBusy[*selfId] = 0;
                                break;
                            }

                            
                            
                        }
                        
                    }
                    else{ //remaining cpu time

                        if(duration.count() >= pcb[procIndex]->cpuTime * 1000000)
                        {
                            //Uncomment for display
                            //sem_wait(&stdoutSem);
                            //cout << "Direct termination " << pcb[procIndex]->pid << pcb[procIndex]->pName <<endl;
                            //sem_post(&stdoutSem);
                            

                            auto dur2 = duration_cast<microseconds>(curr - programStart);
                            terminate(pcb[procIndex],dur2.count());
                            isCpuBusy[*selfId] = 0;
                            break;
                        }
                        
                        
                    }
                }
            }
            if(schedTypeGlobal == 'r'){ //ROUND ROBIN

                auto start2 = high_resolution_clock::now(); //get start time
                auto duration2 = duration_cast<microseconds>(start2 - programStart);
                // Uncomment for display
                //sem_wait(&stdoutSem);
                //cout << "Executing process " << executingProcess[*selfId] << " at time " << duration2.count() / double(1000000) <<endl;
                //sem_post(&stdoutSem);
                

                int procIndex=-1;
                
                for(int i = 0;i < pcb.size(); i++){
                    if(pcb[i]->pid == executingProcess[*selfId]){
                        procIndex = i;
                    }
                }

                auto start = high_resolution_clock::now(); //get start time
                auto curr = high_resolution_clock::now();
                auto duration = duration_cast<microseconds>(curr - start);

                while(1){
                    auto curr = high_resolution_clock::now();
                    auto duration = duration_cast<microseconds>(curr - start);
                    if(pcb[procIndex]->cpuTime != 0)
                    {

                        if(duration.count() >= RRTimeout * 1000000 || duration.count() >= pcb[procIndex]->cpuTime * 1000000)
                        {
                            auto dur2 = duration_cast<microseconds>(curr - programStart);

                            if(pcb[procIndex]->cpuTime * 1000000 <= RRTimeout * 1000000)
                            {
                                /*
                                sem_wait(&stdoutSem);
                                cout << "Here for process " << pcb[procIndex].pid <<endl;
                                sem_post(&stdoutSem);
                                */

                                sem_wait(&pcbSem);
                                pcb[procIndex]->cpuTime = 0; //remaining time
                                pcb[procIndex]->remCpuTime = pcb[procIndex]->cpuTime;
                                sem_post(&pcbSem);

                                if(pcb[procIndex]->IOTime > 0) //if there is I/O left even when burst is finished
                                {
                                    pcb[procIndex]->lastCpu=*selfId;
                                    yield(pcb[procIndex],dur2.count()); //Go for I/O / Waiting if any is left
                                    isCpuBusy[*selfId] = 0;
                        
                                    break;
                                }
                                else //if I/O and burst both are finished
                                {
                                    terminate(pcb[procIndex],dur2.count());
                                    isCpuBusy[*selfId] = 0;
                                    
                                    break;
                                }
                                
                            }
                            //Updating the remaining time
                            sem_wait(&pcbSem);
                            pcb[procIndex]->cpuTime -= RRTimeout; //remaining time
                            pcb[procIndex]->remCpuTime = pcb[procIndex]->cpuTime;
                            pcb[procIndex]->IOTime -= 1; //IO is done during the cpu bursts
                            sem_post(&pcbSem);
                            
                            pcb[procIndex]->lastCpu=*selfId;
                            preempt(pcb[procIndex]); //Send to the ready Queue/preempt

                            //yield(&pcb[procIndex],dur2.count()); //use preempt() here, not yield
                            isCpuBusy[*selfId] = 0;
                            break;
                        }
                    }
                }
            }
        }
    }
    pthread_exit(NULL);
}

void implement_start()
{
    ifstream inFile;
    inFile.open("Processes1.txt");
    string garbage;
    getline(inFile,garbage);
    while(1){
        PCB* temp = new PCB;
        inFile >> temp->pName;
        inFile >> temp->priority;
        inFile >> temp->arrivTime;
        inFile >> temp->procType;
        inFile >> temp->cpuTime;
        inFile >> temp->IOTime;
        if( schedTypeGlobal == 'f' && temp->IOTime != -1 && temp->cpuTime > 4){
            temp->remCpuTime = temp->cpuTime - floor(temp->cpuTime / 2);
            temp->cpuTime = temp->cpuTime - temp->remCpuTime;
        }
        else{
            temp->remCpuTime = temp->cpuTime;
        }
        temp->remIOTime = temp->IOTime;
        temp->pState="NEW";
        temp->pid = processCounter;
        processCounter++;
        if(inFile.eof()){
            break;
        }
        pcb.push_back(temp);
        // cout << temp << endl;   
    }
    cout << "\n\n\n\n";
    inFile.close();
}
void implement_start_real()
{
    srand(time(0));
    ifstream inFile;
    inFile.open("Processes1.txt");
    string garbage;
    getline(inFile,garbage);
    while(1){
        PCB* temp = new PCB;
        inFile >> temp->pName;
        inFile >> temp->priority;
        inFile >> temp->arrivTime;
        inFile >> temp->procType;
        temp->cpuTime = rand() % 15;

        if(temp->procType == 'I')
            temp->IOTime = rand() % 4 + 1;
        else
            temp->IOTime = -1;

        if( schedTypeGlobal == 'f' && temp->IOTime != -1 && temp->cpuTime > 4){
            temp->remCpuTime = temp->cpuTime - floor(temp->cpuTime / 2);
            temp->cpuTime = temp->cpuTime - temp->remCpuTime;
        }
        else{
            temp->remCpuTime = temp->cpuTime;
        }
        temp->remIOTime = temp->IOTime;
        temp->pState="NEW";
        temp->pid = processCounter;
        processCounter++;
        if(inFile.eof()){
            break;
        }
        pcb.push_back(temp);
    }
    inFile.close();
}


void* terminationCheck(void* args){
    while(1){
        int count=0;
        for(int i =0;i<pcb.size();i++){
            if(pcb[i]->pState=="TERMINATED"){
                count++;
            }
        }
        if(count==pcb.size()){
            cout<<"\nEXECUTION FINISHED"<<endl;
            exit(0);
        }
    }
}


int main(){
    sem_init(&pcbSem,0,1);
    sem_init(&stdoutSem,0,1);
    sem_init(&executionSem, 0, 1);
    sem_init(&outputSem,0,1);
    sem_init(&schedSem,0,1);

    const int numOfCpu = 2;
    isCpuBusy.push_back(0);
    isCpuBusy.push_back(0);
    //isCpuBusy.push_back(0);
    //isCpuBusy.push_back(0);
    //executingProcess.push_back(-1);
    //executingProcess.push_back(-1);
    executingProcess.push_back(-1);
    executingProcess.push_back(-1);
    noOfCpusGlobal = numOfCpu;

    string inputFile = "Processes1.txt";
    string outputFile = "results.txt";
    const char schedType = 'r';
    schedTypeGlobal = schedType;

    const string mode = "sim"; //real or sim 

    if(mode == "real") //For the other file
    {
        implement_start_real();
        /* For displaying
        for(int i = 0; i < pcb.size(); i++)
        {
            cout << pcb[i].pName << pcb[i].priority << " " <<pcb[i].arrivTime << " " <<pcb[i].procType;
            cout << " " << pcb[i].cpuTime << " " << pcb[i].IOTime << endl;
        }
        */
    }
    else
    {
        implement_start();
    }


    sleep(2);
    pthread_t cpuT[numOfCpu];
    for(int i = 0;i < numOfCpu;i++){
        int* tempI = new int;
        *tempI = i;
        pthread_create(&cpuT[i], NULL,cpuFunc, tempI);
        pthread_detach(cpuT[i]);
    }
    pthread_t controllerT;
    pthread_create(&controllerT,NULL,controller,NULL);
    pthread_detach(controllerT);
    pthread_t jobs;
    pthread_create(&jobs,NULL,jobQueue,NULL);
    
    pthread_t schedulerT[numOfCpu];
    for(int i = 0;i < numOfCpu;i++){
        int* tempI = new int;
        *tempI = i;
        pthread_create(&schedulerT[i],NULL,scheduler,tempI);
        pthread_detach(schedulerT[i]);
    }

    pthread_t termination;
    pthread_create(&termination,NULL,terminationCheck,NULL);

    
    // for(int i = 0; i < proc.size();i++){
    //     proc[i].display();
    // }
    // readyQueue.display();
    pthread_exit(NULL);
}