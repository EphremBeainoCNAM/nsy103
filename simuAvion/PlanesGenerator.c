//
//  main.c
//  NSY103Generator
//
//  Created by Ephrem Beaino on 7/14/17.
//  Copyright © 2017 ephrembeaino. All rights reserved.
//

#include <stdio.h>
#include <unistd.h>

int main(int argc, const char * argv[]) {
    for(int i=0;i<20;i++){
        if(fork()){
            execv("./PATHTOavionExecutable", 0);
        }
    }
    return 0;
}
