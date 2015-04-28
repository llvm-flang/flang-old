! RUN: %flang -fsyntax-only -verify < %s
PROGRAM test
    INTEGER I
    REAL RR
    CHARACTER(Len = 10) FMTS

10  FORMAT('Message= ',A)

    ASSIGN 20 TO I

    FMTS = '(A)'

    WRITE(*,*) 'Kevin'
    WRITE(*,10) 'Hello world'
    WRITE(*,'(A)') 'World hello'
    WRITE(*,FMTS) 'anewstart'
    WRITE(*,I) 'Anyong'

    WRITE(*,2.0) 'zz' ! expected-error {{expected a valid format specifier instead of an expression with type 'real'}}
    WRITE(*,RR)  'xz' ! expected-error {{expected a valid format specifier instead of an expression with type 'real'}}
    WRITE(*,30) 'no'  ! expected-error {{use of undeclared statement label '30'}}

20  FORMAT('Future is ',A)

END PROGRAM
