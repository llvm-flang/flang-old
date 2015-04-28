C RUN: %flang -fsyntax-only %s

       RECURSI VE SUB ROUTINE SU B()
       END
       RECU RSIV E FUNCTIO N F OO()
       END

       RECU RSIVE INTE GER FU NCTION FUNC()
         FUNC = 1
       END

       INTEGER RECURS IVE FU NCTION FUNC2()
         FUNC2 = 2
       END

       INTEGER(8) RECURS IVE FU NCTION FUNC3()
         FUNC3 = 2
       END

       RECU RSIVE INTE GER (2) FU NCTION FUNC4()
         FUNC4 = 1
       END
