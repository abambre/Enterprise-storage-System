/*!
 * \file    example2.c
 * \brief   Usage example of dropbox C library.
 * \author  Rohit Arora
 * \version 1.0
 * \date    02.17.2014
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dropbox.h>
#include <memStream.h>

int connectToDRB(drbClient* cli, char *t_key, char *t_secret);
void uploadDRBFile(drbClient* cli, char *localFilePath, char *fileName, char *DRBFilePath);
void downloadDRBFile(drbClient* cli, char *localFilePath, char *fileName, char *DRBFilePath);

int main (int argc, char **argv) {
   
    int err;
    void* output;
    
    // Application key and secret. To get them, create an app here:
    // https://www.dropbox.com/developers/apps
    char *c_key    = "e9kwa4b6lnxk0r5";  //< consumer key
    char *c_secret = "sr8lt6xj5izjtol";  //< consumer secret
    
    // User key and secret. Leave them NULL or set them with your AccessToken.
    char *t_key    = "KSkQCyhUYNIAAAAAAAAAC87g7xlvpRUDDk0WPc65iLPudEugvXhi6v1iq3hukLS_"; //< access token key
    char *t_secret = NULL;  //< access token secret

	char *commandType;
	char *path;
	char fileName[255];
	char DRBFilePath[255];

     	if(argc >= 2){
		commandType = argv[1];
		path = argv[2];
		strcpy(fileName,argv[3]);
		strcpy(DRBFilePath, "/");
		strcat(DRBFilePath, fileName);
		strcat(path, fileName);
		printf("r or w: %s \n", commandType);
		printf("File name: %s \n", fileName);
		printf("Local file path: %s \n", path);
		printf("Dropbox file path: %s \n", DRBFilePath);

	} 
	else{
		printf("Please provide valid command line arguments");
	}
    
    // Global initialisation
    drbInit();
    
    // Create a Dropbox client
    drbClient* cli = drbCreateClient(c_key, c_secret, t_key, t_secret);
    
    // Request a AccessToken if undefined (NULL)
    connectToDRB(cli, t_key, t_secret);
    
    // Set default arguments to not repeat them on each API call
    drbSetDefault(cli, DRBOPT_ROOT, DRBVAL_ROOT_AUTO, DRBOPT_END);
    
    // Try to download a file specified by the user
    if(strcmp(commandType, "r") == 0){
	downloadDRBFile(cli, path, fileName, DRBFilePath);
    }
    
    // Try to upload a file specified by the user
    if(strcmp(commandType, "w") == 0 ){
	uploadDRBFile(cli, path, fileName, DRBFilePath);
    }

    // Free all client allocated memory
    drbDestroyClient(cli);
    
    // Global Cleanup
    drbCleanup();
    
    return EXIT_SUCCESS;
}

void uploadDRBFile(drbClient* cli, char *localFilePath, char *fileName, char *DRBFilePath){

	int err;
	FILE *file = fopen(localFilePath, "r");
	printf("Uploading File %s...\n", fileName);
        err = drbPutFile(cli, NULL,
                         DRBOPT_PATH, DRBFilePath,
                         DRBOPT_IO_DATA, file,
                         DRBOPT_IO_FUNC, fread,
                         DRBOPT_END);
	fclose(file);
        printf("File upload: %s\n", err ? "Failed" : "Successful");
}

void downloadDRBFile(drbClient* cli, char *localFilePath, char *fileName, char *DRBFilePath){

	int err;
    	FILE *file = fopen(localFilePath, "w"); // Write it in this file
    	void* output = NULL;
	printf("Downloading File %s...\n", fileName);
	err = drbGetFile(cli, &output,
                     	DRBOPT_PATH, DRBFilePath,
                     	DRBOPT_IO_DATA, file,
                     	DRBOPT_IO_FUNC, fwrite,
                     	DRBOPT_END);
    	fclose(file);
	printf("File Download: %s\n", err ? "Failed" : "Successful");
}

int connectToDRB(drbClient* cli, char *t_key, char *t_secret){ 
	// Request a AccessToken if undefined (NULL)
    	if(!t_key || !t_secret) {
        	drbOAuthToken* reqTok = drbObtainRequestToken(cli);
        
        	if (reqTok) {
            		char* url = drbBuildAuthorizeUrl(reqTok);
            		printf("Please visit %s\nThen press Enter...\n", url);
            		free(url);
            		fgetc(stdin);
            
            		drbOAuthToken* accTok = drbObtainAccessToken(cli);
            
            		if (accTok) {
                		// This key and secret can replace the NULL value in t_key and
                		// t_secret for the next time.
                		printf("key:    %s\nsecret: %s\n", accTok->key, accTok->secret);
            		} else{
                		fprintf(stderr, "Failed to obtain an AccessToken...\n");
                		return EXIT_FAILURE;
            		}
        	} else {
            		fprintf(stderr, "Failed to obtain a RequestToken...\n");
            		return EXIT_FAILURE;
        	}
    	}	
}
