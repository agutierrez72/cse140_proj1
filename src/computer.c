#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include "computer.h"
#undef mips			/* gcc already has a def for mips */


unsigned int endianSwap(unsigned int);

// Personal functions
void getOpName(char *, DecodedInstr*);
void helpMessage();
// Function Definitions
void PrintInfo (int changedReg, int changedMem);
// Five stages
unsigned int Fetch (int);
void Decode (unsigned int, DecodedInstr*, RegVals*);
int Execute (DecodedInstr*, RegVals*);
int Mem(DecodedInstr*, int, int *);
void RegWrite(DecodedInstr*, int, int *);
void UpdatePC(DecodedInstr*, int);
void PrintInstruction (DecodedInstr*);

/*Globally accessible Computer variable*/
Computer mips;
RegVals rVals;

/** 
  * Trying to make interactive mode user friendly 
  * Why you may ask?  "\_(ʘ_ʘ)_/" I DON'T KNOW...it helps calm down the nervers I guess.
  * Thank you for reading this pointless comment...it really means alot to me. No very professional I know, but fun :)
*/
void helpMessage(){
    printf("Type 'm' to print memory\n");
    printf("     'r' to print registers  \n");
    printf("     'h' to print help\n");
    printf("     'p' to print info\n");
    printf("     'q' to quit\n\n");
    printf("Press Enter for next instruction\n");
}

/*
 *  Return an initialized computer with the stack pointer set to the
 *  address of the end of data memory, the remaining registers initialized
 *  to zero, and the instructions read from the given file.
 *  The other arguments govern how the program interacts with the user.
 */
void InitComputer (FILE* filein, int printingRegisters, int printingMemory,
        int debugging, int interactive) {
    int k;
    unsigned int instr;

    /* Initialize registers and memory */

    for (k=0; k<32; k++) {
        mips.registers[k] = 0;
    }

    /* stack pointer - Initialize to highest address of data segment */
    mips.registers[29] = 0x00400000 + (MAXNUMINSTRS+MAXNUMDATA)*4;

    for (k=0; k<MAXNUMINSTRS+MAXNUMDATA; k++) {
        mips.memory[k] = 0;
    }

    k = 0;
    while (fread(&instr, 4, 1, filein)) {
        /*swap to big endian, convert to host byte order. Ignore this.*/
        mips.memory[k] = ntohl(endianSwap(instr));
        k++;
        if (k>MAXNUMINSTRS) {
            fprintf (stderr, "Program too big.\n");
            exit (1);
        }
    }

    mips.printingRegisters = printingRegisters;
    mips.printingMemory = printingMemory;
    mips.interactive = interactive;
    mips.debugging = debugging;
}

unsigned int endianSwap(unsigned int i) {
    return (i>>24)|(i>>8&0x0000ff00)|(i<<8&0x00ff0000)|(i<<24);
}

/*
 *  Run the simulation.
 */
void Simulate () {
    char s[40];  /* used for handling interactive input */
    unsigned int instr;
    int changedReg=-1, changedMem=-1, val;
    DecodedInstr d;


    /* Initialize the PC to the start of the code section */
    mips.pc = 0x00400000;

    /** 
     * tmps use for interactive mode
     * easier debugging in interactive mode
     */
    int tmp_prMemory= mips.printingMemory;
    int tmp_prRegisters = mips.printingRegisters;

    /*** 
      * We don't want to display the help message 
      * if interactive mode is off
      */
    if (mips.interactive)
        helpMessage();

    while (1) {
        if (mips.interactive) {
            printf ("> ");
            fgets (s,sizeof(s),stdin);
            if (s[0] == 'q') {
                printf("BYE BYE\n");
                return; // bye bye interactive mode...it was fun while it lasted :(
            }else if(s[0] == 'm'){
                mips.printingMemory = 1;
                mips.pc-=4; // update PC to stay with the current instruction
            }else if(s[0] == 'r'){
                mips.printingRegisters = 1;
                mips.pc-=4; // update PC to stay with the current instruction
            } else if(s[0] == 'p'){
                if(mips.pc == 0x00400000){
                    printf("No instructions have been executed\n");
                    continue;
                }

                printf ("Previous Executed instruction at %8.8x: %8.8x\n", mips.pc, instr);
                PrintInstruction(&d);
                PrintInfo (changedReg, changedMem); 
                continue;
            }else if(s[0] == 'h'){ 
                helpMessage();
                continue;
            }                      
        }

        /* Fetch instr at mips.pc, returning it in instr */
        instr = Fetch (mips.pc);                                                                               

        printf ("Executing instruction at %8.8x: %8.8x\n", mips.pc, instr);

        /* 
         * Decode instr, putting decoded instr in d
         * Note that we reuse the d struct for each instruction.
         */
        Decode (instr, &d, &rVals);

        /*Print decoded instruction*/
        PrintInstruction(&d);

        /* 
         * Perform computation needed to execute d, 
         * returning computed value in val 
         */
        val = Execute(&d, &rVals);

        UpdatePC(&d,val);

        /* 
         * Perform memory load or store. Place the
         * address of any updated memory in *changedMem, 
         * otherwise put -1 in *changedMem. 
         * Return any memory value that is read, otherwise return -1.
         */
        val = Mem(&d, val, &changedMem);

        /* 
         * Write back to register. If the instruction modified a register--
         * (including jal, which modifies $ra) --
         * put the index of the modified register in *changedReg,
         * otherwise put -1 in *changedReg.
         */
        RegWrite(&d, val, &changedReg);

        PrintInfo (changedReg, changedMem);

        printf("\n"); // printing new line to separate the instructions

        // For Debugging Purposes :p
        mips.printingMemory = tmp_prMemory;
        mips.printingRegisters = tmp_prRegisters;
    }
}

