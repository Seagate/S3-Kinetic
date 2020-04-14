#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "types.h"
#include "sc_pbkdf.h"
#include "sc_aes.h"
#include "aes_gcm.h"
#include "sc_aescrypt.h"
#include "sc_hmac.h"

#include "sc_sha2.h"


#define ITERATION_COUNT 1000   // this needs to match Seagate value


//Test routine to take in a user admin key and generate an encrytped master key
//Requires parameters

// Test_Security(Password,Counter)
//   Output will be the master key

int main(int argc, char* argv[]) {
  uint32 status ;

  uint8 Admin_Password[32];    // unit8*
  uint8 Salt_Value[32];   // unit8*
  uint8 output[32];
  uint32 KLen;
  uint32 PLen, SLen;
  
  printf("Test Security Called: \n");

  //Case where user did not enter Password
  if (argc == 1) 
   {
    // Call AES test routine
    printf(" RUN AES_GCM_Mode_Test \n");
   // AES_GCM_Mode_Test(  );
    

   }
  else
   {
    // set password to an arbitrary value
    strcpy (Admin_Password, "12345678901234567890");
    strcpy (Salt_Value,"98765432109876543210");
     // read admin key passed in 
     printf("Argc != 1: \n");
     
   }

  PLen = strlen(Admin_Password);
  SLen = strlen(Salt_Value);

  printf("Input Parameters are	: \n");
  printf("	Admin Password	: '%s'\n", Admin_Password);
  printf("	Password Length	: %d \n", PLen);
  printf("	SALT    	: '%s' \n", Salt_Value);
  printf("	SALT Length     : %d \n", SLen);
  


  // Call pbkdf routine to get master Key using SALT from TCG get random
  /*status PBKDF(uint8* P, 
		 uint32 PLen,
		 uint8* S,
		 uint32 SLen,
		 uint32 counter,
		 uint32 KLen, 
		 uint8* output );  
  */
  /* take the following 2 lines out for actual call */
 // strcpy (output,"Test Output Debug");
 // KLen = strlen(output);

  //printf("Returned Values: \n");
 // printf("	Master Key Len	: '%d' \n", KLen);
 // printf("	Master Key  	: '%s' \n", output);
 // printf("	STATUS   	: '%d' \n\n", status);
  


    return 0;
  
}  // main
