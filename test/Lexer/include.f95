! RUN: %flang -fsyntax-only -I%test_dir/Lexer/ < %s
PROGRAM inc
INCLUDE 'includedFile.inc'
END PROGRAM inc
