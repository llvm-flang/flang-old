! RUN: %flang -fsyntax-only -verify < %s
PROGRAM what ! expected-note {{to match this 'program'}}
CONTINUE ! expected-error@+1 {{expected 'end program'}}
