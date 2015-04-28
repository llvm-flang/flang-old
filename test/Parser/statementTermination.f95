! RUN: %flang -fsyntax-only -verify < %s
PROGRAM termtest
  INTEGER I

2 FORMAT(A) A ! expected-error {{expected line break or ';' at end of statement}}

1 CONTINUE 11 ! expected-error {{expected line break or ';' at end of statement}}
  PRINT *,I 2 ! expected-error {{expected line break or ';' at end of statement}}
  GOTO 1 + 10 ! expected-error {{expected line break or ';' at end of statement}}
! FIXME: GOTO 1;GOTO 1
  IF(.false.) THEN what ! expected-error {{expected line break or ';' at end of statement}}
  END IF
  DO I = 1,10 man  ! expected-error {{expected line break or ';' at end of statement}}
  END DO
  ASSIGN 1 TO I I ! expected-error {{expected line break or ';' at end of statement}}

END PROGRAM termtest
