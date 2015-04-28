! RUN: %flang -fsyntax-only -verify < %s
PROGRAM dotest
    INTEGER I
    REAL R
    REAL x

    R = 1.0
    DO 10 I = 1,10
      R = I * R
10  CONTINUE

    R = 0.0
    DO 20 I = 1,10,2
20    R = R + I

    DO 30 'V' = 1,100,5 ! expected-error {{expected identifier}}
      R = R - I
30  CONTINUE

    DO 40 I 1,100 ! expected-error {{expected '='}}
40    R = R * 1

    DO 50 I = 1 100 ! expected-error {{expected ','}}
      R = R + I
50  CONTINUE

    DO 60 I = ! expected-error {{expected an expression after '='}}
60    CONTINUE

    DO 70 I = 1, ! expected-error {{expected an expression after ','}}
70    CONTINUE

    DO 80 I = 1,3, ! expected-error {{expected an expression after ','}}
80    CONTINUE

    DO 90 I = ! expected-error {{expected an expression after '='}}
     1 , 4    ! expected-error {{}}
90  CONTINUE

    DO 100 I = 1, ! expected-error {{expected an expression after ','}}
      I ! expected-error {{expected '='}}
100 CONTINUE

    DO I = 1,10
      R = 0
    END DO

    DO I = 1,10,2
      R = R + I
    ENDDO

    DO x I = 1,2 ! expected-error {{expected '='}}
    END DO

    DO 200,I = 1,5
200 CONTINUE

    DO,I=1,5
    ENDDO

    DO,WHILE(.false.)
    END DO

END PROGRAM
