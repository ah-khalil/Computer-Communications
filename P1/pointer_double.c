/*Prac 1, Part B, Ex.2, (i)*/
#include <stdio.h>
#include <stdlib.h>

int main(){
    int i;
    double* d = (double*)malloc(sizeof(double));
    double* d_arr = (double*)malloc(10 * sizeof(double));

    for(i = 0; i < 10; i++){
        *d = (double) i;
        d_arr[i] = *d;
    }

    printf("This is the double in 'd' variable: %f\n", *d);

    for(i = 0; i < 10; i++){
        printf("%dth element: %f\n", i, d_arr[i]);
    }

    free(d);
    free(d_arr);

    return 0;
}
