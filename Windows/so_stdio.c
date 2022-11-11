#define _CRT_SECURE_NO_WARNINGS
#include<Windows.h>
#include<stdio.h>
#include<stdlib.h>
#define DLL_EXPORTS
#include"so_stdio.h"
//#define SO_EOF -1
//#define SO_SEEK_SET 0
//#define SO_SEEK_CUR 1
//#define SO_SEEK_END 2
#define BUFFER_SIZE 4096
struct _so_file {
	HANDLE file_descriptor;
	int end_of_file; //1 daca sunt la sf, 0 daca nu
	int error; //1 daca am erori , 0 daca nu am
	long int size;
	int file_cursor;
	int last_operation;
	int open_mode; //1 r,2 r+,3 w,4 w+,5 a,6 a+
	char buffer[BUFFER_SIZE];
	int buffer_cursor;
	int no_bytes_rw;
	int child_pid;
	int lungime_buffer;
};
typedef struct _so_file SO_FILE;
HANDLE proc_handle;
int so_fflush(SO_FILE* stream) {
	if (stream == NULL) {
		return -1;
	}
	
	stream->no_bytes_rw = 0;
	if (stream->open_mode == 5 || stream->open_mode == 6) {
		stream->file_cursor = SetFilePointer(stream->file_descriptor,
			0, NULL, FILE_END);
	}
	DWORD bytes_written;
	while (stream->no_bytes_rw < stream->buffer_cursor) {
		BOOL rezultat = WriteFile(stream->file_descriptor,
			stream->buffer + stream->no_bytes_rw,
			stream->buffer_cursor - stream->no_bytes_rw,
			&bytes_written,
			NULL);

		if (!rezultat) {
			stream->error = 1;
			return SO_EOF;
		}
		stream->no_bytes_rw += bytes_written;
		
		
	}
	memset(stream->buffer, 0, BUFFER_SIZE);
	//stream->last_operation = -1;
	stream->buffer_cursor = 0;
	//stream->no_bytes_rw = 0;
	return 0;
}
int so_fseek(SO_FILE* stream, long offset, int whence) {
	if (stream->last_operation == 0) {
		memset(stream->buffer, 0, BUFFER_SIZE);
		stream->buffer_cursor = 0;
	}
	else if (stream->last_operation == 1) {
		so_fflush(stream);
	}
	DWORD location = SetFilePointer(stream->file_descriptor, offset, NULL, whence);
	if (location == INVALID_SET_FILE_POINTER)
	{
		stream->error = 1;
		return -1;
	}
	stream->file_cursor = location;
	return 0;
}
SO_FILE* so_fopen(const char* pathname, const char* mode)
{
	HANDLE fd = INVALID_HANDLE_VALUE;
	int mode_flag;
	if (mode[0] == 'r') {
		if (strlen(mode) > 1 && mode[1] == '+') {
			mode_flag = 2;
			fd = CreateFileA(pathname,
				GENERIC_READ | GENERIC_WRITE,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				NULL,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				NULL);
		}
		else {
			mode_flag = 1;
			fd = CreateFileA(pathname,
				GENERIC_READ,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				NULL,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
				NULL);
		}
	}
	else if (mode[0] == 'w') {
		if (strlen(mode) > 1 && mode[1] == '+') {
			mode_flag = 4;
			fd = CreateFileA(pathname,
				GENERIC_WRITE | GENERIC_READ,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				NULL,
				CREATE_NEW | TRUNCATE_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				NULL);
		}
		else {
			mode_flag = 3;
			fd = CreateFileA(pathname,
				GENERIC_WRITE,
				FILE_SHARE_READ| FILE_SHARE_WRITE,
				NULL,
				CREATE_NEW | TRUNCATE_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				NULL);
		}
	}
	else if (mode[0] == 'a') {
		if (strlen(mode) > 1 && mode[1] == '+') {
			mode_flag = 6;
			fd = CreateFileA(pathname,
				FILE_APPEND_DATA | GENERIC_READ,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				NULL,
				OPEN_EXISTING | CREATE_NEW,
				FILE_ATTRIBUTE_NORMAL,
				NULL);
		}
		else {
			mode_flag = 5;
			fd = CreateFileA(pathname,
				FILE_APPEND_DATA,
				FILE_SHARE_READ|FILE_SHARE_WRITE,
				NULL,
				OPEN_EXISTING | CREATE_NEW,
				FILE_ATTRIBUTE_NORMAL,
				NULL);
		}
	}
	else {
		return NULL;
	}
	if (fd == INVALID_HANDLE_VALUE) {
		return NULL;
	}
	SO_FILE* file = (SO_FILE*)malloc(sizeof(SO_FILE));
	if (file == NULL) {
		return NULL;
	}
	DWORD attributes = GetFileAttributesA((LPCSTR)pathname);
	if (attributes == INVALID_FILE_ATTRIBUTES) {
		return NULL;
	}
	//aici trebuie sa fac fseek sa vad size-ul fisierului
	file->file_descriptor = fd;
	file->no_bytes_rw = 0;
	file->buffer_cursor = 0;
	file->size = 0;
	file->error = 0;
	file->end_of_file = 0;
	file->open_mode = mode_flag;
	file->last_operation = -1; //de la open
	file->child_pid = 0;
	file->file_cursor = 0;
	memset(file->buffer, 0, BUFFER_SIZE);
	return file;


}

