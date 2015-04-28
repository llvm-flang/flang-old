! RUN: %flang -fsyntax-only -verify < %s
PROGRAM constants
  CHARACTER * 11 C ! expected-error@+1 {{missing terminating ' character}}
  C = 'hello world
END PROGRAM constants
