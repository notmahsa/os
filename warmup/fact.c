#include "common.h"

int factorial(int input){
    if (input < 3){
        return input;
    }
    return input * factorial(input - 1);
}

int main(int argc, char **argv)
{
	if (argc != 2){
	    printf("Huh?");
	    return 0;
	}

    int int_input = strtol(argv[1], NULL, 10);
    if (int_input > 12){
        printf("Overflow");
        return 0;
    }

	float dec_input = atof(argv[1]);
    if (int_input < 1 || dec_input != int_input){
        printf("Huh?");
        return 0;
    }

    printf("%d", factorial(int_input));

	return 0;
}