int so_fclose(SO_FILE* stream) {
	

	if (stream == NULL) {
		return SO_EOF;
	}
	if (stream->last_operation == 1) {
		int ff = so_fflush(stream);
		if (ff == -1) {
			free(stream);
			return SO_EOF;
		}
	}
	BOOL raspuns = CloseHandle(stream->file_descriptor);
	if (raspuns == FALSE) {
		stream->error = 1;
		return SO_EOF;
	}
	return 0;
}

int so_fgetc(SO_FILE* stream) {
	if (stream == NULL) {
		return SO_EOF;
	}
	DWORD file_read=1;
	if (stream->buffer_cursor == 0 || stream->buffer_cursor == BUFFER_SIZE || stream->buffer_cursor == stream->no_bytes_rw) {
		memset(stream->buffer, 0, BUFFER_SIZE);
		stream->buffer_cursor = 0;
		BOOL raspuns = ReadFile(stream->file_descriptor, stream->buffer,
			BUFFER_SIZE, &file_read, NULL);
		stream->no_bytes_rw = file_read;
		if (raspuns == 0) {
			stream->error = 1;
			return SO_EOF;
		}
		if (file_read == 0) {
			stream->end_of_file = 1;
			return SO_EOF;
		}
		

	}
	if (file_read > 0) {
		stream->last_operation = 0;
		stream->file_cursor+=1;
		stream->buffer_cursor+=1;
		return stream->buffer[stream->buffer_cursor-1];
	}
	else {
		stream->error = 1;
		return SO_EOF;
	}
	
}
long so_ftell(SO_FILE* stream) {
	return stream->file_cursor;
}
int so_feof(SO_FILE* stream) {
	return stream->end_of_file;
}
int so_ferror(SO_FILE* stream) {
	if (stream == NULL) {
		return -1;
	}
	else {
		return stream->error; //1 pentru eroare
	}
	return -1;

}
size_t so_fread(void* ptr, size_t size, size_t nmemb, SO_FILE* stream) {
	if (stream == NULL) {
		return SO_EOF;
	}
	if (stream->open_mode == 3 || stream->open_mode == 5) {
		return SO_EOF;
	}
	int bytes_read = 0;
	
	
		/*artificiu de calcul pentru dimensiunea fisierului*/
	so_fseek(stream, 0, SEEK_END);
	stream->size = so_ftell(stream);
	so_fseek(stream, 0, SEEK_SET);
		

	while ((nmemb * size) > bytes_read && stream->size >= bytes_read) {
		for (int i = 0; i < size; i++) {
			int caracter= so_fgetc(stream);
			*((char*)ptr + bytes_read) = caracter;
			bytes_read++;
		}
	}
	
	if (stream->size < bytes_read) {
		stream->error = 1;
		bytes_read--;
	}
	//(word);
	//printf("%d\n", bytes_read);
	return bytes_read / size;

}
HANDLE so_fileno(SO_FILE* stream) {
	return stream->file_descriptor;
}
int so_fputc(int c, SO_FILE* stream) {
	if (stream == NULL) {
		return SO_EOF;
	}
	int ff_st = 0;
	if (stream->buffer_cursor < BUFFER_SIZE) {
		stream->last_operation = 1; //write
		stream->buffer[stream->buffer_cursor] = c;
		stream->buffer_cursor++;
		stream->file_cursor++;
		return stream->buffer[stream->buffer_cursor - 1];

	}
	ff_st = so_fflush(stream);
	if (ff_st == -1) {
		stream->error = 1;
		return SO_EOF;
	}
	stream->last_operation = 1;
	stream->buffer[stream->buffer_cursor] =(char)c;
	stream->buffer_cursor++;
	stream->file_cursor++;
	return (int)stream->buffer[stream->buffer_cursor - 1];
}
size_t so_fwrite(const void* ptr, size_t size, size_t nmemb, SO_FILE* stream) {
	if (stream == NULL) {
		return 0;
	}
	int i;
	char c;
	for (i = 0; i < (size * nmemb); i++) {
		c = *((char*)(ptr)+i);
		so_fputc(c, stream);
	}
	return i / size;

}
SO_FILE* so_popen(const char* command, const char* type) {
	/*		BOOL CreateProcess(
				LPCTSTR               lpApplicationName, //numele modulului de rulat +extensia fisierului
				LPTSTR                lpCommandLine,
				LPSECURITY_ATTRIBUTES lpProcessAttributes,
				LPSECURITY_ATTRIBUTES lpThreadAttributes,
				BOOL                  bInheritHandles,
				DWORD                 dwCreationFlags,
				LPVOID                lpEnvironment,
				LPCTSTR               lpCurrentDirectory,
				LPSTARTUPINFO         lpStartupInfo,
				LPPROCESS_INFORMATION lpProcessInformation
			);*/

	SO_FILE* file;
	BOOL error_n;
	char* command_process = (char*)malloc(strlen(command));
	if (command_process)
	{
		strcpy(command_process, command);
	}
	SECURITY_ATTRIBUTES sec_attributes;
	HANDLE head_read, head_write;
	STARTUPINFO start_info;
	PROCESS_INFORMATION process_info;

	file = malloc(sizeof(SO_FILE));
	if (file == NULL)
		return NULL;

	ZeroMemory(&sec_attributes, sizeof(sec_attributes));
	ZeroMemory(&process_info, sizeof(process_info));
	ZeroMemory(&start_info, sizeof(start_info));
	sec_attributes.bInheritHandle = TRUE;

	//creez pipe-ul si verific daca totul e ok
	error_n = CreatePipe(&head_read, &head_write, &sec_attributes, 0);
	if (error_n == FALSE) {
		free(file);
		return NULL;
	}

	start_info.cb = sizeof(start_info);
	start_info.hStdInput = GetStdHandle(STD_INPUT_HANDLE); //handle pentru intrarea standard
	start_info.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE); //handle pentru iesirea standard
	start_info.hStdError = GetStdHandle(STD_ERROR_HANDLE);
	start_info.dwFlags |= STARTF_USESTDHANDLES;

	// redirectare intrari, iesiri standard in functie de type 
	//in functie de parametrul din type eu trebuie sa deschid un fisier de sciere sau de citire
	if (strcmp(type, "r") == 0) {
		file->file_descriptor = head_read;
		file->open_mode = 'r';
		start_info.hStdOutput = head_write;
		error_n = SetHandleInformation(head_read, HANDLE_FLAG_INHERIT, 0); //informatii despre handlerul pt capatul de citire

		if (error_n == FALSE)
		{
			CloseHandle(head_read);
			CloseHandle(head_write);
			free(file);
			return NULL;
		}

	}
	else if (strcmp(type, "w") == 0) {
		file->file_descriptor = head_write;
		file->open_mode = 'w';
		start_info.hStdInput = head_read;
		error_n = SetHandleInformation(head_write, HANDLE_FLAG_INHERIT, 0);

		if (error_n == FALSE)
		{
			CloseHandle(head_read);
			CloseHandle(head_write);
			free(file);
			return NULL;
		}

	}
	else
	{
		CloseHandle(head_read);
		CloseHandle(head_write);
		free(file);
		return NULL;
	}
	if (command_process)
	{
		error_n = CreateProcessA(
			NULL,
			command_process,
			NULL,
			NULL,
			TRUE,
			0,
			NULL,
			NULL,
			&start_info,
			&process_info
		);
		proc_handle = process_info.hProcess;
		//WaitForSingleObject(process_info.hProcess, INFINITE);

		CloseHandle(process_info.hThread);
		//CloseHandle(process_info.hProcess);

	}

	if (error_n == FALSE)
	{
		CloseHandle(head_read);
		CloseHandle(head_write);
		free(file);
		return NULL;
	}

	/** se inchid capetele nefolosite */
	if (strcmp(type, "r") == 0)
		CloseHandle(head_write);
	else if (strcmp(type, "w") == 0)
		CloseHandle(head_read);

	return file;
}
int so_pclose(SO_FILE* stream) {
	int ret_v = 0;
	DWORD dwRes;
	BOOL bRes;
	dwRes = WaitForSingleObject(proc_handle, INFINITE);
	bRes = GetExitCodeProcess(proc_handle, &dwRes);
	if (stream->child_pid > 0) {
		if (so_fflush(stream) != 0) {
			ret_v = SO_EOF;
		}
	}
	else { return -1; }


	CloseHandle(stream->file_descriptor);
	free(stream);
	return ret_v;
	
}

int main() {
	//SO_FILE* file = so_fopen("Text.txt", "r+");
	//SO_FILE* file2 = so_fopen("C:/Users/Asus/Desktop/Tema1 pso/so_stdio/so_stdio/fisier.txt", "w");
	
	/*printf("%c\n",(char)so_fgetc(file));
	printf("%c\n", (char)so_fgetc(file));
	printf("%c\n", (char)so_fgetc(file));*/
	/*char buffer[5];
	memset(buffer, 0, 5);
	int citit =so_fread(buffer, 1, 5, file);
	for (int i = 0; i < 5; i++) {
		printf("%c", buffer[i]);
	}*/
	/*SO_FILE* file2 = so_fopen("Text1.txt", "w+");
	char buffer2[5] = "Anaa";
	so_fputc(111, file2);
	so_fflush(file2);
	so_fwrite(buffer2, 1, 4, file2);
	so_fflush(file2);

	int r=so_fclose(file);
	printf("r=%d\n", r);
	int r2 = so_fclose(file2);
	printf("r=%d\n", r2);*/
	return 0;

}
