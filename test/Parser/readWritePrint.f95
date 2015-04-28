! RUN: %flang -fsyntax-only < %s
PROGRAM test

10  FORMAT('Message= ',A)

    WRITE(*,*) 'Hello world'
    PRINT *, 'Hello world'
    PRINT *, 'Hello', 'world', '!'
    WRITE (*,10) 'Hello world'
    PRINT 10, 'Test'
    WRITE (*,20) 'now'
    WRITE (*, FMT=10)'then'

20  FORMAT('Future is ',A)

END PROGRAM
