#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <stdlib.h>

#define OPCODE(instr)  (instr >> 12 & 0x000F)
#define DSTREG(instr)  (instr >> 9 & 0x0007)
#define SRCREG(instr)  (instr >> 6 & 0x0007)
#define BASEREG(instr)  (instr >> 6 & 0x0007)
#define SRCREG2(instr)  (instr & 0x0007)
#define IMMBIT(instr)  ((instr & 0x0020) >> 5)
#define NBIT(instr)  ((instr & 0x0800) >> 11)
#define ZBIT(instr)  ((instr & 0x0400) >> 10)
#define PBIT(instr)  ((instr & 0x0200) >> 9)
#define IMMVAL(instr)  ((instr << 27 ) >> 27)
#define OFFSET6(instr) ((instr << 26 ) >> 26)
#define PCOFFSET9(instr) ((instr << 23 ) >> 23)
#define PCOFFSET11(instr) ((instr << 21 ) >> 21)
#define SIGBIT(instr)  (((instr << 16 ) >> 31) & 0x0001)
#define TRAPVECT8(instr) ((instr << 24 ) >> 24)
#define DDRCHAR(instr) ((instr << 24 ) >> 24)

int16_t loadFiles(int argc, char **argv);

int16_t  memory[0x10000];
int16_t regs[8] = {0};
int16_t pc, ir;
int read_count;

struct {
    int junk:13;
    unsigned int p:1;
    unsigned int n:1;
    unsigned int z:1;
} psr;   // process status register

void setCC(int16_t dest_reg) {
    if (regs[dest_reg] == 0) {
        psr.z = 1;
        psr.n = 0;
        psr.p = 0;
    } else if (regs[dest_reg] < 0) {
        psr.z = 0;
        psr.n = 1;
        psr.p = 0;
    } else {   // else if > 0
        psr.z = 0;
        psr.n = 0;
        psr.p = 1;
    }
}

