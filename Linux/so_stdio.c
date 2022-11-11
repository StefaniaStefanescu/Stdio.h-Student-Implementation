#include<unistd.h>
#include<string.h>
#include<errno.h>
#include<stdbool.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<stdio.h>
#include<stdlib.h>
#include<paths.h>
#include<sys/wait.h>
#include<sys/param.h>
#include "so_stdio.h"
//#define SO_EOF -1
//#define SEEK_SET 0
//#define SEEK_CUR 1
//#define SEEK_END 2
#define BUFFER_SIZE 4096


struct _so_file{
    int file_descriptor; //descritpor de fisier
    bool eof; //true daca sunt la sf. fisierului
    bool error; //erori aparute in cadrul lucrului cu fisierul 
    //char*filepath; //path catre fisier
    long int size; //dimensiunea totala a fisierului
    int file_cursor; //cursor in fisier
    bool isOpened; //daca fisierul este inca deschis
    int last_operation; //ultima operatie facuta->pt flush -1 o , 0 r, 1 w
    char openMode; //modul in care a fost deschis fisierul
    char buffer[BUFFER_SIZE];
    int buffer_cursor;
    //pt procese
    int no_bytes_rw; //bytes cititi sau scrisi
    int child_pid;
    int popened;
    int mode_prefix;

};
typedef struct _so_file SO_FILE;


SO_FILE* so_fopen(const char *pathname, const char *mode){
    int fd=-1;
     if(mode[0]=='r'){
        if(strlen(mode)>1 && mode[1]=='+'){
          fd=open(pathname, O_RDWR);
        }else{
          fd=open(pathname,O_RDONLY);
        }
     }else if(mode[0]=='w'){
        if(strlen(mode)>1 && mode[1]=='+'){
          fd=open(pathname, O_RDWR |O_CREAT |O_TRUNC,0644);
        }else{
          fd=open(pathname,O_WRONLY | O_CREAT |O_TRUNC, 0644);
        }
     }else if(mode[0]=='a'){
        if(strlen(mode)>1 && mode[1]=='+'){
          fd=open(pathname, O_RDWR | O_APPEND | O_CREAT, 0644);
        }else{
          fd=open(pathname,O_WRONLY| O_APPEND | O_CREAT, 0644);
        }
     }else{
        return NULL;

     }
     if(fd<0){
        return NULL;
     }
    
    SO_FILE* file=malloc(sizeof(SO_FILE));
    struct stat file_stat;
    stat(pathname,&file_stat);
    
    file->size=file_stat.st_size;
   // file->filepath=malloc(sizeof(char)*strlen(pathname)+1);
   // memcpy(file->,pathname,strlen(pathname));
   // file->filepath[strlen(pathname)+1]='\0';
    file->file_descriptor=fd;
    file->no_bytes_rw=0;
    file->buffer_cursor=0;
    file->error=false;
    file->eof=false;
    file->openMode=mode[0];
    file->file_cursor = 0;
    file->isOpened=true;
   // file->popened=false;
    file->last_operation=-1; //de la open
    file->child_pid=0;
    file->popened=0;//nu e deschis din proces
    if(strlen(mode)>1&&mode[1]=='+'){
      file->mode_prefix=1; //am plus
    }else{
        file->mode_prefix=0; //nu am plus
    }
    memset(file->buffer,0,BUFFER_SIZE);
    
    return file;
}

int so_fflush(SO_FILE *stream){
    if(stream==NULL){
        return -1;
    }
    stream->no_bytes_rw = 0;
    if(stream->openMode == 'a'){
        stream->file_cursor=lseek(stream->file_descriptor,0,SEEK_END);
    }
    while(stream->no_bytes_rw < stream->buffer_cursor){
        int bytes_w = write(stream->file_descriptor, stream->buffer + stream->no_bytes_rw,
                            stream->buffer_cursor-stream->no_bytes_rw);
        if(bytes_w <= 0){
            stream->error=true;
            return -1;
        }
        stream->no_bytes_rw += bytes_w;

    }

    memset(stream->buffer, 0 ,BUFFER_SIZE);
    stream->buffer_cursor=0;
    return 0;
}

