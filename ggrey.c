/*
 *  Copyright (c) 2016 Thierry Leconte
 *
 *   
 *   This code is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License version 2
 *   published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#include <stdio.h>
#include <math.h>

double BESSI0(double X)
{
    double Y, P1, P2, P3, P4, P5, P6, P7, Q1, Q2, Q3, Q4, Q5, Q6, Q7, Q8, Q9, AX, BX;
    P1 = 1.0;
    P2 = 3.5156229;
    P3 = 3.0899424;
    P4 = 1.2067492;
    P5 = 0.2659732;
    P6 = 0.360768e-1;
    P7 = 0.45813e-2;
    Q1 = 0.39894228;
    Q2 = 0.1328592e-1;
    Q3 = 0.225319e-2;
    Q4 = -0.157565e-2;
    Q5 = 0.916281e-2;
    Q6 = -0.2057706e-1;
    Q7 = 0.2635537e-1;
    Q8 = -0.1647633e-1;
    Q9 = 0.392377e-2;
    if (fabs(X) < 3.75) {
        Y = (X / 3.75) * (X / 3.75);
        return (P1 + Y * (P2 + Y * (P3 + Y * (P4 + Y * (P5 + Y * (P6 + Y * P7))))));
    } else {
        AX = fabs(X);
        Y = 3.75 / AX;
        BX = exp(AX) / sqrt(AX);
        AX = Q1 + Y * (Q2 + Y * (Q3 + Y * (Q4 + Y * (Q5 + Y * (Q6 + Y * (Q7 + Y * (Q8 + Y * Q9)))))));
        return (AX * BX);
    }
}

double vonmises(double x, double k)
{

    return exp(k * cos(x)) / (2 * M_PI * BESSI0(k));
}

int main()
{
    int i;
    const int K = 10;

    for (i = -128; i <= 128; i++) {
        double pb, p1, pt;

        p1 = pt = 0;

        pb = vonmises(M_PI / 8 - i * M_PI / 128, K);
        pt += pb;

        pb = vonmises(3 * M_PI / 8 - i * M_PI / 128, K);
        pt += pb;

        pb = vonmises(5 * M_PI / 8 - i * M_PI / 128, K);
        pt += pb;

        pb = vonmises(7 * M_PI / 8 - i * M_PI / 128, K);
        pt += pb;

        pb = vonmises(-M_PI / 8 - i * M_PI / 128, K);
        pt += pb;
        p1 += pb;

        pb = vonmises(-3 * M_PI / 8 - i * M_PI / 128, K);
        pt += pb;
        p1 += pb;

        pb = vonmises(-5 * M_PI / 8 - i * M_PI / 128, K);
        pt += pb;
        p1 += pb;

        pb = vonmises(-7 * M_PI / 8 - i * M_PI / 128, K);
        pt += pb;
        p1 += pb;

        printf("%f,", p1 / pt);
        if ((i & 7) == 0)
            printf("\n");
    }

}
