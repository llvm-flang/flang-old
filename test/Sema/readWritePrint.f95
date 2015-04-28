! RUN: %flang -fsyntax-only -verify < %s
PROGRAM test

10  FORMAT('Message= ',A)
20  CONTINUE ! expected-note {{statement label was declared here}}

    WRITE (*,10) 'Hello'
    WRITE (*,FMT=11) 'Hello' ! expected-error {{use of undeclared statement label '11'}}
    WRITE (*,20) 'Hello' ! expected-error {{use of statement label format specifier which doesn't label a 'FORMAT' statement}}
    WRITE (*,FMT=30) 'ABC' ! expected-error {{use of statement label format specifier which doesn't label a 'FORMAT' statement}}

30  CONTINUE ! expected-note {{statement label was declared here}}

END PROGRAM
