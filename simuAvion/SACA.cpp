//
//  main.c
//  NSY103Commander
//
//  Created by Ephrem Beaino on 7/14/17.
//  Copyright © 2017 ephrembeaino. All rights reserved.
//

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#import <pthread/pthread.h>
#import <errno.h>
#import <stdlib.h>
#import <math.h>


#define MYPORT 3503

#define VITMAX 1000
#define VITMIN 200

#define ALTMAX 20000
#define ALTMIN 1




struct Coord {
    int x;
    int y;
    int altitude;
};

struct Movement {
    int cap;
    int speed;
};

struct Plane{
private:
    char *planeID;
public:
    Coord planeCoord;
    Movement planeMvmt;
    Plane();
    Plane(char *planeID, Coord coord, Movement mvmt);
    char* getPlaneID();
    void changeCAP(int newCap);
    void changeSpeed(int newSpeed);
    void updateCoord(int x, int y, int altitude);
};

struct PlaneController{
    Plane plane;
    PlaneController(Plane plane);
    void changeCAP(int newCap);
    void changeSpeed(int newSpeed);
    void changeAltitude(int newAltitude);
};

struct ControllersManager{
    PlaneController *controllers;
    int currentControllersCount=0;
    ControllersManager(int maxControllers);
    PlaneController getPlaneControllerForPlane(Plane plane);
    void removePlaneController(Plane plane);
    ~ControllersManager();
};

void* SACAStartReceivingPlanes(void* param);
void* SACAStartCommunications(void* param);

int initialized=0;
int shouldStop=0;
int startedCommu=0;

int sockfd;
struct sockaddr_in con_addr;
int struct_size;

pthread_t readThread;
pthread_t connectThread;

int planesFD [256];
Plane *planes;
int currentPlaneCount = 0;

void initSACA(){
    planes = (Plane*)malloc(sizeof(Plane)*256);
    struct sockaddr_in my_addr;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(MYPORT);
    my_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    bind(sockfd, (struct sockaddr*)&my_addr, sizeof(struct sockaddr));
    listen(sockfd,256);

    if(pthread_create(&connectThread, NULL, SACAStartReceivingPlanes, NULL)){
        perror("ERROR creating thread.");
    }
    
    initialized=1;
    printf("SACA created, waiting for planes to connect\n");
}

int getIntFromCharArray(int index, char*array);
inline int getIntFromCharArray(int index, char*array){
    return (array[index]<<24)+(array[index+1]<<16)+(array[index+2]<<8)+array[index+3];
}

