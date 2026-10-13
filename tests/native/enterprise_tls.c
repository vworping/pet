#include "pet_enterprise.h"
#include "esp_eap_client.h"
#include "esp_crt_bundle.h"
#include "mbedtls/oid.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int (*esp_crt_bundle_attach_fn)(void *);
static bool trusted=true;
static int bundle_calls;
static int fake_bundle(void *ctx,mbedtls_x509_crt *crt,int depth,uint32_t *flags){
 (void)ctx;(void)crt;(void)depth;bundle_calls++;
 /* Mirrors the SDK bundle's only-NOT_TRUSTED fast path. */
 if(trusted&&*flags==MBEDTLS_X509_BADCERT_NOT_TRUSTED)*flags=0;
 return 0;
}
void mbedtls_ssl_conf_verify(mbedtls_ssl_config *conf,int (*verify)(void *,mbedtls_x509_crt *,int,uint32_t *),void *ctx){conf->MBEDTLS_PRIVATE(f_vrfy)=verify;conf->MBEDTLS_PRIVATE(p_vrfy)=ctx;}
int esp_crt_bundle_attach(void *conf){mbedtls_ssl_conf_verify(conf,fake_bundle,NULL);return 0;}
int esp_eap_client_use_default_cert_bundle(bool enabled){assert(enabled);esp_crt_bundle_attach_fn=esp_crt_bundle_attach;return 0;}
int esp_eap_client_set_domain_name(const char *name){assert(!strcmp(name,"umb.edu"));return 0;}
static void dns(mbedtls_x509_crt *crt,const char *name){crt->subject_alt_names.buf.tag=MBEDTLS_ASN1_CONTEXT_SPECIFIC|MBEDTLS_X509_SAN_DNS_NAME;crt->subject_alt_names.buf.p=(unsigned char *)name;crt->subject_alt_names.buf.len=strlen(name);}
int main(void){
 assert(pet_enterprise_use_umb_trust()==0);mbedtls_ssl_config conf={0};assert(esp_crt_bundle_attach_fn(&conf)==0);
 mbedtls_x509_crt crt={0};dns(&crt,"radius.umb.edu");
 uint32_t flags=MBEDTLS_X509_BADCERT_CN_MISMATCH|MBEDTLS_X509_BADCERT_NOT_TRUSTED;
 assert(conf.MBEDTLS_PRIVATE(f_vrfy)(NULL,&crt,0,&flags)==0);assert(flags==0&&bundle_calls==1);
 trusted=false;flags=MBEDTLS_X509_BADCERT_CN_MISMATCH|MBEDTLS_X509_BADCERT_NOT_TRUSTED;conf.MBEDTLS_PRIVATE(f_vrfy)(NULL,&crt,0,&flags);assert(flags==MBEDTLS_X509_BADCERT_NOT_TRUSTED);trusted=true;
 flags=MBEDTLS_X509_BADCERT_EXPIRED|MBEDTLS_X509_BADCERT_CN_MISMATCH;conf.MBEDTLS_PRIVATE(f_vrfy)(NULL,&crt,0,&flags);assert(flags==MBEDTLS_X509_BADCERT_EXPIRED);
 dns(&crt,"radius.bu.edu");flags=0;conf.MBEDTLS_PRIVATE(f_vrfy)(NULL,&crt,0,&flags);assert(flags==MBEDTLS_X509_BADCERT_CN_MISMATCH);
 crt.subject.oid.p=(unsigned char*)MBEDTLS_OID_AT_CN;crt.subject.oid.len=MBEDTLS_OID_SIZE(MBEDTLS_OID_AT_CN);crt.subject.val.p=(unsigned char*)"radius.umb.edu";crt.subject.val.len=strlen((char*)crt.subject.val.p);
 flags=0;conf.MBEDTLS_PRIVATE(f_vrfy)(NULL,&crt,0,&flags);assert(flags==MBEDTLS_X509_BADCERT_CN_MISMATCH); /* SAN wins over CN */
 memset(&crt.subject_alt_names,0,sizeof(crt.subject_alt_names));flags=MBEDTLS_X509_BADCERT_CN_MISMATCH;conf.MBEDTLS_PRIVATE(f_vrfy)(NULL,&crt,0,&flags);assert(flags==0);
 flags=MBEDTLS_X509_BADCERT_NOT_TRUSTED;conf.MBEDTLS_PRIVATE(f_vrfy)(NULL,&crt,1,&flags);assert(flags==0); /* intermediate delegated unchanged */
 puts("PASS: EAP verifier preserves chain/expiry failures, checks SAN before CN and delegates bundle verification");
}
