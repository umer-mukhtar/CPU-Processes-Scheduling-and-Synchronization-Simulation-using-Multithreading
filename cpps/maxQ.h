#include <iostream>
#include "PCB.h"

using namespace std;

struct QueueNode {
public:
    PCB* pcb;
    QueueNode* next;
    friend struct Queue;
};

struct Queue {
public:
    QueueNode* front;
public:
    Queue() {
        front = NULL;
    }
    void enqueue(PCB* pcb) {
        QueueNode* temp, * currNode;
        temp = new QueueNode;
        temp->pcb = pcb;
        if (front == NULL || (*pcb).priority > front->pcb->priority) {
            temp->next = front;
            front = temp;
        }
        else {
            currNode = front;
            while (currNode->next != NULL && currNode->next->pcb->priority >= (*pcb).priority)
                currNode = currNode->next;
            temp->next = currNode->next;
            currNode->next = temp;
        }
    }
    PCB* dequeue() {
        QueueNode* temp;
        if (front == NULL) //if queue is null
            cout << "Queue Underflow!" << endl;
        else {
            temp = front;
            front = front->next;
            return (temp->pcb);
        }
        PCB nullPCB;
        nullPCB.pid = -99;
        return &nullPCB;
    }
    PCB* peek() {
        if (front == NULL) {
            //cout << "Queue is empty!" << endl;;
        }
        else {
            QueueNode* temp = front;
            return temp->pcb;
        }
        PCB* nullPCB = new PCB;
        nullPCB->pid = -99;
        return nullPCB;
    }
    void display() {
        QueueNode* currNode;
        currNode = front;
        if (front == NULL)
            cout << "Queue is empty";
        else {
            while (currNode != NULL) {
                cout << currNode->pcb->pid << " ";
                currNode = currNode->next;
            }
        }
    }
    bool isEmpty(){
        if(front == NULL){
            return true;
        }
        else{
            return false;
        }
    }
};

//driver program
// int main() {
//     Queue currNode;
//     currNode.enqueue(5, 2);
//     currNode.enqueue(1, 3);
//     currNode.enqueue(7, 5);
//     currNode.enqueue(9, 1);
//     currNode.enqueue(4, 1);
//     currNode.enqueue(3, 1);
//     cout << currNode.dequeue() << endl;
//     cout << currNode.dequeue() << endl;
//     cout << currNode.dequeue() << endl;
//     cout << currNode.dequeue() << endl;
//     cout << currNode.dequeue() << endl;
//     cout << currNode.dequeue() << endl;
//     cout << currNode.peek() << endl;

//     return 0;
// }