int so_fclose(SO_FILE *stream){
   
    if(stream==NULL){
        free(stream);
        return SO_EOF;
    }
    if(stream->isOpened==false){
        //stream->error=true;
        free(stream);
        return SO_EOF;
    }
    if(stream->last_operation == 1)
    {
        //adauga flush
        int ff=so_fflush(stream);
        if(ff==-1){
            free(stream);
            //stream->error=true;
            return SO_EOF;
        }
    }

    int raspuns_close=close(stream->file_descriptor);
    if(raspuns_close==-1){
        free(stream);
       // stream->error=true;
        return SO_EOF;
    }
    free(stream);
    return raspuns_close;
}

int so_fgetc(SO_FILE *stream){
    if(stream==NULL){
        return SO_EOF;
    }
    int file_read = 1;  //0 daca am eof, -1 pt err
    if(stream->buffer_cursor == 0 || stream->buffer_cursor == BUFFER_SIZE || stream->buffer_cursor == stream->no_bytes_rw){
        memset(stream->buffer, 0, BUFFER_SIZE);
        stream->buffer_cursor = 0;
        file_read = read(stream->file_descriptor, stream->buffer, BUFFER_SIZE);
        stream->no_bytes_rw = file_read;
        if(file_read==-1){
            stream->error=true;
        }
        if(file_read == 0){
            stream->eof = true;
        }
    }
    if(file_read > 0){
        stream->last_operation = 0; //read
        stream->buffer_cursor +=1;
        stream->file_cursor +=1;
        return stream->buffer[stream->buffer_cursor -1];
    }else{
        stream->error=true;
        return SO_EOF;
    }
               // return SO_EOF;
}

size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream){
    if(stream==NULL ){
        return SO_EOF;
    }
    if(stream->openMode=='w'&&stream->mode_prefix==0){
        return SO_EOF;
    }else if(stream->openMode=='a'&&stream->mode_prefix==0){
        return SO_EOF;
    }
    int bytes_read = 0;
    char* word = malloc(sizeof(char));
    while((nmemb*size)>bytes_read && stream->size >= bytes_read){
        for(int i = 0; i < size ; i++){
            
            *word = so_fgetc(stream);

           //  if(stream->eof==true){
           //     return 0;
           // }
            memcpy(ptr+bytes_read, word, 1);
            bytes_read++;
            
        }
    }
    if(stream->size < bytes_read){
        //free(word);
        //stream->error=true;
        //return SO_EOF;
        stream->error=true;
        stream->eof=true;
        bytes_read--;
        //free(word);
       // return 0;
        
    }
    //if(stream->size==bytes_read && stream->size != BUFFER_SIZE){
    //    return 0;
    //

   
    free(word);
    return bytes_read/size;
}

int so_fputc(int c, SO_FILE *stream){
    if(stream==NULL){
        return SO_EOF;
    }
    int ff_st = 0;
    if(stream->buffer_cursor < BUFFER_SIZE){
        stream->last_operation = 1; //write
        stream->buffer[stream->buffer_cursor] = c;
        stream->buffer_cursor++;
        stream->file_cursor++;
        return stream->buffer[stream->buffer_cursor - 1];

    }
    ff_st = so_fflush(stream);
    if(ff_st == -1){
        stream->error=true;
        return SO_EOF;
    }
    stream->last_operation = 1;
    stream->buffer[stream->buffer_cursor] = c;
    stream->buffer_cursor++;
    stream->file_cursor++;
    return stream->buffer[stream->buffer_cursor - 1];
}

size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream){
    if(stream == NULL){
        return 0;
    }
    int i;
    char c;
    for( i = 0; i< (size*nmemb); i++){
        c=*((char*)(ptr) +i);
        so_fputc(c, stream);
    }
    return i/size;

}

long so_ftell(SO_FILE *stream){
   
    return stream->file_cursor;
}

int so_feof(SO_FILE *stream){
    return stream->eof;
}

int so_ferror(SO_FILE *stream){
    if(stream==NULL){
        return -1;
    }else{
        return stream->error;
    }
    return -1;
   
}

int so_fileno(SO_FILE *stream){
    return stream->file_descriptor;
}

