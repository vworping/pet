#include "pet_enterprise.h"
#include <string.h>

bool pet_umb_server_name(const unsigned char *name,size_t length){
 const char suffix[]="umb.edu";size_t n=sizeof(suffix)-1;
 if(!name||length<n||length>253)return false;
 /* Reject embedded NULs and malformed DNS names before matching the boundary. */
 for(size_t i=0;i<length;i++){
  unsigned char c=name[i];
  if(i==0&&c=='*'&&length>2&&name[1]=='.')continue;
  if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='.'))return false;
  if(c=='.'&&(i==0||i==length-1||name[i-1]=='.'))return false;
 }
 if(length>n&&name[length-n-1]!='.')return false;
 for(size_t i=0;i<n;i++){unsigned char c=name[length-n+i];if(c>='A'&&c<='Z')c+='a'-'A';if(c!=(unsigned char)suffix[i])return false;}
 return true;
}
#ifdef ESP_PLATFORM
#include "esp_eap_client.h"
#include "esp_crt_bundle.h"
#include "mbedtls/oid.h"
/* ESP-IDF 5.4 EAP-specific attachment hook. Kept here, away from HTTP TLS.
   The SDK's set_domain_name performs exact hostname checks, not Android's
   domain suffix matching. Preserve the bundle verifier and all other flags. */
extern int (*esp_crt_bundle_attach_fn)(void *conf);
static int (*bundle_verify)(void *,mbedtls_x509_crt *,int,uint32_t *);
static void *bundle_context;
static int umb_verify(void *ctx,mbedtls_x509_crt *crt,int depth,uint32_t *flags){
 (void)ctx;if(depth!=0)return bundle_verify(bundle_context,crt,depth,flags);
 bool match=false,has_dns=false;
 for(const mbedtls_x509_sequence *san=&crt->subject_alt_names;san;san=san->next){
  if(san->buf.tag==(MBEDTLS_ASN1_CONTEXT_SPECIFIC|MBEDTLS_X509_SAN_DNS_NAME)){
   has_dns=true;if(pet_umb_server_name(san->buf.p,san->buf.len))match=true;
  }
 }
 /* RFC 6125: use the common name only when there are no DNS SANs. */
 if(!has_dns)for(const mbedtls_x509_name *name=&crt->subject;name;name=name->next)
  if(MBEDTLS_OID_CMP(MBEDTLS_OID_AT_CN,&name->oid)==0&&pet_umb_server_name(name->val.p,name->val.len))match=true;
 if(match)*flags&=~MBEDTLS_X509_BADCERT_CN_MISMATCH;
 else *flags|=MBEDTLS_X509_BADCERT_CN_MISMATCH;
 return bundle_verify(bundle_context,crt,depth,flags);
}
static int umb_attach(void *conf){
 int err=esp_crt_bundle_attach(conf);if(err!=ESP_OK)return err;
 mbedtls_ssl_config *ssl=conf;
 bundle_verify=ssl->MBEDTLS_PRIVATE(f_vrfy);bundle_context=ssl->MBEDTLS_PRIVATE(p_vrfy);
 if(!bundle_verify)return ESP_FAIL;
 mbedtls_ssl_conf_verify(ssl,umb_verify,NULL);return ESP_OK;
}
int pet_enterprise_use_umb_trust(void){
 int err=esp_eap_client_use_default_cert_bundle(true);
 if(err==ESP_OK)err=esp_eap_client_set_domain_name("umb.edu");
 if(err==ESP_OK)esp_crt_bundle_attach_fn=umb_attach;
 return err;
}
#endif
