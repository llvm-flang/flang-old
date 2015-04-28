! RUN: %flang -fsyntax-only -verify < %s

PROGRAM dowhiletest
  DO WHILE(.true.)
  END DO

  DO WHILE(1) ! expected-error {{statement requires an expression of logical type ('integer' invalid)}}
  END DO

  IF(.true.) THEN
    DO WHILE(.false.) ! expected-note {{to match this 'do'}}

    END IF ! expected-error {{expected 'end do'}}

END
