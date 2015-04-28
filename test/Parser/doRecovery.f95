! RUN: %flang -fsyntax-only -verify < %s
PROGRAM dotest
    integer i

! The do statement must be processed no matter what, as we want to match the end do

    do 'a' = 1, 10 ! expected-error {{expected identifier}}
    end do

    do i 1,10 ! expected-error {{expected '='}}
    end do

    do i = ,10 ! expected-error {{expected an expression}}
    end do

    do i = 1 ! expected-error {{expected ','}}
    end do

    do i = 1, ! expected-error {{expected an expression after ','}}
    end do

    do while( ! expected-error {{expected an expression after '('}}
    end do

END
