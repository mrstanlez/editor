#include <stdio.h>
#include <math.h>

int main() {
    double x = 4.0;
    double y = 3.0;
    
    double power = pow(x, y);
    double square_root = sqrt(power);
    
    printf("--- MATH AND PRINTF TEST ---\n");
    printf("Number %.1f to the power of %.1f is: %.1f\n", x, y, power);
    printf("The square root of %.1f is: %.1f\n", power, square_root);
    printf("Calculation completed successfully.\n");

    return 0;
}