/*
 *  Print relevant information about the state of the computer.
 *  changedReg is the index of the register changed by the instruction
 *  being simulated, otherwise -1.
 *  changedMem is the address of the memory location changed by the
 *  simulated instruction, otherwise -1.
 *  Previously initialized flags indicate whether to print all the
 *  registers or just the one that changed, and whether to print
 *  all the nonzero memory or just the memory location that changed.
 */
void PrintInfo ( int changedReg, int changedMem) {
    int k, addr;
    printf ("New pc = %8.8x\n", mips.pc);
    if (!mips.printingRegisters && changedReg == -1) {
        printf ("No register was updated.\n");
    } else if (!mips.printingRegisters) {
        printf ("Updated r%2.2d to %8.8x\n",
                changedReg, mips.registers[changedReg]);
    } else {
        for (k=0; k<32; k++) {
            printf ("r%2.2d: %8.8x  ", k, mips.registers[k]);
            if ((k+1)%4 == 0) {
                printf ("\n");
            }
        }
    }
    if (!mips.printingMemory && changedMem == -1) {
        printf ("No memory location was updated.\n");
    } else if (!mips.printingMemory) {
        printf ("Updated memory at address %8.8x to %8.8x\n",
                changedMem, Fetch (changedMem));
    } else {
        printf ("Nonzero memory\n");
        printf ("ADDR	  CONTENTS\n");
        for (addr = 0x00400000+4*MAXNUMINSTRS;
                addr < 0x00400000+4*(MAXNUMINSTRS+MAXNUMDATA);
                addr = addr+4) {
            if (Fetch (addr) != 0) {
                printf ("%8.8x  %8.8x\n", addr, Fetch (addr));
            }
        }
    }
}

/*
 *  Return the contents of memory at the given address. Simulates
 *  instruction fetch. 
 */
unsigned int Fetch ( int addr) {
    return mips.memory[(addr-0x00400000)/4];
}

// Get the op name 
void getOpName(char * funct_name, DecodedInstr* d){

    int funct = 0;
    int op = 0;

    switch (d->type) {

        case 0: // R-format functions

            funct = d->regs.r.funct; // get the fucnt from d , less typing

            switch (funct) {

                case 0:
                    strcpy(funct_name,"sll");
                    break;
                case 2:
                    strcpy(funct_name,"srl");
                    break;
                case 8:
                    strcpy(funct_name,"jr");
                    break;
                case 32:
                    strcpy(funct_name,"add");
                    break;
                case 33:
                    strcpy(funct_name,"addu");
                    break;
                case 34:
                    strcpy(funct_name,"sub");
                    break;
                case 35:
                    strcpy(funct_name,"subu");
                    break;
                case 36:
                    strcpy(funct_name,"and");
                    break;
                case 37:
                    strcpy(funct_name,"or");
                    break;
                case 39:
                    strcpy(funct_name,"nor");
                    break;
                case 42:
                    strcpy(funct_name,"slt");
                    break;
                case 43:
                    strcpy(funct_name,"sltu");
                    break;
            }
            break;

        case 1: // I-Format functions

            op = d->op;

            switch (op) {
                case 4:
                    strcpy(funct_name,"beq");
                    break;
                case 5:
                    strcpy(funct_name,"bne");
                    break;
                case 8:
                    strcpy(funct_name,"addi");
                    break;
                case 9:
                    strcpy(funct_name,"addiu");
                    break;
                case 10:
                    strcpy(funct_name,"slti");
                    break;
                case 11:
                    strcpy(funct_name,"sltiu");
                    break;
                case 12:
                    strcpy(funct_name,"andi");
                    break;
                case 13:
                    strcpy(funct_name,"ori");
                    break;
                case 15:
                    strcpy(funct_name,"lui");
                    break;
                case 35:
                    strcpy(funct_name,"lw");
                    break;
                case 43:
                    strcpy(funct_name,"sw");
                    break;
            }

        case 2: // J-format functions
            op = d->op;

            switch (op) {
                case 2:
                    strcpy(funct_name,"j");
                    break;
                case 3:
                    strcpy(funct_name,"jal");
                    break;
            }
            break;

    }
}

