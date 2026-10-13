"""Compile the driver's actual CRC functions against ISO14443A vectors."""
from pathlib import Path
import subprocess
import tempfile
source = Path('firmware/main/pet_rfid.c').read_text()
start = source.index('static void crc_a(')
end = source.index('static esp_err_t exchange(', start)
program = '#include <stdint.h>\n#include <stddef.h>\n#include <stdbool.h>\n#include <assert.h>\n' + source[start:end] + r'''
int main(void) {
    uint8_t halt[] = {0x50, 0x00, 0x57, 0xcd};
    uint8_t read[] = {0x30, 0x04, 0x26, 0xee};
    assert(crc_ok(halt, sizeof halt));
    assert(crc_ok(read, sizeof read));
    for (unsigned i=0;i<sizeof read;i++) {
        for(unsigned bit=0;bit<8;bit++) {
            read[i]^=1u<<bit; assert(!crc_ok(read,sizeof read)); read[i]^=1u<<bit;
        }
    }
    assert(!crc_ok(read, 0)); assert(!crc_ok(read, 1)); assert(!crc_ok(read, 2));
}
'''
with tempfile.TemporaryDirectory() as d:
    c=Path(d)/'crc.c';exe=Path(d)/'crc';c.write_text(program)
    subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('CRC vectors, bit corruption, and short frame tests passed')
