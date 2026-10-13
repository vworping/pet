/* MFRC522 I2C bench driver. Register protocol: NXP MFRC522 datasheet.
 * Single-card operation: collisions are rejected, never arbitrated for writes.
 * Only this task accesses reader registers; IDF serializes shared I2C transfers.
 */
#include "pet_rfid.h"
#include "pet_ndef.h"
#include "pet_cloud.h"
#include "waveshare_rgb_lcd_port.h"
#include "esp_timer.h"
#include "driver/usb_serial_jtag.h"
#include <string.h>
#include <stdio.h>

#define TAG "pet_rfid"
#define TRY(x) do { esp_err_t e_ = (x); if (e_ != ESP_OK) return e_; } while (0)
enum { CMD=1, IRQ=4, ERR=6, STATUS2=8, FIFO=9, LEVEL=10, CONTROL=12, FRAME=13 };
typedef struct { uint8_t uid[10], len, sak, atqa[2]; } card_t;
static char pending_uid[21];
static char auto_write_failed_uid[21];
static int64_t pending_until;
static enum { OP_TEST, OP_URL, OP_ERASE } pending_op;
static char pending_url[PET_NDEF_URL_MAX + 1];

static esp_err_t wr(uint8_t reg, uint8_t value) {
    uint8_t b[2]={reg,value};
    return i2c_master_write_to_device(I2C_MASTER_NUM,0x28,b,2,pdMS_TO_TICKS(25));
}
static esp_err_t rd(uint8_t reg,uint8_t *value) {
    return i2c_master_write_read_device(I2C_MASTER_NUM,0x28,&reg,1,value,1,pdMS_TO_TICKS(25));
}
static void crc_a(const uint8_t *p,size_t n,uint8_t out[2]) {
    uint16_t crc=0x6363;
    while(n--) { uint8_t b=*p++ ^ (uint8_t)crc; b^=(uint8_t)(b<<4);
        crc=(crc>>8)^((uint16_t)b<<8)^((uint16_t)b<<3)^(b>>4); }
    out[0]=crc;out[1]=crc>>8;
}
static bool crc_ok(const uint8_t *p,size_t n) {
    if(n<3)return false;
    uint8_t c[2];crc_a(p,n-2,c);return c[0]==p[n-2] && c[1]==p[n-1];
}
static esp_err_t exchange(uint8_t command,const uint8_t *tx,size_t tn,uint8_t bits,
                          uint8_t *rx,size_t *rn,uint8_t *rb) {
    if(tn>64)return ESP_ERR_INVALID_SIZE;
    TRY(wr(CMD,0)); TRY(wr(IRQ,0x7f)); TRY(wr(LEVEL,0x80));
    for(size_t i=0;i<tn;i++)TRY(wr(FIFO,tx[i]));
    TRY(wr(FRAME,bits)); TRY(wr(CMD,command));
    if(command==0x0c)TRY(wr(FRAME,0x80|bits));
    int64_t end=esp_timer_get_time()+50000;
    uint8_t irq=0;
    do {
        TRY(rd(IRQ,&irq));
        if(irq & (command==0x0e ? 0x10:0x30))break;
        if(irq&1){wr(CMD,0);return ESP_ERR_TIMEOUT;}
        vTaskDelay(pdMS_TO_TICKS(1));
    } while(esp_timer_get_time()<end);
    TRY(wr(FRAME,bits));
    if(!(irq & (command==0x0e ? 0x10:0x30))){wr(CMD,0);return ESP_ERR_TIMEOUT;}
    uint8_t err;TRY(rd(ERR,&err));
    if(err&0xdf)return ESP_FAIL;
    if(command==0x0e)return ESP_OK;
    uint8_t count,last;TRY(rd(LEVEL,&count));TRY(rd(CONTROL,&last));
    if(count>*rn)return ESP_ERR_INVALID_SIZE;
    *rn=count;*rb=last&7;
    for(size_t i=0;i<count;i++)TRY(rd(FIFO,&rx[i]));
    return ESP_OK;
}
static esp_err_t transceive(const uint8_t *tx,size_t n,uint8_t bits,uint8_t *rx,size_t *rn,uint8_t *rb) {
    return exchange(0x0c,tx,n,bits,rx,rn,rb);
}
static esp_err_t init_reader(void) {
    TRY(wr(CMD,0x0f));vTaskDelay(pdMS_TO_TICKS(50));
    uint8_t state=0;int64_t end=esp_timer_get_time()+100000;
    do {TRY(rd(CMD,&state));if(!(state&0x10))break;vTaskDelay(pdMS_TO_TICKS(2));}while(esp_timer_get_time()<end);
    if(state&0x10)return ESP_ERR_TIMEOUT;
    uint8_t version;TRY(rd(0x37,&version));
    if(version==0 || version==0xff)return ESP_ERR_INVALID_RESPONSE;
    TRY(wr(0x2a,0x80));TRY(wr(0x2b,0xa9));TRY(wr(0x2c,0x03));TRY(wr(0x2d,0xe8));
    TRY(wr(0x12,0));TRY(wr(0x13,0));TRY(wr(0x15,0x40));TRY(wr(0x11,0x3d));uint8_t tx_control;TRY(rd(0x14,&tx_control));TRY(wr(0x14,tx_control|3));
    TRY(wr(0x26,0x70)); /* Maximum receiver gain for small stickers. */
    uint8_t antenna,gain;TRY(rd(0x14,&antenna));TRY(rd(0x26,&gain));
    ESP_LOGI(TAG,"Antenna TxControl=%02X receiver gain=%02X",antenna,gain);
    ESP_LOGI(TAG,"Reader ready at 0x28, VersionReg=0x%02x; present ONE card",version);
    return ESP_OK;
}
static esp_err_t select_card(card_t *c) {
    memset(c,0,sizeof(*c));TRY(wr(STATUS2,0));TRY(wr(0x0e,0));
    uint8_t rx[18],rb,tx[9]={0x52};size_t n=sizeof(rx);
    TRY(transceive(tx,1,7,rx,&n,&rb));
    if(n!=2 || rb)return ESP_ERR_INVALID_RESPONSE;
    memcpy(c->atqa,rx,2);
    for(int level=0;level<3;level++) {
        tx[0]=0x93+level*2;tx[1]=0x20;n=sizeof(rx);
        TRY(transceive(tx,2,0,rx,&n,&rb));
        if(n!=5 || rb || (rx[0]^rx[1]^rx[2]^rx[3])!=rx[4])return ESP_ERR_INVALID_RESPONSE;
        bool cascade=rx[0]==0x88;
        if(cascade && level==2)return ESP_ERR_INVALID_RESPONSE;
        memcpy(c->uid+c->len,rx+(cascade?1:0),cascade?3:4);c->len+=cascade?3:4;
        tx[1]=0x70;memcpy(tx+2,rx,5);crc_a(tx,7,tx+7);n=sizeof(rx);
        TRY(transceive(tx,9,0,rx,&n,&rb));
        if(n!=3 || rb || !crc_ok(rx,n))return ESP_ERR_INVALID_CRC;
        c->sak=rx[0];
        if(!(c->sak&4))return cascade?ESP_ERR_INVALID_RESPONSE:ESP_OK;
        if(!cascade)return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_ERR_INVALID_RESPONSE;
}
static void halt_card(void) {
    uint8_t tx[4]={0x50,0},rx[4],rb;size_t n=sizeof(rx);
    crc_a(tx,2,tx+2);transceive(tx,4,0,rx,&n,&rb);wr(STATUS2,0);
}
static bool classic(const card_t *c) {return c->sak==0x08 || c->sak==0x18 || c->sak==0x09;}
static esp_err_t authenticate(const card_t *c) {
    uint8_t tx[12]={0x60,4,255,255,255,255,255,255};
    memcpy(tx+8,c->uid+c->len-4,4);
    uint8_t rx[1],rb;size_t n=1;TRY(exchange(0x0e,tx,12,0,rx,&n,&rb));
    uint8_t status;TRY(rd(STATUS2,&status));return status&8?ESP_OK:ESP_ERR_INVALID_RESPONSE;
}
static esp_err_t read_data(uint8_t address,uint8_t out[16]) {
    uint8_t tx[4]={0x30,address},rx[18],rb;size_t n=sizeof(rx);crc_a(tx,2,tx+2);
    TRY(transceive(tx,4,0,rx,&n,&rb));
    if(n!=18 || rb || !crc_ok(rx,n))return ESP_ERR_INVALID_CRC;
    memcpy(out,rx,16);return ESP_OK;
}
static esp_err_t ack_command(uint8_t *tx,size_t n) {
    uint8_t rx[4],rb;size_t rn=sizeof(rx);crc_a(tx,n,tx+n);
    TRY(transceive(tx,n+2,0,rx,&rn,&rb));
    return rn==1 && rb==4 && (rx[0]&15)==10?ESP_OK:ESP_ERR_INVALID_RESPONSE;
}
/* Fixed ordinary user addresses only: Classic block 4, NTAG21x page 8.
 * No API permits writes to UID, lock bits, configuration, or sector trailers. */
static esp_err_t write_data(bool is_classic,const uint8_t *data) {
    uint8_t tx[18];
    if(is_classic) {
        tx[0]=0xa0;tx[1]=4;TRY(ack_command(tx,2));memcpy(tx,data,16);return ack_command(tx,16);
    }
    tx[0]=0xa2;tx[1]=8;memcpy(tx+2,data,4);return ack_command(tx,6);
}
static void uid_string(const card_t *c,char out[21]) {
    for(int i=0;i<c->len;i++)sprintf(out+i*2,"%02X",c->uid[i]);
}
/* Accept NXP NTAG21x and the observed vendor-53 NTAG213-compatible profile.
 * Require a writable Type 2 capability container and unlocked page 8. */
static bool ntag21x(void) {
    uint8_t tx[3]={0x60},rx[12],rb=0;size_t n=sizeof(rx);crc_a(tx,1,tx+1);
    esp_err_t e=transceive(tx,3,0,rx,&n,&rb);
    ESP_LOGI(TAG,"GET_VERSION status=%s bytes=%u bits=%u",esp_err_to_name(e),(unsigned)n,rb);
    if(e==ESP_OK)ESP_LOG_BUFFER_HEX_LEVEL(TAG,rx,n,ESP_LOG_INFO);
    uint8_t header[16];
    if(read_data(0,header)!=ESP_OK)return false;
    ESP_LOGI(TAG,"Type 2 header/CC:");ESP_LOG_BUFFER_HEX_LEVEL(TAG,header,16,ESP_LOG_INFO);
    if(header[12]!=0xe1 || header[13]!=0x10 || header[14]<3 || header[15]!=0 || (header[11]&1))return false;
    if(e!=ESP_OK || n!=10 || rb || !crc_ok(rx,n))return false;
    bool vendor=rx[1]==4 || (rx[1]==0x53 && rx[6]==0x0f && header[14]==0x12);
    return rx[0]==0 && vendor && rx[2]==4 && rx[3]==2 && rx[4]==1 && rx[5]==0 &&
           (rx[6]==0x0f || rx[6]==0x11 || rx[6]==0x13) && rx[7]==3;
}
static void test_write(const card_t *c,const char *uid) {
    bool cl=classic(c);
    if(!cl && !(c->sak==0 && ntag21x())) {
        ESP_LOGW(TAG,"WRITE REFUSED UID=%s: supported types are Classic and identified NTAG21x",uid);return;
    }
    uint8_t saved[16],pattern[16],verify[16];uint8_t addr=cl?4:8;size_t len=cl?16:4;
    esp_err_t e=cl?authenticate(c):ESP_OK;
    if(e==ESP_OK)e=read_data(addr,saved);
    if(e!=ESP_OK){ESP_LOGW(TAG,"WRITE REFUSED: backup read/auth failed: %s",esp_err_to_name(e));return;}
    ESP_LOGI(TAG,"BACKUP UID=%s %s=%u (retain this log)",uid,cl?"block":"page",addr);
    ESP_LOG_BUFFER_HEX_LEVEL(TAG,saved,len,ESP_LOG_INFO);
    for(size_t i=0;i<len;i++)pattern[i]=saved[i]^0x5a;
    e=write_data(cl,pattern);
    if(e==ESP_OK)e=read_data(addr,verify);
    bool passed=e==ESP_OK && memcmp(pattern,verify,len)==0;
    /* Attempt restoration even when the write ACK/read-back was lost. */
    esp_err_t restore=write_data(cl,saved);
    if(restore==ESP_OK)restore=read_data(addr,verify);
    bool restored=restore==ESP_OK && memcmp(saved,verify,len)==0;
    if(restored)ESP_LOGI(TAG,"WRITE TEST UID=%s pattern=%s original=RESTORED+VERIFIED",uid,passed?"VERIFIED":"FAILED");
    else ESP_LOGE(TAG,"RESTORE FAILED UID=%s: keep card on reader; original bytes in BACKUP log",uid);
}
/* Only the 144 user bytes of the identified NTAG213 layout are writable. */
static esp_err_t write_user_page(uint8_t page, const uint8_t data[4]) {
    if (page < 4 || page > 39) return ESP_ERR_INVALID_ARG;
    uint8_t tx[8] = {0xa2, page};
    memcpy(tx + 2, data, 4);
    return ack_command(tx, 6);
}
static esp_err_t read_user_image(uint8_t out[PET_NDEF_BYTES]) {
    for (unsigned offset = 0; offset < PET_NDEF_BYTES; offset += 16)
        TRY(read_data(4 + offset / 4, out + offset));
    return ESP_OK;
}
static bool empty_ndef(const uint8_t image[PET_NDEF_BYTES]) {
    uint8_t empty[PET_NDEF_BYTES];
    return pet_ndef_image(image, NULL, empty) && !memcmp(image, empty, sizeof(empty));
}
static esp_err_t write_ndef(const card_t *c, const char *uid, const char *url) {
    if (c->sak != 0 || !ntag21x()) return ESP_ERR_NOT_SUPPORTED;
    uint8_t header[16], locks[16], saved[PET_NDEF_BYTES], target[PET_NDEF_BYTES], verify[PET_NDEF_BYTES];
    TRY(read_data(0, header));
    /* NTAG213 only; all static user-page locks must be clear. */
    if (header[14] != 0x12 || (header[10] & 0xf0) || header[11]) return ESP_ERR_NOT_SUPPORTED;
    TRY(read_data(40, locks));
    if (locks[0] || (locks[1] & 0x0f)) return ESP_ERR_INVALID_STATE;
    TRY(read_user_image(saved));
    if (!pet_ndef_image(saved, url, target)) return ESP_ERR_INVALID_ARG;
    ESP_LOGI(TAG, "NDEF BACKUP UID=%s pages 4-39", uid);
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, saved, sizeof(saved), ESP_LOG_INFO);
    /* Publish zero length first. Commit the final length only after all other
     * pages are written, so a removed tag does not advertise a partial URL. */
    uint8_t empty_page[4] = {target[4], 0x03, 0, 0xfe};
    TRY(write_user_page(5, empty_page));
    for (unsigned offset = 0; offset < PET_NDEF_BYTES; offset += 4) {
        if (offset == 4 || !memcmp(saved + offset, target + offset, 4)) continue;
        TRY(write_user_page(4 + offset / 4, target + offset));
    }
    TRY(write_user_page(5, target + 4));
    TRY(read_user_image(verify));
    if (memcmp(verify, target, sizeof(target))) return ESP_ERR_INVALID_RESPONSE;
    ESP_LOGI(TAG, "NDEF %s VERIFIED UID=%s%s%s", url ? "URL" : "ERASE", uid,
             url ? " URL=" : "", url ? url : "");
    return ESP_OK;
}
/* Auto-provision only a canonically empty NDEF image. Existing NDEF data is
 * never replaced by the normal Card time flow. */