/* Decode instr, returning decoded instruction. */
void Decode ( unsigned int instr, DecodedInstr* d, RegVals* rVals) {

    int inst_op = instr>>26;  // get the first 6bits
    d->op = inst_op;

    /** Retrive Regiters **/
    //If op is 0 then R-format
    if(inst_op == 0) {

        // declare the instruction to be R-format
        d->type = 0; 

        // Get R-format registers
        RRegs rregs;
        // shifting and masking to get certain values from the instruction
        rregs.rs    = (instr >> 21) & BITS_5;
        rregs.rt    = (instr >> 16) & BITS_5;
        rregs.rd    = (instr >> 11) & BITS_5;
        rregs.shamt = (instr >> 6) & BITS_5;
        rregs.funct = instr & BITS_6;

        // put values into rVals
        rVals->R_rs = rregs.rs;
        rVals->R_rt = rregs.rt;
        rVals->R_rd = rregs.rd;

        // put values in d
        d->regs.r = rregs;

        //Else if op is 2 or 3 then J format  
    }else if(inst_op == 2 || inst_op == 3 ) {

        // declare the instruction to be J-format
        d->type = 2; 

        //get addr
        JRegs jregs;
        jregs.target = instr & BITS_26;

        // nothing to put here
        rVals->R_rs = -1;
        rVals->R_rt = -1;
        rVals->R_rd = jregs.target;

        d->regs.j = jregs;

        //Else is I format
    }else {

        // declare the instruction to be I-format
        d->type = 1;

        // Get I-format registers and immed
        IRegs iregs;
        iregs.rs = (instr >> 21) & BITS_5;
        iregs.rt = (instr >> 16) & BITS_5;
        iregs.addr_or_immed = instr & BITS_16;

        // put values in rVals
        rVals->R_rs = iregs.rs;
        rVals->R_rt = iregs.rt;
        rVals->R_rd = iregs.addr_or_immed;

        // put values in d
        d->regs.i = iregs;
    }
}


/*
 *  Print the disassembled version of the given instruction
 *  followed by a newline.
 */
void PrintInstruction ( DecodedInstr* d) {

    char opName[6] = " ";
    getOpName(opName, d);

    printf("%s ", opName);

    switch (d->type) {

        case 0: // Print R-format instructions
            if(!strcmp(opName, "jr")){ // if jr then only print two values
                printf(" %#010x\n", d->regs.r.rs);
            }else{
                printf(" $%d, $%d, $%d\n", d->regs.r.rs,d->regs.r.rt,d->regs.r.rd);
            }
            break;
        case 1: // Print I-type instructions
            if(!strcmp(opName, "lui")) {

                printf(" $%d, %#010x\n", d->regs.i.rt,d->regs.i.addr_or_immed);

            } else if(!strcmp(opName, "lw") || !strcmp(opName, "sw")){
                printf(" $%d, %#010x($%d)\n", d->regs.i.rt,d->regs.i.addr_or_immed, d->regs.i.rs);

            } else {
                printf(" $%d, $%d, %#010x\n", 
                        d->regs.i.rt,d->regs.i.rs,d->regs.i.addr_or_immed);
            }
            break;
        case 2: // Print J-type instructions
            printf("%#010x\n", d->regs.j.target); 
            break;

    }
}

