#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#define MAX_PATH_LENGTH 256

void shuffle_array(int arr[], int length) {
    srand(time(NULL));
    for (int i = length - 1; i > 0; --i) {
        int j = rand() % (i + 1);
        int temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
    }
}

int main(int argc, char *argv[]) {
    int pageSize = getpagesize();

    if (argc == 1) {
        printf("Welcome to Encryptor!\n");
        printf("This program encrypts and decrypts text files.\n");
        printf("To encrypt a file, run: ./encryptor [path_to_file]\n");
        printf("The encrypted file and permutations will be saved in the current directory.\n");
        printf("To decrypt, you need both the encrypted file and the permutations file.\n");
        printf("Run: ./encryptor [path_to_encrypted_file] [path_to_permutations_file]\n");
        return 0;
    }

    else if (argc == 2) {
        char permsFilename[] = "permutations";
        char encryptedFile[MAX_PATH_LENGTH];

        snprintf(encryptedFile, sizeof(encryptedFile), "%sEncrypted.txt", argv[1]);

        FILE *inputFile = fopen(argv[1], "r");
        FILE *permsFile = fopen(permsFilename, "w");
        FILE *outputFile = fopen(encryptedFile, "w");

        if (!inputFile || !permsFile || !outputFile) {
            perror("Error opening files");
            return 1;
        }

        int wordCount = 1;
        char ch;
        while ((ch = fgetc(inputFile)) != EOF) {
            if (ch == ' ') wordCount++;
        }
        rewind(inputFile);

        char shmName[] = "sharedMem";
        int shmFd = shm_open(shmName, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);

        if (shmFd < 0) {
            perror(NULL);
            return 1;
        }

        size_t shmSize = pageSize * wordCount;

        if (ftruncate(shmFd, shmSize) == -1) {
            perror(NULL);
            shm_unlink(shmName);
            return 1;
        }

        char *sharedMemPtr;
        for (int i = 0; i < wordCount; ++i) {
            sharedMemPtr = mmap(0, pageSize, PROT_WRITE, MAP_SHARED, shmFd, pageSize * i);

            if (sharedMemPtr == MAP_FAILED) {
                perror(NULL);
                shm_unlink(shmName);
                return 1;
            }

            while ((ch = fgetc(inputFile)) != EOF) {
                if (ch == ' ') break;
                else sharedMemPtr += sprintf(sharedMemPtr, "%c", ch);
            }
        }

        for (int i = 0; i < wordCount; ++i) {
            sharedMemPtr = mmap(0, pageSize, PROT_READ, MAP_SHARED, shmFd, pageSize * i);
            pid_t pid = fork();

            if (pid < 0) {
                return errno;
            } else if (pid == 0) {
                int wordLength = strlen(sharedMemPtr);
                int permutation[wordLength];

                for (int j = 0; j < wordLength; ++j) {
                    permutation[j] = j;
                }
                shuffle_array(permutation, wordLength);

                char encryptedWord[wordLength + 1];
                for (int j = 0; j < wordLength; ++j) {
                    encryptedWord[j] = sharedMemPtr[permutation[j]];
                }

                for (int j = 0; j < wordLength; ++j) {
                    fprintf(permsFile, "%d ", permutation[j]);
                    fprintf(outputFile, "%c", encryptedWord[j]);
                }

                if (i < wordCount - 1) {
                    fprintf(permsFile, "\n");
                    fprintf(outputFile, " ");
                }

                exit(0);
            } else {
                wait(NULL);
            }
            munmap(sharedMemPtr, pageSize);
        }

        printf("File successfully encrypted!\n");
        shm_unlink(shmName);
        fclose(inputFile);
        fclose(outputFile);
        fclose(permsFile);
    }
        // Continuarea codului pentru decriptare
    else if (argc == 3) {
        char decryptedFile[MAX_PATH_LENGTH];

        snprintf(decryptedFile, sizeof(decryptedFile), "%sDecrypted.txt", argv[1]);

        FILE *inputFile = fopen(argv[1], "r");
        FILE *permsFile = fopen(argv[2], "r");
        FILE *outputFile = fopen(decryptedFile, "w");

        if (!inputFile || !permsFile || !outputFile) {
            perror("Error opening files");
            return 1;
        }

        int wordCount = 1, wordLen = 0, maxWordLen = 0;
        char ch;
        while ((ch = fgetc(inputFile)) != EOF) {
            if (ch == ' ') {
                wordCount++;
                if (wordLen > maxWordLen) maxWordLen = wordLen;
                wordLen = 0;
                continue;
            }
            wordLen++;
        }
        rewind(inputFile);

        int permutations[wordCount][maxWordLen];
        char shmName[] = "sharedMem";
        int shmFd = shm_open(shmName, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);

        if (shmFd < 0) {
            perror(NULL);
            return 1;
        }

        size_t shmSize = pageSize * wordCount;

        if (ftruncate(shmFd, shmSize) == -1) {
            perror(NULL);
            shm_unlink(shmName);
            return 1;
        }

        char *sharedMemPtr;
        for (int i = 0; i < wordCount; ++i) {
            sharedMemPtr = mmap(0, pageSize, PROT_WRITE, MAP_SHARED, shmFd, pageSize * i);

            if (sharedMemPtr == MAP_FAILED) {
                perror(NULL);
                shm_unlink(shmName);
                return 1;
            }

            int currentWordLen = 0;
            while ((ch = fgetc(inputFile)) != EOF) {
                if (ch == ' ') break;
                else {
                    sharedMemPtr += sprintf(sharedMemPtr, "%c", ch);
                    currentWordLen++;
                }
            }

            for (int j = 0; j < currentWordLen; ++j) {
                fscanf(permsFile, "%d", &permutations[i][j]);
            }
        }

        for (int i = 0; i < wordCount; ++i) {
            sharedMemPtr = mmap(0, pageSize, PROT_READ, MAP_SHARED, shmFd, pageSize * i);
            pid_t pid = fork();

            if (pid < 0) {
                return errno;
            } else if (pid == 0) {
                int wordLength = strlen(sharedMemPtr);
                char decryptedWord[wordLength + 1];

                for (int j = 0; j < wordLength; ++j) {
                    decryptedWord[permutations[i][j]] = sharedMemPtr[j];
                }

                for (int j = 0; j < wordLength; ++j) {
                    fprintf(outputFile, "%c", decryptedWord[j]);
                }

                if (i < wordCount - 1) {
                    fprintf(outputFile, " ");
                }

                exit(0);
            } else {
                wait(NULL);
            }
            munmap(sharedMemPtr, pageSize);
        }

        printf("File successfully decrypted!\n");
        shm_unlink(shmName);
        fclose(inputFile);
        fclose(permsFile);
        fclose(outputFile);
    } else {
        printf("Usage:\nEncrypt: ./encryptor [input_file]\nDecrypt: ./encryptor [input_file] [permutations_file]\n");
    }

    return 0;

}

