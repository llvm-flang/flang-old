! RUN: %flang -fsyntax-only -verify < %s
PROGRAM formattest

1000 FORMAT (A)
1001 FORMAT (I1, ' string ', A2)
1002 FORMAT (JK) ! expected-error {{'JK' isn't a valid format descriptor}}
1003 FORMAT (A, I1A2) ! expected-error {{invalid ending for a format descriptor}}
1004 FORMAT (I) ! expected-error {{expected an integer literal constant after 'I'}}
1005 FORMAT (I1.) ! expected-error {{expected an integer literal constant after '.'}}
1006 FORMAT ('Hello ', A)
1007 FORMAT (1X, 'String', 1X)
1008 FORMAT (X, 'String', 2X) ! expected-error {{expected an integer literal constant before 'X'}}
1009 FORMAT (1) ! expected-error {{expected a format descriptor}}
1010 FORMAT (F1.1, E1.1, G1.1, E1.1E2)
1011 FORMAT (F1) ! expected-error {{expected '.'}}
1012 FORMAT (E12.) ! expected-error {{expected an integer literal constant after '.'}}
1013 FORMAT (E1.4e) ! expected-error {{expected an integer literal constant after 'E'}}
1014 FORMAT (T1, TL20, TR99)
1015 FORMAT (T, TL2) ! expected-error {{expected an integer literal constant after 'T'}}
1016 FORMAT ('Hello', :, I1)

END PROGRAM