/* Perform computation needed to execute d, returning computed value */
int Execute ( DecodedInstr* d, RegVals* rVals) {

    /**
      * TODO:
      *** 
      * Whatever is done mark it as done
      ***
Pretty sure the values were swapped here too like rd(first input register) is actually stored in rVals->R_rt
Will have to keep running debug until R-type is executed to see what values need to be changed, but close
As of now bne and beq are only returning t/f to rs == rt not sure if we should return jump addr instead, also again
 need to double check reg values are correctly used
      R-format instructions to implement
      --------------------------------
      * addu Rdest, Rsrc1, Rsrc2    - done
      * subu Rdest, Rsrc1, Rsrc2    - done
      * sll  Rdest, Rsrc, shamt
      * srl  Rdest, Rsrc, shamt
      * and  Rdest, Rsrc1, Rsrc2
      * or   Rdest, Rsrc1, Rsrc2
      * slt  Rdest, Rsrc1, Rsrc2

 So the inputs were a little funky you can see I put comments about the values being taken in for rs and rt,
 for sure sorted for I format, we would just need to talk out the names to make sure all the rVals are labeled correctly
      I-format instructions to implement
      --------------------------
      * ori   Rdest, Rsrc, imm
      * andi  Rdest, Rsrc, imm      - done
      * addiu Rdest, Rsrc1, imm     - done
      * lui   Rdest, imm
      * beq   Rsrc1, Rsrc2, raddr
      * bne   Rsrc1, Rsrc2, raddr
      * lw    Rdest, offset (Radd)
      * sw    Rdest, offset (Radd) 


     Need to confirm which addresses should be send to PC but its close
      J-format instructions to implement
      --------------------------
      * j   address
      * jal address                 - done
      * jr  Rsrc
      

      */
 
 
       /* addu, addiu, subu, sll, srl, and, andi, or, ori, lui, slt,
``     * beq, bne, jal, jr, lw, sw */
        //d has all register number, rVals should hold numerical value

        int rd = 0; //return value sent to register
        switch(d->type){
                //Need to get type to know how to get registers & their values
                case 0: 
                        switch(d->op){
                                case 0: //sll rd, rs, shampt (rd = rt<<shampt)
                                        rd = (rVals->R_rs)<<(d->regs.r.shamt); 
                                        return rd;
                                        break;
                                case 2: //srl rd, rs, shampt (rd = rt>>shampt)
                                        rd = (rVals->R_rs)>>(d->regs.r.shamt);
                                        return rd;      
                                        break;
                                case 8: //jr rs (PC = JumpAddr)
                                        return d->regs.j.target;
                                        break;
                                case 21://addu rd, rs, rt (rd = rs + rt)
                                        rd = (rVals->R_rs) + (rVals->R_rt);
                                        return rd;
                                        break;
                                case 23://subu rd, rs, rt (rd = rs - rt)
                                        rd = (rVals->R_rs) -(rVals->R_rt);
                                        return rd;
                                        break;
                                case 24://and rd, rs, rt (rd = rs & rt)
                                        rd = (rVals->R_rs) & (rVals->R_rt);
                                        return rd;
                                        break;
                                case 25://or rd, rs, rt (rd = rs | rt)
                                        rd = (rVals->R_rs) | (rVals->R_rt);
                                        return rd;
                                        break;
                                case 43://slt rd, rs, rt (rd = rs < rt)
                                        if(rVals->R_rs < rVals->R_rt)
                                                return 1;
                                        else
                                                return 0;
                                        break;
                        }
                        break;
               case 1:
                        switch(d->op){
                                case 4: //beq rs, rt, raddr (if rs == rt --> PC+4+addr)
                                        return (rVals->R_rt == rVals->R_rs);
                                        break;
                                case 5://bne rs, rt, raddr (if rs != rt --> PC+4+addr)
                                        return (rVals->R_rt != rVals->R_rs);
                                        break;
                                case 9://addiu rt, rs, imm (rt = rs + imm)
                                        //Note: names are wrong rt holds the second given reg,
                                        //and rs holds the first given register
                                        rd = (rVals->R_rt + d->regs.i.addr_or_immed);
                                        return rd;      
                                        break;
                                case 12://andi rd, rs, imm (rt = rs & zeroExtImm)
                                        rd = (rVals->R_rt) & (d->regs.i.addr_or_immed);
                                        return rd;
                                        break;
                                case 13://ori (rt = rs | zeroExtImm)
                                        rd = (rVals->R_rt) | (d->regs.i.addr_or_immed);
                                        return rd;
                                        break;
                                case 15://lui rd, imm (rt = imm<<16)
                                        rd = (d->regs.i.addr_or_immed)<<16;
                                        return rd;
                                        break;
                        }
                        break;
                case 2:
                        switch(d->op){
                                case 2://j (pc = jumpAddr)
                                        return (d->regs.j.target);
                                        break;
                                case 3://jal (r31 = pc, pc =target <<2)
                                        rd = (d->regs.j.target<<2);
                                        return rd;
                                        break;
                        }
        }


     return 0;

 
    /*char opName[6] = " ";
    getOpName(opName, d);

    // R-format
    switch (d->type) {

        case 0: // R-format instructions

            // Most of the code below is wrong...we need to fix :/
            if(!strcmp(opName, "addu") || !strcmp(opName, "add")) {
                return rVals->R_rs + rVals->R_rt;        
            } else if(!strcmp(opName, "subu") || !strcmp(opName, "sub")) {
                return rVals->R_rs - rVals->R_rt;        
            } else if(!strcmp(opName, "sltu") || !strcmp(opName, "slt") ) {
                return rVals->R_rs < rVals->R_rt;        
            } else if(!strcmp(opName, "and")) {
                return rVals->R_rs & rVals->R_rt;        
            } else if(!strcmp(opName, "or")) {
                return rVals->R_rs | rVals->R_rt;        
            } else if(!strcmp(opName, "nor")) {
                return ~(rVals->R_rs | rVals->R_rt);        
            }else if(!strcmp(opName, "sll")) {
                return rVals->R_rs - rVals->R_rt;        
            } else if(!strcmp(opName, "srl")) {
                return rVals->R_rs - rVals->R_rt;        
            } else if(!strcmp(opName, "jr")) {
                return rVals->R_rs - rVals->R_rt;        
            }         
            break;

        case 1: // I-type instructions

            // this works, but we need to add
            if(!strcmp(opName, "addi") || !strcmp(opName, "addi") ){
                return rVals->R_rs + rVals->R_rd;        
            }
            break;
        case 2: // J-type instructions --might need some work
            
            return d->regs.j.target;
            break;

    }

    return 0;*/
}

