#include <unistd.h>
#include <dirent.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#define K 1024

void pushFile(char* name, char* path, int tmpInfo, int tmpArc)
{
	int in = open(name, O_RDONLY); 
	long size = 0;
	if (strcmp(name, ".") != 0) {
		char block[64*K];
		long red = 0;
		while ((red = read(in, block, sizeof(block))) > 0 )
			write(tmpArc, block, red);
		size = lseek(in, 0, SEEK_END);
	} 
	char infoString[K]; 
	sprintf(infoString, "%s||%s||%ld||", path, name, size);
	write(tmpInfo, infoString, strlen(infoString)); 
}

int digPath(char* dir, char* path, int tmpInfo, int tmpArc)
{
	int numOfFiles = 0;
	char nextPath[K];
	DIR* dp;
	struct dirent *entry;
	struct stat statbuf;
	dp = opendir(dir);
	chdir(dir);
	while((entry = readdir(dp)) != NULL) {
		lstat(entry->d_name, &statbuf);
		if (S_ISDIR(statbuf.st_mode)) {
			if (strcmp("..", entry->d_name) == 0 || 
				strcmp(".", entry->d_name) == 0) 
				continue;
			sprintf(nextPath, "%s%s/", path, entry->d_name);
			digPath(entry->d_name, nextPath, tmpInfo, tmpArc);
		} else {
			pushFile(entry->d_name, path, tmpInfo, tmpArc);
			numOfFiles++;
		}
	}
	if (numOfFiles == 0) {
		pushFile(".", path, tmpInfo, tmpArc);
	}
	chdir("..");
	closedir(dp);
}

void pack(int argn, char** args, int archive)
{
	char cwd[K];
	getcwd(cwd, K);
	int tmpInfo = open(".tmpInfo", O_RDWR | O_CREAT | O_APPEND, S_IWUSR); 	
	int tmpArc = open(".tmpArc", O_RDWR | O_CREAT | O_APPEND, S_IWUSR ); 	
	struct stat statbuf;
	for (int i = 3; i < argn; i++) {
		if (lstat(args[i], &statbuf) == 0) { 
			if (S_ISDIR(statbuf.st_mode)) {
				char dirName[K];
				sprintf(dirName, "%s/", basename(args[i]));
				digPath(args[i], dirName, tmpInfo, tmpArc);
				chdir(cwd);
			} else {
				char fileName[K], dirName[K]; 
				sprintf(fileName, "%s", basename(args[i]));
				sprintf(dirName, "%s", dirname(args[i]));
				chdir(dirName);	
				pushFile(fileName, ".", tmpInfo, tmpArc);
				chdir(cwd);
			}
		}
	}
	write(tmpInfo, "\n", 1);
	lseek(tmpInfo, 0, SEEK_SET);
	lseek(tmpArc, 0, SEEK_SET);
	char block[64*K];
	int red;
	while ((red = read(tmpInfo, block, sizeof(block))) > 0)
		write(archive, block, red);
	while ((red = read(tmpArc, block, sizeof(block))) > 0)
		write(archive, block, red);
	unlink(".tmpInfo");
	unlink(".tmpArc");
	printf("Архивация прошла успешно\n");
}
	
int checkPath(char* path)
{
	char fPath[strlen(path)];
	sprintf(fPath, "%s", path);
	DIR* dp;
	char *newDir;
	newDir = strtok(fPath, "/");
	if ((dp=opendir(newDir)) == NULL) {
		if (mkdir(newDir, S_IRUSR | S_IWUSR | S_IXUSR) != 0) return -1;
	} else 
		closedir(dp);
	char cwd[K];
	getcwd(cwd, K);
	chdir(newDir);
	while ((newDir = strtok(NULL, "/")) != NULL) {
		if ((dp=opendir(newDir)) == NULL) {
			if (mkdir(newDir, S_IRUSR | S_IWUSR | S_IXUSR) != 0) 
				return -1;
		} else 
			closedir(dp);
		chdir(newDir);
	}
	chdir(cwd);
	return 0;
}

int popFile(char* path, char* name, char* size, int archive)
{
	int outFile;
	if ((outFile = open(name, O_WRONLY | O_CREAT | O_APPEND,
						S_IRUSR | S_IWUSR)) == -1) {
	struct stat statbuf;
		if (lstat(name, &statbuf) == 0) {
			printf("Файл с именем %s уже существует\nУдалить? [y\\n]\n", name); 
			char q[K];
			scanf("%s", q);
			if (strcmp(q, "y") == 0 || strcmp(q, "Y") == 0) {
				if (S_ISDIR(statbuf.st_mode))
					rmdir(name);
				else
					unlink(name);  
				if ((outFile = open(name, O_WRONLY | O_CREAT | O_APPEND,
									S_IRUSR | S_IWUSR)) == -1) {
					printf("Невозможно создать файл %s%s%s",
						   "\nРазархивация прервана\n", path, name);
					return -1;
				}
			}
		} else {
			printf("Невозможно создать файл %s%s%s",
				   "\nРазархивация прервана\n", path, name);
			return -1;
		}	
	}
	char *result = malloc(atoi(size));;
	read(archive, result, atoi(size));
	write(outFile, result, atoi(size));
	free(result);
	close(outFile);
	return 0;
}