int so_fseek(SO_FILE *stream, long offset, int whence){
    if(stream->last_operation == 0){
        memset(stream->buffer, 0, BUFFER_SIZE);
        stream->buffer_cursor = 0;
    }else if(stream->last_operation == 1){
        so_fflush(stream);
    }
    stream->file_cursor= lseek(stream->file_descriptor, offset, whence);
    if(stream->file_cursor==-1){
        stream->error=true;
        return -1;
    }
    return 0;
}
SO_FILE* so_popen(const char*command, const char*type){
        //lanseaza un proces nou care executa comanda specificata de parametru
        //se utilizeaza sh -c command
        //pipe pentru redirectare intrare standard si iesire standard
        //return value :
                        //succes->fisier in care se poate scrie
	
    
    //stream=malloc(sizeof(SO_FILE));
    pid_t pid;
    int file_descriptors[2];
    int open_parent;
    int open_child; //0 pt read, 1 pt write
    if(type[0]=='r'){
        open_parent=0;
        open_child=1;
    }else if(type[0]=='w'){
        open_parent=1;
        open_child=0;
    }else{
        return NULL;
    }
    int r=pipe(file_descriptors);
    if(-1==r){
        return NULL;
    }
    pid=fork();
    if(pid==-1){
        close(file_descriptors[0]);
        close(file_descriptors[1]);
    }else if(pid==0){
        
        close(file_descriptors[open_parent]);
        if(type[0]=='r'){
            int r2=dup2(file_descriptors[open_child],STDOUT_FILENO); //1 pt stdout
            if(r2==-1){
                //trb sa inchid 
                close(file_descriptors[open_child]); //1
               
            }
        }else if(type[0]=='w'){
            int r3=dup2(file_descriptors[open_child],STDIN_FILENO); //0 pt stdin
            if(r3==-1){
                close(file_descriptors[open_child]);//0
               
            }
        }

        int comanda=execlp("sh","sh","-c",command,NULL);
        if(comanda==-1){
            return NULL;
        }
        close(file_descriptors[open_child]);
        
    }else if(pid>0){
        //printf("In parinte");
        close(file_descriptors[open_child]);
        SO_FILE *stream=malloc(sizeof(SO_FILE));;
        if(stream==NULL){ return NULL;}
        //asignare valori fisier
        stream->child_pid=pid;
        //struct stat file_stat;
       //stat(pathname,&file_stat);
        stream->size=0;
        stream->file_descriptor=file_descriptors[open_parent];
        stream->no_bytes_rw=0;
        stream->buffer_cursor=0;
        stream->error=false;
        stream->eof=false;
        
        stream->file_cursor = 0;
        stream->isOpened=true;
        stream->last_operation=-1; 
        stream->popened=1;
            if(open_parent==0){
                stream->openMode='r';
            }else {
                stream->openMode='w';
            }
        stream->mode_prefix=0;
        memset(stream->buffer,0,BUFFER_SIZE);
        return stream;
        
    }
    
    return NULL;
}



int so_pclose(SO_FILE*stream){
   int status;
   int r=so_fflush(stream); //vars rezultatul
   if(r==-1){
     return -1;
   }
   close(stream->file_descriptor);
   int waiting=waitpid(stream->child_pid,&status,0);
   if(waiting == -1){
     free(stream);
     return -1;
   }
   //WIFEXITED este o optiune prin care aflu modul in care s-a terminat un proces
   //returneaza true daca copilul a terminat normal sau valoarea de exit a acestuia
   if(WIFEXITED(status))
    {
        free(stream);
    }else{
        stream->error=true;
    }
   return status;
   //return 0;

}
int main(){
    //SO_FILE*f=so_fopen("/home/nia/Desktop/Tema/file.txt","r");
    //printf("%c",so_fgetc(f));
    //printf("%c",so_fgetc(f));
    //printf("%c",so_fgetc(f));
    //char buffer[10];
    //so_fread(buffer,1,4,f);
    //printf("%s",buffer);
    //so_fputc(98,f);
    //char buffer[5]="anaa12";
    //so_fwrite(buffer,1,5,f);
    //so_fseek(f,4,SEEK_SET);
   //printf("%ld",so_ftell(f));
    //so_fclose(f);
   // SO_FILE*f = so_popen("ls-l", "r");
   // if(f==NULL){
   //     return 1;
   // }
  // SO_FILE* f=so_popen("cat file.txt","r");
  // if(!f){
  //  printf("Failed");
  // }
  // int r=so_pclose(f);
   //printf("%d",r);
    return 0;
}