void* SACAStartReceivingPlanes(void* param){
    while(!shouldStop){
        struct_size = sizeof(con_addr);
        planesFD[currentPlaneCount] = accept(sockfd, (struct sockaddr*)&con_addr, (socklen_t*)&struct_size);
        
        char *buffer = (char*)malloc(sizeof(char)*100);
        int bytes_read = (int)read(planesFD[currentPlaneCount], buffer, 100);
        if(bytes_read<=0 || bytes_read==EAGAIN || bytes_read==EWOULDBLOCK){
            printf("Failedd to read initial data");
        }else{
            int tempCounter=0;int index=0;
            char tempChar = buffer[index+tempCounter];
            while(tempChar!='*'){tempChar=buffer[index+tempCounter++];}
            
            char *planeName = (char*)malloc(sizeof(char)*tempCounter-1);
            strncpy(planeName, (char*)buffer, sizeof(char)*tempCounter-1);
            
            index+=tempCounter;
            tempCounter=0;
            
            int *tempBuffer = (int*)(buffer+8);
            Coord coord = Coord();coord.x=tempBuffer[0];coord.y=tempBuffer[1];coord.altitude=tempBuffer[2];
            Movement mov = Movement();mov.speed=tempBuffer[3];mov.cap=tempBuffer[4];
            planes[currentPlaneCount]= Plane(planeName, coord, mov);
            free(planeName);
        }
        
        struct timeval tv; tv.tv_sec = 0; tv.tv_usec = 10000; //10 milliseconds
        setsockopt(planesFD[currentPlaneCount], SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv,sizeof(struct timeval));
        
        currentPlaneCount++;
        if(!startedCommu){
            startedCommu=1;
        }
    }
    return 0;
}
void checkForCrash(){
    for(int i=0;i<currentPlaneCount;i++){
        //line equation : y = ax + b // a = tan(angle);  //b= y - ax
        float a1 = tan(planes[i].planeMvmt.cap *M_PI/360.f);
        float b1 = planes[i].planeCoord.y - a1*planes[i].planeCoord.x;
        
        for(int j=i+1;j<currentPlaneCount;j++){
            if(planes[i].planeCoord.altitude==planes[j].planeCoord.altitude){
                //same height, maybe a crash will happen
                float a2 = tan(planes[j].planeMvmt.cap *M_PI/360.f);
                float b2 = planes[j].planeCoord.y - a2*planes[j].planeCoord.x;
                
                if(planes[i].planeMvmt.cap%180 != planes[j].planeMvmt.cap%180){
                    //2 lines are not parrallel, find intersection
                    int x3 = (b2-b1)/(a1-a2);
                    int y3 = a1*x3+b1;
                    
                    if((planes[i].planeMvmt.cap<180 && y3>planes[i].planeCoord.y) || (planes[i].planeMvmt.cap>180 && y3<planes[i].planeCoord.y)){
                        //plane 1 eligible for a crash
                        if((planes[j].planeMvmt.cap<180 && y3>planes[j].planeCoord.y) || (planes[j].planeMvmt.cap>180 && y3<planes[j].planeCoord.y)){
                            //plane 2 eligible for a crash
                            
                            //CRASH WILL HAPPEN AT x3,y3
                            printf("Plane %s and plane %s will crash at position : (%d , %d)\n",planes[i].getPlaneID(),planes[j].getPlaneID(), x3,y3);
                        }
                    }
                }
                
            }
        }
    }
}
void SACAStartCommunications(){
    if(pthread_create(&readThread, NULL, SACAStartCommunications, NULL)){
        perror("ERROR creating thread.");
    }
}
void* SACAStartCommunications(void* param){
    char *buffer=(char*)malloc(sizeof(char)*100);
    if(!shouldStop){
        while(1){
            if(currentPlaneCount>0){
                for(int i=0;i<currentPlaneCount;i++){
                    memset(buffer, 0, 100);
                    int bytes_read = (int)read(planesFD[i], buffer, 100);
                    if(bytes_read<=0 || bytes_read==EAGAIN || bytes_read==EWOULDBLOCK){
                    }else{
                        int tempCounter=0;int index=0;
                        char tempChar = buffer[index+tempCounter];
                        while(tempChar!='*'){tempChar=buffer[index+tempCounter++];}
                        
                        char *planeName = (char*)malloc(sizeof(char)*tempCounter-1);
                        strncpy(planeName, (char*)buffer, sizeof(char)*tempCounter-1);
                        
                        index+=tempCounter;
                        tempCounter=0;
                        
                        int *tempBuffer = (int*)(buffer+8);
                        planes[i].updateCoord(tempBuffer[0], tempBuffer[1], tempBuffer[2]);
                        planes[i].changeSpeed(tempBuffer[3]);
                        planes[i].changeCAP(tempBuffer[4]);
                        free(planeName);
                        printf("Plane %s coord:(%d,%d,%d) speed:%d cap:%d\n",planes[i].getPlaneID(),planes[i].planeCoord.x,planes[i].planeCoord.y,planes[i].planeCoord.altitude,planes[i].planeMvmt.speed,planes[i].planeMvmt.cap);
                        
                        checkForCrash();
                    }
                }
            }else{
                sleep(1);//should never be executed
            }
        }
    }
    return 0;
}


Plane::Plane(){}
Plane::Plane(char *planeID, Coord coord, Movement mvmt){
    this->planeID = (char*)malloc(strlen(planeID));
    strcpy(this->planeID, planeID);
    this->planeCoord=coord;
    this->planeMvmt=mvmt;
}
char* Plane::getPlaneID(){
    char * result = (char*)malloc(strlen(planeID));
    strcpy(result, this->planeID);
    return result;
}
void Plane::changeCAP(int newCap){
    planeMvmt.cap=newCap;
}
void Plane::changeSpeed(int newSpeed){
    planeMvmt.speed=newSpeed;
}
void Plane::updateCoord(int x, int y, int altitude){
    planeCoord.x=x;planeCoord.y=y;planeCoord.altitude=altitude;
}

PlaneController::PlaneController(Plane plane){
    this->plane=plane;
}
void SACAWriteToPlane(char* planeID, int command, int value){
    char *buffer = (char*)malloc(sizeof(char)*5);
    memset(buffer, 0, 5);
    int *intBuffer = (int*)buffer;
    intBuffer[0]=value;
    buffer[4]=(char)command;
    
    int planeIDInt=-1;
    for(int i=0;i<currentPlaneCount;i++){
        char *plane1=planes[i].getPlaneID();
        if(!strcmp(plane1,planeID)){
            planeIDInt=i;
            free(plane1);
            break;
        }
        free(plane1);
    }
    if(planeIDInt>=0){
        write(planesFD[planeIDInt], buffer, 5);
    }
    free(buffer);
}
void PlaneController::changeCAP(int newCap){
    if(newCap>=0&&newCap<360){
        printf("change cap of plane\n");
        SACAWriteToPlane(plane.getPlaneID(), 1, newCap);
    }
}
void PlaneController::changeSpeed(int newSpeed){
    if(newSpeed>VITMIN&&newSpeed<VITMAX){
        printf("change speed of plane\n");
        SACAWriteToPlane(plane.getPlaneID(), 2, newSpeed);
    }
}
void PlaneController::changeAltitude(int newAltitude){
    if(newAltitude>ALTMIN&&newAltitude<ALTMAX){
        printf("change altitude of plane\n");
        SACAWriteToPlane(plane.getPlaneID(), 3, newAltitude);
    }
}

