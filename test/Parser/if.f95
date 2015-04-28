! RUN: %flang -fsyntax-only -verify < %s
PROGRAM iftest
  CHARACTER (LEN=11) :: C
  IF(1 == 1) C = "YES"

  IF(0 == 0) ! expected-error {{expected an executable statement}}
    C = "NO"

  IF 1 == 2) C = "NO" ! expected-error {{expected '(' after 'IF'}}
  IF(1 == 2 C = "NO" ! expected-error {{expected ')'}}

  IF( ! expected-error {{expected an expression after '('}}
    C == "YES") C = "NO" ! expected-error {{expected '='}} FIXME proper error

  IF(1 == 2) THEN
    C = "NO"
  END IF

  IF(3 == 3) THEN
    C = "MAYBE"
  ELSE
    C = "ENDOFDAYS"
  ENDIF

  IF(42 == 69) THEN
    C = "NOPE"
  ELSE IF(12 == 13) THEN
    C = "POSSIBLY"
  ELSEIF(123 == 123) THEN
    C = "CORRECT"
    PRINT *, C
  ELSE
    C = "NEVER"
  END IF

  IF(1 == 0) THEN
    C = "YES"
  ELSE IF 12 == 23) THEN ! expected-error {{expected '(' after 'ELSE IF'}}
    C = "NO"
  END IF

  IF(1 == 0) THEN
    C = "NO"
  ELSE IF( ! expected-error {{expected an expression after '('}}
    C = "NO"
  END IF

  !Here comes nesting
  IF(33 == 22) THEN
    IF(22 == 33) THEN
      IF(11 == 11) THEN
        STOP
      END IF
    ELSE
      PRINT *, C
    ENDIF
  END IF

  IF(1 == 2) THEN ! expected-note {{to match this 'if'}}
    C = "NO" ! expected-error@+1 {{expected 'end if'}}
END PROGRAM iftest
