@echo STATIC FOUND:
@findstr -s -n -i -l "static" code\*

@echo ========
@echo ========

@echo GLOBALS FOUND:
@findstr -s -n -i -l "global_variable" code\*

@echo ========
@echo ========

@echo LOCAL PERSISTS FOUND:
@findstr -s -n -i -l "local_persist" code\*