/* 
 * Update the program counter based on the current instruction. For
 * instructions other than branches and jumps, for example, the PC
 * increments by 4 (which we have provided).
 */
void UpdatePC ( DecodedInstr* d, int val) {
    mips.pc+=4;

    char opName[6] = " ";
    getOpName(opName, d);

    switch (d->type) {

        case 0: // R-format instructions
            // Since R-format do nothing with PC(program counter) return
            return; 
            break;

        case 1: // I-type instructions

            // There are a few I-format instructions that mess with the PC
            if(!strcmp(opName, "beq")){

                printf("pc change beq");

            } else if (!strcmp(opName, "bne" )) {

                printf("pc changed bne");

            } else if (!strcmp(opName, "lw" )) {

                printf("pc changed lw");

            } else if (!strcmp(opName, "sw" )) {

                printf("pc changed sw");

            } 

            break;

        case 2: // J-type instructions
            /**
              * We just need to pass the target address
              * since Mars already did all the calculation
              */
            if(!strcmp(opName, "jal")){
                mips.pc = val<<2;
            }

            // All J-type instructions mess with the PC
            break;

    }
}

/*
 * Perform memory load or store. Place the address of any updated memory 
 * in *changedMem, otherwise put -1 in *changedMem. Return any memory value 
 * that is read, otherwise return -1. 
 *
 * Remember that we're mapping MIPS addresses to indices in the mips.memory 
 * array. mips.memory[0] corresponds with address 0x00400000, mips.memory[1] 
 * with address 0x00400004, and so forth.
 *
 */
int Mem( DecodedInstr* d, int val, int *changedMem) {
    
    char opName[6] = " ";
    getOpName(opName, d);

    // R-format
    switch (d->type) {

        case 0: // R-format instructions
            *changedMem = -1;
            break;

        case 1: // I-type instructions
            if (!strcmp(opName, "lw" )) {

                printf("pc changed lw");

            } else if (!strcmp(opName, "sw" )) {

                printf("pc changed sw");

            } else {
                *changedMem = -1;
                return val;
            }
           
            break;

        case 2: // J-type instructions
            *changedMem = -1;
            break;

    }
    return 0;
}

/* 
 * Write back to register. If the instruction modified a register--
 * (including jal, which modifies $ra) --
 * put the index of the modified register in *changedReg,
 * otherwise put -1 in *changedReg.
 */
void RegWrite( DecodedInstr* d, int val, int *changedReg) {
    char opName[6] = " ";
    getOpName(opName, d);

    // R-format
    switch (d->type) {

        case 0: // R-format instructions
            mips.registers[d->regs.r.rd] = val;
            *changedReg = d->regs.r.rd;
            break;

        case 1: // I-type instructions
     
            mips.registers[d->regs.i.rt] = val;
            *changedReg = d->regs.i.rt;
            break;

        case 2: // J-type instructions
            if(!strcmp(opName, "jal")){
                mips.registers[31] = mips.pc;
                *changedReg = 31;
            }

            break;

    }
}
