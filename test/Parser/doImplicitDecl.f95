! RUN: %flang -fsyntax-only -verify < %s
PROGRAM dotest
    REAL R
    DO 10 I = 1,10
      R = I * R
10  CONTINUE
END

subroutine foo
  implicit none
  do i = 1,10 ! expected-error {{use of undeclared identifier 'i'}}
  end do
end
