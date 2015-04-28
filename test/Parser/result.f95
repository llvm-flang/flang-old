! RUN: %flang -fsyntax-only -verify < %s

INTEGER FUNCTION FOO() RESULT(I)
END

FUNCTION BAR() RESULT ! expected-error {{expected '('}}
END

FUNCTION FUNC() RESULT( ! expected-error {{expected identifier}}
END
