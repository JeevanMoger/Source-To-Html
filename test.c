/* this is multi-line comment */
// SOURCE TO HTML
#include <stdio.h>
#include "stdlib.h"
#include <string.h>
#define HELLO -2
int main()
{
    printf("hello world\n");
    char ch = 'a';
    sizeof(ch);
    int num = 123;
    for(int i = 0 ; i < num ; i++)
    {
        printf("SOURCE TO HTML\n");
    }
    while( 1 )
    {
        printf("HELLO");
    }   
    switch(ch)
    {
        case 'a': printf("CASE 1 \n");
         break;
        case 'b' : printf("CASE 2\n");
         break;
         default : printf("DEFAULT CASE \n");
    }                                            
}