ControllersManager::ControllersManager(int maxControllers){
    controllers = (PlaneController*) malloc(sizeof(PlaneController)*maxControllers);
}
PlaneController ControllersManager::getPlaneControllerForPlane(Plane plane){
    for(int i=0;i<currentControllersCount;i++){
        char *plane1=controllers[i].plane.getPlaneID();
        char *plane2=plane.getPlaneID();
        if(!strcmp(plane1,plane2)){
            free(plane1);free(plane2);
            return controllers[i];
        }
        free(plane1);free(plane2);
    }
    controllers[currentControllersCount]=PlaneController(plane);
    return controllers[currentControllersCount++];
}
void ControllersManager::removePlaneController(Plane plane){
    for(int i=0;i<currentControllersCount;i++){
        char *plane1=controllers[i].plane.getPlaneID();
        char *plane2=plane.getPlaneID();
        if(!strcmp(plane1,plane2)){
            free(plane1);free(plane2);
            for(int j=i;j<currentControllersCount-1;j++){
                controllers[j] = controllers[j+1];
            }
            currentControllersCount--;
            return;
        }
        free(plane1);free(plane2);
    }
}
ControllersManager::~ControllersManager(){
    free(controllers);controllers=0;
}

int main(){
    printf("Hello to the SACA, once a plane is connected, the feed will automatically starts showing in front of you.\n");
    printf("The only commands supported is: c {planeID} {valueToChange} {value}\n");
    printf("PlaneID is the name of the plane showing with the feed, example : TQ790\n");
    printf("ValueToChange is either of these 3 : 'alt' for altitude, 'spd' for speed, 'cap' for cap aka angle\n");
    printf("Value is int and the actual value depends on the type, for alt between 1 and 20000, for spd between 200 and 1000, cap between 0 and 359\n");
    printf("Developped by Ephrem BEAINO at CNAM Naher Ibrahim for the NSY103 project\n\n\n\n");
    
    initSACA();
    ControllersManager manager = ControllersManager(256);
    while(1){
        sleep(1);
        if(startedCommu){
            break;
        }
    }
    SACAStartCommunications();
    char buffer[1024];
    printf("Enter a message: \n");
    while(*(fgets(buffer, 1024, stdin)) != '\n'){
        if(buffer[0]=='c'){
            //only command support is control
            int index=2;
            char tempChar = buffer[index];
            while(tempChar!=' '){tempChar=buffer[index++];}
            
            char *planeName = (char*)malloc(sizeof(char)*index-3);
            strncpy(planeName, (char*)buffer+2, sizeof(char)*index-3);
            
            int planeID=-1;
            
            for(int i=0;i<currentPlaneCount;i++){
                char *plane1=planes[i].getPlaneID();
                if(!strcmp(plane1,planeName)){
                    planeID=i;
                    free(plane1);
                    break;
                }
                free(plane1);
            }
            if(planeID>=0){
                PlaneController controller = manager.getPlaneControllerForPlane(planes[planeID]);
                int command = 0;
                if(buffer[index]=='c' && buffer[index+1]=='a' && buffer[index+2]=='p'&&buffer[index+3]==' '){
                    //change cap
                    command=1;
                }else if(buffer[index]=='a' && buffer[index+1]=='l' && buffer[index+2]=='t'&&buffer[index+3]==' '){
                    //change altitude
                    command=3;
                }else if(buffer[index]=='s' && buffer[index+1]=='p' && buffer[index+2]=='d'&&buffer[index+3]==' '){
                    //change speed
                    command=2;
                }
                if(command>0 && command<4){
                    //command valid
                    index+=4;
                    int startingPoint=index;
                    char tempChar2 = buffer[index];
                    while(tempChar2!='\n'){tempChar2=buffer[index++];}
                    
                    char *value = (char*)malloc(sizeof(char)*index-startingPoint-1);
                    strncpy(value, (char*)buffer+startingPoint, sizeof(char)*index-startingPoint-1);
                    
                    int valueInt=0;
                    for(int e=0; e<index-startingPoint-1; e++){
                        valueInt = valueInt * 10 + ( value[e] - '0' );
                    }
                    
                    switch(command){
                        case 1:controller.changeCAP(valueInt); break;
                        case 2:controller.changeSpeed(valueInt); break;
                        case 3:controller.changeAltitude(valueInt); break;
                        default:break;
                    }
                    
                }
            }
            manager.removePlaneController(planes[planeID]);
            free(planeName);
        }
    }
    printf("exited");
}
