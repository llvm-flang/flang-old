! RUN: %flang -emit-llvm -o - %s | %file_check %s
PROGRAM test
  STOP       ! CHECK: call void @libflang_stop()
END PROGRAM
