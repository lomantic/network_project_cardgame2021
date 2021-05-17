#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(){

    int list[5]={1,2,3,4,5};
    int list2[5];

    //strcpy(list2,list);  //     불가능 
    /*
    for(int i=0;i<5; i++){
        printf("list2[%d] -> %d\n",i,list2[i]);
    }
    */
    char word =2;

    printf("word : %d \n",word);
    word++;
    printf("word : %d \n",word);
    return 0;
}