static void create_card_on_blank_tag(const card_t *c, const char *uid) {
    if (!pet_cloud_card_armed()) return;
    if (!strcmp(auto_write_failed_uid, uid)) return;
    uint8_t image[PET_NDEF_BYTES];
    if (read_user_image(image) != ESP_OK || !empty_ndef(image)) return;
    char url[PET_NDEF_URL_MAX + 1];
    if (!pet_cloud_card_url(url, sizeof(url))) {
        pet_cloud_card_error("Creating the card page. Keep the blank tag here.");
        return;
    }
    esp_err_t e = write_ndef(c, uid, url);
    if (e != ESP_OK) {
        ESP_LOGW(TAG, "Automatic card write failed UID=%s: %s", uid, esp_err_to_name(e));
        strlcpy(auto_write_failed_uid, uid, sizeof(auto_write_failed_uid));
        pet_cloud_card_error("Remove it. Try a blank, writable NTAG213 sticker.");
        return;
    }
    auto_write_failed_uid[0] = 0;
    pet_cloud_card(url);
}
static bool valid_uid(const char *uid) {
    size_t n = strlen(uid);
    if (n != 8 && n != 14 && n != 20) return false;
    for (size_t i = 0; i < n; i++)
        if (!((uid[i] >= '0' && uid[i] <= '9') || (uid[i] >= 'A' && uid[i] <= 'F'))) return false;
    return true;
}
static void console_poll(void) {
    static char line[256]; static size_t used; static bool overflow;
    char ch;
    while (usb_serial_jtag_read_bytes(&ch, 1, 0) == 1) {
        if (ch == '\r' || ch == '\n') {
            line[used] = 0;
            if (!overflow && !strncmp(line,"pet-cloud ",10)) {
                char *key=strchr(line+10,' ');bool ok=false;
                if(key){*key++=0;ok=pet_cloud_configure(line+10,key);}
                memset(line,0,sizeof(line));used=0;overflow=false;
                ESP_LOGI(TAG,"Public Hello setup %s",ok?"saved":"rejected (finish current Hello and sync cards first)");continue;
            }
            if(!overflow && (!strcmp(line,"pet-wifi-scan")||!strcmp(line,"pet-hello")||!strcmp(line,"pet-cancel")||!strcmp(line,"pet-card-arm")||!strcmp(line,"pet-card-save")||!strcmp(line,"pet-done")||!strcmp(line,"pet-status"))){
                if(!strcmp(line,"pet-wifi-scan"))pet_services_scan();
                else if(!strcmp(line,"pet-hello"))pet_services_hello_start();
                else if(!strcmp(line,"pet-cancel"))pet_services_hello_cancel();
                else if(!strcmp(line,"pet-card-arm"))pet_services_card_arm();
                else if(!strcmp(line,"pet-card-save"))pet_services_card_save();
                else if(!strcmp(line,"pet-done"))pet_services_accept();
                pet_status_t status;pet_services_snapshot(&status);
                ESP_LOGI(TAG,"PUBLIC HELLO wifi=%d starting=%d active=%d submitted=%d card=%u pending=%u",status.wifi_connected,status.hello_starting,status.hello_active,status.introduction_ready,status.card_stage,status.pending_cards);
                if(!strcmp(line,"pet-status")){ESP_LOGI(TAG,"WIFI STATUS=%s networks=%u scanning=%d",status.wifi_status,status.network_count,status.scanning);for(unsigned i=0;i<status.network_count;i++)ESP_LOGI(TAG,"NETWORK %s %d",status.networks[i].ssid,status.networks[i].rssi);}
                if(status.hello_url[0])ESP_LOGI(TAG,"HELLO URL=%s",status.hello_url);
                if(status.card_status[0])ESP_LOGI(TAG,"CARD STATUS=%s",status.card_status);
                memset(line,0,sizeof(line));used=0;overflow=false;continue;
            }
            char *uid = NULL, *url = NULL;
            int op = OP_TEST;
            if (!overflow) {
                if (!strncmp(line, "rfid-test ", 10)) uid = line + 10;
                else if (!strncmp(line, "rfid-url ", 9)) {uid = line + 9; op = OP_URL;}
                else if (!strncmp(line, "rfid-erase ", 11)) {uid = line + 11; op = OP_ERASE;}
            }
            if (uid && op == OP_URL) {
                url = strchr(uid, ' ');
                if (url) *url++ = 0;
            }
            bool valid = uid && valid_uid(uid);
            if (valid && op == OP_URL) {
                uint8_t original[PET_NDEF_BYTES] = {1,3,0xa0,0x0c,0x34,3}, image[PET_NDEF_BYTES];
                valid = url && strlen(url) <= PET_NDEF_URL_MAX && pet_ndef_image(original, url, image);
            }
            if (valid) {
                strcpy(pending_uid, uid); pending_op = op;
                if (op == OP_URL) strcpy(pending_url, url);
                pending_until = esp_timer_get_time() + 30000000;
                ESP_LOGI(TAG, "RFID operation=%d armed for UID=%s for 30 seconds", op, uid);
            } else if (used) ESP_LOGW(TAG, "Invalid RFID command; nothing armed");
            used = 0; overflow = false;
        } else if (used < sizeof(line) - 1) line[used++] = ch;
        else overflow = true;
    }
}
static void worker(void *arg) {
    esp_err_t e=init_reader();
    if(e!=ESP_OK){ESP_LOGE(TAG,"Reader initialization failed: %s",esp_err_to_name(e));vTaskDelete(NULL);return;}
    usb_serial_jtag_driver_config_t cfg={.tx_buffer_size=256,.rx_buffer_size=256};
    bool console=usb_serial_jtag_driver_install(&cfg)==ESP_OK;
    if(!console)ESP_LOGW(TAG,"USB command input unavailable; reads only");
    char last[21]="";unsigned misses=0;
    for(;;) {
        if(console)console_poll();
        if(pending_uid[0] && esp_timer_get_time()>pending_until){pending_uid[0]=0;ESP_LOGW(TAG,"Write test expired");}
        card_t c;e=select_card(&c);
        if(e==ESP_OK) {
            char uid[21];uid_string(&c,uid);misses=0;
            if(strcmp(last,uid)!=0) {
                if(strcmp(auto_write_failed_uid,uid))auto_write_failed_uid[0]=0;
                ESP_LOGI(TAG,"CARD UID=%s ATQA=%02X%02X SAK=%02X type=%s",uid,c.atqa[1],c.atqa[0],c.sak,classic(&c)?"MIFARE Classic":c.sak==0?"Type 2 candidate":"other ISO14443A");
                if(pet_cloud_clear_requested()) {
                    esp_err_t cleared=write_ndef(&c,uid,NULL);
                    pet_cloud_clear_result(cleared==ESP_OK);
                    if(cleared!=ESP_OK)ESP_LOGW(TAG,"User-requested tag clear failed UID=%s: %s",uid,esp_err_to_name(cleared));
                    else ESP_LOGI(TAG,"User-requested NDEF clear verified UID=%s",uid);
                    strcpy(last,uid);halt_card();continue;
                }
                uint8_t data[16];esp_err_t r=classic(&c)?authenticate(&c):ESP_OK;
                if(!classic(&c) && c.sak!=0)r=ESP_ERR_NOT_SUPPORTED;
                if(r==ESP_OK)r=read_data(4,data);
                if(r==ESP_OK){ESP_LOGI(TAG,"READ %s 4: CRC verified",classic(&c)?"block":"pages starting at");ESP_LOG_BUFFER_HEX_LEVEL(TAG,data,16,ESP_LOG_INFO);}
                else ESP_LOGW(TAG,"Read failed: %s (Classic requires factory key FFFFFFFFFFFF)",esp_err_to_name(r));
                if(c.sak==0 && r==ESP_OK){
                    uint8_t memory[PET_NDEF_BYTES];char url[PET_NDEF_URL_MAX+1];
                    if(read_user_image(memory)!=ESP_OK){
                        ESP_LOGW(TAG,"Card URL read failed UID=%s",uid);
                        pet_cloud_card_error("Could not read card. Remove it, then hold it on the reader.");
                    }else if(pet_ndef_url(memory,sizeof(memory),url,sizeof(url))){
                        ESP_LOGI(TAG,"Card URL read UID=%s",uid);
                        pet_cloud_card(url);
                    }else{
                        ESP_LOGW(TAG,"No supported NDEF URL UID=%s",uid);
                        create_card_on_blank_tag(&c, uid);
                    }
                }else if(c.sak!=0)pet_cloud_card_error("This card type is not supported. Use a writable NTAG213 sticker.");
                else pet_cloud_card_error("Could not read card. Remove it, then hold it on the reader.");
                strcpy(last,uid);
                /* Reselect after a read/auth so test begins in a known state. */
                halt_card();
            } else {
                create_card_on_blank_tag(&c, uid);
                if(pending_uid[0] && strcmp(pending_uid,uid)==0) {
                    pending_uid[0]=0;
                    if (pending_op == OP_TEST) test_write(&c, uid);
                    else {
                        esp_err_t result = write_ndef(&c, uid, pending_op == OP_URL ? pending_url : NULL);
                        if (result != ESP_OK) ESP_LOGE(TAG, "NDEF operation FAILED: %s; keep tag present and retry", esp_err_to_name(result));
                    }
                }
                halt_card();
            }
        } else if(++misses>=3) {
            if(last[0]){ESP_LOGI(TAG,"Card removed or no longer readable");last[0]=0;auto_write_failed_uid[0]=0;}
            if(misses%20==0)ESP_LOGW(TAG,"Waiting for card: %s; use one card",esp_err_to_name(e));
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}
void pet_rfid_start(void) {
    if(xTaskCreate(worker,"pet_rfid",6144,NULL,3,NULL)!=pdPASS)ESP_LOGE(TAG,"Cannot start reader task");
}
