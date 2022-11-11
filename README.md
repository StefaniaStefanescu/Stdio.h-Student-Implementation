# Stdio.h-Student-Implementation
This is a simple implementation of the stdio.h library via system calls
//StefanescuStefania//

The SO_FILE structure contains a lot of useful data that made it easier for me to work with the other functions.

The so_fopen function opens a file in r/r+/w/w+/a/a+ taking care of the correct data allocation for the returned SO_FILE structure. In linux I used the "open" system call and in windows the "CreateFileA" system call.

The so_ftell, so_feof, so_ferror, so_fileno functions have as return value one of the data recorded in the SO_FILE structure. These data are modified during operations both in Windows and in Linux.

In the so_fflush function I check if the opening mode of the file is a/a+, if so I have to move the cursor to the end of the file.
After that, I start writing to the file with a check on the return value of the write function because it may not always write as much as it should.

In the fclose function, I check if the pointer to the file, sent as a parameter, is not NULL, in case the last operation done in the file is writing, I must execute so_fflush on the file and then close the file descriptor and deallocate the allocated memory.

The so_fgetc function checks where the cursor is in the buffer and, depending on the position, to make a read system call or not, after which it returns the value from the current position in the buffer. In case of error, it returns SO_EOF. The function so_fputc has similar functionality.

Both the function so_fread and so_fwrite are created through loops with so_fgetc/so_fputc. I tried to implement the functions without the help of fgetc or fputc, but I had many cases in which the correct outputs were not displayed, so I chose the "easy version"

The so_popen and so_pclose functions are implemented according to the code from the laboratory, the so_popen function works correctly according to my tests and the so_pclose function waits for the end of the execution of the child process, after which it frees the memory allocated to the file and closes the file descriptor. Here I chose to name the "reading ends" as open_child and open_process because it seemed more intuitive and much easier to follow in the code than 0/1.

OBS: SO_POPEN AND SO_PCLOSE DO NOT WORK CORRECTLY IN WINDOWS.