int unpack(int archive)
{
	int yesForAll = 0;
	char cwd[K];
	getcwd(cwd, K);
	char infoString[64*K] = {'\0'};
	char c;
	read(archive, &c, 1);
	int red;
	while (c != '\n') {
		infoString[strlen(infoString)]=c;
		read(archive, &c, 1);
	}
	char *path, *name, *size, *saveptr;
	path = strtok_r(infoString, "||", &saveptr);
	name = strtok_r(NULL, "||", &saveptr);
	size = strtok_r(NULL, "||", &saveptr);
	while (path != NULL || name != NULL || size != NULL) {
		if (checkPath(path) == -1) {
			printf("Невозможно создать директорию %s\n", path); 
			return -1;
		}
		if (strcmp(name, ".") != 0) {
			chdir(path);
			struct stat statbuf;
			if (lstat(name, &statbuf) == 0) {
				if (yesForAll == 0) {
					int answered = 0;
					char q[K];
					while (answered == 0) {
						printf("Файл %s%s уже существует. Заменить?%s",
							   path,name,"\n[Y/YF/N/NF] (YES/YES FOR ALL/NO/NO FOR ALL)\n");
						scanf("%s", q);
						if (strcmp(q, "yf") == 0 || strcmp(q, "YF") == 0) {
							answered = 1;
							yesForAll = 1;
						}
						if (strcmp(q, "y") == 0 || strcmp(q, "Y") == 0) {
							answered = 1;
							unlink(name);
							popFile(path, name, size, archive); 
						}
						if (strcmp(q, "nf") == 0 || strcmp(q, "NF") == 0) {
							answered = 1;
							yesForAll = -1;
						}
						if (strcmp(q, "n") == 0 || strcmp(q, "N") == 0) {
							answered = 1;
						}
					}
				}
				if (yesForAll == 1) {
					unlink(name);
					popFile(path, name, size, archive); 
				}		
			} else 
				popFile(path, name, size, archive); 
		}
		path = strtok_r(NULL, "||", &saveptr);
		name = strtok_r(NULL, "||", &saveptr);
		size = strtok_r(NULL, "||", &saveptr);
		chdir(cwd);
	}
	close(archive);
	printf("Разархивация прошла успешно\n");
	return 0;
}

int main(int argn, char** args)
{	
	if (argn > 1) {
		if (strcmp(args[1], "-p") == 0) {
			if (argn < 4) 
				printf("Укажите имя архива, и путь к архивируемым файлам\n");
			else {
				struct stat statbuf;
				if(lstat(args[2], &statbuf) == 0) { 
					printf("Файл с именем %s уже существует\nУдалить? [y\\n]\n", args[2]); 
					char q[K];
					scanf("%s", q);
					if (strcmp(q, "y") == 0 || strcmp(q, "Y") == 0) {
						if (S_ISDIR(statbuf.st_mode))
							rmdir(args[2]);
						else
							unlink(args[2]); 
					} else { 
						printf("Архивация прервана\n");
						return -1; 
					}
				}
				for (int i = 3; i < argn; i++) {
					if( lstat(args[i], &statbuf ) != 0) { 
						printf("Файла %s не существует\n", args[i]); 
						printf("Архивация прервана\n");
						return -1; 		
					}
				}
				char arcName[K], arcPath[K], arcFullName[K];
				sprintf(arcName,"%s", basename(args[2]));
				sprintf(arcPath,"%s", dirname(args[2]));
				sprintf(arcFullName,"%s/%s", arcPath, arcName);
				checkPath(arcPath);
				int archive; // дескриптор файла - архива
				if ((archive = open(arcFullName, 
									O_WRONLY | O_CREAT | O_APPEND, S_IRUSR)) == -1) {
					printf("Невозможно создать файл %s\n", arcFullName);
					return -1;
				} else {
					pack(argn, args, archive);	
				}
			}
		} else 
			if (strcmp(args[1], "-u") == 0) {
				if (argn < 4)
					printf("Укажите путь к архиву и путь разархивации\n");
				else {
					struct stat statbuf;
					if(lstat(args[2], &statbuf) != 0) { 
						printf("Файла %s не существует\n", args[2]); 
						return 0;
					}
					int archive;
					if ((archive = open(args[2], O_RDONLY)) == -1) {
						printf("Невозможно открыть файл %s\n", args[2]);
						return -1;
					}
					if(checkPath(args[3]) != 0) {
						printf("Невозможно создать директорию %s\n", args[3]);
						return -1;
					}
					chdir(args[3]);
					unpack(archive);
				}
			} else 
				printf("Ключ -p/-u обязателен\n");
	}
	else { 
		printf("Запускайте программу со следующими параметрами:%s",
			   "\n-p\\-u <имя_архива> <имена архивируемых файлов>\n");
	}
	return 0;
}