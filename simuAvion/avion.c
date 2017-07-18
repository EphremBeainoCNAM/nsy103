#include <math.h>
#import <stdio.h>
#import <stdlib.h>
#import <unistd.h>
#import <time.h>
#import <string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<fcntl.h>
#include <arpa/inet.h>
#import <pthread/pthread.h>

#include "avion.h"

// caractéristiques du déplacement de l'avion
struct deplacement dep;

// coordonnées spatiales de l'avion
struct coordonnees coord;

// numéro de vol de l'avion : code sur 5 caractéres
char numero_vol[6];

/********************************
 ***  3 fonctions à implémenter
 ********************************/

int ouvrir_communication() {
    // fonction à implémenter qui permet d'entrer en communication via TCP
    // avec le gestionnaire de vols
    return 1;
}

void fermer_communication() {
    // fonction à implémenter qui permet de fermer la communication
    // avec le gestionnaire de vols
}

void envoyer_caracteristiques() {
    // fonction à implémenter qui envoie l'ensemble des caractéristiques
    // courantes de l'avion au gestionnaire de vols
}

/********************************
 ***  Fonctions gérant le déplacement de l'avion : ne pas modifier
 ********************************/

// initialise aléatoirement les paramétres initiaux de l'avion

void initialiser_avion() {
    coord.x = 1000 + random() % 1000;
    coord.y = 1000 + random() % 1000;
    coord.altitude = 900 + random() % 100;

    dep.cap = random() % 360;
    dep.vitesse = 600 + random() % 200;

    numero_vol[0] = (random() % 26) + 'A';
    numero_vol[1] = (random() % 26) + 'A';
    sprintf(&numero_vol[2], "%03ld", (random() % 999) + 1);
    numero_vol[5] = 0;
}

// modifie la valeur de l'avion avec la valeur pass�e en param�tre

void changer_vitesse(int vitesse) {
    if (vitesse < 0)
        dep.vitesse = 0;
    else if (vitesse > VITMAX)
        dep.vitesse = VITMAX;
    else dep.vitesse = vitesse;
}

// modifie le cap de l'avion avec la valeur passée en paramètre

void changer_cap(int cap) {
    if ((cap >= 0) && (cap < 360))
        dep.cap = cap;
}

// modifie l'altitude de l'avion avec la valeur passée en paramètre

void changer_altitude(int alt) {
    if (alt < 0)
        coord.altitude = 0;
    else if (alt > ALTMAX)
        coord.altitude = ALTMAX;
    else coord.altitude = alt;
}

// affiche les caractéristiques courantes de l'avion

void afficher_donnees() {
    printf("Avion %s -> localisation : (%d,%d), altitude : %d, vitesse : %d, cap : %d\n",
            numero_vol, coord.x, coord.y, coord.altitude, dep.vitesse, dep.cap);
}

// recalcule la localisation de l'avion en fonction de sa vitesse et de son cap

void calcul_deplacement() {
    float cosinus, sinus;
    float dep_x, dep_y;
    int nb;

    if (dep.vitesse < VITMIN) {
        printf("Vitesse trop faible : crash de l'avion\n");
        fermer_communication();
        exit(2);
    }
    if (coord.altitude == 0) {
        printf("L'avion s'est ecrase au sol\n");
        fermer_communication();
        exit(3);
    }
    //cos et sin ont un paramétre en radian, dep.cap en degré nos habitudes francophone
    /* Angle en radian = pi * (angle en degré) / 180 
       Angle en radian = pi * (angle en grade) / 200 

       Angle en grade = 200 * (angle en degré) / 180 
       Angle en grade = 200 * (angle en radian) / pi 

       Angle en degré = 180 * (angle en radian) / pi 
       Angle en degré = 180 * (angle en grade) / 200 
     */

    cosinus = cos(dep.cap * 2 * M_PI / 360);
    sinus = sin(dep.cap * 2 * M_PI / 360);

    //newPOS = oldPOS + Vt
    dep_x = cosinus * dep.vitesse * 10 / VITMIN;
    dep_y = sinus * dep.vitesse * 10 / VITMIN;

    // on se d�place d'au moins une case quels que soient le cap et la vitesse
    // sauf si cap est un des angles droit
    if ((dep_x > 0) && (dep_x < 1)) dep_x = 1;
    if ((dep_x < 0) && (dep_x > -1)) dep_x = -1;

    if ((dep_y > 0) && (dep_y < 1)) dep_y = 1;
    if ((dep_y < 0) && (dep_y > -1)) dep_y = -1;

    //printf(" x : %f y : %f\n", dep_x, dep_y);

    coord.x = coord.x + (int) dep_x;
    coord.y = coord.y + (int) dep_y;

    afficher_donnees();
}

// fonction principale : gère l'exécution de l'avion au fil du temps

void se_deplacer() {
    while (1) {
        sleep(PAUSE);
        calcul_deplacement();
        envoyer_caracteristiques();
    }
}

//int main() {
//    // on initialise l'avion
//    initialiser_avion();
//
//    afficher_donnees();
//    // on quitte si on arrive à pas contacter le gestionnaire de vols
//    if (!ouvrir_communication()) {
//        printf("Impossible de contacter le gestionnaire de vols\n");
//        exit(1);
//    }
//
//    // on se déplace une fois toutes les initialisations faites
//    se_deplacer();
    
    
//}
#define DESTPORT 3503

int sockfd;
char bufferRead[5];

void* PLANEReadFromServer(void *sender){
    while(1){
        int bytes_read = (int)read(sockfd, bufferRead, 5);
        if(bytes_read>0){
            char command = bufferRead[4];
            int *bufferInt = (int*)bufferRead;
            int value = bufferInt[0];
            if(command==1){//change cap
                changer_cap(value);
            }else if(command==2){//change speed
                changer_vitesse(value);
            }else if(command==3){
                changer_altitude(value);
            }else{
                //Not supported in this version or timeout or another shiat.
            }
        }
    }
    return 0;
}
int main(){
    int seed;
    time((time_t*)&seed);
    srandom(seed);
    initialiser_avion();
    
    struct sockaddr_in dest_addr;
    sockfd = socket(AF_INET,SOCK_STREAM,0);
    
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(DESTPORT);
    dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    int x =1;
    
    
    while(x){
        x= connect(sockfd,(struct sockaddr*)&dest_addr, sizeof(struct sockaddr));
        if(x){
            printf("Failed to connect, retrying in 1s\n");
            sleep(1);
        }else{
            printf("Connected to SACA\n");
            break;
        }
    }
    
    pthread_t cThread;
    if(pthread_create(&cThread, NULL, PLANEReadFromServer, NULL)){
        perror("ERROR creating thread.");
    }
    
    char *buf=(char*)malloc(sizeof(char)*100);
    int index = (int)strlen(numero_vol);
    strcpy(buf, numero_vol);
    buf[index]='*';//separator for server
    index++;
    while(1){
        memset(buf+index, 0, 100-index);
        calcul_deplacement();
        int *bufferInt = (int*)(buf+8);
        bufferInt[0]=coord.x;
        bufferInt[1]=coord.y;
        bufferInt[2]=coord.altitude;
        bufferInt[3]=dep.vitesse;
        bufferInt[4]=dep.cap;
        
        write(sockfd, buf, 100);
        sleep(1);
    }
    free(buf);
    disconnectx(sockfd, 0, 0);
    close(sockfd);
    printf("Client Dying.....\n");
    return 0;
}