int main(int argc, char** argv) {
  int startAdd;
  pc = loadFiles(argc, argv);
  startAdd = pc;
  memory[0xFFFE] = 0x8000; //MCR
  // memory[0xFE04] = 0x8000; //DSR
   // main loop for fetching and executing instructions
   // for now, we do this until we run into the instruction with opcode 13
   // printf("%d\n",(unsigned short)SIGBIT(memory[0xFE04]) );
   psr.p = 1;  // need a valid psr
   while ((unsigned short)SIGBIT(memory[0xFFFE])) {   // one instruction executed on each rep.
        // getchar();
        memory[0xFE04] = 0x8000; // set DSR to ready
        ir = memory[pc]; //fetched the instruction
        int16_t opcode = OPCODE(ir);
    //    printf("pc = %04hx opcode = %02hx\n", pc, opcode);
        pc++;

        int16_t dest_reg, src_reg, src_reg2, base_reg,
                imm_bit, imm_val, pcoffset9, pcoffset11,
                offset6, trapvect8, nbit, zbit, pbit;

        switch(opcode) {
            case 0: // BR
                  nbit = NBIT(ir);
                  zbit = ZBIT(ir);
                  pbit = PBIT(ir);
                  pcoffset9 = PCOFFSET9(ir);

                  if (psr.n && nbit || psr.z && zbit || psr.p && pbit) {
                      pc = pc + pcoffset9;
                  }
                  break;

            case 9:  // NOT
                  dest_reg = DSTREG(ir);
                  src_reg = SRCREG(ir);
                  regs[dest_reg] = ~regs[src_reg];
                  setCC(dest_reg);
                  break;

            case 1:  // ADD
            case 5:  // AND
                  dest_reg = DSTREG(ir);
                  src_reg = SRCREG(ir);
                  imm_bit = IMMBIT(ir);
                  if (imm_bit) {
                      imm_val = IMMVAL(ir);
                      if (opcode == 1)
                          regs[dest_reg] = regs[src_reg] + imm_val;
                      else
                          regs[dest_reg] = regs[src_reg] & imm_val;
                  } else {
                      src_reg2 = SRCREG2(ir);
                      if (opcode == 1)
                          regs[dest_reg] = regs[src_reg] + regs[src_reg2];
                      else
                          regs[dest_reg] = regs[src_reg] & regs[src_reg2];
                  }
                  setCC(dest_reg);
                  break;

            case 2:
            case 10:
            case 14:
                  dest_reg = DSTREG(ir);
                  pcoffset9 = PCOFFSET9(ir);
                  if (opcode == 2)
                      regs[dest_reg] = memory[pc+pcoffset9];   // LD
                  else if (opcode == 14)
                      regs[dest_reg] = pc+pcoffset9; // LEA
                  else
                      regs[dest_reg] = memory[(unsigned short)memory[pc+pcoffset9]]; // LDI
                  setCC(dest_reg);
                  break;

            case 6:
                  dest_reg = DSTREG(ir);
                  base_reg = BASEREG(ir);
                  offset6 = OFFSET6(ir);
                  regs[dest_reg] = memory[regs[base_reg]+offset6];  // LDR
                  setCC(dest_reg);
                  break;

            case 3:
            case 11:
                  src_reg = DSTREG(ir);
                  pcoffset9 = PCOFFSET9(ir);
                  if (opcode == 3) {
                      memory[pc+pcoffset9] = regs[src_reg];   // ST
                      if ((pc+pcoffset9 == 0xFE06) && (unsigned short)SIGBIT(memory[0xFE04])){
                        printf("%c", DDRCHAR(memory[0xFE06]));
                       memory[0xFE04] = 0x0000;
                    }
                  }
                  else
                      memory[(unsigned short)memory[pc+pcoffset9]] = regs[src_reg]; // STI
                      if (((unsigned short)memory[pc+pcoffset9] == 0xFE06) && (unsigned short)SIGBIT(memory[0xFE04])){
                        printf("%c", DDRCHAR(memory[0xFE06]));
                       memory[0xFE04] = 0x0000;
                      }
                  break;

            case 7:  // STR
                  src_reg = DSTREG(ir);
                  base_reg = BASEREG(ir);
                  offset6 = OFFSET6(ir);
                  memory[regs[base_reg]+offset6] = regs[src_reg];  // STR
                  if ((regs[base_reg]+offset6 == 0xFE06) && (unsigned short)SIGBIT(memory[0xFE04])){
                    printf("%c", DDRCHAR(memory[0xFE06]));
                    memory[0xFE04] = 0x0000;
                  }
                  break;

            case 4: // JSR
                  pcoffset11 = PCOFFSET11(ir);
                  regs[7] = pc;
                  pc = pc + pcoffset11;
                  break;

            case 12: // RET
                  pc = regs[7];
                  break;

            case 15: //TRAP
                  regs[7] = pc;
                  trapvect8 = TRAPVECT8(ir);
                  pc = memory[trapvect8];
                  break;
        } // switch ends
        // puts("\nRegisters");
        // int k;
        // for(k = 0; k <= 7; k++)
        //   printf("R%d %04hx %d\n", k, regs[k], regs[k]);
    }
    // puts("\nExecution Completed\nRegisters");
    // int k;
    // for(k = 0; k <= 7; k++)
    //   printf("R%d %04hx %d\n", k, regs[k], regs[k]);
    // puts("\nMemory contents");
    // int j;
    //  for(j = startAdd; j < startAdd+read_count; j++)
    //   printf("%04hx %04hx\n", j, memory[j]);
}

int16_t loadFiles(int argc, char **argv) {
  int l, startAddr;
  for (l = 1; l < argc; l++) {
    // printf("\n%s\n", *(argv+l));

    char* fileName = *(argv+l);
    struct stat st;
    stat(fileName,&st);
    // printf("%d\n", st.st_size);

    FILE* infile;
    infile = fopen(fileName,"r");
    if (!infile) {
      printf("NO INPUT FILE\n");
      exit(1);
    }

    char twochars[2];
    int16_t starting_addr;
    read_count = fread(&starting_addr, sizeof(int16_t) , 1 , infile);

    //big switcheroo
    char *ptr = (char *)&starting_addr;
    char temp = *ptr;
    *ptr = *(ptr+1);
    *(ptr+1) = temp;
    // printf("read %d items, start addr = x%x\n", read_count, starting_addr);
    //  printf("start addr = x%x\n", starting_addr);

    read_count = fread(&memory[starting_addr], sizeof(int16_t) ,  (st.st_size-2)/2 , infile);
    //printf("number of instructions = %d\n", read_count);

    startAddr = starting_addr;
    int i;
    ptr = (char *)&memory[starting_addr];
    for (i = 0; i < read_count; i++) {
      char temp = *ptr;
      *ptr = *(ptr+1);
      *(ptr+1) = temp;
      ptr += 2;
    }
    int j;
     // for(j = starting_addr; j < starting_addr+read_count; j++)
     //  printf("%04hx %04hx\n", j, memory[j]);
  }
  // puts("");
  return startAddr;
}
