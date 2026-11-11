// Un finto bootloader Sinowealth in memoria, per provare web/js/isp.js senza tastiera.
//
// Imita quello che sinowisp si aspetta (e che abbiamo letto nel suo sorgente):
//  - ERASE riempie l'area di 0xFF e rimette il vettore di reset sul bootloader (02 F0 00);
//  - INIT_WRITE/INIT_READ fissano l'indirizzo, le pagine avanzano da sole;
//  - alla scrittura i primi tre byte a 0x0000 sono IGNORATI (restano i suoi);
//  - alla lettura i byte 1 e 2 sono quelli salvati a FIRMWARE_SIZE-4/-3 e quei due byte finali si leggono come zero;
//  - ENABLE piazza l'LJMP a FIRMWARE_SIZE-5 verso l'indirizzo salvato; REBOOT fa sparire il dispositivo.
// Registra ogni operazione in `log` per controllare l'ordine.
import { CMD_ERASE, CMD_INIT_WRITE, CMD_INIT_READ, CMD_ENABLE, CMD_REBOOT, XFER_WRITE, XFER_READ, PAGE_SIZE, FIRMWARE_SIZE } from "../../web/js/isp.js";

export class FakeBootloader {
  constructor({ corruptAt = -1, rebootThrows = true } = {}) {
    this.mem = new Uint8Array(FIRMWARE_SIZE).fill(0xaa); // contenuto "vecchio" qualunque
    this.log = [];
    this.writeAddress = 0;
    this.readAddress = 0;
    this.enabled = false;
    this.disconnected = false;
    this.corruptAt = corruptAt;       // se >= 0: quel byte viene danneggiato alla scrittura (prova della verifica)
    this.rebootThrows = rebootThrows; // il comando REBOOT fallisce come un dispositivo che sparisce
  }

  async command(bytes) {
    if (this.disconnected) throw new Error("device disconnected");
    if (bytes.length !== 5) throw new Error("un comando ha 5 byte di dati");
    const [cmd, lo, hi] = bytes;
    const address = lo | (hi << 8);
    switch (cmd) {
      case CMD_ERASE:
        this.mem.fill(0xff);
        this.mem.set([0x02, 0xf0, 0x00], 0); // il vettore di reset torna sul bootloader
        this.enabled = false;
        this.log.push("erase");
        break;
      case CMD_INIT_WRITE: this.writeAddress = address; this.log.push(`init-write ${address}`); break;
      case CMD_INIT_READ: this.readAddress = address; this.log.push(`init-read ${address}`); break;
      case CMD_ENABLE:
        this.mem[FIRMWARE_SIZE - 5] = 0x02; // LJMP verso l'indirizzo salvato in FIRMWARE_SIZE-4/-3
        this.enabled = true;
        this.log.push("enable");
        break;
      case CMD_REBOOT:
        this.log.push("reboot");
        this.disconnected = true;
        if (this.rebootThrows) throw new Error("Failed to write the feature report: device disconnected");
        break;
      default: throw new Error(`comando sconosciuto ${cmd}`);
    }
  }

  async writePage(bytes) {
    if (bytes.length !== 1 + PAGE_SIZE || bytes[0] !== XFER_WRITE) throw new Error("pagina scritta con forma sbagliata");
    for (let i = 0; i < PAGE_SIZE; i++) {
      const at = this.writeAddress + i;
      if (at < 3) continue; // i primi tre byte a 0x0000 restano quelli del bootloader
      this.mem[at] = bytes[1 + i];
    }
    if (this.corruptAt >= this.writeAddress && this.corruptAt < this.writeAddress + PAGE_SIZE) this.mem[this.corruptAt] ^= 0xff;
    this.log.push(`write ${this.writeAddress / PAGE_SIZE}`);
    this.writeAddress += PAGE_SIZE;
  }

  async readPage() {
    const out = new Uint8Array(1 + PAGE_SIZE);
    out[0] = XFER_READ;
    for (let i = 0; i < PAGE_SIZE; i++) {
      const at = this.readAddress + i;
      let value = this.mem[at];
      if (at === 1 || at === 2) value = this.mem[FIRMWARE_SIZE - 4 + (at - 1)]; // il vettore di reset "letto" è quello salvato
      if (at === FIRMWARE_SIZE - 4 || at === FIRMWARE_SIZE - 3) value = 0;      // e il posto dove è salvato si legge come zero
      out[1 + i] = value;
    }
    this.log.push(`read ${this.readAddress / PAGE_SIZE}`);
    this.readAddress += PAGE_SIZE;
    return out;
  }
}
