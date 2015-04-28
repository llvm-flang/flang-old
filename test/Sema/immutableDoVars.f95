! RUN: %flang -fsyntax-only -verify < %s
PROGRAM dotest
    INTEGER II, K

    DO II = 1, 10 ! expected-note {{which is used in a do statement here}}
      K = II ! expected-note@-1 {{which is used in a do statement here}}
      II = K ! expected-error {{assignment to a do variable 'ii'}}
      ASSIGN 100 TO II ! expected-error {{assignment to a do variable 'ii'}}

      DO II = 1,10 ! expected-error {{assignment to a do variable 'ii'}}
      END DO ! expected-note@-6 {{which is used in a do statement here}}
100 END DO

    DO II = 1,10
      DO K = 1,10 ! expected-note {{which is used in a do statement here}}
        K = II ! expected-error {{assignment to a do variable 'k'}}
      END DO
    END DO

    I = 1
    K = 0

END PROGRAM
