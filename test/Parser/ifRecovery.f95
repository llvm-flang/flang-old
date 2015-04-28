! RUN: %flang -fsyntax-only -verify < %s
PROGRAM dotest
    integer i

! The if statement must be processed no matter what, as we want to match the end if

    if .true. then ! expected-error {{expected '(' after 'IF'}}
    end if

    if () then ! expected-error {{expected an expression}}
    end if

    if (.false. ! expected-error {{expected ')'}}
    end if

    if(.true.) then
    else if then  ! expected-error {{expected '(' after 'ELSE IF'}}
    end if

    if(::) i = 0 ! expected-error {{expected an expression}}

END
