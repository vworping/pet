#include "pet_enterprise.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
 const char *yes[]={"umb.edu","radius.umb.edu","RADIUS.UMB.EDU","radius.auth.umb.edu","*.umb.edu","*.auth.umb.edu"};
 const char *no[]={"bu.edu","evilumb.edu","umb.edu.evil.com","umb.edu.",".umb.edu","a..umb.edu","*umb.edu","a*.umb.edu","*.*.umb.edu","https://umb.edu","umb.edu/",""};
 for(unsigned i=0;i<sizeof(yes)/sizeof(*yes);i++)assert(pet_umb_server_name((const unsigned char*)yes[i],strlen(yes[i])));
 for(unsigned i=0;i<sizeof(no)/sizeof(*no);i++)assert(!pet_umb_server_name((const unsigned char*)no[i],strlen(no[i])));
 const unsigned char nul[]={ 'x',0,'.','u','m','b','.','e','d','u' };assert(!pet_umb_server_name(nul,sizeof(nul)));assert(!pet_umb_server_name(NULL,7));
 puts("PASS: UMB certificate DNS boundaries, case folding, malformed and unrelated names");
}
